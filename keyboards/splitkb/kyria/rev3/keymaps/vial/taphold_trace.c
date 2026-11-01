/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Instrumentation TEMPORAIRE des décisions tap/hold (Kyria rev3, Vial).
 *
 * Compilé seulement avec : make splitkb/kyria/rev3:vial CONVERT_TO=liatris TAPHOLD_TRACE=yes
 * Mode d'emploi, limites et protection de la vie privée :
 * keymaps/vial/docs/instrumentation.md
 *
 * Principes :
 * - Rien n'est enregistré au démarrage. La touche THT_REC lance ou arrête
 *   l'enregistrement ; il s'arrête seul après THT_MAX_RECORD_MS.
 * - Les événements restent en RAM, perdus au débranchement. Effacement
 *   automatique THT_WIPE_AFTER_MS après l'arrêt.
 * - Rien ne sort du clavier pendant la frappe : pas d'effet de la console
 *   sur les délais mesurés. Le contenu n'est envoyé sur la console QMK
 *   qu'à la demande (touche THT_DUMP).
 * - Touches ordinaires anonymes : seuls la main, la rangée et la couche sont
 *   notés, jamais la lettre. La position exacte n'est notée que pour les
 *   touches à double fonction, Tap Dance et les touches de couche.
 */
#include <string.h>
#include QMK_KEYBOARD_H
#include "print.h"
#include "action_tapping.h"
#ifdef QMK_SETTINGS
#    include "qmk_settings.h"
#endif

/* Avec USER_PRINT, ce QMK vide aussi THT_PRINT() sur ChibiOS (uprintf passe par
 * xprintf). On appelle directement printf() de lib/printf, branché sur la
 * console. USER_PRINT reste actif pour couper les messages du cœur QMK. */
#define THT_PRINT printf

#ifndef THT_CAPACITY
#    define THT_CAPACITY 3072 /* 16 octets par événement : 48 Kio de RAM */
#endif
#ifndef THT_MAX_RECORD_MS
#    define THT_MAX_RECORD_MS (10UL * 60UL * 1000UL) /* arrêt auto : 10 min */
#endif
#ifndef THT_WIPE_AFTER_MS
#    define THT_WIPE_AFTER_MS (30UL * 60UL * 1000UL) /* effacement auto : 30 min après l'arrêt */
#endif
#ifndef THT_DUMP_LINES_PER_TICK
#    define THT_DUMP_LINES_PER_TICK 8
#endif

/* À placer dans Vial via « Any » : 0x7E00, 0x7E01, 0x7E02. */
enum { THT_REC = QK_KB_0, THT_DUMP = QK_KB_1, THT_CLEAR = QK_KB_2 };

enum {
    CLS_BASIC = 0, /* lettre, chiffre, ponctuation : anonyme */
    CLS_MODS,      /* LSFT(x), LCTL(x)... : passe par PRE_MOD_DELAY, anonyme */
    CLS_NO,        /* KC_NO : touche masquée par la couche active, anonyme */
    CLS_MODTAP,    /* MT(...) */
    CLS_LAYERTAP,  /* LT(...) */
    CLS_TAPDANCE,  /* TD(...) */
    CLS_LAYER,     /* MO, TG, TO, TT, OSL... */
    CLS_MODIFIER,  /* KC_LSFT... */
    CLS_OTHER      /* macros, souris, média... : anonyme */
};

typedef struct {
    uint32_t t;     /* ms depuis le début de l'enregistrement */
    uint8_t  ev;    /* 'S' début, 'E' fin, 'd'/'u' brut, 'P'/'R' traité, 'L' couches */
    uint8_t  pos;   /* (rangée << 4) | colonne, ou 0xFF si anonyme */
    uint8_t  where; /* bit 7 : main droite ; bits 0-1 : rangée 0..3 ; 0xFF : spécial */
    uint8_t  layer; /* couche la plus haute active */
    uint8_t  cls;   /* CLS_* */
    uint8_t  tap;   /* tap.count | (interrupted << 7) */
    uint16_t lat;   /* ms entre l'événement physique et son traitement */
    uint16_t term;  /* délai tap/hold en vigueur, ou durée du blocage PRE_MOD_DELAY */
    uint16_t extra; /* 'P' : bit0 permissive hold, bit1 hold-on-other-key ; 'L' : couches */
} tht_event_t;

_Static_assert(sizeof(tht_event_t) == 16, "tht_event_t doit rester à 16 octets");

static tht_event_t tht_buf[THT_CAPACITY];
static uint16_t    tht_count;
static bool        tht_recording;
static bool        tht_overflow;
static uint32_t    tht_t0;
static uint32_t    tht_stopped_at;
static bool        tht_has_data;
static int32_t     tht_dump_idx = -1; /* -1 : pas de vidage en cours */

static uint8_t tht_class(uint16_t kc) {
    if (kc == KC_NO) return CLS_NO;
    if (IS_QK_MOD_TAP(kc)) return CLS_MODTAP;
    if (IS_QK_LAYER_TAP(kc)) return CLS_LAYERTAP;
    if (IS_QK_TAP_DANCE(kc)) return CLS_TAPDANCE;
    if (IS_QK_MOMENTARY(kc) || IS_QK_TOGGLE_LAYER(kc) || IS_QK_TO(kc) || IS_QK_LAYER_TAP_TOGGLE(kc) || IS_QK_ONE_SHOT_LAYER(kc) || IS_QK_LAYER_MOD(kc) || IS_QK_DEF_LAYER(kc)) return CLS_LAYER;
    if (IS_MODIFIER_KEYCODE(kc)) return CLS_MODIFIER;
    if (IS_QK_MODS(kc)) return CLS_MODS;
    if (IS_QK_BASIC(kc)) return CLS_BASIC;
    return CLS_OTHER;
}

/* Position exacte seulement si elle ne révèle pas un caractère tapé. */
static bool tht_identify(uint8_t cls) {
    return cls == CLS_MODTAP || cls == CLS_LAYERTAP || cls == CLS_TAPDANCE || cls == CLS_LAYER || cls == CLS_MODIFIER;
}

static uint8_t tht_where(keypos_t k) {
    if (k.row >= MATRIX_ROWS) return 0xFF; /* combo, encodeur, Vial... */
    uint8_t half = MATRIX_ROWS / 2;
    return (k.row >= half ? 0x80 : 0) | (k.row % half);
}

/* event.time est sur 16 bits : on le ramène sur l'horloge 32 bits. */
static uint32_t tht_time(uint16_t event_time) {
    uint32_t now = timer_read32();
    return now - (uint16_t)((uint16_t)now - event_time) - tht_t0;
}

static tht_event_t *tht_push(uint8_t ev) {
    if (!tht_recording) return NULL;
    if (tht_count >= THT_CAPACITY) {
        tht_recording  = false;
        tht_overflow   = true;
        tht_stopped_at = timer_read32();
        return NULL;
    }
    tht_event_t *e = &tht_buf[tht_count++];
    memset(e, 0, sizeof(*e));
    e->ev     = ev;
    e->t      = timer_read32() - tht_t0;
    e->layer  = get_highest_layer(layer_state | default_layer_state);
    e->pos    = 0xFF;
    e->where  = 0xFF;
    tht_has_data = true;
    return e;
}

static void tht_start(void) {
    if (tht_count == 0) tht_t0 = timer_read32();
    tht_recording = true;
    tht_push('S');
}

static void tht_stop(uint8_t reason) {
    tht_event_t *e = tht_push('E');
    if (e) e->cls = reason; /* 0 manuel, 1 durée max */
    tht_recording  = false;
    tht_stopped_at = timer_read32();
}

static void tht_clear(void) {
    tht_recording = false;
    tht_overflow  = false;
    tht_has_data  = false;
    tht_count     = 0;
    tht_dump_idx  = -1;
    memset(tht_buf, 0, sizeof(tht_buf));
}

/* Événements bruts, avant arbitrage tap/hold et combos. */
bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (tht_recording && IS_EVENT(record->event)) {
        tht_event_t *e = tht_push(record->event.pressed ? 'd' : 'u');
        if (e) {
            uint8_t cls = tht_class(keycode);
            e->t     = tht_time(record->event.time);
            e->cls   = cls;
            e->where = tht_where(record->event.key);
            if (tht_identify(cls)) e->pos = (record->event.key.row << 4) | (record->event.key.col & 0x0F);
        }
    }
    return true;
}

/* Événements traités, après décision tap/hold : enrobe process_record_user(). */
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (keycode == THT_REC || keycode == THT_DUMP || keycode == THT_CLEAR) {
        if (record->event.pressed) {
            if (keycode == THT_REC) {
                if (tht_recording) tht_stop(0); else tht_start();
            } else if (keycode == THT_DUMP) {
                if (tht_recording) tht_stop(0);
                tht_dump_idx = -2; /* entête d'abord */
            } else {
                tht_clear();
            }
        }
        return false;
    }

    uint32_t before = timer_read32();
    bool     res    = process_record_user(keycode, record);
    uint32_t stall  = timer_read32() - before;

    if (tht_recording && IS_EVENT(record->event)) {
        tht_event_t *e = tht_push(record->event.pressed ? 'P' : 'R');
        if (e) {
            uint8_t cls = tht_class(keycode);
            e->lat   = (uint16_t)(timer_read() - record->event.time);
            e->t     = tht_time(record->event.time);
            e->cls   = cls;
            e->where = tht_where(record->event.key);
#ifndef NO_ACTION_TAPPING
            e->tap = record->tap.count | (record->tap.interrupted ? 0x80 : 0);
#endif
            if (tht_identify(cls)) e->pos = (record->event.key.row << 4) | (record->event.key.col & 0x0F);
            if (cls == CLS_MODTAP || cls == CLS_LAYERTAP || cls == CLS_TAPDANCE) {
#ifdef TAPPING_TERM_PER_KEY
                e->term = get_tapping_term(keycode, record);
#else
                e->term = TAPPING_TERM;
#endif
#ifdef PERMISSIVE_HOLD_PER_KEY
                if (get_permissive_hold(keycode, record)) e->extra |= 1;
#endif
#ifdef HOLD_ON_OTHER_KEY_PRESS_PER_KEY
                if (get_hold_on_other_key_press(keycode, record)) e->extra |= 2;
#endif
            } else if (cls == CLS_MODS) {
                e->term = (uint16_t)stall; /* durée réelle du blocage PRE_MOD_DELAY */
            }
        }
    }
    return res;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    tht_event_t *e = tht_push('L');
    if (e) {
        e->extra = (uint16_t)state;
        e->layer = get_highest_layer(state | default_layer_state);
    }
    return state;
}

static void tht_dump_header(void) {
    THT_PRINT("THT,HDR,v1,count=%u,cap=%u,overflow=%u", tht_count, THT_CAPACITY, tht_overflow);
#ifdef QMK_SETTINGS
    THT_PRINT(",vial_tapping_term=%u,vial_tapping_flags=%u,combo_term=%u,tap_code_delay=%u", QS.tapping_term, QS.tapping, QS.combo_term, QS.tap_code_delay);
#endif
    THT_PRINT(",tapping_term_default=%u,quick_tap_term=%u", TAPPING_TERM, QUICK_TAP_TERM);
#ifdef PRE_MOD_DELAY
    THT_PRINT(",pre_mod_delay=%u", PRE_MOD_DELAY);
#else
    THT_PRINT(",pre_mod_delay=none");
#endif
    THT_PRINT("\n");
    THT_PRINT("THT,COLS,i,t_ms,ev,pos,where,layer,cls,tap,lat_ms,term_ms,extra\n");
}

void housekeeping_task_user(void) {
    uint32_t now = timer_read32();
    if (tht_recording && (now - tht_t0) > THT_MAX_RECORD_MS) tht_stop(1);
    if (!tht_recording && tht_has_data && tht_dump_idx == -1 && (now - tht_stopped_at) > THT_WIPE_AFTER_MS) tht_clear();

    if (tht_dump_idx == -2) {
        tht_dump_header();
        tht_dump_idx = 0;
        return;
    }
    if (tht_dump_idx < 0) return;
    for (uint8_t n = 0; n < THT_DUMP_LINES_PER_TICK && tht_dump_idx < tht_count; n++, tht_dump_idx++) {
        const tht_event_t *e = &tht_buf[tht_dump_idx];
        THT_PRINT("THT,%u,%lu,%c,%u,%u,%u,%u,%u,%u,%u,%u\n", (unsigned)tht_dump_idx, (unsigned long)e->t, e->ev, e->pos, e->where, e->layer, e->cls, e->tap, e->lat, e->term, e->extra);
    }
    if (tht_dump_idx >= tht_count) {
        THT_PRINT("THT,END,%u\n", tht_count);
        tht_dump_idx   = -1;
        tht_stopped_at = timer_read32(); /* le délai d'effacement repart du vidage */
    }
}

#ifdef RGB_MATRIX_ENABLE
/* Témoin : la moitié reliée en USB devient rouge pendant l'enregistrement. */
bool rgb_matrix_indicators_user(void) {
    if (tht_recording) rgb_matrix_set_color_all(RGB_RED);
    return true;
}
#endif
