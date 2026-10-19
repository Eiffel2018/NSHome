# SPDX-License-Identifier: GPL-2.0-only
"""NSHome TCP 6802 client. Python 3.10+, standard library, no automatic retries."""
from contextlib import contextmanager
import math
import re
import socket
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
    def _line(reader):
        data = reader.readline(2049)
        if len(data) > 2048 or not data.endswith(b'\n'):
            raise ProtocolError('Incomplete or oversized response; do not replay the operation')
        try:
            return data.decode('utf-8').rstrip('\r\n')
        except UnicodeDecodeError as error:
            raise ProtocolError('Invalid response encoding') from error

    def _exchange(self, command=None, binary=False):
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
                reply = self._line(reader)
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

    def game_start(self, title_id):
        """Queue a PC launch. 0.1.8 candidate restores Breeze; status tracks outcome.

        An OK queued reply does not prove game readiness. 0.1.7 rejects this call.
        """
        if not isinstance(title_id, str) or not re.fullmatch(r'[0-9a-fA-F]{16}', title_id) or int(title_id, 16) == 0:
            raise ValueError('Title ID must be exactly 16 hexadecimal digits with a nonzero value (no 0x prefix)')
        return self._exchange('gamestart ' + title_id)
