# Instrumentation temporaire des décisions tap/hold

Proposition **séparée** du correctif de timing (voir `audit_timing.md`). Elle sert à observer, pendant une
courte session sur un texte connu, ce que le firmware a décidé pour chaque touche à double fonction. Rien
n'est actif dans le firmware normal : il faut une compilation dédiée.

## 1. Principe

Trois sources sont confrontées. Aucune ne suffit seule :

| Source | Ce qu'elle dit | Ce qu'elle ne dit pas |
|---|---|---|
| Texte de référence (banc de frappe) | ce que vous vouliez taper | ce que vos doigts ont fait |
| Sortie obtenue (banc de frappe) | ce que l'application a reçu | pourquoi |
| Trace du clavier (`taphold_trace.c`) | appuis/relâchements bruts, décision tap/hold, délai appliqué, couches, touches `KC_NO`, blocage `PRE_MOD_DELAY` | le caractère produit par l'OS (disposition, touches mortes, RDP) |

Un écart sans décision suspecte à proximité orientera vers l'OS, la disposition ou une erreur de doigt.
Un écart à côté d'un HOLD « en pleine frappe » orientera vers l'arbitrage tap/hold.

## 2. Ce qui est enregistré

| Événement | Moment | Contenu |
|---|---|---|
| `d` / `u` | appui / relâchement physique, avant arbitrage et combos | heure, main + rangée, couche active, classe du keycode |
| `P` / `R` | même touche après décision tap/hold | + nombre de taps (0 = hold), interruption, latence de décision, délai en vigueur, permissive hold oui/non |
| `L` | changement de couche | couches actives |
| `S` / `E` | début / fin d'enregistrement | motif de fin (manuel, 10 min atteintes) |

**Position exacte** (rangée/colonne) seulement pour les touches à double fonction, Tap Dance, touches de
couche et modificateurs. Pour toutes les autres touches (lettres, chiffres, ponctuation, `LSFT(x)`, `KC_NO`,
macros), on ne garde que la main (G/D), la rangée (0 haut, 1 milieu, 2 bas, 3 pouces) et la couche.
Ce découpage suffit : le texte étant connu, l'ordre des événements permet de retrouver chaque lettre attendue.

## 3. Protection de la vie privée et limites

- **Arrêt par défaut** : rien n'est enregistré au démarrage. Une touche lance l'enregistrement, qui s'arrête
  seul après **10 minutes** (`THT_MAX_RECORD_MS`). La moitié branchée en USB devient **rouge** pendant
  l'enregistrement (sauf si le RGB est éteint).
- **RAM seulement** : 3 072 événements maximum (48 Kio). Rien n'est écrit en flash ou en EEPROM. Débrancher
  le clavier efface tout ; effacement automatique 30 minutes après l'arrêt ou le dernier vidage.
- **Pas de flux pendant la frappe** : rien ne part vers l'ordinateur tant que vous n'appuyez pas sur la touche
  de vidage. La console ne fausse donc pas les délais, et le partage reste volontaire.
- **Ce qui reste sensible** : les positions des touches doubles révèlent ces lettres (A, S, D, F, G, J, Z, B,
  V, M, `'`, `/`), et les délais entre touches sont une forme de dynamique de frappe. **Ne jamais enregistrer
  pendant un mot de passe, une conversation ou un texte libre.** Utiliser seulement le banc de frappe et ses
  textes. Supprimer les fichiers après analyse.
- Le firmware instrumenté est **temporaire** : on reflashe le firmware normal après la session.

## 4. Console QMK sur ce matériel : vérifié et non vérifié

Vérifié par compilation (Liatris RP2040, `CONVERT_TO=liatris`) :

- `CONSOLE_ENABLE=yes` compile et se lie avec Vial (interface HID console en plus du raw HID de Vial).
  Flash : +3,8 Ko. RAM : le tampon de 48 Kio tient (édition des liens réussie).
- `printf` appelé est bien celui de `lib/printf`, branché sur la console (`vprintf` → `putchar_` → `sendchar`).
- Vial impose `-DNO_DEBUG` (`builddefs/build_vial.mk`) : les traces internes de QMK (`ac_dprintf` du moteur
  tap/hold, `debug_enable`) **restent indisponibles**. D'où cette instrumentation par hooks.
- Particularité de cette version : avec `USER_PRINT`, `uprintf()` est vidé sur ChibiOS (il passe par
  `xprintf`). Le code appelle donc `printf()` directement.

Non vérifié (pas d'accès au clavier depuis cette session) :

- la réception réelle par l'outil hôte ; à tester d'abord avec une session de 10 secondes ;
- le témoin rouge sur votre réglage RGB.

Limites connues :

- **Ouvrir l'outil console AVANT de vider.** Sur ChibiOS, chaque caractère attend jusqu'à 100 ms une place
  dans le tampon USB (`send_report_buffered`, `TIME_MS2I(100)`). Si aucun programme ne lit (cas probable
  sous Linux/macOS), le vidage figerait le clavier plusieurs minutes. Le vidage est découpé (8 lignes par
  boucle) mais reste bloquant dans ce cas.
- Seule la moitié branchée en USB émet. Elle traite les touches des deux moitiés, donc la trace est complète.
- Résolution 1 ms ; `event.time` est sur 16 bits (reconstruit sur 32 bits, valable pour des sessions courtes).
- La trace voit les décisions du firmware, pas les rapports USB réellement émis ni leur traitement par l'OS
  (touches mortes US-International, AltGr, RDP). Pour Tap Dance, l'instant de l'action est **estimé**
  (fin du délai ou touche suivante).
- Coût en frappe : quelques microsecondes par événement (copie en RAM), négligeable devant 1 ms.

Outils de lecture : QMK Toolbox (Windows/macOS, zone « HID Console »), `qmk console`, ou `hid_listen`.

## 5. Mise en œuvre

### 5.1 Compiler et flasher

```bash
make splitkb/kyria/rev3:vial CONVERT_TO=liatris TAPHOLD_TRACE=yes
```

Le firmware par défaut (sans `TAPHOLD_TRACE`) est identique octet pour octet à celui de `main`.

Pour observer une variante, combiner **une seule** variable, par ex. `EXTRAFLAGS=-DPRE_MOD_DELAY=10` :
la comparaison se fait alors contre une trace de référence faite avec le firmware instrumenté par défaut.

Avant de flasher : voir la procédure de sauvegarde et de retour arrière dans `audit_timing.md` §7
(sauvegarde du firmware actuel, rechargement obligatoire de `Kyria.vil` après chaque flash).

### 5.2 Touches de commande (Vial, temporaire)

Après rechargement de `Kyria.vil`, placer dans Vial, onglet « Any », sur la couche 9 (Tab maintenu) :

| Fonction | Keycode « Any » | Position proposée (`KC_NO` sur la couche 9) |
|---|---|---|
| Lancer/arrêter l'enregistrement | `0x7E00` | R01 (position du Y) |
| Vider sur la console | `0x7E01` | R02 (position du U) |
| Effacer le tampon | `0x7E02` | R05 (position du P) |

Ces affectations disparaissent au retour au firmware normal : flasher remet l'EEPROM Vial à zéro, puis vous
rechargez `Kyria.vil`.

### 5.3 Déroulé d'une session (≈ 10 min)

1. Ouvrir `trace_tools/banc_de_frappe.html` dans un navigateur (fichier local). Ouvrir l'outil console.
2. Choisir un texte, cliquer « Commencer ».
3. Tab + Y : l'enregistrement démarre (moitié USB rouge).
4. Taper le texte **sans corriger**. Appuyer sur `Échap` dès qu'une erreur est **ressentie**. La marque est
   notée par la page et visible dans la trace (`L20 LCTL_T(KC_ESCAPE)` en TAP).
5. Tab + Y : arrêt. « Terminer et comparer » dans la page, noter le ressenti dans « Notes ».
6. Tab + U : vidage. Copier la sortie console dans un fichier, par ex. `console.txt`.
   « Exporter JSON » dans la page.
7. Tab + P : effacer le tampon.
8. Analyser :

   ```bash
   python3 trace_tools/tht_analyze.py console.txt --vil Kyria.vil --bench banc_xxx.json
   ```

9. Après la série de sessions : reflasher le firmware normal, recharger `Kyria.vil`, supprimer les fichiers
   de trace qui ne servent plus.

### 5.4 Lire le rapport

- **Décisions tap/hold** : une ligne par touche double. `règle` vaut `tap`, `délai écoulé` (tenue plus
  longtemps que le délai), `permissive hold` (autre touche pressée et relâchée pendant l'appui) ou `autre`.
  Alertes :
  - `HOLD en pleine frappe` : la touche précédente date de moins de 150 ms (`--streak`). Candidat typique
    d'erreur de roulement ; c'est ce que corrigerait un mécanisme de type Flow Tap.
  - `même main` : la touche qui a provoqué le HOLD est de la même main. C'est ce que corrigerait un
    mécanisme de type Chordal Hold.
  - `durée proche du délai` : tap à ±20 ms du délai, réglage limite.
- **Touches KC_NO pressées** : lettre avalée par une couche (`audit_timing.md` §6, H3).
- **PRE_MOD_DELAY** : nombre et durée réelle des blocages.
- **Couches** : chronologie ; une couche 5 inattendue signale un combo F+J involontaire.
- **Écarts du banc de frappe** : chaque écart attendu/obtenu et chaque marque Échap, avec les décisions
  tap/hold à ±400 ms (`--window`). L'alignement suppose que la première touche tapée est la première saisie
  dans la page.

## 6. Retour arrière

- Reflasher le firmware normal, ou le firmware sauvegardé avant l'essai, puis recharger `Kyria.vil`.
- Supprimer le tampon : Tab + P, ou débrancher le clavier.
- Supprimer l'instrumentation du code : revenir sur le commit qui l'ajoute, ou simplement ne jamais compiler
  avec `TAPHOLD_TRACE=yes`.
