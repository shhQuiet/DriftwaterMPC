#!/usr/bin/env python3
"""Lay out the Main and Info tabs for the MPC touchscreen."""

import json
from pathlib import Path
import sys


SKIN = Path(sys.argv[1])
positions = {
    "Rate": (130, 108),
    "Damp": (557, 108),
    "Depth": (984, 108),
    "LPG": (132, 413),
    "Mix": (986, 413),
}

path = SKIN / "Plugin Skins" / "TUI.json"
skin = json.loads(path.read_text())
tab = next(tab for tab in skin["pageData"]["tabs"] if tab["tabName"] == "MAIN")
controls = tab["componentDefinition"]["componentsData"]
assert {entry["componentData"]["name"] for entry in controls} == positions.keys()
rows = {}
for entry in controls:
    x, y = positions[entry["componentData"]["name"]]
    bounds = entry["bounds"]
    _, _, width, height = map(int, bounds["bounds"].split())
    bounds["bounds"] = f"{x} {y} {width} {height}"
    rows.setdefault(y, []).append((x, width, height))

rectangles = []
for y, row in sorted(rows.items()):
    left = min(x for x, _, _ in row)
    right = max(x + width for x, width, _ in row)
    height = max(height for _, _, height in row)
    rectangles.append(f"{left - 10} {y - 10} {right - left + 20} {height + 20}")
tab["qlinkBoundsData"] = rectangles
info_tab = next(tab for tab in skin["pageData"]["tabs"] if tab["tabName"] == "INFO")
info = info_tab["componentDefinition"]
image = info["backgroundData"]["focussed"]["image"]
for state in info["backgroundData"].values():
    if isinstance(state, dict) and "image" in state:
        state["image"] = ""
info["componentsData"] = [{
    "version": 2,
    "componentData": {
        "version": 1,
        "name": "Info artwork",
        "type": "Image",
        "data": {"version": 2, "imageType": "Regular", "colour": "0", "image": image},
    },
    "handle remapping": {"version": 1, "map": []},
    "bounds": {
        "version": 1,
        "acceptsHWFocus": "No",
        "showWhenDataModelInvalid": "Show",
        "whenVisible": "Always",
        "boundsType": "Absolute",
        "bounds": "0 0 1280 628",
    },
}]
path.write_text(json.dumps(skin, indent=4) + "\n")
