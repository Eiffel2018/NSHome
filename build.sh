#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
: "${AMS_LIBRARIES:?Set AMS_LIBRARIES to Atmosphere-libs aae2da0155a490abd3bfd17a70ccd16bb73c916a}"
: "${DEVKITPRO:?Set DEVKITPRO to an isolated devkitPro installation}"
cd "$(dirname "$0")"
actual=$(git -C "$AMS_LIBRARIES" rev-parse HEAD)
test "$actual" = aae2da0155a490abd3bfd17a70ccd16bb73c916a
# Link wrapping must see non-inlined ForwardRequest references.
make -C "$AMS_LIBRARIES/libstratosphere" nx_release -j4 SETTINGS='$(ATMOSPHERE_SETTINGS) -O2 -Wextra -Werror -Wno-missing-field-initializers -Wno-error=unused-result -fno-lto'
make AMS_LIBRARIES="$AMS_LIBRARIES" -j4
python3 verify.py build/am-observer.elf build/exefs.nsp build/disassembly.txt
