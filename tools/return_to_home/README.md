# Return to HOME proof tool

Build with devkitA64 and libnx: `make`. Run `return-to-home.nro` from hbmenu
in **Applet Mode**, after ending any debugger session.

The tool sets libnx's `__nx_applet_exit_mode` to 1, then exits. Library-applet
cleanup calls `ExitProcessAndReturn`, ending the current applet and returning
to its caller (normally qlaunch for the Album route). It does not launch or
replace qlaunch. It refuses to force an exit in Application Mode.

This closes the current homebrew applet; it does not retain Breeze alongside
hbmenu. Return from NSHome's retained holder still needs device verification.
This proof tool is not part of the published 0.1.8 ZIP.
