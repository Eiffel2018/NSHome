#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Disable experimental HOME switching; keep the sysmodule running."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    return client.home_off()


if __name__ == '__main__':
    cli = parser(__doc__)
    raise SystemExit(run(cli, action))
