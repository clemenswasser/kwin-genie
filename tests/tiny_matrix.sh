#!/bin/bash
# SPDX-FileCopyrightText: 2026 Clemens Wasser
# SPDX-License-Identifier: GPL-2.0-or-later
# The macOS Dock switches to its scale effect for windows narrower or shorter
# than 5 pt. Verify that the effect takes the same path.
set -u
here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/genie-tiny-matrix
mkdir -p "$out"
fail=0

# name | frame x,y,w,h | expected mode
cases=(
    "tiny-width|100,100,4,29|scale"
    "normal|320,200,640,400|genie"
)

for c in "${cases[@]}"; do
    IFS='|' read -r name geom mode <<< "$c"
    png=$out/$name.png
    GENIE_TEST_GEOMETRY="$geom" GENIE_DEBUG_PANEL=0,738,1280,62 \
        "$here/tests/run_nested.sh" "$png" 0.25 '' 1280x800 1 >/dev/null 2>&1
    if [ ! -s "$png.json" ]; then
        echo "FAIL $name: no debug json"
        fail=1
        continue
    fi
    if python3 - "$png.json" "$geom" "$mode" <<'PY'
import json, sys
j = json.load(open(sys.argv[1]))
gx, gy, gw, gh = (float(v) for v in sys.argv[2].split(','))
fx, fy, fw, fh = j["frame"]
ok = (abs(fx - gx) < 1.0 and abs(fy - gy) < 1.0
      and abs(fw - gw) < 1.0 and abs(fh - gh) < 1.0
      and j.get("mode") == sys.argv[3])
if sys.argv[3] == "scale":
    ok = ok and j["mesh"] == [0, 0]
else:
    ok = ok and j["mesh"][0] > 0 and j["mesh"][1] > 0
if not ok:
    print(f"  got frame=({fx:.1f},{fy:.1f},{fw:.1f},{fh:.1f}) "
          f"mode={j.get('mode')} mesh={j['mesh']}")
    print(f"  want frame=({gx:.1f},{gy:.1f},{gw:.1f},{gh:.1f}) mode={sys.argv[3]}")
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
