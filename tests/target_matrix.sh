# SPDX-FileCopyrightText: 2026 Clemens Wasser
# SPDX-License-Identifier: GPL-2.0-or-later
set -u
here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/genie-target-matrix
mkdir -p "$out"
fail=0

cases=(
    "bottom  58,746,54,46    0,738,1280,62  85  769 2"
    "top     58,8,54,46      0,0,1280,62    85  31  1"
    "left    8,58,46,54      0,0,62,800     31  85  4"
    "right   1226,58,46,54   1218,0,62,800  1249 85 3"
    "label   100,746,200,46  0,738,1280,62  123 769 2"
)

for c in "${cases[@]}"; do
    set -- $c
    name=$1 icon=$2 panel=$3 ex=$4 ey=$5 edir=$6
    png=$out/$name.png
    GENIE_DEBUG_PANEL="$panel" "$here/tests/run_nested.sh" "$png" 0.5 "$icon" 1280x800 1 >/dev/null 2>&1
    if [ ! -s "$png.json" ]; then
        echo "FAIL $name: no debug json"
        fail=1
        continue
    fi
    if python3 - "$png.json" "$ex" "$ey" "$edir" <<'PY'
import json, sys
j = json.load(open(sys.argv[1]))
x0, y0, x1, y1 = j["target"]
cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
ex, ey, edir = (float(v) for v in sys.argv[2:5])
ok = (abs(cx - ex) < 0.02 and abs(cy - ey) < 0.02
      and abs((x1 - x0) - 41.125) < 0.02 and abs((y1 - y0) - 30.8398) < 0.02
      and j["dir"] == int(edir))
if not ok:
    print(f"  got center=({cx:.3f},{cy:.3f}) size=({x1-x0:.3f},{y1-y0:.3f}) dir={j['dir']}")
    print(f"  want center=({ex:.3f},{ey:.3f}) size=(41.125,30.8398) dir={edir}")
sys.exit(0 if ok else 1)
PY
    then
        echo "ok   $name"
    else
        echo "FAIL $name"
        fail=1
    fi
done

exit $fail
