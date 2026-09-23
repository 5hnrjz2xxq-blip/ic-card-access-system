#!/usr/bin/env python3
"""Snap same-net track endpoints, drop near-dup stubs, zero pad mask margin."""
from __future__ import annotations

import math
import re
import sys
from collections import defaultdict
from pathlib import Path


def extract_blocks(text: str, kind: str):
    out = []
    token = f"\t({kind}"
    start = 0
    while True:
        i = text.find(token, start)
        if i < 0:
            break
        depth = 0
        j = i
        while j < len(text):
            if text[j] == "(":
                depth += 1
            elif text[j] == ")":
                depth -= 1
                if depth == 0:
                    j += 1
                    break
            j += 1
        out.append([i, j, text[i:j]])
        start = j
    return out


def parse_seg(block: str):
    m = re.search(
        r"\(start\s+([-\d.]+)\s+([-\d.]+)\)\s*\(end\s+([-\d.]+)\s+([-\d.]+)\)\s*\(width\s+([-\d.]+)\)\s*\(layer\s+\"([^\"]+)\"\)(?:\s*\(net\s+(\d+)\))?",
        block,
        re.S,
    )
    if not m:
        return None
    return {
        "x1": float(m.group(1)),
        "y1": float(m.group(2)),
        "x2": float(m.group(3)),
        "y2": float(m.group(4)),
        "w": float(m.group(5)),
        "layer": m.group(6),
        "net": int(m.group(7) or 0),
    }


def main(pcb_path: str) -> None:
    p = Path(pcb_path)
    text = p.read_text(encoding="utf-8")

    # zero pad solder mask margins
    text = text.replace("(solder_mask_margin 0.051)", "")
    text = re.sub(r"\(solder_mask_margin [^\)]+\)\s*", "", text)

    segs = extract_blocks(text, "segment")
    parsed = []
    for s, e, b in segs:
        info = parse_seg(b)
        if info:
            parsed.append((s, e, b, info))

    # collect endpoints per net+layer
    ends = defaultdict(list)
    for idx, (s, e, b, info) in enumerate(parsed):
        if info["net"] == 0:
            continue
        key = (info["net"], info["layer"])
        ends[key].append((idx, "a", info["x1"], info["y1"]))
        ends[key].append((idx, "b", info["x2"], info["y2"]))

    # snap close endpoints to centroid of cluster
    TOL = 0.35  # mm
    replacements = defaultdict(dict)  # seg idx -> {a:(x,y), b:(x,y)}
    for key, pts in ends.items():
        used = [False] * len(pts)
        for i in range(len(pts)):
            if used[i]:
                continue
            cluster = [i]
            used[i] = True
            for j in range(i + 1, len(pts)):
                if used[j]:
                    continue
                if abs(pts[i][2] - pts[j][2]) <= TOL and abs(pts[i][3] - pts[j][3]) <= TOL:
                    cluster.append(j)
                    used[j] = True
            if len(cluster) < 2:
                continue
            mx = sum(pts[k][2] for k in cluster) / len(cluster)
            my = sum(pts[k][3] for k in cluster) / len(cluster)
            for k in cluster:
                si, which, _, _ = pts[k]
                replacements[si][which] = (mx, my)

    changed = 0
    # apply from end using ORIGINAL offsets
    for idx in sorted(replacements.keys(), reverse=True):
        s, e, b, info = parsed[idx]
        nb = b
        reps = replacements[idx]
        if "a" in reps:
            nb = re.sub(
                r"\(start\s+[-\d.]+\s+[-\d.]+\)",
                f"(start {reps['a'][0]:.4f} {reps['a'][1]:.4f})",
                nb,
                count=1,
            )
        if "b" in reps:
            nb = re.sub(
                r"\(end\s+[-\d.]+\s+[-\d.]+\)",
                f"(end {reps['b'][0]:.4f} {reps['b'][1]:.4f})",
                nb,
                count=1,
            )
        if nb != b:
            text = text[:s] + nb + text[e:]
            changed += 1

    # drop zero-length / tiny dangling stubs (<0.05mm) of any net
    text2 = text
    segs = extract_blocks(text2, "segment")
    drop = 0
    for s, e, b in reversed(segs):
        info = parse_seg(b)
        if not info:
            continue
        length = math.hypot(info["x2"] - info["x1"], info["y2"] - info["y1"])
        if length < 0.05:
            ls = s
            while ls > 0 and text2[ls - 1] in "\t":
                ls -= 1
            if ls > 0 and text2[ls - 1] == "\n":
                ls -= 1
            text2 = text2[:ls] + text2[e:]
            drop += 1

    p.write_text(text2, encoding="utf-8")
    print(f"snapped_segments={changed} dropped_tiny={drop}")


if __name__ == "__main__":
    main(sys.argv[1])
