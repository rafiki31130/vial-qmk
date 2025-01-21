/* Copyright 2022 splitkb.com <support@splitkb.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "quantum.h"

#ifdef SWAP_HANDS_ENABLE
// clang-format off
    __attribute__ ((weak)) const keypos_t PROGMEM hand_swap_config[MATRIX_ROWS][MATRIX_COLS] = {
        {{0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}},
        {{0, 5}, {1, 5}, {2, 5}, {3, 5}, {4, 5}, {5, 5}},
        {{0, 6}, {1, 6}, {2, 6}, {3, 6}, {4, 6}, {5, 6}},
        {{0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7}},
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}},
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2}},
        {{0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}},
    };
// clang-format on
#    ifdef ENCODER_MAP_ENABLE
        const uint8_t PROGMEM encoder_hand_swap_config[NUM_ENCODERS] = {1,0};
#    endif
#endif

// uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
//     switch (keycode) {
//         case LT(4, KC_GRV):
//             return 220;

//         case LT(6, KC_SPC):
//             return 220;

//         case LT(7, KC_ENT):
//             return 220;

//         case LT(8, KC_ENT):
//             return 220;



//         case SFT_T(KC_A):
//             return 140;

//         case SFT_T(KC_QUOT):
//             return 140;

//         case SFT_T(KC_TAB):
//             return 140;

//         case CTL_T(KC_ESC):
//             return 140;



//         case CTL_T(KC_SPC):
//             return 140;

//         // Par défaut, on utilise le TAPPING_TERM global
//         default:
//             return TAPPING_TERM;
//     }
// }