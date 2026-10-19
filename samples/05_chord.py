#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Press 2-4 distinct main buttons simultaneously."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    if '+' not in args.keys:
        raise ValueError('A chord needs at least two buttons')
    with client.input_session():
        return client.press(args.keys, args.ms)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--keys', type=buttons, default='L+R', help='Example: ZL+ZR+A+B; no HOME/CAPTURE/rail buttons')
    cli.add_argument('--ms', type=ranged(1, 1000), default=100)
    raise SystemExit(run(cli, action))
