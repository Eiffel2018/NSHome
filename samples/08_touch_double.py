#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Touch twice with a gap; each contact is lifted."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    with client.input_session():
        return client.touch('double', args.x, args.y, args.ms, args.gap)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--x', type=ranged(0, 1279), required=True)
    cli.add_argument('--y', type=ranged(0, 719), required=True)
    cli.add_argument('--ms', type=ranged(1, 300), default=100, help='Duration of each contact')
    cli.add_argument('--gap', type=ranged(50, 500), default=100, help='Gap between contacts in milliseconds')
    raise SystemExit(run(cli, action))
