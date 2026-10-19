# Installation and operation

## Build

Run `bash tests/run.sh`, then `bash build.sh` with `AMS_LIBRARIES` and
`DEVKITPRO` set. Dependency revisions are pinned in the root README.
Package with `python3 package.py /absolute/path/to/NSHome.zip`.
The build checks executable/container formats, module ID, forwarding and allocation.
All newlib reentrant allocation entry points, including `_memalign_r` used by
stdio aligned buffers, must route to Stratosphere. The default newlib memalign
implementation edits malloc chunk metadata and cannot use Stratosphere pointers;
`verify.py` checks the linked allocation routes.

## Installation

Disable any SwitchU qlaunch override first. Extract to SD, then reboot:

```text
atmosphere/contents/00FF4E5348414D01/exefs.nsp
atmosphere/contents/00FF4E5348414D01/mitm.lst
atmosphere/contents/00FF4E5348414D01/flags/boot2.flag
atmosphere/contents/00FF4E5348414D01/breeze_loader/main
atmosphere/contents/00FF4E5348414D01/breeze_loader/main.npdm
```

`mitm.lst` contains exactly `appletAE`, beside `exefs.nsp`.
Original Nintendo qlaunch remains in use.

## Remote control and HOME

TCP6802 uses `switch/Breeze/config.ini`, section `[PC connect]`.
Controls, touch, capture and diagnostics work independently of Breeze6801.
The 0.1.8 development candidate adds PC6802 game launch; generic NRO launch still
returns `launch-bridge-unavailable`. Version 0.1.7 rejects both.
The matched Breeze loader pair above is required for automatic return.
See [launch behavior, origin rules and pending checks](LAUNCH.md).
Use bounded input pulses, disable the input gate afterwards, and never replay
an input whose result is unknown.

The candidate automatically arms after its scoped PC6802 launch verifies Breeze.
For the separate manual diagnostic control, with Breeze in front,
verify its6801 service, then send authenticated6802 `home status` followed by
`home arm GENERATION`, using the current `album` generation. A live game is
required. `home off` restores ordinary HOME handling. Session closure or observed
completion disarms switching; other NROs are not automatically armed.

Hidden Breeze UI/debug RPC may return busy. Stock qlaunch suspends the game
behind Breeze; pending Step may require game foreground. The new automatic flow,
continuous trace and general exit/chainload behavior still need hardware checks.

## Boot unlock and recovery

An empty `config/NSHome/boot-touch-once.flag` enables one100ms touch after two
matching boot-screen captures and AM readiness checks. The final check runs
before contact. Uncertain frames, sleep, applet/game launch or the five-second
window expiring cancels input. No retry or A fallback follows a touch attempt.
One cold-boot touch unlock has been verified; other layouts remain untested.
The legacy `boot-a-once.flag` applies only when the touch flag is absent.
`skip-lock-screen.flag` changes the native sleep-lock setting; it does not alone
bypass the cold-boot entrance.

Logs and the boot diagnostic JPEG are in `config/NSHome/`. Startup stage31
records input attempt/result. If the module prevents boot, power off and remove
its entire `00FF4E5348414D01` directory from `atmosphere/contents` via SD access.
Forwarding cannot guarantee recovery if the MITM process crashes.

See [NOTICE](../NOTICE.md) and [LICENSE](../LICENSE).
