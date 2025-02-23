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


#ifdef DEAD_ACCENT_AUTOREMOVAL
bool accent_tap_pending = false;
bool accent_key_pressed = false;
bool accent_used_while_pressed = false;
uint32_t accent_timer = 0;
uint32_t accent_delay = 1000;
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
#ifdef DEAD_ACCENT_AUTOREMOVAL
    if ( (accent_tap_pending || accent_key_pressed) && record->event.pressed) {
        switch (keycode) {
            case LT(4, KC_GRV):
            case RSFT_T(KC_QUOT):
            case LSFT(KC_6):
            case LSFT_T(KC_BSPC):
            case KC_BSPC:
                break;

            default:
                accent_tap_pending = false;
                if (accent_key_pressed) {
                    accent_used_while_pressed = true;
                }
                break;
        }
    }

    switch (keycode) {
        case LSFT_T(KC_BSPC):
        case KC_BSPC:
            if (record->event.pressed && (accent_tap_pending || accent_key_pressed) ) {
                tap_code(KC_SPC);
                accent_tap_pending = false;
                if (accent_key_pressed) {
                    accent_used_while_pressed = true;
                }
            }
            break;

        case LT(4, KC_GRV):
        case RSFT_T(KC_QUOT):
            if (!record->event.pressed && record->tap.count > 0) {
                if (!accent_tap_pending && !accent_used_while_pressed) {
                    accent_tap_pending = true;
                    accent_timer = timer_read();
                } else {
                    accent_tap_pending = false;
                }
                accent_key_pressed = false;
                accent_used_while_pressed = false;
            }
            else if (record->event.pressed) {
                accent_key_pressed = true;
                accent_used_while_pressed = false;
            }
            else {
                accent_key_pressed = false;
                accent_used_while_pressed = false;
                accent_tap_pending = false;
            }
            break;

        case LSFT(KC_6):
            if (!record->event.pressed) {
                if (!accent_tap_pending && !accent_used_while_pressed) {
                    accent_tap_pending = true;
                    accent_timer = timer_read();
                } else {
                    accent_tap_pending = false;
                }
                accent_key_pressed = false;
                accent_used_while_pressed = false;
            }
            else {
                accent_key_pressed = true;
                accent_used_while_pressed = false;
            }
            break;

        default:
            break;
    }

#endif

    return true;
}

void matrix_scan_user(void) {
#ifdef DEAD_ACCENT_AUTOREMOVAL
    if (accent_tap_pending) {
        if (timer_elapsed(accent_timer) > accent_delay) {
            tap_code(KC_SPC);
            tap_code(KC_BSPC);

            accent_tap_pending = false;
        }
    }

#endif
}
