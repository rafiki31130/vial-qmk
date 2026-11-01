# Audit timing Kyria rev3 (Vial) — novembre 2026

Branche `claude/dreamy-johnson-7rfkfw`, partie de `main` = `90fe33584` (même révision que le premier audit :
`main` n'a pas bougé depuis). Rien n'a été flashé.

## 0. Résumé

1. **`TAPPING_TERM_PER_KEY` était déjà actif.** `builddefs/build_vial.mk:7` le passe en `-D` au compilateur.
   Le délai effectif est donc : délai propre de l'entrée Tap Dance, puis exceptions 140/220/250 ms de
   `quantum/vial.c`, puis réglage Vial (175 ms). Le 180 ms de `config.h` n'est que la valeur d'usine.
   Preuves : options de compilation, code préprocessé, code machine (§2). Le correctif rend cela explicite et
   ajoute une garde de compilation, **sans changer un octet du firmware**.
2. **Deux exceptions ne servent à rien.** `SFT_T(KC_QUOT)` vaut `LSFT_T`, or l'export utilise
   `RSFT_T(KC_QUOTE)`, qui reçoit donc 175 ms et non 250. `LT(8, KC_ENT)` n'existe pas dans l'export.
3. **Chaque nouveau flash remet Vial à zéro** (`BUILD_ID` aléatoire) : disposition `keymap.json`, Tap Dance
   vides, délai 180 ms, **permissive hold désactivé et hold-on-other-key-press activé**. Il faut recharger
   `Kyria.vil` et vérifier les réglages après chaque flash (§7).
4. **Permissive hold domine le délai** sur A, S, D, F, J, Z, Espace, Retour arrière droit et le pouce `'`.
   Un roulement imbriqué donne un HOLD quel que soit le délai. Porter A à 250 ms ne protège donc pas des
   roulements « a-i » imbriqués (§5).
5. **Espace (LT6) a permissive hold** : un pouce qui traîne sur Espace pendant la lettre suivante produit
   Ctrl+A, Ctrl+X, Ctrl+Z, Maj+Inser, Ctrl+Tab… (§5.2). C'est le premier candidat à tester.
6. **`PRE_MOD_DELAY=35`** bloque la boucle principale 35 ms pour 87 touches de type `LSFT(x)`/`LCTL(x)`
   (§3). Il n'est pas supprimé : il est rendu réglable à la compilation pour un test A/B en RDP.
7. **Combo couche 5 corrigé dans l'export** (`LT2(KC_J)` + `LT2(KC_F)` → `TG(5)`, 50 ms). Retour par
   `TG(5)` au pouce droit (R24). La couche 1 est orpheline (§4).

## 1. Sources et périmètre

| Élément | Valeur |
|---|---|
| Firmware | `rafiki31130/vial-qmk`, `main` = `90fe33584bba21ccec0cfc21c761de191f03d59b` |
| Export Vial | `Kyria.vil` (Drive, modifié le 2026-11-01 à 15:17 UTC), 9 099 octets, SHA-256 `05eb09c5da0370a2…` ; lu, **non versionné** |
| Contrôleur | Liatris (RP2040), déduit de `users/vial/power_led.c` (GP24) et de `LED_CAPS_LOCK_PIN 24`. Compilation : `CONVERT_TO=liatris` |
| Base QMK du fork | ≈ 0.26 (dernier changelog `docs/ChangeLog/20240825.md`) |
| Firmware réellement flashé | **inconnu** ; test comportemental au §2.4 |

Si le contrôleur n'est pas un Liatris, les conclusions sur le code restent valables, mais les vérifications
binaires sont à refaire avec la bonne cible.

## 2. Point 1 — `TAPPING_TERM_PER_KEY`

### 2.1 Chemin effectif (prouvé)

1. **Options de compilation** (`.build/obj_splitkb_kyria_rev3_vial_liatris/cflags.txt`) : `-DTAPPING_TERM_PER_KEY`,
   `-DPERMISSIVE_HOLD_PER_KEY`, `-DHOLD_ON_OTHER_KEY_PRESS_PER_KEY`, `-DQMK_SETTINGS`. Origine :
   `builddefs/build_vial.mk:5-7` (`TAP_DANCE_ENABLE ?= yes` → `-DTAPPING_TERM_PER_KEY`) et `:23-28`.
   Les `-D` sont lus avant les `config.h`. Dans `keymaps/vial/config.h`, la condition
   `#ifndef PERMISSIVE_HOLD_PER_KEY` était donc toujours fausse, mais sans conséquence.
2. **Préprocesseur** : dans `quantum/action_tapping.c` préprocessé avec ces options,
   `WITHIN_TAPPING_TERM(e)` devient `TIMER_DIFF_16(...) < get_tapping_term(get_record_keycode(&tapping_key, false), &tapping_key)`
   (3 occurrences). Le cas `(TAPPING_TERM)` de `action_tapping.h:58` n'est pas utilisé.
3. **Code machine** : `get_tapping_term.isra.0` (fichier `.map` : provient de `quantum/vial.o`) compare le
   keycode à `0x222b` `SFT_T(KC_TAB)`, `0x212c` `CTL_T(KC_SPC)`, `0x2129` `CTL_T(KC_ESC)` → 140 ;
   `0x2204` `SFT_T(KC_A)`, `0x2234` `SFT_T(KC_QUOT)` → 250 ; `0x4435`, `0x4234`, `0x462c`, `0x4728`/`0x4828`
   → 220. Sinon il lit `QS+24` (`QS.tapping_term`, le réglage Vial). La constante 180 n'y figure pas.

**L'hypothèse « le moteur utilise 180 ms » est donc fausse pour un firmware compilé depuis ce dépôt.**
Elle redevient vraie seulement **entre un flash et le rechargement du `.vil`** (§2.3).

### 2.2 Délai et permissive hold effectifs, export du 1er novembre

Priorité : Tap Dance → exception exacte (`quantum/vial.c:498-545`) → réglage Vial 175 ms.
Permissive hold : liste de `quantum/qmk_settings.c:263`, active si la case Vial « Permissive hold » est
cochée (bit 0 du réglage 8 = 7 → oui). Hold-on-other-key-press : désactivé (bit 1 coché = « Ignore Mod Tap
Interrupt »).

| Position | Couche 0 | Délai | Origine | Permissive hold |
|---|---|---|---|---|
| L11 | `LSFT_T(KC_A)` | 250 | exception | oui |
| L10 | `LT3(KC_S)` | 175 | Vial | oui |
| L09 | `LT8(KC_D)` | 175 | Vial | oui |
| L08 | `LT2(KC_F)` | 175 | Vial | oui |
| L07 | `LSFT_T(KC_G)` | 175 | Vial | non |
| L12 | `LT9(KC_TAB)` | 175 | Vial | non |
| L15 / L16 / L19 | `LCTL_T(KC_B)` / `LALT_T(KC_V)` / `LCTL_T(KC_Z)` | 175 | Vial | non / non / oui |
| L20 | `LCTL_T(KC_ESCAPE)` | 140 | exception | non |
| L21 | `LT7(KC_ENTER)` | 220 | exception | non |
| L22 | `LT6(KC_SPACE)` | 220 | exception | **oui** |
| L23 | `LT4(KC_GRAVE)` | 220 | exception | non |
| R08 | `LT2(KC_J)` | 175 | Vial | oui |
| R11 | `RSFT_T(KC_QUOTE)` | **175** | Vial (l'exception 250 vise `LSFT_T`) | non |
| R16 / R19 | `RALT_T(KC_M)` / `RCTL_T(KC_SLASH)` | 175 | Vial | non |
| R21 | `LT3(KC_ENTER)` | 175 | Vial | non |
| R22 | `LSFT_T(KC_BSPACE)` | 175 | Vial | oui |
| R23 | `LT2(KC_QUOTE)` | 220 | exception | oui |
| R13 / R25 | `TD(5)` / `TD(3)` | 200 / 200 | Tap Dance | — |
| couche 3 L21 / L22 | `RALT_T(KC_COMMA)` / `LCTL_T(KC_SPACE)` | 175 / 140 | Vial / exception | non |
| couche 4 L11 | `LSFT_T(KC_TAB)` | 140 | exception | non |

### 2.3 Anomalies relevées (non corrigées : chacune changerait le comportement)

- `SFT_T(KC_QUOT)` (`vial.c:528`) ne couvre pas `RSFT_T(KC_QUOTE)` (R11). Si le but était le même 250 ms
  que A, il faut écrire `RSFT_T(KC_QUOT)`. C'est l'essai N2c (§5).
- `LT(8, KC_ENT)` (`vial.c:520`) : aucune touche de l'export (couche 8 = `LT8(KC_D)`).
- Case Vial « Tapping force hold » cochée : **sans effet** dans cette version. `get_tapping_force_hold()`
  (`qmk_settings.c:286`) n'est appelé nulle part ; c'est `QUICK_TAP_TERM=80`, fixé à la compilation, qui
  s'applique.
- **Remise à zéro à chaque flash.** `via_eeprom_is_valid()` (`quantum/via.c:85`) compare l'EEPROM à
  `BUILD_ID`, tiré au hasard à chaque compilation (`util/build_id.py`, `builddefs/build_keyboard.mk:228`).
  Au premier démarrage d'un nouveau firmware, `dynamic_keymap_reset()` recharge `keymap.json` (disposition
  d'origine splitkb) et `qmk_settings_reset()` remet `tapping_term = 180` et `tapping = 0` : permissive
  hold désactivé et **hold-on-other-key-press activé sur toutes les touches**. Tant que le `.vil` n'est
  pas rechargé, chaque roulement déclenche un HOLD.

### 2.4 Savoir ce que fait le firmware flashé, sans flasher

Test à faire dans un éditeur de texte, réversible (remettre 175 à la fin) :

1. Vial → QMK Settings → Tap-Hold → Tapping term : **500**.
2. Maintenir **A seul** environ 350 ms (un « un-deux » lent), relâcher. Faire 5 essais. Même chose avec **S seul**.
3. Lecture :

| A seul (350 ms) | S seul (350 ms) | Conclusion |
|---|---|---|
| rien | `s` | exceptions **et** réglage Vial actifs (attendu, 250 < 350 < 500) |
| `a` | `s` | réglage Vial actif, exceptions absentes : firmware différent de ce dépôt |
| rien | rien | réglage Vial ignoré, constante `TAPPING_TERM` (hypothèse initiale) |

4. Remettre 175.

### 2.5 Correctif (commit `b7e0225af`)

- `keymaps/vial/config.h` : `TAPPING_TERM_PER_KEY` défini sans condition (dans un `#ifndef`, pour éviter
  une redéfinition qui serait une erreur avec `-Werror`). Commentaire : 180 = valeur d'usine.
- `rev3.c` : `#error` si `QMK_SETTINGS` est actif sans `TAPPING_TERM_PER_KEY`. Testé : forcer
  `#undef TAPPING_TERM_PER_KEY` fait bien échouer la compilation.
- `quantum/vial.c` : ordre de priorité documenté au-dessus de `get_tapping_term()`.

Le délai global reste réglable dans Vial. Les exceptions restent prioritaires sur lui ; pour qu'une touche
suive le réglage Vial, il faut la retirer du `switch`.

**Preuve de non-régression** : avec `SKIP_VERSION=1` et un `BUILD_ID` fixé localement (modification non
commitée de `util/build_id.py`), `main` et la branche produisent le même `.uf2` :
SHA-256 `84f4ce5c0c1ff61b4f3aa9ddf0c9a60e7e94f9d3401a0818f0596c5c2f43a523`. Le correctif n'apporte donc rien à
flasher : il évite qu'une future modification casse silencieusement le chemin.

## 3. Point 3 — `PRE_MOD_DELAY = 35 ms`

### 3.1 Ce que fait le code actuel (`rev3.c`, `process_record_user`)

Pour tout keycode `QK_MODS` (`LSFT(x)`, `LCTL(x)`, `C_S(x)`, `LCA(x)`, `LGUI(x)`, `SGUI(x)`, `RALT(x)`…)
**issu d'une touche** :

- appui : ajout du modificateur en « weak mod », envoi d'un rapport USB avec le modificateur seul,
  **`wait_ms(35)`**, puis `register_code16(touche)` (touche maintenue, donc répétition par l'OS possible) ;
- relâchement : relâche la touche et retire le modificateur.

Ne passent **pas** par ce chemin : macros `M0…M23`, sorties de Tap Dance (`vial_keycode_down` →
`register_code16`), modificateurs des mod-taps (vrais modificateurs), modificateurs physiques.

Touches concernées dans l'export : couche 0 : 2 (`"` en L06, `^` mort en R06) ; couche 2 : 21 symboles ;
couche 3 : 10 ; couche 4 : 10 ; couche 6 : 22 raccourcis ; couche 7 : 6 ; couche 8 : 15 ; couche 5 : 1.

### 3.2 Historique et hypothèse

- `e38810c25` « p-e que ça règlera les problème de RDP » : ajoute `TAP_CODE_DELAY 20`. Ce délai ne
  s'applique qu'à `tap_code()` (macros, Tap Dance), **pas** aux `LSFT(x)` des couches.
- `2264bc7e9` « 35 ms d'interval avant application des modifiers c'est OK - par contre … ne se répètent
  plus » : modificateur, attente, `tap_code16`. Pas de maintien, donc pas de répétition.
- Code actuel : même attente, mais la touche est maintenue, donc la répétition fonctionne.

Hypothèse cohérente avec cet ordre : sans délai, QMK envoie le rapport « modificateur » puis le rapport
« modificateur + touche » à quelques microsecondes d'intervalle (`USB_POLLING_INTERVAL_MS = 1`).
Un client RDP qui retraduit les événements peut alors transmettre la touche sans son modificateur (`9` au
lieu de `(`). **Le symptôme exact n'est pas établi** : à reproduire avec `PRE_MOD_DELAY=0` (§3.4) avant
toute décision.

### 3.3 Coûts

| Coût | Ampleur | Quand ça se voit |
|---|---|---|
| Latence +35 ms par symbole ou raccourci | systématique | code (`(){}:"_*`), raccourcis en rafale |
| Boucle principale bloquée 35 ms (`chThdSleepMilliseconds`) : pas de scan matriciel, pas de synchro entre moitiés, pas de minuterie tap/hold | systématique | une touche pressée **et** relâchée pendant ces 35 ms est perdue (rare) ; une touche pressée pendant le blocage reçoit une heure d'appui en retard (≤ 35 ms) |
| Décision tap/hold faussée | possible | une touche double pressée pendant le blocage semble tenue jusqu'à 35 ms de moins |
| Répétition de la 2e touche sans modificateur si deux `LSFT(x)` se chevauchent | mineur | maintien long de `)` après `(` |

Le binaire contient bien 35 000 ticks à 1 MHz (`0x88b8`), soit 35 ms.

### 3.4 Essai A/B (commit `64b12f0f2`)

`PRE_MOD_DELAY` se règle maintenant sans modifier de fichier ; `0` désactive le mécanisme :

```bash
make splitkb/kyria/rev3:vial CONVERT_TO=liatris EXTRAFLAGS=-DPRE_MOD_DELAY=0
```

Variantes vérifiées par compilation : défaut (35, identique à `main`), 10 (`0x2710` ticks), 0 (aucune attente).

Protocole :

1. Texte « symboles » et « code » du banc de frappe, plus 10 raccourcis (Ctrl+C/V/Z dans un Bloc-notes),
   **en local puis dans la session RDP**, 2 passages chacun.
2. Ordre : 35 (référence) → **0** (le symptôme réapparaît-il en RDP ?) → 10 → 20.
3. Critère : nombre de symboles sans modificateur ou de raccourcis perdus pour 100 essais, en RDP ;
   ressenti de latence en local.
4. Décision : garder la plus petite valeur sans erreur RDP sur deux journées. Si 0 ne produit aucune
   erreur, le symptôme a disparu (client RDP mis à jour ?) : garder 35 jusqu'à une seconde confirmation,
   puis passer à 0.
5. Retour arrière : reflasher le firmware de référence et recharger `Kyria.vil` (§7).

Hors périmètre, à ne considérer qu'après mesure : un délai non bloquant (`defer_exec`) supprimerait le gel
du scan, mais pourrait inverser l'ordre avec une touche suivante. Le blocage actuel est plus sûr pour l'ordre.

## 4. Point 4 — Combo couche 5, accès et retour des couches

**Combo 1** : `LT2(KC_J)` + `LT2(KC_F)` → `TG(5)`, terme 50 ms. Les deux keycodes correspondent exactement
à la couche 0 : la correction est bonne.

- Retour : `TG(5)` en **R24** (pouce droit, position de `KC_RGUI` en couche 0). Sur la couche 5, F et J sont
  de simples lettres : le combo ne peut pas servir à revenir. Pas d'autre sortie, pas de témoin visuel.
- Symptôme d'une entrée involontaire : plus aucun symbole, raccourci ni majuscule par maintien. Remède : R24.
- Effets de bord : F et J sont retenus jusqu'à la touche suivante ou 50 ms (tampon du combo), avec leur heure
  d'origine, donc sans effet sur l'arbitrage. Déclenchement accidentel peu probable : F et J à moins de 50 ms
  n'apparaissent dans aucun mot français courant.

| Couche | Accès | Retour | Remarques |
|---|---|---|---|
| 1 | **aucun** (seulement `TG(1)` sur la couche 1 elle-même) | `TG(1)` L06/R09 | orpheline : à supprimer ou à relier |
| 2 | `LT2` sur F, J, pouce `'` (R23) | relâcher | pouces L13 L14 L24 L25 R13 R14 R24 R25 = `KC_NO` |
| 3 | `LT3` sur S, pouce Entrée droit (R21) | relâcher | pouces gauches à double fonction : `RALT_T(,)`, `LCTL_T(Espace)` 140 ms ; S P Q X W = `KC_NO` |
| 4 | `LT4` pouce `` ` `` (L23) | relâcher | |
| 5 | combo F+J | `TG(5)` R24 | voir ci-dessus |
| 6 | `LT6` Espace (L22) | relâcher | permissive hold : voir §5.2 |
| 7 | `LT7` Entrée gauche (L21) | relâcher | |
| 8 | `LT8` sur D (seul accès) | relâcher | E, S, C, D, Q = `KC_NO` ; `LT9(KC_F2)` sur X → couche 9 |
| 9 | `LT9` Tab (L12), `LT9(F2)` depuis la couche 8 | relâcher | pouces transparents |

## 5. Point 2 — Erreurs de roulement : solutions graduées

### 5.1 Les trois issues possibles

Pour une touche double tenue pendant qu'une autre touche arrive :

| Cas | Décision | Le délai compte ? |
|---|---|---|
| relâchée avant le délai, autre touche relâchée **après** elle (roulement simple) | TAP | oui |
| tenue au-delà du délai | HOLD | oui |
| autre touche pressée **et relâchée** pendant l'appui (roulement imbriqué), touche avec permissive hold | **HOLD immédiat** | **non** |

Avec permissive hold, l'erreur typique vient d'un doigt qui relâche la touche double un peu tard : rien à
voir avec le délai. Les 250 ms de A ne protègent que du cas « A tenu seul trop longtemps ».

### 5.2 Signatures : ce que devient la lettre suivante si la touche double est prise pour un HOLD

(Disposition hôte US-International, déduite des macros de touches mortes.)

| Touche tenue | Lettre suivante → résultat |
|---|---|
| D (`LT8`) | e, s, c, d, q → **rien** (`KC_NO`) ; i → ↑ ; l → → ; n, m → Retour arr. ; a → Maj |
| S (`LT3`) | e → `#`, i → `8`, o → `9`, u → `7`, t → `%`, r → `$`, n → `)`, d → `*`, l → `6`, s/p/q/x/w → rien |
| F, J (`LT2`) | e → `#`, a → `!`, i → `*`, o → `_`, r → `$`, t → `%`, u → `&`, s → `(`, d → `)`, l → `-`, p → `+` |
| A, G (`LSFT_T`) | lettre suivante en **majuscule**, le a/g disparaît (« ai » → « I ») |
| B, Z (`LCTL_T`) | Ctrl+lettre : « bo » → Ctrl+O, « br » → Ctrl+R, « bl » → Ctrl+L |
| V (`LALT_T`) | Alt+lettre (menus) |
| M (`RALT_T`) | AltGr+lettre en US-Intl : « me » → `é`, « ma » → `á`, « mo » → `ó`, « mi » → `í` |
| `'` (`RSFT_T`) | « 'e » → `E` au lieu de `é` |
| Espace (`LT6`, permissive) | a → Ctrl+A, c → Ctrl+C, x → Ctrl+X, z → Ctrl+Z, v → Maj+Inser, r → Ctrl+Tab, t → Ctrl+Maj+T, f → Ctrl+F |

Les plus coûteuses en français : **« de »/« des » avalés** (D + E = rien), **Espace + lettre → raccourci**,
« se/si/so » → `#`/`8`/`9`, « me/ma » → voyelle accentuée.

### 5.3 Niveaux d'essai (un seul changement à la fois)

Chaque essai : ≥ 3 sessions sur 2 jours avec les mêmes textes (§5.4), comparées à la référence N0.
Garder un changement seulement s'il réduit les erreurs ciblées sans en créer d'autres (raccourcis voulus
devenus difficiles, symboles ratés).

**N0 — Référence.** Firmware et `.vil` actuels. Banc de frappe, textes « fr-sans-accent », « fr-accents »,
« code », 3 fois chacun. Optionnel : trace instrumentée (`instrumentation.md`).

**N1 — Vial seul, sans flash, annulable en rechargeant `Kyria.vil`.**

| Essai | Changement | Ce qu'on attend | Coût |
|---|---|---|---|
| N1a | S : `LT3(KC_S)` → `KC_S` | disparition de `#` `8` `9` `7` dans « se si so su » | couche 3 seulement par le pouce droit (R21) |
| N1b | A : `LSFT_T(KC_A)` → `KC_A` | disparition des majuscules après « a » | Maj par `RSFT_T(')`, `LSFT_T(G)` ou pouce droit |
| N1c | **décocher « Permissive hold »** dans Vial (1 session, diagnostic) | si les erreurs imbriquées disparaissent, la cause est confirmée | symboles F/J et raccourcis Espace à tenir ≥ 175/220 ms |
| N1d | délai global 175 → 200 | moins de HOLD « tenu trop longtemps » sur S, D, F, J, G… | maintiens volontaires plus lents ; sans effet sur A, Espace (exceptions) |

D (`LT8`) n'est pas testable sans la couche 8 : c'est son seul accès. Le déplacer est un changement de
disposition, à proposer après mesure. Même remarque pour M : `RALT_T(M)` sert probablement au `ç`
(AltGr + `,`). Ne pas le retirer sans autre accès au `ç`.

**N2 — Firmware, réglages par touche** (`quantum/qmk_settings.c`, `quantum/vial.c`), un flash par essai.

| Essai | Changement | Pourquoi |
|---|---|---|
| N2a | retirer `LT(6, KC_SPC)` de `get_permissive_hold` | Espace + lettre → raccourcis destructeurs ; Espace est la touche la plus fréquente |
| N2b | retirer `LSFT_T(KC_A)` de `get_permissive_hold` | majuscule involontaire sur roulement imbriqué ; le délai 250 ms reprend son effet |
| N2c | `SFT_T(KC_QUOT)` → `RSFT_T(KC_QUOT)` dans `get_tapping_term` ; supprimer `LT(8, KC_ENT)` | aligner `'` sur l'intention (250 ms), retirer le code mort |
| N2d | délai par touche pour D et S (par ex. 200) | seulement si la trace montre des HOLD par délai écoulé |

**N3 — Mécanismes absents de ce fork.** Ce fork date de QMK ≈ 0.26 (août 2024). Il n'a ni Chordal Hold
(QMK 0.28), ni Flow Tap (QMK 0.29), ni Speculative Hold. Options, par coût croissant :

1. **Flow Tap maison** (≈ 40 lignes dans `pre_process_record_user`) : une touche double pressée moins de
   `FLOW_TAP_TERM` ms après une lettre devient un tap pur. Ce fork honore `record->keycode`
   (`quantum/action.c:291`, grâce à `COMBO_ENABLE`). Esquisse vérifiée à la compilation, non versionnée :

   ```c
   #define FLOW_TAP_TERM 130
   static uint16_t ft_last_press;
   static bool     ft_last_was_letter;
   static uint8_t  ft_forced[MATRIX_ROWS];

   static bool ft_eligible(uint16_t kc) {
       switch (kc) {
           case LSFT_T(KC_A): case LT(3, KC_S): case LT(8, KC_D):
           case LT(2, KC_F):  case LT(2, KC_J): case LSFT_T(KC_G):
               return true;
           default:
               return false;
       }
   }
   static uint16_t ft_tap_keycode(uint16_t kc) {
       return IS_QK_MOD_TAP(kc) ? QK_MOD_TAP_GET_TAP_KEYCODE(kc) : QK_LAYER_TAP_GET_TAP_KEYCODE(kc);
   }
   bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
       keypos_t k = record->event.key;
       if (!IS_EVENT(record->event) || k.row >= MATRIX_ROWS) return true;
       if (record->event.pressed) {
           bool streak = ft_last_was_letter && TIMER_DIFF_16(record->event.time, ft_last_press) < FLOW_TAP_TERM;
           if (streak && ft_eligible(keycode)) {
               record->keycode = ft_tap_keycode(keycode);
               ft_forced[k.row] |= (uint8_t)(1 << k.col);
           }
           uint16_t kc        = record->keycode ? record->keycode : keycode;
           ft_last_was_letter = (kc >= KC_A && kc <= KC_Z) || ft_eligible(keycode);
           ft_last_press      = record->event.time;
       } else if (ft_forced[k.row] & (1 << k.col)) {
           record->keycode = ft_tap_keycode(keycode);
           ft_forced[k.row] &= (uint8_t)~(1 << k.col);
       }
       return true;
   }
   ```

   Coût : un raccourci ou un symbole tapé au milieu d'un mot demande une courte pause. Incompatible tel quel
   avec l'instrumentation, qui définit aussi `pre_process_record_user` : il faudrait fusionner les deux.
2. **Achordion** (bibliothèque de P. Getreuer, compatible avec les QMK anciens) : règle de la main opposée et
   détection de frappe en cours. Elle convient bien à cette disposition : la couche 2 est accessible des deux
   mains (F et J), les chiffres de la couche 3 sont à droite et S à gauche, la navigation de la couche 8 est
   à droite et D à gauche. Exceptions à prévoir : raccourcis gauches de la couche 8 (Ctrl+I, Ctrl+B…).
3. **Rebaser le fork** sur un vial-qmk récent, s'il intègre QMK ≥ 0.29 (à vérifier) : Chordal Hold et
   Flow Tap natifs. C'est le plus gros chantier.

### 5.4 Séquences de comparaison

| Mécanisme visé | Français | Code |
|---|---|---|
| D + voyelle (`KC_NO`) | « de des depuis demande deux » | `def`, `del`, `idx` |
| S (couche 3) | « se si son sur sous sans souvent » | `self`, `str`, `args` |
| F/J (couche 2) | « je jamais faire fait fin enfin » | `if`, `fn`, `json`, `obj` |
| A, G (Maj) | « au ai avant aussi » / « gare grand » | `as`, `and`, `args`, `get` |
| M (AltGr) | « me ma moi mieux comme » | `map`, `import` |
| B, V, Z (Ctrl/Alt) | « beau blanc bonjour bref vous vivre » | `bool`, `break`, `void`, `zip` |
| Espace (couche 6) | phrases normales : surveiller les raccourcis interceptés | idem |
| `PRE_MOD_DELAY` | « : ? ! » de la typographie française | `(){}[]"_*&|<>` |

Les textes sont intégrés à `trace_tools/banc_de_frappe.html`.

## 6. Point 5 — Erreurs difficiles à attribuer

| # | Hypothèse | Séquence reproductible | Ce qu'on voit | Critère de discrimination |
|---|---|---|---|---|
| H1 | Lettre perdue ou majuscule par arbitrage tap/hold | « ai au an » en relâchant A un peu tard ; « as » avec A tenu 300 ms | « I », « U » ; ou rien | Sans trace : l'erreur disparaît avec N1b. Trace : A `HOLD`, règle `permissive hold` ou `délai écoulé` |
| H2 | Symbole produit par une couche momentanée | « se si fe je » roulés vite | `#`, `8`, `!`… à la place des deux lettres | Le symbole correspond **exactement** à la table §5.2 pour la touche précédente. Trace : S/F/J `HOLD` puis `L` couche 2/3 |
| H3 | `KC_NO` masquant la couche de base | « de des » roulés vite | **les deux** lettres manquent | D + E absents tous les deux, sans symbole. Trace : `KC_NO` sur couche 8 |
| H4 | Action retardée par Tap Dance, pas par `TAPPING_TERM` | PgUp (R25, `TD(3)`) seul, puis PgUp suivi aussitôt d'une autre touche | ≈ 200 ms de retard seul ; immédiat si une autre touche suit | Le retard suit le délai TD (200/180 ms), pas 175. `TD(0)` (tap + hold seuls) n'a pas de retard au relâchement. Trace : section Tap Dance |
| H5 | Raccourci ralenti par `PRE_MOD_DELAY` | Ctrl+C/V depuis la couche 6, `()` depuis la couche 2 | +35 ms, gêne en rafale ; en RDP, symbole sans Maj si délai trop court | Varie avec `EXTRAFLAGS=-DPRE_MOD_DELAY=…` ; les macros et les Tap Dance n'ont pas ce retard. Trace : section `PRE_MOD_DELAY` |
| H6 | Dépend de l'OS, de la disposition ou de l'application | « 'e », « ^e », « me » en local puis en RDP, puis dans une autre application | `é` vs `'e` ; `é` après M ; raccourcis captés par l'application | Même trace firmware, sorties différentes selon le contexte → cause hôte. Le banc de frappe note les raccourcis interceptés |
| H7 | Combo F+J → couche 5 silencieuse | F et J ensemble à moins de 50 ms | plus de symboles ni de Maj | Revient avec R24. Trace : `L` couche 5 |
| H8 | Erreur de doigt (pas le firmware) | — | lettre voisine, inversion | Trace brute conforme à la sortie : aucune décision HOLD à proximité |

Règle générale : confronter **intention** (texte de référence), **sortie** (banc de frappe) et **trace**. Une
trace seule ne dit pas ce que vous vouliez taper ; une sortie seule ne dit pas pourquoi.

## 7. Protocole A/B et retour arrière

**Règles**
- Une seule variable par essai : délai, permissive hold, disposition **ou** instrumentation, jamais deux.
  L'instrumentation sert d'abord à mesurer la référence N0, puis une variante à la fois.
- Mêmes textes, même application, même contexte (local ou RDP), même moment de la journée si possible.
- Journal (un fichier local) :

| Date | Firmware (commit + SHA-256 du .uf2) | `.vil` | Variable testée | Texte | Contexte | Écarts / 100 car. | Marques Échap | Ressenti |
|---|---|---|---|---|---|---|---|---|

**Avant le premier flash**
1. Vial → File → Save current layout → `Kyria_avant_essai.vil` (en plus de `Kyria.vil`).
2. Sauvegarder le firmware actuel s'il n'existe nulle part : en mode BOOTSEL, `picotool save -a kyria_actuel.uf2`
   (outil Raspberry Pi, si disponible). Sinon, le retour arrière se fera vers un firmware compilé depuis `main`.

**Flasher une variante**
1. Compiler (§3.4 ou `instrumentation.md`). Noter le SHA-256 du `.uf2`.
2. Double appui sur reset de la moitié → lecteur `RPI-RP2` → copier le `.uf2`. Faire de même pour l'autre
   moitié (même fichier).
3. **Vial → File → Load saved layout → `Kyria.vil`** (obligatoire : l'EEPROM vient d'être remise à zéro).
4. Vérifier dans Vial → QMK Settings → Tap-Hold : Tapping term 175, Permissive hold ☑, Ignore Mod Tap
   Interrupt ☑ (Tapping force hold ☑ sans effet, Retro tapping ☐). Vérifier le combo et les Tap Dance.
5. Faire le test du §2.4.

**Retour arrière** : flasher `kyria_actuel.uf2` (ou `main`), recharger `Kyria.vil`, refaire l'étape 4.
Pour un essai N1 (Vial seul) : recharger `Kyria.vil` suffit.

## 8. Livrables de la branche

| Commit | Contenu | Effet sur le firmware par défaut |
|---|---|---|
| `b7e0225af` | `TAPPING_TERM_PER_KEY` explicite, garde `#error`, priorité documentée | aucun (SHA-256 identique) |
| `64b12f0f2` | `PRE_MOD_DELAY` réglable par `EXTRAFLAGS`, `0` = désactivé | aucun (SHA-256 identique) |
| `b52a6b77d` | `taphold_trace.c`, `trace_tools/`, `docs/instrumentation.md`, activée par `TAPHOLD_TRACE=yes` | aucun (SHA-256 identique) |
| ce document | audit et protocoles | — |

Compiler le firmware normal :

```bash
make splitkb/kyria/rev3:vial CONVERT_TO=liatris
```
