# SPDX-License-Identifier: GPL-2.0-only
"""The actual Stop button must use scoped native close, with no 6801 fallback."""
import ast
from pathlib import Path
import queue
from types import SimpleNamespace
import unittest

source = Path(__file__).parents[1] / 'tools/breezepc/pcconnect_app.py'
tree = ast.parse(source.read_text(encoding='utf-8'))
app = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'App')
names = {'on_stop_game', '_switchu_stop_worker'}
methods = [n for n in app.body if isinstance(n, ast.FunctionDef) and n.name in names]

class StopTests(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self.remote = SimpleNamespace(RemoteError=RuntimeError,
            stop_game=lambda h,c,t: self.calls.append(('stop',t)) or 'OK queued 7',
            request_id=lambda text: 7,
            wait_for_request=lambda *a,**kw: 'OK status id=7 state=finished',
            status_state=lambda text: 'finished')
        self.ns = dict(switchu_remote=self.remote, valid_tid=lambda t:t,
            messagebox=SimpleNamespace(askokcancel=lambda *a,**kw:True),
            threading=SimpleNamespace(Thread=lambda **kw:SimpleNamespace(start=lambda:kw['target'](*kw['args']))))
        for method in methods:
            exec(compile(ast.Module(body=[method],type_ignores=[]),str(source),'exec'),self.ns)
        self.obj = SimpleNamespace(save_tid='0100005027A18000',save={'game_running':True},
            switchu_busy=False,switchu_generation=1,cfg={'host':'invalid','code':'dummy'},root=None,
            status=SimpleNamespace(config=lambda **kw:None),out=queue.Queue(),
            link=SimpleNamespace(send=lambda cmd:self.fail('Unexpected 6801 command: '+cmd)),
            _set_switchu_buttons=lambda:None)
        self.obj._switchu_stop_worker=lambda *a:self.ns['_switchu_stop_worker'](self.obj,*a)

    def test_button_closes_only_exact_title_over_6802(self):
        self.ns['on_stop_game'](self.obj)
        self.assertEqual(self.calls,[('stop',self.obj.save_tid)])
        self.assertIn('original HOME',self.obj.out.get()[2])

    def test_busy_does_not_queue_another_stop(self):
        self.obj.switchu_busy=True
        self.ns['on_stop_game'](self.obj)
        self.assertEqual(self.calls,[])

    def test_no_game_does_not_stop(self):
        self.obj.save['game_running']=False
        self.ns['on_stop_game'](self.obj)
        self.assertEqual(self.calls,[])

    def test_network_failure_does_not_fallback_to_force_kill(self):
        def fail(*args):raise RuntimeError('unknown outcome')
        self.remote.stop_game=fail
        self.ns['on_stop_game'](self.obj)
        self.assertEqual(self.calls,[])
        self.assertIn('No stop was resent',self.obj.out.get()[2])

if __name__=='__main__':unittest.main()
