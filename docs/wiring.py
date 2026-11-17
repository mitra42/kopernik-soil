#!/usr/bin/env python3
"""Draws docs/wiring_d1_mini.png and docs/wiring_supermini.png (and .svg) - run from anywhere.
Uses the wiring-diagrams skill in ~/.claude/skills/wiring-diagrams."""
import os, sys
sys.path.insert(0, os.path.expanduser("~/.claude/skills/wiring-diagrams/scripts"))
from wiring import Diagram
HERE = os.path.dirname(os.path.abspath(__file__))
CHECK = "Check soil sensor connections, it varies from brand to brand"

# The probe points right in both, so its header reads GND, VCC, AOUT top to bottom.

# ---- D1 Mini: probe to the right. D1 sits above G on the board but VCC sits below GND on the
# probe, so those two cross once. A0 is on the far side and its pin is the bottom one, so it goes
# under the board.
d = Diagram(1240, 780, "kopernik-soil on a Wemos D1 mini",
            "Battery not shown - it is not read on this board yet, see the README")
b = d.board("wemos_d1_mini", x=150, y=140, width=360)
p = d.soil_probe(x=680, y=330, facing="left")
d.wire(b["A0"], p["AOUT"], "signal", via=["x=106", "y=665", "x=630"], label="A0 reads the probe", label_at=2)
d.wire(b["D1"], p["VCC"], "power", via=["x=570"])
d.wire(b["G"], p["GND"], "ground", via=["x=600"])
d.wire(b["RST"], b["D0"], "signal2", via=["x=126"])
d.note(700, 456, CHECK, size=13, bold=True)
d.note(700, 484, "D1 powers the probe, only while it reads", size=13, colour="#d62728")
d.note(18, 158, "D0 to RST:\nwakes it\nfrom deep\nsleep", size=13, colour="#1f77b4")
print(d.save(os.path.join(HERE, "wiring_d1_mini")))

# ---- ESP32-C3 SuperMini: probe to the right, so G and 3 run straight to GND and AOUT. 5 is on
# the far side and VCC is the middle pin, so it comes under the board and crosses AOUT once.
# The LiPo sits above, leads down: + from 5V, - from the ground wire. The divider's tap is on 4;
# its 100k to + crosses the ground wire, which nothing can avoid with + and - both above it.
d = Diagram(1460, 930, "kopernik-soil on an ESP32-C3 SuperMini",
            "GPIO numbers are printed on the back of the board - shown here beside each pin")
b = d.board("esp32c3_supermini", x=150, y=270, width=380, pin_labels=True)
xr, gy, vy, fy = b["5V"][0], b["G"][1], b["5V"][1], b["4"][1]
p = d.soil_probe(x=xr + 342, y=gy - 22, length=520, facing="left")
cell = d.lipo(x=xr + 110, y=vy - 210, leads="bottom")
bottom = b["0"][1] + 110
d.wire(b["5V"], cell["+"], "power", via=["x=" + str(cell["+"][0])])
d.wire(b["G"], p["GND"], "ground")
d.wire(d.junction(cell["-"][0], gy), cell["-"], "ground")
d.wire(b["3"], p["AOUT"], "signal", via=["x=" + str(xr + 270), "y=" + str(p["AOUT"][1])])
d.wire(b["5"], p["VCC"], "power", via=["x=" + str(b["5"][0] - 40), "y=" + str(bottom), "x=" + str(xr + 300)],
       label="5 powers the probe, only while it reads", label_at=2)
# Divider: tap T on 4; one 100k up to the ground wire, one to the 5V wire, crossing the ground wire
T = d.junction(xr + 60, fy)
d.wire(b["4"], T, "signal2")
d.resistor(xr + 60, fy, xr + 60, gy, "100k")
d.junction(xr + 60, gy)
d.wire(T, (xr + 140, fy), "#9a9a9a", width=3)
d.resistor(xr + 140, fy, xr + 140, gy + 10, "100k")
d.wire((xr + 140, gy + 10), (xr + 140, vy), "power")
d.junction(xr + 140, vy)
d.note(cell["-"][0] + 80, cell["-"][1] - 70, "4 reads the battery\nthrough the two 100k", size=13, colour="#1f77b4")
d.note(p.box[0] + 20, p.box[1] + p.box[3] + 34, CHECK, size=13, bold=True)
d.note(p.box[0] + 20, p.box[1] + p.box[3] + 60, "3 reads the probe", size=13, colour="#2ca02c")
print(d.save(os.path.join(HERE, "wiring_supermini")))
