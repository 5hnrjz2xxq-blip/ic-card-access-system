#!/usr/bin/env python3
"""Convert EasyEDA/Protel Advanced PCB ASCII (.pcbdoc) to KiCad .kicad_pcb."""
from __future__ import annotations

import math
import re
import sys
from pathlib import Path

MIL = 0.0254  # mm per mil


def parse_val(s: str) -> float:
    s = s.strip()
    if not s:
        return 0.0
    if s.endswith("mil"):
        return float(s[:-3]) * MIL
    if s.endswith("mm"):
        return float(s[:-2])
    try:
        return float(s) * MIL  # unitless treated as mil
    except ValueError:
        return 0.0


def parse_fields(line: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for part in line.split("|"):
        if "=" in part:
            k, v = part.split("=", 1)
            out[k] = v
    return out


def layer_map(name: str) -> str:
    n = (name or "").upper().replace(" ", "")
    table = {
        "TOP": "F.Cu",
        "BOTTOM": "B.Cu",
        "TOPOVERLAY": "F.SilkS",
        "BOTTOMOVERLAY": "B.SilkS",
        "TOPSILKSCREENLAYER": "F.SilkS",
        "BOTTOMSILKSCREENLAYER": "B.SilkS",
        "TOPPASTE": "F.Paste",
        "BOTTOMPASTE": "B.Paste",
        "TOPPASTEMASKLAYER": "F.Paste",
        "BOTTOMPASTEMASKLAYER": "B.Paste",
        "TOPSOLDER": "F.Mask",
        "BOTTOMSOLDER": "B.Mask",
        "TOPSOLDERMASKLAYER": "F.Mask",
        "BOTTOMSOLDERMASKLAYER": "B.Mask",
        "MULTILAYER": "F.Cu",
        "MECHANICAL6": "F.Fab",
        "MECHANICAL7": "F.CrtYd",
        "MECHANICAL8": "F.Fab",
        "BOARDOUTLINELAYER": "Edge.Cuts",
        "BOARDOULETLINE": "Edge.Cuts",
    }
    if n in table:
        return table[n]
    if n.startswith("MECHANICAL"):
        return "Dwgs.User"
    return "Dwgs.User"


def shape_map(shape: str) -> str:
    s = (shape or "").upper()
    if s in ("RECTANGLE", "RECT", "SQUARE"):
        return "rect"
    if s in ("OCTAGON",):
        return "roundrect"
    return "circle"


def main(src: str, dst: str) -> None:
    text = Path(src).read_text(errors="replace")
    nets: dict[int, str] = {}
    comps: list[dict] = []
    pads: list[dict] = []
    tracks: list[dict] = []
    vias: list[dict] = []
    arcs: list[dict] = []
    texts: list[dict] = []
    polys: list[dict] = []
    outline_pts: list[tuple[float, float]] = []

    for line in text.splitlines():
        if not line.startswith("|RECORD="):
            continue
        f = parse_fields(line)
        rec = f.get("RECORD")
        if rec == "Net":
            nets[int(f.get("ID", "0"))] = f.get("NAME", "")
        elif rec == "Component":
            comps.append(
                {
                    "id": int(f.get("ID", "0")),
                    "x": parse_val(f.get("X", "0")),
                    "y": parse_val(f.get("Y", "0")),
                    "rot": float(f.get("ROTATION", "0") or 0),
                    "des": f.get("SOURCEDESIGNATOR", f"P{f.get('ID','0')}"),
                    "pattern": f.get("PATTERN", "FP"),
                    "layer": f.get("LAYER", "TOP"),
                }
            )
        elif rec == "Pad":
            pads.append(
                {
                    "comp": int(f.get("COMPONENT", "0")),
                    "name": f.get("NAME", "1"),
                    "x": parse_val(f.get("X", "0")),
                    "y": parse_val(f.get("Y", "0")),
                    "sx": parse_val(f.get("XSIZE", "0")),
                    "sy": parse_val(f.get("YSIZE", "0")),
                    "hole": parse_val(f.get("HOLESIZE", "0")),
                    "shape": shape_map(f.get("SHAPE", "")),
                    "rot": float(f.get("ROTATION", "0") or 0),
                    "layer": f.get("LAYER", "TOP"),
                    "net": f.get("NET"),
                    "plated": f.get("PLATED", "TRUE") == "TRUE",
                }
            )
        elif rec == "Track":
            tracks.append(
                {
                    "comp": f.get("COMPONENT"),
                    "layer": f.get("LAYER", "TOP"),
                    "x1": parse_val(f.get("X1", "0")),
                    "y1": parse_val(f.get("Y1", "0")),
                    "x2": parse_val(f.get("X2", "0")),
                    "y2": parse_val(f.get("Y2", "0")),
                    "w": parse_val(f.get("WIDTH", "0.2mm")),
                    "net": f.get("NET"),
                }
            )
        elif rec == "Via":
            vias.append(
                {
                    "x": parse_val(f.get("X", "0")),
                    "y": parse_val(f.get("Y", "0")),
                    "size": parse_val(f.get("DIAMETER", f.get("TOPLAYERSIZE", "24mil"))),
                    "drill": parse_val(f.get("HOLESIZE", "12mil")),
                    "net": f.get("NET"),
                }
            )
        elif rec == "Arc":
            arcs.append(
                {
                    "comp": f.get("COMPONENT"),
                    "layer": f.get("LAYER", "TOP"),
                    "cx": parse_val(f.get("LOCATION.X", "0")),
                    "cy": parse_val(f.get("LOCATION.Y", "0")),
                    "r": parse_val(f.get("RADIUS", "0")),
                    "w": parse_val(f.get("WIDTH", "0.15mm")),
                    "sa": float(f.get("STARTANGLE", "0") or 0),
                    "ea": float(f.get("ENDANGLE", "360") or 360),
                }
            )
        elif rec == "Text":
            texts.append(
                {
                    "comp": f.get("COMPONENT"),
                    "layer": f.get("LAYER", "TOPOVERLAY"),
                    "text": f.get("TEXT", ""),
                    "x": parse_val(f.get("X", "0")),
                    "y": parse_val(f.get("Y", "0")),
                    "h": parse_val(f.get("HEIGHT", "40mil")),
                    "rot": float(f.get("ROTATION", "0") or 0),
                }
            )
        elif rec == "Polygon":
            pts = []
            i = 0
            while f"VX{i}" in f:
                pts.append((parse_val(f[f"VX{i}"]), parse_val(f[f"VY{i}"])))
                i += 1
            polys.append(
                {
                    "net": f.get("NET"),
                    "layer": f.get("LAYER", "TOP"),
                    "pts": pts,
                }
            )
        elif rec == "Board" and not outline_pts:
            i = 0
            while f"VX{i}" in f:
                outline_pts.append((parse_val(f[f"VX{i}"]), parse_val(f[f"VY{i}"])))
                i += 1

    # track net names from pads when missing: use track NET if present
    def net_name(nid: str | None) -> str:
        if nid is None or nid == "":
            return ""
        try:
            return nets.get(int(nid), "")
        except ValueError:
            return nid

    # Build footprint groups
    comp_by_id = {c["id"]: c for c in comps}
    pads_by_comp: dict[int, list[dict]] = {}
    for p in pads:
        pads_by_comp.setdefault(p["comp"], []).append(p)

    def fmt(n: float) -> str:
        return f"{n:.4f}"

    out: list[str] = []
    out.append("(kicad_pcb")
    out.append("\t(version 20260206)")
    out.append('\t(generator "protel2kicad")')
    out.append('\t(generator_version "10.0")')
    out.append("\t(general")
    out.append("\t\t(thickness 1.6)")
    out.append("\t\t(legacy_teardrops no)")
    out.append("\t)")
    out.append('\t(paper "A3")')
    out.append("\t(layers")
    out.append('\t\t(0 "F.Cu" signal)')
    out.append('\t\t(2 "B.Cu" signal)')
    out.append('\t\t(9 "F.Adhes" user "F.Adhesive")')
    out.append('\t\t(11 "B.Adhes" user "B.Adhesive")')
    out.append('\t\t(13 "F.Paste" user)')
    out.append('\t\t(15 "B.Paste" user)')
    out.append('\t\t(5 "F.SilkS" user "F.Silkscreen")')
    out.append('\t\t(7 "B.SilkS" user "B.Silkscreen")')
    out.append('\t\t(1 "F.Mask" user)')
    out.append('\t\t(3 "B.Mask" user)')
    out.append('\t\t(17 "Dwgs.User" user "User.Drawings")')
    out.append('\t\t(19 "Cmts.User" user "User.Comments")')
    out.append('\t\t(21 "Eco1.User" user "User.Eco1")')
    out.append('\t\t(23 "Eco2.User" user "User.Eco2")')
    out.append('\t\t(25 "Edge.Cuts" user)')
    out.append('\t\t(27 "Margin" user)')
    out.append('\t\t(31 "F.CrtYd" user "F.Courtyard")')
    out.append('\t\t(29 "B.CrtYd" user "B.Courtyard")')
    out.append('\t\t(35 "F.Fab" user)')
    out.append('\t\t(33 "B.Fab" user)')
    out.append("\t)")
    out.append("\t(setup")
    out.append("\t\t(stackup")
    out.append("\t\t\t(layer \"F.SilkS\"")
    out.append("\t\t\t\t(type \"Top Silk Screen\")")
    out.append("\t\t\t)")
    out.append("\t\t\t(layer \"F.Paste\"")
    out.append("\t\t\t\t(type \"Top Solder Paste\")")
    out.append("\t\t\t)")
    out.append("\t\t\t(layer \"F.Mask\"")
    out.append("\t\t\t\t(type \"Top Solder Mask\")")
    out.append("\t\t\t\t(thickness 0.01)")
    out.append("\t\t\t)")
    out.append("\t\t\t(layer \"F.Cu\"")
    out.append("\t\t\t\t(type \"copper\")")
    out.append("\t\t\t\t(thickness 0.035)")
    out.append("\t\t\t)")
    out.append("\t\t\t(layer \"dielectric 1\"")
    out.append("\t\t\t\t(type \"core\")")
    out.append("\t\t\t\t(thickness 1.51)")
    out.append("\t\t\t\t(material \"FR4\")")
    out.append("\t\t\t\t(epsilon_r 4.5)")
    out.append("\t\t\t\t(loss_tangent 0.02)")
    out.append("\t\t\t)")
    out.append("\t\t\t(layer \"B.Cu\"")
    out.append("\t\t\t\t(type \"copper\")")
    out.append("\t\t\t\t(thickness 0.035)")
    out.append("\t\t\t)")
    out.append("\t\t\t(layer \"B.Mask\"")
    out.append("\t\t\t\t(type \"Bottom Solder Mask\")")
    out.append("\t\t\t\t(thickness 0.01)")
    out.append("\t\t\t)")
    out.append("\t\t\t(layer \"B.Paste\"")
    out.append("\t\t\t\t(type \"Bottom Solder Paste\")")
    out.append("\t\t\t)")
    out.append("\t\t\t(layer \"B.SilkS\"")
    out.append("\t\t\t\t(type \"Bottom Silk Screen\")")
    out.append("\t\t\t)")
    out.append("\t\t\t(copper_finish \"None\")")
    out.append("\t\t\t(dielectric_constraints no)")
    out.append("\t\t)")
    out.append("\t\t(pad_to_mask_clearance 0.051)")
    out.append("\t\t(allow_soldermask_bridges_in_footprints no)")
    out.append("\t\t(pcbplotparams")
    out.append("\t\t\t(layerselection 0x00010fc_ffffffff)")
    out.append("\t\t\t(plot_on_all_layers_selection 0x0000000_00000000)")
    out.append("\t\t\t(disableapertmacros no)")
    out.append("\t\t\t(usegerberextensions no)")
    out.append("\t\t\t(usegerberattributes yes)")
    out.append("\t\t\t(usegerberadvancedattributes yes)")
    out.append("\t\t\t(creategerberjobfile yes)")
    out.append("\t\t\t(dashed_line_dash_ratio 12.000000)")
    out.append("\t\t\t(dashed_line_gap_ratio 3.000000)")
    out.append("\t\t\t(svgprecision 4)")
    out.append("\t\t\t(plotframeref no)")
    out.append("\t\t\t(mode 1)")
    out.append("\t\t\t(useauxorigin no)")
    out.append("\t\t\t(hpglpennumber 1)")
    out.append("\t\t\t(hpglpenspeed 20)")
    out.append("\t\t\t(hpglpendiameter 15.000000)")
    out.append("\t\t\t(pdf_front_fp_property_popups yes)")
    out.append("\t\t\t(pdf_back_fp_property_popups yes)")
    out.append("\t\t\t(dxfpolygonmode yes)")
    out.append("\t\t\t(dxfimperialunits yes)")
    out.append("\t\t\t(dxfusepcbnewfont yes)")
    out.append("\t\t\t(psnegative no)")
    out.append("\t\t\t(psa4output no)")
    out.append("\t\t\t(plot_black_and_white yes)")
    out.append("\t\t\t(sketchpadsonfab no)")
    out.append("\t\t\t(plotpadnumbers no)")
    out.append("\t\t\t(hidednponfab no)")
    out.append("\t\t\t(sketchdnponfab yes)")
    out.append("\t\t\t(crossoutdnponfab yes)")
    out.append("\t\t\t(subtractmaskfromsilk no)")
    out.append("\t\t\t(outputformat 1)")
    out.append("\t\t\t(mirror no)")
    out.append("\t\t\t(drillshape 1)")
    out.append("\t\t\t(scaleselection 1)")
    out.append("\t\t\t(outputdirectory \"gerbers/\")")
    out.append("\t\t)")
    out.append("\t)")

    # nets
    out.append('\t(net 0 "")')
    used_nets = sorted({int(v) for v in [*(p.get("net") for p in pads if p.get("net") not in (None, "")),
                                        *(t.get("net") for t in tracks if t.get("net") not in (None, "")),
                                        *(v.get("net") for v in vias if v.get("net") not in (None, ""))]
                        if str(v).isdigit() or (str(v).lstrip("-").isdigit())})
    # safer collect
    net_ids = set()
    for src_list, key in ((pads, "net"), (tracks, "net"), (vias, "net"), (polys, "net")):
        for item in src_list:
            val = item.get(key)
            if val is None or val == "":
                continue
            try:
                net_ids.add(int(val))
            except ValueError:
                pass
    for nid in sorted(net_ids):
        out.append(f'\t(net {nid + 1} "{net_name(str(nid))}")')

    def nid(v) -> int:
        if v is None or v == "":
            return 0
        try:
            return int(v) + 1
        except ValueError:
            return 0

    # footprints
    for c in comps:
        cx, cy = c["x"], c["y"]
        out.append(f'\t(footprint "FP:{c["pattern"]}"')
        out.append('\t\t(layer "F.Cu")')
        out.append(f'\t\t(uuid "{c["des"]}-{c["id"]}")')
        out.append(f'\t\t(at {fmt(cx)} {fmt(cy)} {fmt(c["rot"])})')
        out.append('\t\t(property "Reference" "%s"' % c["des"])
        out.append(f'\t\t\t(at 0 -2 0)')
        out.append('\t\t\t(layer "F.SilkS")')
        out.append('\t\t\t(uuid "ref-%s")' % c["des"])
        out.append('\t\t\t(effects')
        out.append('\t\t\t\t(font')
        out.append('\t\t\t\t\t(size 0.8 0.8)')
        out.append('\t\t\t\t\t(thickness 0.12)')
        out.append("\t\t\t\t)")
        out.append("\t\t\t)")
        out.append("\t\t)")
        out.append('\t\t(property "Value" "%s"' % c["pattern"])
        out.append(f'\t\t\t(at 0 2 0)')
        out.append('\t\t\t(layer "F.Fab")')
        out.append('\t\t\t(uuid "val-%s")' % c["des"])
        out.append('\t\t\t(effects')
        out.append('\t\t\t\t(font')
        out.append('\t\t\t\t\t(size 0.8 0.8)')
        out.append('\t\t\t\t\t(thickness 0.12)')
        out.append("\t\t\t\t)")
        out.append("\t\t\t)")
        out.append("\t\t)")
        out.append('\t\t(attr smd)')
        out.append('\t\t(fp_text reference "%s"' % c["des"])
        out.append(f'\t\t\t(at 0 -2 0)')
        out.append('\t\t\t(layer "F.SilkS")')
        out.append('\t\t\t(uuid "fpref-%s")' % c["des"])
        out.append('\t\t\t(effects')
        out.append('\t\t\t\t(font')
        out.append('\t\t\t\t\t(size 0.8 0.8)')
        out.append('\t\t\t\t\t(thickness 0.12)')
        out.append("\t\t\t\t)")
        out.append("\t\t\t)")
        out.append("\t\t)")
        # local pads (skip board mounting holes that were mis-attached)
        rot = math.radians(c["rot"])
        for p in pads_by_comp.get(c["id"], []):
            if str(p["name"]).upper() in ("H1", "H2", "H3", "H4", "MH1", "MH2", "MH3", "MH4"):
                continue
            dx = p["x"] - cx
            dy = p["y"] - cy
            # undo component rotation for local coords
            lx = dx * math.cos(-rot) - dy * math.sin(-rot)
            ly = dx * math.sin(-rot) + dy * math.cos(-rot)
            hole = p["hole"]
            if hole > 0.001:
                padtype = "thru_hole"
                layers = '"*.Cu" "*.Mask"'
            else:
                padtype = "smd"
                if p["layer"].upper().startswith("BOTTOM"):
                    layers = '"B.Cu" "B.Paste" "B.Mask"'
                else:
                    layers = '"F.Cu" "F.Paste" "F.Mask"'
            sz_x = p["sx"] or 0.5
            sz_y = p["sy"] or 0.5
            netv = nid(p.get("net"))
            netn = net_name(p.get("net"))
            out.append(f'\t\t(pad "{p["name"]}" {padtype} {p["shape"]}')
            out.append(f'\t\t\t(at {fmt(lx)} {fmt(ly)} {fmt(p["rot"] - c["rot"])})')
            out.append(f'\t\t\t(size {fmt(sz_x)} {fmt(sz_y)})')
            if hole > 0.001:
                out.append(f'\t\t\t(drill {fmt(hole)})')
            out.append(f"\t\t\t(layers {layers})")
            if netv:
                out.append(f'\t\t\t(net {netv} "{netn}")')
            out.append('\t\t\t(uuid "pad-%s-%s")' % (c["des"], p["name"]))
            out.append("\t\t)")
        out.append("\t)")

    # tracks on copper
    for i, t in enumerate(tracks):
        ly = layer_map(t["layer"])
        if ly not in ("F.Cu", "B.Cu"):
            continue
        netv = nid(t.get("net"))
        netn = net_name(t.get("net"))
        out.append("\t(segment")
        out.append(f'\t\t(start {fmt(t["x1"])} {fmt(t["y1"])})')
        out.append(f'\t\t(end {fmt(t["x2"])} {fmt(t["y2"])})')
        out.append(f'\t\t(width {fmt(t["w"] or 0.2)})')
        out.append(f'\t\t(layer "{ly}")')
        if netv:
            out.append(f'\t\t(net {netv})')
        out.append(f'\t\t(uuid "seg-{i}")')
        out.append("\t)")

    for i, v in enumerate(vias):
        netv = nid(v.get("net"))
        out.append("\t(via")
        out.append(f'\t\t(at {fmt(v["x"])} {fmt(v["y"])})')
        out.append(f'\t\t(size {fmt(v["size"] or 0.6)})')
        out.append(f'\t\t(drill {fmt(v["drill"] or 0.3)})')
        out.append('\t\t(layers "F.Cu" "B.Cu")')
        if netv:
            out.append(f'\t\t(net {netv})')
        out.append(f'\t\t(uuid "via-{i}")')
        out.append("\t)")

    # board mounting holes (M3 NPTH)
    mh_positions = [
        (5.0, 5.0),
        (128.5, 5.0),
        (5.0, 167.0),
        (128.5, 167.0),
    ]
    # place relative to outline bbox
    if outline_pts:
        xs = [p[0] for p in outline_pts]
        ys = [p[1] for p in outline_pts]
        minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
        inset = 4.0  # mm from edge
        mh_positions = [
            (minx + inset, miny + inset),
            (maxx - inset, miny + inset),
            (minx + inset, maxy - inset),
            (maxx - inset, maxy - inset),
        ]
    for i, (hx, hy) in enumerate(mh_positions, 1):
        out.append('\t(footprint "MountingHole:MountingHole_3.2mm_M3"')
        out.append('\t\t(layer "F.Cu")')
        out.append(f'\t\t(uuid "MH{i}")')
        out.append(f'\t\t(at {fmt(hx)} {fmt(hy)})')
        out.append('\t\t(property "Reference" "MH%d"' % i)
        out.append('\t\t\t(at 0 -3 0)')
        out.append('\t\t\t(layer "F.SilkS")')
        out.append('\t\t\t(uuid "mhref-%d")' % i)
        out.append("\t\t\t(effects")
        out.append("\t\t\t\t(font")
        out.append("\t\t\t\t\t(size 0.8 0.8)")
        out.append("\t\t\t\t\t(thickness 0.12)")
        out.append("\t\t\t\t)")
        out.append("\t\t\t)")
        out.append("\t\t)")
        out.append('\t\t(property "Value" "MountingHole_3.2mm_M3"')
        out.append('\t\t\t(at 0 3 0)')
        out.append('\t\t\t(layer "F.Fab")')
        out.append('\t\t\t(uuid "mhval-%d")' % i)
        out.append("\t\t\t(effects")
        out.append("\t\t\t\t(font")
        out.append("\t\t\t\t\t(size 0.8 0.8)")
        out.append("\t\t\t\t\t(thickness 0.12)")
        out.append("\t\t\t\t)")
        out.append("\t\t\t)")
        out.append("\t\t)")
        out.append('\t\t(attr exclude_from_pos_files exclude_from_bom)')
        out.append('\t\t(fp_circle')
        out.append('\t\t\t(center 0 0)')
        out.append('\t\t\t(end 2.2 0)')
        out.append("\t\t\t(stroke")
        out.append("\t\t\t\t(width 0.12)")
        out.append("\t\t\t\t(type solid)")
        out.append("\t\t\t)")
        out.append('\t\t\t(fill no)')
        out.append('\t\t\t(layer "F.SilkS")')
        out.append('\t\t\t(uuid "mhc-%d")' % i)
        out.append("\t\t)")
        out.append('\t\t(pad "" np_thru_hole circle')
        out.append('\t\t\t(at 0 0)')
        out.append('\t\t\t(size 3.2 3.2)')
        out.append('\t\t\t(drill 3.2)')
        out.append('\t\t\t(layers "F&B.Cu" "*.Mask")')
        out.append('\t\t\t(uuid "mhp-%d")' % i)
        out.append("\t\t)")
        out.append("\t)")

    # edge cuts from board outline
    pts = outline_pts
    if len(pts) >= 2:
        for i in range(len(pts)):
            a = pts[i]
            b = pts[(i + 1) % len(pts)]
            if a == b:
                continue
            out.append("\t(gr_line")
            out.append(f'\t\t(start {fmt(a[0])} {fmt(a[1])})')
            out.append(f'\t\t(end {fmt(b[0])} {fmt(b[1])})')
            out.append('\t\t(stroke')
            out.append("\t\t\t(width 0.1)")
            out.append("\t\t\t(type default)")
            out.append("\t\t)")
            out.append('\t\t(layer "Edge.Cuts")')
            out.append(f'\t\t(uuid "edge-{i}")')
            out.append("\t)")

    # silk texts (non-component-rooted simple)
    for i, t in enumerate(texts):
        if not t["text"] or t["text"].strip() == "":
            continue
        ly = layer_map(t["layer"])
        if ly not in ("F.SilkS", "B.SilkS", "F.Fab", "Dwgs.User", "Cmts.User"):
            continue
        txt = t["text"].replace('"', "'")
        out.append("\t(gr_text")
        out.append(f'\t\t"{txt}"')
        out.append(f'\t\t(at {fmt(t["x"])} {fmt(t["y"])} {fmt(t["rot"])})')
        out.append(f'\t\t(layer "{ly}")')
        out.append(f'\t\t(uuid "txt-{i}")')
        out.append("\t\t(effects")
        out.append("\t\t\t(font")
        out.append(f'\t\t\t\t(size {fmt(max(t["h"] or 1.0, 0.5))} {fmt(max(t["h"] or 1.0, 0.5))})')
        out.append("\t\t\t\t(thickness 0.12)")
        out.append("\t\t\t)")
        out.append("\t\t)")
        out.append("\t)")

    # copper pours as zones
    for i, p in enumerate(polys):
        if len(p["pts"]) < 3:
            continue
        ly = layer_map(p["layer"])
        if ly not in ("F.Cu", "B.Cu"):
            continue
        netv = nid(p.get("net"))
        netn = net_name(p.get("net"))
        out.append("\t(zone")
        out.append(f'\t\t(net {netv})')
        out.append(f'\t\t(net_name "{netn}")')
        out.append(f'\t\t(layer "{ly}")')
        out.append(f'\t\t(uuid "zone-{i}")')
        out.append('\t\t(hatch edge 0.5)')
        out.append("\t\t(connect_pads")
        out.append("\t\t\t(clearance 0.2)")
        out.append("\t\t)")
        out.append("\t\t(min_thickness 0.2)")
        out.append("\t\t(filled_areas_thickness no)")
        out.append("\t\t(fill yes")
        out.append("\t\t\t(thermal_gap 0.3)")
        out.append("\t\t\t(thermal_bridge_width 0.3)")
        out.append("\t\t)")
        out.append("\t\t(polygon")
        out.append("\t\t\t(pts")
        for x, y in p["pts"]:
            out.append(f"\t\t\t\t(xy {fmt(x)} {fmt(y)})")
        out.append("\t\t\t)")
        out.append("\t\t)")
        out.append("\t)")

    out.append(")")
    Path(dst).write_text("\n".join(out) + "\n", encoding="utf-8")
    print(
        f"wrote {dst}: comps={len(comps)} pads={len(pads)} segs={sum(1 for t in tracks if layer_map(t['layer']) in ('F.Cu','B.Cu'))} "
        f"vias={len(vias)} zones={len(polys)} edges={len(pts)} nets={len(net_ids)}"
    )


if __name__ == "__main__":
    src = sys.argv[1]
    dst = sys.argv[2]
    main(src, dst)
