# SPDX-License-Identifier: GPL-2.0-only
"""Desktop Stop uses NSHome's normal-close request over 6802."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest

source=Path(__file__).parents[1]/'tools/breezepc/pcconnect_app.py'
tree=ast.parse(source.read_text(encoding='utf-8'))
app=next(n for n in tree.body if isinstance(n,ast.ClassDef) and n.name=='App')
method=next(n for n in app.body if isinstance(n,ast.FunctionDef) and n.name=='on_stop_game')

class StopTests(unittest.TestCase):
    def invoke(self,approve,tid='0100005027A18000',fallback=''):
        sent=[]
        namespace={'messagebox':SimpleNamespace(
            askokcancel=lambda *a,**kw:approve,
            showerror=lambda *a,**kw:None),
            'valid_tid':lambda value:value if value=='0100005027A18000' else ''}
        exec(compile(ast.Module(body=[method],type_ignores=[]),str(source),'exec'),namespace)
        app=SimpleNamespace(root=None,save={'title_id':tid},save_tid=fallback,
                             send_switchu_stop_game=sent.append)
        namespace['on_stop_game'](app)
        return sent

    def test_stop_uses_the_6802_close_request_with_current_title(self):
        self.assertEqual(self.invoke(True),['0100005027A18000'])

    def test_cancel_sends_nothing(self):
        self.assertEqual(self.invoke(False),[])

    def test_missing_title_id_does_not_send_a_close(self):
        self.assertEqual(self.invoke(True,tid=''),[])

    def test_stale_saved_title_is_not_used_as_fallback(self):
        self.assertEqual(self.invoke(True,tid='',fallback='0100005027A18000'),[])

if __name__=='__main__':unittest.main()
