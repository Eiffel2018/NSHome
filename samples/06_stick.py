#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Move a stick for a bounded interval, then center it."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    with client.input_session():
        return client.stick(args.side, args.x, args.y, args.ms)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--side', choices=('L', 'R'), default='L')
    cli.add_argument('--x', type=ranged(-32768, 32767), required=True)
    cli.add_argument('--y', type=ranged(-32768, 32767), required=True)
    cli.add_argument('--ms', type=ranged(1, 1000), default=200)
    raise SystemExit(run(cli, action))
