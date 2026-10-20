# SPDX-License-Identifier: GPL-2.0-only
"""6802 Stop must be title-scoped and never replay a lost receipt."""
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

path=Path(__file__).parents[1]/'tools/breezepc/switchu_remote.py'
spec=importlib.util.spec_from_file_location('stop_transport',path)
remote=importlib.util.module_from_spec(spec);spec.loader.exec_module(remote)

class Socket:
    def __init__(self,reply=b'OK queued 9\n'):
        self.input=bytearray(b'SWITCHU 1\nOK auth\n'+reply);self.sent=[]
    def __enter__(self):return self
    def __exit__(self,*args):pass
    def settimeout(self,*args):pass
    def sendall(self,data):self.sent.append(data)
    def recv(self,n):
        out=bytes(self.input[:n]);del self.input[:n];return out

class StopTransportTests(unittest.TestCase):
    def test_actual_transport_accepts_scoped_stop(self):
        sock=Socket()
        with patch.object(remote.socket,'create_connection',return_value=sock) as connect:
            self.assertEqual(remote.stop_game('invalid','dummy','0100005027A18000'),'OK queued 9')
        self.assertEqual(sock.sent,[b'auth dummy\n',b'gamestop 0100005027A18000\n'])
        connect.assert_called_once()
    def test_missing_receipt_is_not_replayed(self):
        sock=Socket(b'')
        with patch.object(remote.socket,'create_connection',return_value=sock) as connect:
            with self.assertRaises(remote.AmbiguousLaunchError):remote.stop_game('invalid','dummy','0100005027A18000')
        connect.assert_called_once()
        self.assertEqual(len(sock.sent),2)
    def test_invalid_title_never_opens_connection(self):
        with patch.object(remote.socket,'create_connection') as connect:
            for tid in ['0000000000000000','0x0100005027A18000','0100005027A18000 extra']:
                with self.assertRaises(ValueError):remote.stop_game('invalid','dummy',tid)
        connect.assert_not_called()

if __name__=='__main__':unittest.main()
