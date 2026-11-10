#!/bin/bash
# SPDX-FileCopyrightText: 2026 Clemens Wasser
# SPDX-License-Identifier: GPL-2.0-or-later
# GPU frame-time benchmark for the genie effect in an isolated headless KWin.
#
# usage: tools/benchmark_gpu.sh [--out DIR] [--reps N] [--p LIST] [--sizes LIST] [--seconds N]
#
# Defaults: one repetition at p=0.5, sizes 8,12,16,24,32 plus the exact mesh.
# Results are platform/GPU specific. Parse them with tools/benchmark_parse.py.
set -u

here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/genie-benchmark
reps=1
ps=0.5
sizes=8,12,16,24,32
seconds=5

while [ $# -gt 0 ]; do
    case "$1" in
        --out) out=$2; shift 2 ;;
        --reps) reps=$2; shift 2 ;;
        --p) ps=$2; shift 2 ;;
        --sizes) sizes=$2; shift 2 ;;
        --seconds) seconds=$2; shift 2 ;;
        -h|--help)
            sed -n '2,9p' "$0"
            exit 0
            ;;
        *)
            echo "unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

so=$here/build/bin/kwin/effects/plugins/genie.so
client=$here/build/bin/genie_test_client
for f in "$so" "$client"; do
    [ -e "$f" ] || { echo "build the effect first: cmake --build '$here/build'" >&2; exit 1; }
done
command -v kwin_wayland >/dev/null || { echo "kwin_wayland not found" >&2; exit 1; }
command -v dbus-run-session >/dev/null || { echo "dbus-run-session not found" >&2; exit 1; }
qdbus_cmd=$(command -v qdbus6 || command -v qdbus || true)
[ -n "$qdbus_cmd" ] || { echo "qdbus not found" >&2; exit 1; }
magick_cmd=$(command -v magick || command -v convert || true)
[ -n "$magick_cmd" ] || { echo "ImageMagick (magick/convert) not found" >&2; exit 1; }

mkdir -p "$out"
pattern=$out/pattern-1794x1076.png
"$magick_cmd" "$here/tests/pattern.png" -resize 1794x1076! "$pattern" >/dev/null 2>&1
[ -s "$pattern" ] || { echo "failed to create $pattern" >&2; exit 1; }

tmp=$(mktemp -d "${TMPDIR:-/tmp}/genie-bench.XXXXXX")
cleanup()
{
    if [ "${fail:-0}" = 0 ]; then
        rm -rf "$tmp"
    else
        echo "benchmark logs kept in $tmp" >&2
    fi
}
trap cleanup EXIT

run_one()
{
    cfg=$1
    mode=$2
    px=$3
    p=$4
    rep=$5
    dir=$out/${cfg}_p${p}_r${rep}
    run=$tmp/run-${cfg}-${p}-${rep}
    mkdir -p "$dir" "$run/config" "$run/runtime" "$run/home"
    chmod 700 "$run/runtime"

    cat > "$run/config/kwinrc" <<CFG
[Plugins]
genieEnabled=$([ "$mode" = exact ] || [ "$mode" = smooth ] && echo true || echo false)
magiclampEnabled=false
squashEnabled=false
scaleEnabled=false
glideEnabled=false
CFG
    cat > "$run/place.js" <<'JS'
function place(w) {
    if (w.caption === "genie-test") {
        w.frameGeometry = {x: 2002, y: 1907, width: 1794, height: 1104};
    }
}
workspace.windowList().forEach(place);
workspace.windowAdded.connect(place);
JS

    XDG_CONFIG_HOME="$run/config" XDG_RUNTIME_DIR="$run/runtime" HOME="$run/home" \
    KWIN_LOG_PERFORMANCE_DATA=1 GENIE_QUALITY="$mode" GENIE_MESH_PX="$px" \
    GENIE_DEBUG_PROGRESS="$p" GENIE_DEBUG_DUMP="$run/dump.png" \
    GENIE_DEBUG_ICON=166,2106,54,46 GENIE_DEBUG_PANEL=0,2098,3840,62 \
    timeout $((seconds + 20)) dbus-run-session -- bash -c "
      cd '$run'
      KWIN_EFFECTS_FORCE_ANIMATIONS=1 QT_FORCE_STDERR_LOGGING=1 \
        QT_PLUGIN_PATH='$here/build/bin':/usr/lib64/qt6/plugins \
        KWIN_COMPOSE=O2ES kwin_wayland --virtual --width 3840 --height 2160 \
        --socket wayland-genie > '$run/kwin.log' 2>&1 &
      kwin=\$!
      for i in \$(seq 50); do [ -S '$run/runtime/wayland-genie' ] && break; sleep 0.1; done
      for i in \$(seq 50); do
        '$qdbus_cmd' org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript '$run/place.js' place >/dev/null 2>&1 && break
        sleep 0.1
      done
      '$qdbus_cmd' org.kde.KWin /Scripting org.kde.kwin.Scripting.start >/dev/null 2>&1 || true
      WAYLAND_DISPLAY=wayland-genie QT_QPA_PLATFORM=wayland \
        '$client' '$pattern' 1200 > '$run/client.log' 2>&1 &
      client=\$!
      sleep $seconds
      kill \$kwin 2>/dev/null; sleep 0.5; kill -9 \$kwin 2>/dev/null; kill -9 \$client 2>/dev/null; true
    " > "$run/session.log" 2>&1 < /dev/null

    cp "$run"/kwin\ perf\ statistics\ *.csv "$dir/" 2>/dev/null || true
    cp "$run/dump.png" "$dir/$cfg.png" 2>/dev/null || true
    if ls "$dir"/*.csv >/dev/null 2>&1; then
        echo "ok   $cfg p=$p rep=$rep -> $dir"
    else
        echo "FAIL $cfg p=$p rep=$rep (logs in $run)" >&2
        grep -iE 'error|fail|genie' "$run/kwin.log" 2>/dev/null | head -5 >&2
        return 1
    fi
}

fail=0
IFS=',' read -ra p_list <<< "$ps"
IFS=',' read -ra size_list <<< "$sizes"
for p in "${p_list[@]}"; do
    for rep in $(seq 1 "$reps"); do
        run_one exact exact 8 "$p" "$rep" || fail=1
        for px in "${size_list[@]}"; do
            [ "$px" = exact ] && continue
            run_one "$px" smooth "$px" "$p" "$rep" || fail=1
        done
    done
done

echo "results: $out"
[ "$fail" = 0 ] || exit 1
