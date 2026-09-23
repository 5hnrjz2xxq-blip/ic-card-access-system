#!/usr/bin/env python3
"""Remove shorting copper from KiCad PCB based on DRC JSON, then report leftovers."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path


def extract_blocks(text: str, kind: str) -> list[tuple[int, int, str]]:
    """Return (start, end, block) for top-level (kind ...) s-expr blocks."""
    out = []
    token = f"\t({kind}"
    start = 0
    while True:
        i = text.find(token, start)
        if i < 0:
            break
        # parse balanced parens from i
        depth = 0
        j = i
        while j < len(text):
            ch = text[j]
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    j += 1
                    break
            j += 1
        out.append((i, j, text[i:j]))
        start = j
    return out


def main(pcb_path: str, drc_path: str) -> None:
    pcb_p = Path(pcb_path)
    drc = json.loads(Path(drc_path).read_text())
    text = pcb_p.read_text(encoding="utf-8")

    kill: set[str] = set()
    short_pairs = []
    for v in drc.get("violations", []):
        if v.get("type") != "shorting_items":
            continue
        items = v.get("items") or []
        # delete track/via items, never pads/footprints
        for it in items:
            desc = it.get("description") or ""
            uuid = it.get("uuid")
            if not uuid:
                continue
            if desc.startswith("走线") or desc.startswith("Via") or "过孔" in desc or desc.startswith("Track") or desc.startswith("Via"):
                kill.add(uuid)
            elif "焊盘" in desc or "Pad" in desc or "PTH" in desc or "SMD" in desc:
                pass
            else:
                # ambiguous: prefer deleting track-like
                if "走线" in desc or "过孔" in desc:
                    kill.add(uuid)
        short_pairs.append([it.get("description") for it in items])

    print(f"shorts={len(short_pairs)} kill_copper_uuids={len(kill)}")

    removed = 0
    for kind in ("segment", "via", "arc"):
        blocks = extract_blocks(text, kind)
        # delete from end to start
        for s, e, block in reversed(blocks):
            m = re.search(r'\(uuid "([^"]+)"\)', block)
            if m and m.group(1) in kill:
                # also remove preceding newline
                ls = s
                while ls > 0 and text[ls - 1] in "\t":
                    ls -= 1
                if ls > 0 and text[ls - 1] == "\n":
                    ls -= 1
                text = text[:ls] + text[e:]
                removed += 1
    print(f"removed copper objects: {removed}")
    pcb_p.write_text(text, encoding="utf-8")

    # summarize leftover unconnected
    print(f"unconnected={len(drc.get('unconnected_items') or [])}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
