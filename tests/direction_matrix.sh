#!/bin/bash
# Direction-selection regression matrix.
#
# The effect must choose the same side as the Dock, based on the window edge
# relative to the panel edge. In particular, a window that extends far below a
# bottom panel has its centre below the icon even though the icon is still
# below the window; that must still select GENIE_DIR_DOWN.
set -u
here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/genie-direction-matrix
mkdir -p "$out"
fail=0

# name | panel x,y,w,h | icon x,y,w,h | frame x,y,w,h | screen | expected dir | min rows
cases=(
    "bottom-normal|0,738,1280,62|58,746,54,46|320,200,640,400|1280x800|2|0"
    "bottom-under|0,2098,3840,62|166,2106,54,46|2002,2000,640,400|3840x2160|2|0"
    "bottom-below|0,2098,3840,62|166,2106,54,46|2002,2110,640,400|3840x2160|1|0"
    "top-normal|0,0,1280,62|58,8,54,46|320,200,640,400|1280x800|1|0"
    "left-normal|0,0,62,800|8,58,46,54|320,200,640,400|1280x800|4|0"
    "right-normal|1218,0,62,800|1226,58,46,54|320,200,640,400|1280x800|3|0"
    "bottom-label|0,738,1280,62|100,746,200,46|320,200,640,400|1280x800|2|0"
)

for c in "${cases[@]}"; do
    IFS='|' read -r name panel icon geom size edir minrows <<< "$c"
    png=$out/$name.png
    GENIE_TEST_GEOMETRY="$geom" GENIE_DEBUG_PANEL="$panel" \
        "$here/tests/run_nested.sh" "$png" 0.5 "$icon" "$size" 1 >/dev/null 2>&1
    if [ ! -s "$png.json" ]; then
        echo "FAIL $name: no debug json"
        fail=1
        continue
    fi
    if python3 - "$png.json" "$geom" "$edir" "$minrows" <<'PY'
import json, math, sys
j = json.load(open(sys.argv[1]))
gx, gy, gw, gh = (float(v) for v in sys.argv[2].split(','))
edir = int(sys.argv[3])
minrows = int(sys.argv[4])
fx, fy, fw, fh = j["frame"]
tx0, ty0, tx1, ty1 = j["target"]
ok = (abs(fx - gx) < 1.0 and abs(fy - gy) < 1.0
      and abs(fw - gw) < 1.0 and abs(fh - gh) < 1.0
      and j["dir"] == edir
      and j["mesh"][0] > 0 and j["mesh"][1] >= minrows
      and all(math.isfinite(v) for v in j["target"]))
if not ok:
    print(f"  got frame=({fx:.1f},{fy:.1f},{fw:.1f},{fh:.1f}) "
          f"dir={j['dir']} mesh={j['mesh']} target={j['target']}")
    print(f"  want frame=({gx:.1f},{gy:.1f},{gw:.1f},{gh:.1f}) dir={edir} minrows={minrows}")
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
