/* SPDX-License-Identifier: Apache-2.0 */
/** Serial gesture checks feed the BSP input driver, never bypass the game's UI callbacks. */
#include "sdgoods_board.h"
#include "sdgoods_tap.h"
#include "diagnostics.h"
#include "esp_log.h"
void race_diagnostics_init(void){ESP_LOGI("race_input","a/b: lower/upper button; [/]: left/right swipe; m: hold center");}
void sdgoods_console_ext_cmd(char key){
    switch(key){
    case 'a':sdgoods_tap_synth(180,268,0,0,1);break;
    case 'b':sdgoods_tap_synth(180,196,0,0,1);break;
    case '[':sdgoods_tap_synth(280,180,-25,0,8);break;
    case ']':sdgoods_tap_synth(80,180,25,0,8);break;
    case 'm':sdgoods_tap_synth_hold(180,180,950);break;
    case '0':sdgoods_tap_synth(180,218,0,0,1);break;
    case '1':sdgoods_tap_synth(180,180,0,0,1);break;
    case '2':sdgoods_tap_synth(180,40,0,25,3);break;
    case '3':sdgoods_tap_synth(180,320,0,-25,3);break;
    case '4':sdgoods_tap_synth(20,180,25,0,3);break;
    default:break;
    }
}
