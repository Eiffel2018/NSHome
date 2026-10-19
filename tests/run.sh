#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
flags=(-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g)
g++ "${flags[@]}" -Isource tests/boot_gate_test.cpp -o build/tests/boot_gate
build/tests/boot_gate
g++ "${flags[@]}" -Isource tests/boot_jpeg_test.cpp source/boot_jpeg.cpp -o build/tests/boot_jpeg
build/tests/boot_jpeg
g++ "${flags[@]}" -Itests/runtime_stub tests/runtime_test.cpp source/runtime.cpp -o build/tests/runtime
build/tests/runtime
g++ "${flags[@]}" -DNSH_BRIDGE_HOST_TEST -Isource -Itests/home_bridge_stub tests/home_bridge_test.cpp source/home_bridge.cpp -o build/tests/home_bridge
build/tests/home_bridge

g++ "${flags[@]}" -Isource tests/launch_policy_test.cpp -o build/tests/launch_policy
build/tests/launch_policy
g++ "${flags[@]}" -Isource tests/launch_handoff_test.cpp -o build/tests/launch_handoff
build/tests/launch_handoff
g++ "${flags[@]}" -Isource tests/loader_ticket_test.cpp -o build/tests/loader_ticket
build/tests/loader_ticket

g++ "${flags[@]}" -Isource tests/breeze_background_test.cpp -o build/tests/breeze_background
build/tests/breeze_background
g++ "${flags[@]}" -Itests/launch_storage_stub -Isource tests/launch_storage_test.cpp -o build/tests/launch_storage
build/tests/launch_storage

g++ "${flags[@]}" -Itests/launch_storage_stub -Isource tests/breeze_channel_test.cpp -o build/tests/breeze_channel
build/tests/breeze_channel
g++ "${flags[@]}" -Isource tests/codec_test.cpp -o build/tests/codec
build/tests/codec
for component in protocol input touch capture; do
    includes=()
    case "$component" in
        input|touch) includes=(-Itests/remote_nro_input_stub);;
        capture) includes=(-Itests/remote_nro_capture_stub);;
    esac
    g++ "${flags[@]}" "${includes[@]}" "tests/remote_nro_${component}_test.cpp" \
        "source/remote_nro_${component}.cpp" -o "build/tests/$component"
    "build/tests/$component"
done
