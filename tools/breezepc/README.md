# BreezePC beta126 companion for NSHome

Python desktop companion based on the user's Breeze beta126 package from
[tomvita/Breeze-Beta](https://github.com/tomvita/Breeze-Beta), under GPL-3.0.
The console module has its own GPL-2.0 license; these are separate programs.

Install Python 3 with Tk support, then `python -m pip install Pillow capstone`.
Run `pcconnect_app.bat` on Windows, or `python pcconnect_app.py`.
Close an already running old window and reopen it to load these changes.
Existing IP and pairing settings in `~/.breeze_pc.json` are reused; no settings
or credentials are included in the download.

Changes relative to the original beta126 desktop UI:

- Independent 6802 Open Breeze/Open NRO, status and controls while 6801 is offline.
- Buttons, HOME, sticks, chords and coordinate touch through 6802.
- Screen capture through 6802, falling back to Breeze's original 6801 capture.
- Start game through 6802, recent-title history and manual Title ID entry.
- Virtual-input handoff with the existing Breeze Game input and explicit OFF.
- Stop game uses NSHome's 6802 `gamestop` request. The console closes the
  identified game through the original HOME Close flow and reopens Breeze;
  Breeze's 6801 terminate command is not used by this button.

Beta126's 15 FPS default, 30 FPS selector and 6801 capture-ahead scheduling are
retained. The independent 6802 picture worker remains capped at 10 FPS.
Other beta126 modules, including its disassembler, decompiler, text and Unity
helpers, are preserved. Optional Ghidra/Java support is not bundled.

The 6802 controls work with NSHome or a compatible SwitchU endpoint. For NSHome,
keep Nintendo's original qlaunch and do not install SwitchU's HOME replacement.
No Stop-NRO prerequisite is sent. A queued/started launch does not prove an
arbitrary NRO is ready; unknown input/launch outcomes are never replayed.

Build the optional standalone desktop ZIP from the NSHome repository root:

```sh
python tools/package_breezepc.py /path/to/BreezePC-beta126-NSHome.zip
```
