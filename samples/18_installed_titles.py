# SPDX-License-Identifier: GPL-2.0-only
"""Read the verified installed application catalogue; never starts a game."""
import json
from _common import parser, ranged, run

def action(client, args):
    return json.dumps(client.installed_titles(args.wait, args.page_size), ensure_ascii=False, indent=2)

if __name__ == '__main__':
    cli = parser('List installed applications from NS and verified NCM base content (0.1.11+)')
    cli.add_argument('--page-size', type=ranged(1, 16), default=16, help='Entries per page (default: 16)')
    cli.add_argument('--wait', type=ranged(1, 120), default=60, help='Overall polling deadline, seconds (default: 60)')
    raise SystemExit(run(cli, action))
