/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * Serial test input for on-device UI checks, added to the board console's application hook.
 * Built-in console characters (digits, s, R, X, ...) keep their board meaning, so coordinates
 * are spelled with the letters "abcdefgijk" for 0..9:
 *   T<xxx><yyy>  tap        G<xxx><yyy>  long press (900 ms)
 *   Q            swipe left Y            swipe right
 * Any other character cancels a partly received command.
 */
#include <string.h>
#include "sdkconfig.h"
#include "sdgoods_tap.h"

#if CONFIG_LAUNCHER_SERIAL_TEST_INPUT
void sdgoods_console_ext_cmd(char c) {
    static const char digits[] = "abcdefgijk";
    static char command;
    static int value, count;
    if (command) {
        const char *digit = c ? strchr(digits, c) : NULL;
        if (!digit) { command = 0; return; }
        value = value * 10 + (int)(digit - digits);
        if (++count < 6) return;
        int x = value / 1000, y = value % 1000;
        if (command == 'T') sdgoods_tap_synth(x, y, 0, 0, 1);
        else sdgoods_tap_synth_hold(x, y, 900);
        command = 0;
        return;
    }
    switch (c) {
    case 'T': case 'G': command = c; value = 0; count = 0; break;
    case 'Q': sdgoods_tap_synth(260, 180, -30, 0, 6); break;
    case 'Y': sdgoods_tap_synth(100, 180, 30, 0, 6); break;
    default: break;
    }
}
#endif
