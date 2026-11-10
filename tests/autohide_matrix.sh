#!/bin/bash
# SPDX-FileCopyrightText: 2026 Clemens Wasser
# SPDX-License-Identifier: GPL-2.0-or-later
# Auto-hidden panel regression matrix: the target must stay on the visible screen edge.
set -u
here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/genie-autohide-matrix
mkdir -p "$out"
fail=0

# name | panel x,y,w,h | icon x,y,w,h | frame x,y,w,h | center x,y | target size | dir
cases=(
    "bottom-hidden|0,800,1280,62|58,808,54,46|320,200,640,400|85 777|41.125 28.953|2"
    "top-hidden|0,-62,1280,62|58,-54,54,46|320,200,640,400|85 23|41.125 28.953|1"
    "left-hidden|-62,0,62,800|-54,58,46,54|200,320,400,640|23 85|24.953 45.126|4"
    "right-hidden|1280,0,62,800|1288,58,46,54|200,320,400,640|1257 85|24.953 45.126|3"
)

for c in "${cases[@]}"; do
    IFS='|' read -r name panel icon geom center tsize edir <<< "$c"
    ex=${center% *}
    ey=${center#* }
    png=$out/$name.png
    GENIE_TEST_GEOMETRY="$geom" GENIE_DEBUG_PANEL="$panel" \
        "$here/tests/run_nested.sh" "$png" 0.5 "$icon" 1280x800 1 >/dev/null 2>&1
    if [ ! -s "$png.json" ]; then
        echo "FAIL $name: no debug json"
        fail=1
        continue
    fi
    if python3 - "$png.json" "$geom" "$ex" "$ey" "$tsize" "$edir" <<'PY'
import json, sys
j = json.load(open(sys.argv[1]))
gx, gy, gw, gh = (float(v) for v in sys.argv[2].split(','))
ex, ey = float(sys.argv[3]), float(sys.argv[4])
tw, th = (float(v) for v in sys.argv[5].split())
edir = int(sys.argv[6])
fx, fy, fw, fh = j["frame"]
x0, y0, x1, y1 = j["target"]
cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
ok = (abs(fx - gx) < 1.0 and abs(fy - gy) < 1.0
      and abs(fw - gw) < 1.0 and abs(fh - gh) < 1.0
      and abs(cx - ex) < 0.02 and abs(cy - ey) < 0.02
      and abs((x1 - x0) - tw) < 0.02 and abs((y1 - y0) - th) < 0.02
      and 0 <= cx <= 1280 and 0 <= cy <= 800
      and j["dir"] == edir)
if not ok:
    print(f"  got frame=({fx:.1f},{fy:.1f},{fw:.1f},{fh:.1f}) center=({cx:.3f},{cy:.3f}) "
          f"size=({x1-x0:.3f},{y1-y0:.3f}) dir={j['dir']}")
    print(f"  want frame=({gx:.1f},{gy:.1f},{gw:.1f},{gh:.1f}) center=({ex:.3f},{ey:.3f}) "
          f"size=({tw:.3f},{th:.3f}) dir={edir}")
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
