# SPDX-License-Identifier: GPL-2.0-only
"""Offline transport, validation and runnable-sample tests; never uses a console."""
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'samples'))
import nshome_remote as remote


class Fragments(io.BytesIO):
    def read(self, size=-1):
        return super().read(min(size, 2))


class Socket:
    def __init__(self, reply, banner=b'SWITCHU 1\n', auth=b'OK auth\n'):
        self.reader = Fragments(banner + auth + reply)
        self.sent = []

    def __enter__(self): return self
    def __exit__(self, *_): pass
    def settimeout(self, value): self.timeout = value
    def makefile(self, _): return self.reader
    def sendall(self, data): self.sent.append(data)


class RemoteClientTests(unittest.TestCase):
    def setUp(self):
        self.client = remote.Client('offline.invalid', '0001')

    def wire(self, operation, expected, reply=b'OK result\n'):
        sock = Socket(reply)
        with patch.object(remote.socket, 'create_connection', return_value=sock) as connect:
            result = operation()
        connect.assert_called_once_with(('offline.invalid', 6802), timeout=10.0)
        self.assertEqual(sock.sent, [b'auth 0001\n', (expected + '\n').encode()])
        return result

    def test_all_text_operations_and_parameters(self):
        cases = [
            (lambda: self.client.status(), 'status'),
            (lambda: self.client.controller(), 'controller status'),
            (lambda: self.client.controller('on'), 'controller on'),
            (lambda: self.client.controller('off'), 'controller off'),
            (lambda: self.client.press('ZL+ZR+A+B', 1000), 'press ZL+ZR+A+B 1000'),
            (lambda: self.client.stick('R', -32768, 32767, 1), 'stick R -32768 32767 1'),
            (lambda: self.client.touch('tap', 0, 0, 1), 'touch tap 0 0 1'),
            (lambda: self.client.touch('double', 1279, 719, 300, 500), 'touch double 1279 719 300 500'),
            (lambda: self.client.touch('long', 640, 360, 5000), 'touch long 640 360 5000'),
            (lambda: self.client.home_status(), 'home status'),
            (lambda: self.client.home_arm(9), 'home arm 9'),
            (lambda: self.client.home_off(), 'home off'),
            (lambda: self.client.launch('/switch/My App/測試.nro'), 'launch sdmc:/switch/My App/測試.nro'),
            (lambda: self.client.game_start('0100000000000001'), 'gamestart 0100000000000001'),
        ]
        for operation, expected in cases:
            with self.subTest(command=expected):
                self.assertEqual(self.wire(operation, expected), 'OK result')

    def test_every_supported_button_has_a_wire_example(self):
        for key in remote.BUTTONS:
            with self.subTest(key=key):
                self.wire(lambda: self.client.press(key), f'press {key} 100')

    def test_authentication_only_preserves_leading_zeroes(self):
        sock = Socket(b'')
        with patch.object(remote.socket, 'create_connection', return_value=sock):
            self.assertEqual(self.client.authenticate(), 'OK auth')
        self.assertEqual(sock.sent, [b'auth 0001\n'])

    def test_wrong_endpoint_or_auth_never_sends_operation(self):
        for sock in (Socket(b'', banner=b'BREEZE\n'), Socket(b'', auth=b'ERR auth\n')):
            with self.subTest(), patch.object(remote.socket, 'create_connection', return_value=sock):
                with self.assertRaises(RuntimeError):
                    self.client.press('A')
                self.assertNotIn(b'press A 100\n', sock.sent)

    def test_errors_are_reported_without_retry(self):
        for reply in (b'ERR launch-bridge-unavailable\n', b'ERR busy\n', b'ERR input rc=0xE0CE\n'):
            with self.subTest(reply=reply):
                with self.assertRaises(remote.RemoteError):
                    self.wire(lambda: self.client.launch('/hbmenu.nro'), 'launch sdmc:/hbmenu.nro', reply)
        with patch.object(remote.socket, 'create_connection', side_effect=TimeoutError('unknown')) as connect:
            with self.assertRaises(TimeoutError):
                self.client.press('A')
            self.assertEqual(connect.call_count, 1)

    def test_fragmented_capture_every_stack(self):
        image = b'\xff\xd8jpeg\x00\n\xff\xd9'
        header = f'OK capture bytes={len(image)} width=1280 height=720 format=jpeg\n'.encode()
        for stack in remote.STACKS:
            with self.subTest(stack=stack):
                self.assertEqual(self.wire(lambda: self.client.capture(stack), f'capture {stack}', header + image), image)

    def test_capture_rejects_bad_header_size_truncation_and_envelope(self):
        cases = [
            b'OK capture bytes=524289 width=1280 height=720 format=jpeg\n',
            b'OK capture bytes=3 width=1280 height=720 format=jpeg\nabc',
            b'OK capture bytes=4 width=640 height=360 format=jpeg\n1234',
            b'OK capture bytes=8 width=1280 height=720 format=jpeg\n\xff\xd8',
            b'OK capture bytes=4 width=1280 height=720 format=jpeg\nnope',
        ]
        for reply in cases:
            with self.subTest(reply=reply), self.assertRaises(remote.ProtocolError):
                self.wire(self.client.capture, 'capture 2', reply)

    def test_invalid_or_incomplete_text_response(self):
        for reply in (b'OK incomplete', b'X' * 2049 + b'\n', b'+OK wrong-protocol\n', b'OK \xff\n'):
            with self.subTest(), self.assertRaises(remote.ProtocolError):
                self.wire(self.client.status, 'status', reply)

    def test_input_session_off_after_success_and_uncertain_failure(self):
        with patch.object(self.client, '_exchange', return_value='OK') as request:
            with self.client.input_session():
                self.client.press('A')
            self.assertEqual([c.args[0] for c in request.call_args_list],
                             ['controller on', 'press A 100', 'controller off'])
        for fail_at in (0, 1):
            replies = [TimeoutError('unknown'), 'OK'] if fail_at == 0 else ['OK', TimeoutError('unknown'), 'OK']
            with self.subTest(fail_at=fail_at), patch.object(self.client, '_exchange', side_effect=replies) as request:
                with self.assertRaises(TimeoutError):
                    with self.client.input_session():
                        self.client.press('A')
                self.assertEqual(request.call_args_list[-1].args, ('controller off',))
                self.assertEqual(sum(c.args[0] == 'press A 100' for c in request.call_args_list), fail_at)

    def test_cleanup_failure_does_not_hide_original_error(self):
        with patch.object(self.client, '_exchange', side_effect=[TimeoutError('original'), OSError('offline')]):
            with self.assertWarnsRegex(RuntimeWarning, 'OFF could not be confirmed'):
                with self.assertRaisesRegex(TimeoutError, 'original'):
                    with self.client.input_session():
                        pass
        with patch.object(self.client, '_exchange', side_effect=['OK', OSError('cleanup')]):
            with self.assertRaisesRegex(OSError, 'cleanup'):
                with self.client.input_session():
                    pass

    def test_invalid_values_are_rejected_before_network_access(self):
        operations = [
            lambda: self.client.press('A+A'), lambda: self.client.press('HOME+A'),
            lambda: self.client.press('LSL+R'), lambda: self.client.press('A+B+X+Y+L'),
            lambda: self.client.press('a'), lambda: self.client.press('A', 0),
            lambda: self.client.press('A', 1001), lambda: self.client.press('A', True),
            lambda: self.client.stick('X', 0, 0), lambda: self.client.stick('L', -32769, 0),
            lambda: self.client.stick('R', 0, 32768), lambda: self.client.capture(5),
            lambda: self.client.touch('tap', 1280, 0), lambda: self.client.touch('tap', 0, 720),
            lambda: self.client.touch('long', 1, 1, 299), lambda: self.client.touch('long', 1, 1, 5001),
            lambda: self.client.touch('double', 1, 1, 301), lambda: self.client.touch('double', 1, 1, 50, 49),
            lambda: self.client.touch('double', 1, 1, 50, 501), lambda: self.client.touch('tap', 1, 1, gap=150),
            lambda: self.client.controller('lease'), lambda: self.client.home_arm(0),
            lambda: self.client.home_arm(10**19), lambda: self.client.game_start('0' * 16),
            lambda: self.client.game_start('0x0100000000000001'), lambda: self.client.launch('/../a.nro'),
            lambda: self.client.launch('/switch//a.nro'), lambda: self.client.launch('/switch/a.nro\nstatus'),
            lambda: self.client.launch('/' + '測' * 170 + '.nro'),
            lambda: self.client._exchange('status\nhome off'),
        ]
        with patch.object(remote.socket, 'create_connection') as connect:
            for operation in operations:
                with self.subTest(operation=operation), self.assertRaises(ValueError):
                    operation()
            connect.assert_not_called()

    def test_invalid_codes_ports_timeouts(self):
        for code in ('123', 'a' * 65, '0001\nstatus'):
            with self.subTest(code=code), self.assertRaises(ValueError):
                remote.Client('offline.invalid', code)
        for timeout in (0, -1, float('nan'), float('inf'), 121):
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                remote.Client('offline.invalid', '0001', timeout=timeout)
        for port in (0, 65536):
            with self.subTest(port=port), self.assertRaises(ValueError):
                remote.Client('offline.invalid', '0001', port=port)

    def test_sample_actions_cover_all_feature_groups(self):
        from types import SimpleNamespace
        with tempfile.TemporaryDirectory() as directory:
            args = SimpleNamespace(action='status', key='A', keys='L+R', ms=100,
                side='L', x=640, y=360, gap=100, stack=2, generation=9,
                path='/hbmenu.nro', title_id='0100000000000001', output=Path(directory) / 'screen.jpg', wait=60, page_size=16)
            expected = [
                'auth', 'status', 'controller status', 'press A 100', 'press L+R 100',
                'stick L 640 360 100', 'touch tap 640 360 100', 'touch double 640 360 100 100',
                'touch long 640 360 600', 'capture 2', 'home status', 'home arm 9', 'home off',
                'launch sdmc:/hbmenu.nro', 'gamestart 0100000000000001', 'press HOME 100', 'press CAPTURE 100',
                'titles refresh',
            ]
            samples = sorted((ROOT / 'samples').glob('[0-9][0-9]_*.py'))
            self.assertEqual(len(samples), len(expected))
            for path, command in zip(samples, expected):
                with self.subTest(sample=path.name):
                    spec = importlib.util.spec_from_file_location('sample_under_test', path)
                    module = importlib.util.module_from_spec(spec)
                    spec.loader.exec_module(module)
                    args.ms = 600 if 'touch_long' in path.name else 100
                    def exchange(line=None, binary=False, line_limit=2048):
                        if binary:
                            return b'\xff\xd8\xff\xd9'
                        if line and line.startswith('titles '):
                            return 'OK titles '+json.dumps(dict(schema=1,source='ns+ncm.full_application_contents',boot='1',generation='1',
                                state='ready',rc='0x00000000',total=0,scanned=0,page_max=16,profile_selection_supported=False,offset=0,next=None,items=[]))
                        return 'OK auth' if line is None else 'OK result'
                    with patch.object(self.client, '_exchange', side_effect=exchange) as request:
                        module.action(self.client, args)
                        commands = ['auth' if c.args[0:1] == () or c.args[0] is None else c.args[0]
                                    for c in request.call_args_list]
                        if command=='titles refresh':
                            self.assertEqual(commands,['titles refresh','titles page 1 1 0 16','titles status'])
                        elif command in ('auth', 'status', 'controller status') or command.startswith(('capture ', 'home ', 'launch ', 'gamestart ')):
                            self.assertEqual(commands, [command])
                        else:
                            self.assertEqual(commands, ['controller on', command, 'controller off'])
            self.assertEqual(args.output.read_bytes(), b'\xff\xd8\xff\xd9')

    def test_all_samples_help_without_network_or_pairing_prompt(self):
        for path in sorted((ROOT / 'samples').glob('[0-9][0-9]_*.py')):
            with self.subTest(sample=path.name):
                result = subprocess.run([sys.executable, str(path), '--help'], capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('--host', result.stdout)
                self.assertNotIn('pairing code:', result.stdout)

    def test_capture_sample_refuses_existing_file_and_leaves_no_file_on_transport_failure(self):
        from types import SimpleNamespace
        spec = importlib.util.spec_from_file_location('capture_sample', ROOT / 'samples/10_capture.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'capture.jpg'
            args = SimpleNamespace(output=output, stack=2)
            with patch.object(self.client, 'capture', side_effect=TimeoutError('unknown')):
                with self.assertRaises(TimeoutError):
                    module.action(self.client, args)
            self.assertFalse(output.exists())
            output.write_bytes(b'preserve')
            with patch.object(self.client, 'capture') as capture:
                with self.assertRaises(FileExistsError):
                    module.action(self.client, args)
                capture.assert_not_called()
            self.assertEqual(output.read_bytes(), b'preserve')


if __name__ == '__main__':
    unittest.main()
