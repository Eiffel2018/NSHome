# SPDX-License-Identifier: GPL-2.0-only
"""NSHome TCP 6802 client. Python 3.10+, standard library, no automatic retries."""
from contextlib import contextmanager
import math
import json
import re
import socket
import time
import warnings

BUTTONS = (
    'A', 'B', 'X', 'Y', 'LSTICK', 'RSTICK', 'L', 'R', 'ZL', 'ZR',
    'PLUS', 'MINUS', 'LEFT', 'UP', 'RIGHT', 'DOWN',
    'HOME', 'CAPTURE', 'LSL', 'LSR', 'RSL', 'RSR',
)
STACKS = (0, 1, 2, 3, 4, 6)
MAX_JPEG = 512 * 1024


class ProtocolError(RuntimeError):
    """Malformed/truncated response; the operation may already have executed."""


class RemoteError(RuntimeError):
    """The server returned ERR. The full reply is the exception message."""


def integer(value, low, high, name):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f'{name} must be an integer in {low}..{high}')
    return value


def buttons(value):
    parts = value.split('+')
    if (not 1 <= len(parts) <= 4 or len(set(parts)) != len(parts)
            or any(p not in BUTTONS for p in parts)
            or (len(parts) > 1 and any(p not in BUTTONS[:16] for p in parts))):
        raise ValueError('Use a supported single button or 2-4 distinct main buttons joined by +')
    return value


def nro_path(value):
    path = value[5:] if value[:5].lower() == 'sdmc:' else value
    if (not path.startswith('/') or not path.lower().endswith('.nro')
            or any(ord(c) < 32 or ord(c) == 127 or c in '\\:"' for c in path)
            or any(p in ('', '.', '..') for p in path[1:].split('/'))
            or len(('sdmc:' + path).encode('utf-8')) >= 512):
        raise ValueError('Use an absolute SD .nro path without traversal (canonical UTF-8 length <512)')
    return 'sdmc:' + path


class Client:
    def __init__(self, host, code, port=6802, timeout=10.0):
        if not isinstance(code, str) or not re.fullmatch(r'[A-Za-z0-9_-]{4,64}', code):
            raise ValueError('Pairing code must contain 4-64 letters, digits, - or _')
        if not host:
            raise ValueError('Host is required')
        integer(port, 1, 65535, 'port')
        if not math.isfinite(timeout) or not 0 < timeout <= 120:
            raise ValueError('Timeout must be greater than 0 and at most 120 seconds')
        self.host, self.port, self.timeout = host, port, timeout
        self._code = code

    @staticmethod
    def _line(reader, limit=2048):
        data = reader.readline(limit + 1)
        if len(data) > limit or not data.endswith(b'\n'):
            raise ProtocolError('Incomplete or oversized response; do not replay the operation')
        try:
            return data.decode('utf-8').rstrip('\r\n')
        except UnicodeDecodeError as error:
            raise ProtocolError('Invalid response encoding') from error

    def _exchange(self, command=None, binary=False, line_limit=2048):
        # The server accepts exactly one authenticated operation per connection.
        if command is not None and (not command or len(command.encode()) > 528
                or any(ord(c) < 32 or ord(c) == 127 for c in command)):
            raise ValueError('Command must be one bounded line without control characters')
        with socket.create_connection((self.host, self.port), timeout=self.timeout) as sock:
            sock.settimeout(self.timeout)
            with sock.makefile('rb') as reader:
                if self._line(reader) != 'SWITCHU 1':
                    raise ProtocolError('Not an NSHome-compatible 6802 endpoint')
                sock.sendall(('auth ' + self._code + '\n').encode())
                if self._line(reader) != 'OK auth':
                    raise RemoteError('Authentication rejected')
                if command is None:
                    return 'OK auth'
                sock.sendall((command + '\n').encode('utf-8'))
                reply = self._line(reader, line_limit)
                if reply.startswith('ERR '):
                    raise RemoteError(reply)
                if not reply.startswith('OK '):
                    raise ProtocolError('Unexpected response: ' + reply)
                if not binary:
                    return reply
                match = re.fullmatch(
                    r'OK capture bytes=([0-9]+) width=1280 height=720 format=jpeg', reply)
                if not match or not 4 <= int(match[1]) <= MAX_JPEG:
                    raise ProtocolError('Invalid capture header or size')
                remaining, chunks = int(match[1]), []
                while remaining:
                    chunk = reader.read(remaining)
                    if not chunk:
                        raise ProtocolError('Truncated JPEG; no automatic retry')
                    chunks.append(chunk)
                    remaining -= len(chunk)
                image = b''.join(chunks)
                if not image.startswith(b'\xff\xd8') or not image.endswith(b'\xff\xd9'):
                    raise ProtocolError('Invalid JPEG envelope')
                return image

    def authenticate(self):
        return self._exchange()

    def status(self):
        """Launch queue status, not controller state or application readiness."""
        return self._exchange('status')

    @staticmethod
    def _decimal(value, name, allow_zero=False):
        text = str(value) if type(value) is int else value
        if (not isinstance(text, str) or not re.fullmatch(r'0|[1-9][0-9]{0,19}', text)
                or int(text) > 2**64-1 or (not allow_zero and text == '0')):
            raise ValueError(f'{name} must be an exact unsigned decimal integer')
        return text

    def _titles(self, command):
        reply = self._exchange(command, line_limit=65536)
        if not reply.startswith('OK titles '):
            raise ProtocolError('Unexpected title catalogue reply')
        try:
            def unique(pairs):
                out = {}
                for key, value in pairs:
                    if key in out:
                        raise ValueError('Duplicate JSON key')
                    out[key] = value
                return out
            result = json.loads(reply[10:], object_pairs_hook=unique)
            if (not isinstance(result, dict) or type(result.get('schema')) is not int
                    or result['schema'] != 1 or result.get('source') != 'ns+ncm.full_application_contents'
                    or result.get('state') not in ('idle', 'building', 'ready', 'failed', 'invalidated')
                    or type(result.get('page_max')) is not int or result['page_max'] != 16 or result.get('profile_selection_supported') is not False
                    or type(result.get('total')) is not int or not 0 <= result['total'] <= 4096
                    or type(result.get('scanned')) is not int or not result['total'] <= result['scanned'] <= 4096
                    or not re.fullmatch(r'0x[0-9A-F]{8}', result.get('rc', ''))):
                raise ValueError('Invalid catalogue schema')
            for name in ('boot', 'generation'):
                if not isinstance(result.get(name), str):
                    raise ValueError('Identity must be a decimal string')
                self._decimal(result[name], name, allow_zero=name == 'generation')
        except (TypeError, ValueError) as error:
            raise ProtocolError('Malformed title catalogue: ' + str(error)) from error
        return result

    def titles_refresh(self):
        """Queue a read-only background rebuild; no automatic resubmission."""
        return self._titles('titles refresh')

    def titles_status(self):
        return self._titles('titles status')

    def titles_page(self, boot, generation, offset=0, limit=16):
        boot = self._decimal(boot, 'boot')
        generation = self._decimal(generation, 'generation')
        integer(offset, 0, 4096, 'offset')
        integer(limit, 1, 16, 'limit')
        result = self._titles(f'titles page {boot} {generation} {offset} {limit}')
        rows = result.get('items')
        end = offset + len(rows) if isinstance(rows, list) else -1
        if (result['boot'] != boot or result['generation'] != generation or result['state'] != 'ready'
                or result['rc'] != '0x00000000' or type(result.get('offset')) is not int or result['offset'] != offset
                or not isinstance(rows, list) or len(rows) > limit or end > result['total']
                or (result.get('next') is not None and type(result['next']) is not int)
                or (result.get('next') != (end if end < result['total'] else None))
                or (end < result['total'] and not rows)):
            raise ProtocolError('Stale or inconsistent title page')
        seen = set()
        for row in rows:
            if (not isinstance(row, dict) or not isinstance(row.get('title_id'), str)
                    or not re.fullmatch(r'[0-9A-F]{16}', row['title_id'])
                    or row['title_id'] == '0000000000000000' or row['title_id'] in seen
                    or 'name' not in row or (row['name'] is not None and not isinstance(row['name'], str))
                    or row.get('name_source') not in ('unavailable', 'nacp.system_language', 'nacp.first_available')
                    or not isinstance(row.get('name_rc'), str) or not re.fullmatch(r'0x[0-9A-F]{8}', row['name_rc'])
                    or type(row.get('storage_id')) is not int or row['storage_id'] not in (2, 4, 5)):
                raise ProtocolError('Invalid installed title entry')
            seen.add(row['title_id'])
        return result

    def installed_titles(self, wait=60, page_size=16):
        """Refresh once, then read that snapshot. Changes/errors never trigger a retry."""
        if not math.isfinite(wait) or not 0 < wait <= 120:
            raise ValueError('wait must be greater than 0 and at most 120 seconds')
        integer(page_size, 1, 16, 'page_size')
        deadline = time.monotonic() + wait
        result = self.titles_refresh()
        identity = result['boot'], result['generation']
        while result['state'] == 'building':
            if time.monotonic() >= deadline:
                raise TimeoutError('Catalogue still building; query status, do not auto-refresh')
            time.sleep(0.2)
            result = self.titles_status()
            if (result['boot'], result['generation']) != identity:
                raise ProtocolError('Catalogue changed while building')
        if result['state'] != 'ready':
            raise RemoteError('Title catalogue ' + result['state'] + ' rc=' + result['rc'])
        rows, seen, offset = [], set(), 0
        while True:
            if time.monotonic() >= deadline:
                raise TimeoutError('Catalogue deadline elapsed; partial results discarded')
            page = self.titles_page(*identity, offset, page_size)
            if page['total'] != result['total']:
                raise ProtocolError('Catalogue size changed')
            for row in page['items']:
                if row['title_id'] in seen:
                    raise ProtocolError('Duplicate title across pages')
                seen.add(row['title_id'])
                rows.append(row)
            if page['next'] is None:
                break
            offset = page['next']
        latest = self.titles_status()
        if (latest['boot'], latest['generation']) != identity or latest['state'] != 'ready':
            raise ProtocolError('Catalogue invalidated while paging')
        result['items'] = rows
        return result

    def controller(self, action='status'):
        if action not in ('on', 'off', 'status'):
            raise ValueError('Controller action must be on, off or status')
        return self._exchange('controller ' + action)

    @contextmanager
    def input_session(self):
        """Enable input and attempt OFF even after an ambiguous ON/input outcome.

        OFF is module-wide. This is not a client lease; use one controlling client.
        """
        failed = False
        try:
            self.controller('on')
            yield self
        except BaseException:
            failed = True
            raise
        finally:
            try:
                self.controller('off')
            except (OSError, RuntimeError) as error:
                if not failed:
                    raise
                warnings.warn('Controller OFF could not be confirmed: ' + str(error), RuntimeWarning)

    def press(self, keys, ms=100):
        return self._exchange(f'press {buttons(keys)} {integer(ms, 1, 1000, "ms")}')

    def stick(self, side, x, y, ms=200):
        if side not in ('L', 'R'):
            raise ValueError('Stick side must be L or R')
        integer(x, -32768, 32767, 'x')
        integer(y, -32768, 32767, 'y')
        integer(ms, 1, 1000, 'ms')
        return self._exchange(f'stick {side} {x} {y} {ms}')

    def touch(self, kind, x, y, ms=100, gap=100):
        if kind not in ('tap', 'double', 'long'):
            raise ValueError('Touch kind must be tap, double or long')
        integer(x, 0, 1279, 'x')
        integer(y, 0, 719, 'y')
        integer(ms, 300 if kind == 'long' else 1, 5000 if kind == 'long' else 300, 'ms')
        if kind != 'double' and gap != 100:
            raise ValueError('gap applies only to double touch')
        suffix = f' {integer(gap, 50, 500, "gap")}' if kind == 'double' else ''
        return self._exchange(f'touch {kind} {x} {y} {ms}{suffix}')

    def capture(self, stack=2):
        if type(stack) is not int or stack not in STACKS:
            raise ValueError('Capture stack must be 0, 1, 2, 3, 4 or 6')
        return self._exchange(f'capture {stack}', binary=True)

    def home_status(self):
        return self._exchange('home status')

    def home_arm(self, generation):
        # Current module accepts at most 19 decimal digits. It does not identify Breeze.
        integer(generation, 1, 9_999_999_999_999_999_999, 'generation')
        return self._exchange(f'home arm {generation}')

    def home_off(self):
        return self._exchange('home off')

    def launch(self, path):
        """Queue an SD NRO on 0.1.9+; 0.1.8 raises launch-bridge-unavailable.

        Poll status for this request ID. Queued is not proof of NRO readiness.
        The server replaces the current Album NRO; no Stop-NRO call is needed.
        """
        return self._exchange('launch ' + nro_path(path))

    def game_stop(self, title_id):
        """Queue native AM close for this exact TID; track the receipt to finished."""
        if not isinstance(title_id, str) or not re.fullmatch(r'[0-9a-fA-F]{16}', title_id) or int(title_id, 16) == 0:
            raise ValueError('Title ID must be exactly 16 nonzero hexadecimal digits')
        return self._exchange('gamestop ' + title_id)

    def game_start(self, title_id):
        """Queue a PC launch. 0.1.8 candidate restores Breeze; status tracks outcome.

        An OK queued reply does not prove game readiness. 0.1.7 rejects this call.
        """
        if not isinstance(title_id, str) or not re.fullmatch(r'[0-9a-fA-F]{16}', title_id) or int(title_id, 16) == 0:
            raise ValueError('Title ID must be exactly 16 hexadecimal digits with a nonzero value (no 0x prefix)')
        return self._exchange('gamestart ' + title_id)
