#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Read experimental HOME switching state."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    return client.home_status()


if __name__ == '__main__':
    cli = parser(__doc__)
    raise SystemExit(run(cli, action))
