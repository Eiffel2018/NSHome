# SPDX-License-Identifier: GPL-2.0-only
"""Exercise beta126 capture ordering without Tk, Pillow, or a console."""
import ast
import json
from pathlib import Path
import queue
import threading
import time
import unittest


tree = ast.parse((Path(__file__).parents[1] / 'tools/breezepc/pcconnect_app.py').read_text(encoding='utf-8'))
namespace = dict(threading=threading, queue=queue, time=time, json=json, QUIET=('capture', 'state'))
link_class = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == 'Link')
exec(compile(ast.Module(body=[link_class], type_ignores=[]), '<BreezePC Link>', 'exec'), namespace)
Link = namespace['Link']


class CaptureOrderingTests(unittest.TestCase):
    def run_capture(self, fps, with_command=False):
        link = Link('example.invalid', 'not-a-real-code', queue.Queue())
        link.fps = fps

        class Transport:
            events = []

            def __init__(self):
                self.calls = []
                self.frames = 0

            def cmd(self, command):
                self.calls.append(command)
                if command.startswith('capture'):
                    self.frames += 1
                    if with_command and self.frames == 2:
                        link.send('action 99')
                    if self.frames == 3:
                        link.stop = True
                    return '+OK capture', b'frame'
                return '+OK'

        transport = Transport()
        link.loop(transport)
        return transport.calls

    def test_high_rate_starts_fresh_then_uses_capture_ahead(self):
        self.assertEqual(self.run_capture(15), ['capture', 'capture ahead', 'capture ahead'])

    def test_command_discards_ahead_frame_for_next_picture(self):
        self.assertEqual(self.run_capture(15, with_command=True),
                         ['capture', 'capture ahead', 'action 99', 'capture'])

    def test_low_rate_uses_fresh_captures_only(self):
        self.assertEqual(self.run_capture(7), ['capture', 'capture', 'capture'])


if __name__ == '__main__':
    unittest.main()
