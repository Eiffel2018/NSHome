# NSHome

An independent Nintendo Switch system module that keeps original Nintendo
qlaunch and switches HOME between a running game and the same resident Breeze.

## Download and install

Download `NSHome-v0.1.12.zip` from the [0.1.12 preview](https://github.com/Eiffel2018/NSHome/releases/tag/v0.1.12).
This experimental build includes the HOME and 6802 connection fixes absent from
0.1.9 and closes games through the original HOME Menu. Read the known limitations below before installing.

Copy both `atmosphere` and `switch` to the SD root and reboot.
Read [installation and requirements](docs/INSTALL.md) first, including disabling
an existing SwitchU HOME replacement. Breeze is installed separately.

For PC control, download the separate `BreezePC-beta126-NSHome-v0.1.12.zip`
from the same release. Close and reopen BreezePC after updating. Its Stop game
button uses the new 6802 close flow; unmodified 6801 clients retain their original behavior.

## Features

- [Read-only installed application catalogue](docs/INSTALLED_TITLES.md)
  with NS/NCM installation evidence and snapshot pagination.
- Original qlaunch; independent module ID `00FF4E5348414D01`.
- Authenticated TCP 6802 controls, touch and JPEG capture, independent of Breeze UI.
- Manually opened Breeze is identified and HOME is paired
  automatically with the existing game. Exiting to hbmenu restores ordinary HOME.
- PC6802 game start selects the first system-listed user, initializes Breeze,
  returns to the game and automatically enables HOME switching.
- Direct 6802 SD NRO launch, including Breeze and hbmenu, without a separate
  Stop-NRO command. The included Return to HOME NRO exits to original qlaunch.
- Breeze's existing background-render handshake keeps its UI/debug loop active
  while the game owns foreground. Background BP hit, 16 instruction steps,
  continue and HOME round trips passed on the test console.
- Optional visually checked boot touch unlock.
- Guarded 6802 Stop game through stock HOME's X/Close flow, then reopen Breeze.
  The observed 1280x720 stock layout is required; unknown screens fail without confirming.
- [19 Python samples and all 6802 options](samples/README.md), standard library only.

This release is experimental. Checks used Breeze beta125.05 and beta126.00,
with gen2 v0.20 used for earlier BP/trace checks, and
one game at its main menu, without loading a save. Other firmware/games,
multiple accounts, sleep and general applet/exit compatibility need further testing.
See [launch behavior and validation limits](docs/LAUNCH.md).

Automatic user/Breeze handling applies to explicit 6802 game starts. Manual
console and 6801 starts retain their original behavior. Normal foreground Breeze pauses the game; use game
foreground for instruction stepping. Breeze may display its own overlay on a BP hit.

## Build and test

Host tests (Linux, g++ with ASan/UBSan):

```sh
bash tests/run.sh
python3 -m unittest discover -s tests -p '*_test.py'
```

Switch build requires devkitPro/devkita64, libnx
`20cbcfd90567f7bf84df7725f27e345e2164fa6b` and Atmosphere-libs
`aae2da0155a490abd3bfd17a70ccd16bb73c916a`:

```sh
export AMS_LIBRARIES=/absolute/path/to/Atmosphere-libs
bash build.sh
python3 package.py /absolute/path/to/NSHome.zip
```

See [development notes](docs/DEVELOPMENT.md) for pinned build tools and recovery.

## License

[GPL-2.0-only](LICENSE). Upstream mechanisms are credited in [NOTICE.md](NOTICE.md).
The separate desktop program in `tools/breezepc` uses its own GPL-3.0 license.
No Nintendo code, firmware, keys, Breeze binary or pairing configuration is bundled.
