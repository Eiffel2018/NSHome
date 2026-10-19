// SPDX-License-Identifier: GPL-2.0-only
#include <switch.h>
#include <stdio.h>

extern u32 __nx_applet_exit_mode;

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (appletGetAppletType() == AppletType_LibraryApplet) {
        // libnx cleanup uses ILibraryAppletSelfAccessor::ExitProcessAndReturn.
        // A plain NRO return would instead chain back to hbmenu in hbloader.
        __nx_applet_exit_mode = 1;
        return 0;
    }

    // Never exit an application process merely to try to reach HOME.
    consoleInit(NULL);
    printf("Return to HOME requires Applet Mode.\nPress B to return to hbmenu.\n");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_B) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
