#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Analyse hors ligne d'un vidage de taphold_trace.c (lignes « THT,... »).

Usage :
  python3 tht_analyze.py console.txt [--vil Kyria.vil] [--bench banc.json] [--streak 150]

- console.txt : sortie de la console QMK (QMK Toolbox, `qmk console`, hid_listen),
  copiée telle quelle ; seules les lignes contenant « THT, » sont lues.
- --vil   : export Vial, pour afficher le keycode de base des touches identifiées.
- --bench : export JSON de banc_de_frappe.html, pour placer chaque écart
  (texte attendu / texte obtenu) à côté des décisions tap/hold voisines.

Rien n'est envoyé nulle part : le script lit des fichiers locaux et écrit sur stdout.
"""
import argparse
import json
import os
import re
import statistics
import sys

CLS = {0: "lettre", 1: "MODS", 2: "KC_NO", 3: "mod-tap", 4: "layer-tap", 5: "tap-dance", 6: "couche", 7: "modificateur", 8: "autre"}
HERE = os.path.dirname(os.path.abspath(__file__))
KEYBOARD_JSON = os.path.join(HERE, "..", "..", "..", "keyboard.json")


def load_labels(path):
    """matrice (rangée, colonne) -> étiquette L01..R25, lue sans dépendance hjson."""
    labels = {}
    try:
        txt = open(path, encoding="utf-8").read()
    except OSError:
        return labels
    for lab, r, c in re.findall(r'"label":\s*"(\w+)",\s*"matrix":\s*\[(\d+),\s*(\d+)\]', txt):
        labels[(int(r), int(c))] = lab
    return labels


def parse(path):
    hdr, ev = {}, []
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.search(r"THT,(.*)$", line.strip())
        if not m:
            continue
        f = m.group(1).split(",")
        if f[0] == "HDR":
            for kv in f[2:]:
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    hdr[k] = v
        elif f[0].isdigit() and len(f) >= 11:
            i, t, e, pos, where, layer, cls, tap, lat, term, extra = f[:11]
            ev.append(dict(i=int(i), t=int(t), ev=e, pos=int(pos), where=int(where), layer=int(layer),
                           cls=int(cls), tap=int(tap), lat=int(lat), term=int(term), extra=int(extra)))
    return hdr, ev


def hand(w):
    if w == 255:
        return "?"
    return ("D" if w & 0x80 else "G") + str(w & 3)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("console")
    ap.add_argument("--vil")
    ap.add_argument("--bench")
    ap.add_argument("--keyboard-json", default=KEYBOARD_JSON)
    ap.add_argument("--streak", type=int, default=150, help="ms : en dessous, la touche est pressée en pleine frappe")
    ap.add_argument("--window", type=int, default=400, help="ms autour d'un écart du banc de frappe")
    a = ap.parse_args()

    labels = load_labels(a.keyboard_json)
    vil = json.load(open(a.vil, encoding="utf-8")) if a.vil else None
    base = vil["layout"][0] if vil else None

    def kc_at(layer, pos):
        if vil is None or pos == 255:
            return None
        r, c = pos >> 4, pos & 15
        try:
            k = vil["layout"][layer][r][c]
        except IndexError:
            return None
        return k if isinstance(k, str) else None

    def name(pos):
        if pos == 255:
            return "anonyme"
        r, c = pos >> 4, pos & 15
        s = labels.get((r, c), f"r{r}c{c}")
        if base is not None and r < len(base) and c < len(base[r]) and isinstance(base[r][c], str):
            s += f" {base[r][c]}"
        return s

    hdr, ev = parse(a.console)
    if not ev:
        sys.exit("Aucune ligne THT trouvée.")
    print("# Analyse de trace tap/hold\n")
    print("Réglages relevés au vidage : " + ", ".join(f"{k}={v}" for k, v in hdr.items()) + "\n")

    raw = [e for e in ev if e["ev"] in "du"]
    downs = [e for e in raw if e["ev"] == "d"]

    # Décisions des touches à double fonction
    rows = []
    for p in (e for e in ev if e["ev"] == "P" and e["cls"] in (3, 4)):
        d = next((x for x in reversed(downs) if x["pos"] == p["pos"] and x["t"] <= p["t"] + 1), None)
        if d is None:
            continue
        u = next((x for x in raw if x["ev"] == "u" and x["pos"] == p["pos"] and x["t"] >= d["t"]), None)
        prev = next((x for x in reversed(downs) if x["t"] < d["t"]), None)
        idle = d["t"] - prev["t"] if prev else None
        others = [x for x in downs if d["t"] < x["t"] and (u is None or x["t"] < u["t"])]
        first = others[0] if others else None
        nested = None
        if first is not None and u is not None:
            fu = next((x for x in raw if x["ev"] == "u" and x["t"] >= first["t"] and x["where"] == first["where"] and x["pos"] == first["pos"]), None)
            nested = fu is not None and fu["t"] <= u["t"]
        is_tap = (p["tap"] & 0x0F) > 0
        if is_tap:
            rule = "tap"
        elif p["lat"] >= p["term"] - 2:
            rule = "délai écoulé"
        elif nested and p["extra"] & 1:
            rule = "permissive hold"
        else:
            rule = "autre"
        flags = []
        if not is_tap and idle is not None and idle < a.streak:
            flags.append("HOLD en pleine frappe")
        if not is_tap and first is not None and (first["where"] & 0x80) == (d["where"] & 0x80) and first["where"] != 255:
            flags.append("même main")
        if is_tap and u is not None and abs((u["t"] - d["t"]) - p["term"]) <= 20:
            flags.append("durée proche du délai")
        rows.append(dict(t=d["t"], key=name(p["pos"]), dec="TAP" if is_tap else "HOLD", rule=rule,
                         dur=(u["t"] - d["t"]) if u else None, idle=idle, term=p["term"],
                         ph="oui" if p["extra"] & 1 else "non", nested=nested,
                         first=hand(first["where"]) if first else "-", flags=flags))

    print("## Décisions tap/hold\n")
    print("| t (ms) | touche | décision | règle | durée | pause avant | délai | permissive | 1re autre touche | imbriquée | alerte |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        print(f"| {r['t']} | {r['key']} | {r['dec']} | {r['rule']} | {r['dur']} | {r['idle']} | {r['term']} | {r['ph']} | {r['first']} | {r['nested']} | {', '.join(r['flags'])} |")

    print("\n## Synthèse par touche\n")
    print("| touche | taps | holds | holds en pleine frappe | holds même main | durée médiane tap (ms) |")
    print("|---|---|---|---|---|---|")
    for k in sorted({r["key"] for r in rows}):
        rk = [r for r in rows if r["key"] == k]
        taps = [r for r in rk if r["dec"] == "TAP"]
        holds = [r for r in rk if r["dec"] == "HOLD"]
        med = statistics.median([r["dur"] for r in taps if r["dur"] is not None]) if any(r["dur"] is not None for r in taps) else "-"
        print(f"| {k} | {len(taps)} | {len(holds)} | {sum('HOLD en pleine frappe' in r['flags'] for r in holds)} | {sum('même main' in r['flags'] for r in holds)} | {med} |")

    nos = [e for e in ev if e["ev"] == "P" and e["cls"] == 2]
    print(f"\n## Touches KC_NO pressées : {len(nos)}\n")
    for e in nos:
        print(f"- t={e['t']} ms, couche {e['layer']}, main/rangée {hand(e['where'])}")

    mods = [e for e in ev if e["ev"] == "P" and e["cls"] == 1]
    if mods:
        st = [e["term"] for e in mods]
        print(f"\n## PRE_MOD_DELAY : {len(mods)} touches, blocage moyen {statistics.mean(st):.1f} ms, max {max(st)} ms, total {sum(st)} ms\n")

    print("\n## Couches\n")
    for e in (x for x in ev if x["ev"] == "L"):
        active = [i for i in range(16) if e["extra"] >> i & 1]
        print(f"- t={e['t']} ms : couches actives {active or [0]}")

    tds = [e for e in ev if e["ev"] == "P" and e["cls"] == 5]
    if tds:
        print("\n## Tap Dance (instant de l'action estimé)\n")
        print("Vial résout au relâchement une entrée qui n'a que tap + hold ; sinon il attend la fin du délai")
        print("ou l'appui d'une autre touche (quantum/vial.c, process_record_vial).\n")
        for e in tds:
            nxt = next((x for x in downs if x["t"] > e["t"] and x["pos"] != e["pos"]), None)
            rel = next((x for x in raw if x["ev"] == "u" and x["pos"] == e["pos"] and x["t"] >= e["t"]), None)
            out = min(e["t"] + e["term"], nxt["t"]) if nxt else e["t"] + e["term"]
            kc, how = kc_at(e["layer"], e["pos"]), "fin du délai ou touche suivante"
            m = re.match(r"TD\((\d+)\)", kc or "")
            if m and vil and int(m.group(1)) < len(vil.get("tap_dance", [])):
                tap, hold, dtap, taphold, _ = vil["tap_dance"][int(m.group(1))]
                if tap != "KC_NO" and hold != "KC_NO" and dtap == "KC_NO" and taphold == "KC_NO" and rel:
                    out, how = min(out, rel["t"]), "relâchement (tap + hold seulement)"
            print(f"- t={e['t']} ms {name(e['pos'])} [{kc or '?'}, couche {e['layer']}] : délai {e['term']} ms, action vers t≈{out} ms (+{out - e['t']} ms, {how})")

    if a.bench:
        b = json.load(open(a.bench, encoding="utf-8"))
        first_fw = downs[0]["t"] if downs else 0
        first_page = b.get("first_input_ms", 0)
        print("\n## Écarts du banc de frappe et décisions voisines\n")
        print(f"(Alignement : première touche du clavier t={first_fw} ms ↔ première saisie de la page t={first_page:.0f} ms)\n")
        for err in b.get("errors", []) + [dict(kind="marque Échap", t=m, expected="", got="") for m in b.get("marks", [])]:
            tfw = err["t"] - first_page + first_fw
            near = [r for r in rows if abs(r["t"] - tfw) <= a.window]
            desc = "; ".join(f"{r['key']} {r['dec']} ({r['rule']}, {r['dur']} ms)" for r in near) or "aucune touche double à proximité"
            print(f"- {err['kind']} : attendu « {err.get('expected', '')} », obtenu « {err.get('got', '')} » (t≈{tfw:.0f}) → {desc}")


if __name__ == "__main__":
    main()
