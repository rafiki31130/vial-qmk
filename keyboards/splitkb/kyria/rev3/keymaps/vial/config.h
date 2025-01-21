/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2023 splitkb.com <support@splitkb.com> */

#pragma once

#define VIAL_KEYBOARD_UID {0x0F, 0xF9, 0x13, 0x6C, 0x1D, 0xBF, 0x7D, 0xA6}

#define VIAL_UNLOCK_COMBO_ROWS { 0, 4 }
#define VIAL_UNLOCK_COMBO_COLS { 1, 1 }

#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define RGB_MATRIX_KEYPRESSES

// Usage of the microcontroler's LED as indicator of the CAPS Lock
#define LED_CAPS_LOCK_PIN 24  
#define LED_PIN_ON_STATE 0

#define DYNAMIC_KEYMAP_LAYER_COUNT 10 // default 8

// Tweak of special features numbers
#define VIAL_TAP_DANCE_ENTRIES 6 // default 32
#define VIAL_COMBO_ENTRIES 6 // default 32
// #define VIAL_KEY_OVERRIDE_ENTRIES x // Deactivated in rules.mk
#define DYNAMIC_KEYMAP_MACRO_COUNT 32 //default 16




/* intervale de répétition empêchant le trigger de la fonction HOLD - default 120 */
#define QUICK_TAP_TERM 100

/* Permet de considérer la fonction HOLD lorsque la touche combinée est enfoncée+relachée avant que HOLD ne soit relaché, sans tenir compte du TAPPING_TERM */
#define PERMISSIVE_HOLD

/* Délais pour prendre en compte une fonction HOLD */
#define TAPPING_TERM 180

/* Désactive une correction dont j'ai pas tout compris mais sans ça, les home-row modifiers déclenchent tout le temps  */
/* Deprecated: #define IGNORE_MOD_TAP_INTERRUPT */

//#define RGB_DISABLE_AFTER_TIMEOUT 300000 // Désactive les LEDs après X ms d'inactivité (0 pour désactiver)