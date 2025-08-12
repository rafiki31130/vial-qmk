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

#pragma once

// #define OLED_DISPLAY_128X64
// #define SPLIT_OLED_ENABLE

// Voir .\keyboards\splitkb\kyria\rev3\rev3.c - process_record_user()
// #define DEAD_ACCENT_AUTOREMOVAL

// Permissive Hold perkey .\quantum\qmk_settings.c
#ifndef PERMISSIVE_HOLD_PER_KEY
#define PERMISSIVE_HOLD_PER_KEY
#endif

// Voir .\keyboards\splitkb\kyria\rev3\rev3.c - process_record_user()
#define PRE_MOD_DELAY 35
