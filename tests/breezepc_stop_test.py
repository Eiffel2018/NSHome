# SPDX-License-Identifier: GPL-2.0-only
"""Desktop Stop keeps the upstream 6801 protocol; compatibility lives on Switch."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest

source=Path(__file__).parents[1]/'tools/breezepc/pcconnect_app.py'
tree=ast.parse(source.read_text(encoding='utf-8'))
app=next(n for n in tree.body if isinstance(n,ast.ClassDef) and n.name=='App')
method=next(n for n in app.body if isinstance(n,ast.FunctionDef) and n.name=='on_stop_game')

class StopTests(unittest.TestCase):
    def invoke(self,approve,send):
        namespace={'messagebox':SimpleNamespace(askokcancel=lambda *a,**kw:approve)}
        exec(compile(ast.Module(body=[method],type_ignores=[]),str(source),'exec'),namespace)
        namespace['on_stop_game'](SimpleNamespace(root=None,link=SimpleNamespace(send=send)))

    def test_upstream_stop_command_needs_no_6802(self):
        sent=[]
        self.invoke(True,sent.append)
        self.assertEqual(sent,['gamestop'])

    def test_cancel_sends_nothing(self):
        sent=[]
        self.invoke(False,sent.append)
        self.assertEqual(sent,[])

    def test_uncertain_send_is_not_replayed(self):
        sent=[]
        def send(command):
            sent.append(command)
            raise OSError('connection lost')
        with self.assertRaises(OSError):self.invoke(True,send)
        self.assertEqual(sent,['gamestop'])

if __name__=='__main__':unittest.main()
