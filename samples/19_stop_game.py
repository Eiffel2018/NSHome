#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Queue original-HOME close for this running TID. Check the same request ID until finished."""
from _common import parser, run


def action(client, args):
    return client.game_stop(args.title_id)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--title-id', required=True, help='Exactly 16 hex digits; must match the running game')
    raise SystemExit(run(cli, action))
