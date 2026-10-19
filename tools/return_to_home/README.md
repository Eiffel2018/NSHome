# Return to HOME

Build with devkitA64 and libnx: `make`. Run `return-to-home.nro` from hbmenu
in **Applet Mode**, after ending any debugger session.

The tool sets libnx's `__nx_applet_exit_mode` to 1, then exits. Library-applet
cleanup calls `ExitProcessAndReturn`, ending the current applet and returning
to its caller (normally qlaunch for the Album route). It does not launch or
replace qlaunch. It refuses to force an exit in Application Mode.

This closes the current homebrew applet; it does not retain Breeze alongside
hbmenu. Device verification passed with NSHome 0.1.8 and hbmenu 3.6.1: leave the owned
Breeze for hbmenu, select Return to HOME, and return to original qlaunch.
System Settings opened and the game remained running; no save was loaded and
no debugger was attached. No reboot was needed. This is one console/game case,
not an Application Mode or general applet compatibility test.

SD installation: `switch/ReturnToHOME.nro`, included in the 0.1.9 ZIP. Version
0.1.9 also passed direct 6802 launch from its retained hbmenu: original qlaunch
appeared with the same game still running. The module sends stock HOME notification
after its owned tool finishes so qlaunch restores its UI.
