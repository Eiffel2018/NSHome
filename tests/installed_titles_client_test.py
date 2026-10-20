# SPDX-License-Identifier: GPL-2.0-only
"""Offline catalogue framing, pagination and stale-snapshot tests."""
import copy
import json
import unittest
from unittest.mock import patch
from remote_client_test import Socket, remote


def state(**fields):
    value = dict(schema=1, source='ns+ncm.full_application_contents', boot='9007199254740993', generation='7',
                 state='ready', rc='0x00000000', total=0, scanned=0, page_max=16, profile_selection_supported=False)
    value.update(fields)
    return value


def row(index, **fields):
    value = dict(title_id=f'{0x0100000000000000 + index:016X}', name='遊戲', name_source='nacp.system_language',
                 name_rc='0x00000000', storage_id=5)
    value.update(fields)
    return value


class TitlesTests(unittest.TestCase):
    def setUp(self):
        self.client = remote.Client('offline.invalid', '0001')

    def wire(self, data, command):
        sock = Socket(('OK titles ' + json.dumps(data, ensure_ascii=False) + '\n').encode())
        with patch.object(remote.socket, 'create_connection', return_value=sock) as connect:
            result = command()
        self.assertEqual(connect.call_count, 1)
        return result, sock.sent

    def test_large_utf8_page_and_exact_decimal_identity(self):
        rows = [row(i, name='漢' * 170) for i in range(16)]
        data = state(total=16, scanned=16, offset=0, next=None, items=rows)
        result, sent = self.wire(data, lambda: self.client.titles_page('9007199254740993', '7'))
        self.assertEqual(result['items'], rows)
        self.assertEqual(sent[-1], b'titles page 9007199254740993 7 0 16\n')

    def test_missing_name_is_preserved_and_empty_catalogue_is_valid(self):
        data = state(total=1, scanned=3, offset=0, next=None,
                     items=[row(1, name=None, name_source='unavailable', name_rc='0x0000007B')])
        result, _ = self.wire(data, lambda: self.client.titles_page(data['boot'], '7'))
        self.assertIsNone(result['items'][0]['name'])
        self.wire(state(offset=0, next=None, items=[]), lambda: self.client.titles_page(data['boot'], '7'))

    def test_stale_or_malformed_page_never_retries(self):
        good = state(total=1, scanned=1, offset=0, next=None, items=[row(1)])
        bad = []
        for fields in ({'generation': '8'}, {'boot': '1'}, {'offset': 1}, {'offset': False}, {'next': 0}, {'state': 'invalidated'}, {'page_max': 16.0}):
            value = copy.deepcopy(good); value.update(fields); bad.append(value)
        for fields in ({'title_id': 123}, {'title_id': '0000000000000000'}, {'name_rc': 0}, {'storage_id': True}):
            value = copy.deepcopy(good); value['items'][0].update(fields); bad.append(value)
        for data in bad:
            with self.subTest(data=data), self.assertRaises(remote.ProtocolError):
                self.wire(data, lambda: self.client.titles_page(good['boot'], '7'))

    def test_read_all_pages_and_final_generation_check(self):
        boot = state()['boot']
        ready = state(total=2, scanned=4)
        pages = [state(total=2, scanned=4, offset=0, next=1, items=[row(1)]),
                 state(total=2, scanned=4, offset=1, next=None, items=[row(2)])]
        with patch.object(self.client, 'titles_refresh', return_value=ready), \
             patch.object(self.client, 'titles_status', return_value=ready), \
             patch.object(self.client, 'titles_page', side_effect=pages) as page:
            self.assertEqual(len(self.client.installed_titles(page_size=1)['items']), 2)
            self.assertEqual(page.call_args_list[1].args, (boot, '7', 1, 1))
        with patch.object(self.client, 'titles_refresh', return_value=ready), \
             patch.object(self.client, 'titles_status', return_value=state(generation='8', state='invalidated')), \
             patch.object(self.client, 'titles_page', side_effect=pages), self.assertRaises(remote.ProtocolError):
            self.client.installed_titles(page_size=1)

    def test_failed_rebuild_and_cross_page_duplicates_do_not_return_partial_results(self):
        with patch.object(self.client, 'titles_refresh', return_value=state(state='failed', rc='0x00000001')) as refresh, \
             self.assertRaises(remote.RemoteError):
            self.client.installed_titles()
        self.assertEqual(refresh.call_count, 1)
        ready = state(total=2, scanned=2)
        pages = [state(total=2, offset=0, next=1, items=[row(1)]), state(total=2, offset=1, next=None, items=[row(1)])]
        with patch.object(self.client, 'titles_refresh', return_value=ready), \
             patch.object(self.client, 'titles_page', side_effect=pages), self.assertRaises(remote.ProtocolError):
            self.client.installed_titles(page_size=1)

    def test_bad_parameters_do_not_connect(self):
        with patch.object(remote.socket, 'create_connection') as connect:
            for args in ((True, '1'), ('01', '1'), ('1', '0'), (1.5, '1'), ('1', '1', 0, 17)):
                with self.assertRaises(ValueError): self.client.titles_page(*args)
            connect.assert_not_called()


if __name__ == '__main__':
    unittest.main()
