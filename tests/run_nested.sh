# SPDX-FileCopyrightText: 2026 Clemens Wasser
# SPDX-License-Identifier: GPL-2.0-or-later
# Render one frozen genie frame in an isolated, headless KWin (virtual backend) and dump it.
# usage: tests/run_nested.sh OUT.png PROGRESS [ICON x,y,w,h (logical)] [SCREEN WxH (device px)] [SCALE]
set -u
here=$(cd "$(dirname "$0")/.." && pwd)
out=$(realpath -m "$1"); p=$2; icon=${3:-}; size=${4:-1280x800}; scale=${5:-1}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/kwin-genie.XXXXXX")
mkdir -p "$tmp/config" "$tmp/runtime"; chmod 700 "$tmp/runtime"
cat > "$tmp/config/kwinrc" <<CFG
[Plugins]
genieEnabled=true
magiclampEnabled=false
squashEnabled=false
CFG
qdbus_cmd=$(command -v qdbus6 || command -v qdbus || true)
geom=${GENIE_TEST_GEOMETRY:-}
if [ -n "$geom" ]; then
    IFS=, read -r gx gy gw gh <<EOF
$geom
EOF
    cat > "$tmp/place.js" <<JS
function place(w) {
    if (w.caption === "genie-test") {
        w.frameGeometry = {x: $gx, y: $gy, width: $gw, height: $gh};
    }
}
workspace.windowList().forEach(place);
workspace.windowAdded.connect(place);
JS
fi
rm -f "$out" "$out.json"
XDG_CONFIG_HOME="$tmp/config" XDG_RUNTIME_DIR="$tmp/runtime" timeout 40 dbus-run-session -- bash -c "
  KWIN_EFFECTS_FORCE_ANIMATIONS=1 QT_FORCE_STDERR_LOGGING=1 QT_PLUGIN_PATH='$here/build/bin':/usr/lib64/qt6/plugins GENIE_DEBUG_PROGRESS='$p' GENIE_DEBUG_DUMP='$out' GENIE_DEBUG_ICON='$icon' \
    KWIN_COMPOSE=O2ES kwin_wayland --virtual --width ${size%x*} --height ${size#*x} --socket wayland-genie > '$tmp/kwin.log' 2>&1 &
  kwin=\$!
  for i in \$(seq 50); do [ -S '$tmp/runtime/wayland-genie' ] && break; sleep 0.1; done
  if [ -n '$geom' ] && [ -n '$qdbus_cmd' ]; then
    for i in \$(seq 50); do '$qdbus_cmd' org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript '$tmp/place.js' place >/dev/null 2>&1 && break; sleep 0.1; done
    '$qdbus_cmd' org.kde.KWin /Scripting org.kde.kwin.Scripting.start >/dev/null 2>&1 || true
  fi
  if [ '$scale' != 1 ]; then WAYLAND_DISPLAY=wayland-genie kscreen-doctor output.1.scale.$scale > '$tmp/kscreen.log' 2>&1; sleep 0.5; fi
  WAYLAND_DISPLAY=wayland-genie QT_QPA_PLATFORM=wayland '$here/build/bin/genie_test_client' '$here/tests/pattern.png' 1200 ${GENIE_TEST_BG:-} > '$tmp/client.log' 2>&1 &
  client=\$!
  for i in \$(seq 100); do [ -f '$out' ] && break; sleep 0.1; done
  kill \$kwin 2>/dev/null; sleep 0.5; kill -9 \$kwin 2>/dev/null; kill -9 \$client 2>/dev/null; true
" > "$tmp/session.log" 2>&1 < /dev/null
if [ -f "$out" ]; then echo "ok $out $(cat "$out.json")"; else echo "FAILED (logs in $tmp)"; grep -iE 'genie|error|fail' "$tmp/kwin.log" | head -8; fi
