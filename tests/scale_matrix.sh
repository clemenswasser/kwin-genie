#!/bin/bash
# SPDX-FileCopyrightText: 2026 Clemens Wasser
# SPDX-License-Identifier: GPL-2.0-or-later
# Fractional-scale regression matrix: target coordinates stay logical and the mesh follows the scale.
set -u
here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/genie-scale-matrix
mkdir -p "$out"
fail=0

# name | panel x,y,w,h | icon x,y,w,h | frame x,y,w,h | screen | scale | center x,y | target size | dir
cases=(
    "scale1|0,738,1280,62|58,746,54,46|320,200,640,400|1280x800|1|85 769|41.125 28.953|2"
    "scale2|0,338,640,62|200,346,54,46|100,50,400,300|1280x800|2|227 369|41.125 34.3438|2"
)

for c in "${cases[@]}"; do
    IFS='|' read -r name panel icon geom size scale center tsize edir <<< "$c"
    ex=${center% *}
    ey=${center#* }
    png=$out/$name.png
    GENIE_TEST_GEOMETRY="$geom" GENIE_DEBUG_PANEL="$panel" \
        "$here/tests/run_nested.sh" "$png" 0.5 "$icon" "$size" "$scale" >/dev/null 2>&1
    if [ ! -s "$png.json" ]; then
        echo "FAIL $name: no debug json"
        fail=1
        continue
    fi
    if python3 - "$png.json" "$geom" "$scale" "$ex" "$ey" "$tsize" "$edir" <<'PY'
import json, sys
j = json.load(open(sys.argv[1]))
gx, gy, gw, gh = (float(v) for v in sys.argv[2].split(','))
scale = float(sys.argv[3])
ex, ey = float(sys.argv[4]), float(sys.argv[5])
tw, th = (float(v) for v in sys.argv[6].split())
edir = int(sys.argv[7])
fx, fy, fw, fh = j["frame"]
x0, y0, x1, y1 = j["target"]
cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
ok = (abs(fx - gx) < 1.0 and abs(fy - gy) < 1.0
      and abs(fw - gw) < 1.0 and abs(fh - gh) < 1.0
      and abs(cx - ex) < 0.02 and abs(cy - ey) < 0.02
      and abs((x1 - x0) - tw) < 0.02 and abs((y1 - y0) - th) < 0.02
      and abs(j["scale"] - scale) < 0.001
      and j["dir"] == int(edir))
if not ok:
    print(f"  got frame=({fx:.1f},{fy:.1f},{fw:.1f},{fh:.1f}) center=({cx:.3f},{cy:.3f}) "
          f"size=({x1-x0:.3f},{y1-y0:.3f}) scale={j['scale']} dir={j['dir']}")
    print(f"  want frame=({gx:.1f},{gy:.1f},{gw:.1f},{gh:.1f}) center=({ex:.3f},{ey:.3f}) "
          f"size=({tw:.3f},{th:.3f}) scale={scale} dir={edir}")
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
