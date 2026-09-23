#!/usr/bin/env python3
"""Bridge unconnected same-net copper pairs from KiCad DRC JSON."""
from __future__ import annotations

import json
import re
import sys
import uuid
from pathlib import Path


def main(pcb_path: str, drc_path: str) -> None:
    pcb = Path(pcb_path)
    drc = json.loads(Path(drc_path).read_text())
    text = pcb.read_text(encoding="utf-8")

    # map net name -> id from (net N "name")
    net_ids = {}
    for m in re.finditer(r'\(net\s+(\d+)\s+"([^"]*)"\)', text):
        net_ids[m.group(2)] = int(m.group(1))

    additions = []
    vias = 0
    segs = 0
    for item in drc.get("unconnected_items") or []:
        pts = []
        netname = None
        for it in item.get("items") or []:
            pos = it.get("pos") or {}
            if "x" not in pos:
                continue
            desc = it.get("description") or ""
            m = re.search(r"\[([^\]]+)\]", desc)
            if m:
                netname = m.group(1)
            layer = "F.Cu"
            if "B.Cu" in desc:
                layer = "B.Cu"
            elif "F.Cu" in desc or "PTH" in desc or "焊盘" in desc:
                layer = "F.Cu"
            pts.append((float(pos["x"]), float(pos["y"]), layer, desc))
        if not netname or len(pts) < 2:
            continue
        nid = net_ids.get(netname, 0)
        a, b = pts[0], pts[1]
        # different layers -> via at each and segment on F
        if a[2] != b[2] and ("走线" in a[3] or "走线" in b[3] or "过孔" in a[3] or "过孔" in b[3]):
            additions.append(
                f'\t(via\n\t\t(at {a[0]:.4f} {a[1]:.4f})\n\t\t(size 0.6)\n\t\t(drill 0.3)\n\t\t(layers "F.Cu" "B.Cu")\n\t\t(net {nid})\n\t\t(uuid "{uuid.uuid4()}")\n\t)'
            )
            vias += 1
            additions.append(
                f'\t(segment\n\t\t(start {a[0]:.4f} {a[1]:.4f})\n\t\t(end {b[0]:.4f} {b[1]:.4f})\n\t\t(width 0.25)\n\t\t(layer "F.Cu")\n\t\t(net {nid})\n\t\t(uuid "{uuid.uuid4()}")\n\t)'
            )
            segs += 1
        else:
            layer = a[2] if a[2] == b[2] else "F.Cu"
            width = 0.5 if netname in ("GND", "+3V3", "VBUS", "VBAT", "BAT_GND") else 0.2
            additions.append(
                f'\t(segment\n\t\t(start {a[0]:.4f} {a[1]:.4f})\n\t\t(end {b[0]:.4f} {b[1]:.4f})\n\t\t(width {width})\n\t\t(layer "{layer}")\n\t\t(net {nid})\n\t\t(uuid "{uuid.uuid4()}")\n\t)'
            )
            segs += 1

    if additions:
        # insert before final closing paren
        idx = text.rstrip().rfind(")")
        text = text[:idx] + "\n".join(additions) + "\n" + text[idx:]
    pcb.write_text(text, encoding="utf-8")
    print(f"added_segments={segs} added_vias={vias}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
