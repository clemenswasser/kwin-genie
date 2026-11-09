# Install the freshly built effect under a new plugin id and activate it in the running KWin.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
plug=/usr/lib64/qt6/plugins/kwin/effects/plugins
id=${1:-genie-$(date +%s)}
so=$here/build/bin/kwin/effects/plugins/genie.so

[ -f "$so" ] || { echo "build the effect first: cmake --build '$here/build'" >&2; exit 1; }

sudo install -m755 "$so" "$plug/$id.so"
kwriteconfig6 --file kwinrc --group Plugins --key "${id}Enabled" true

mapfile -t keys < <(kreadconfig6 --file kwinrc --group Plugins --dump 2>/dev/null |
    sed -n '/^Group: Plugins$/,/^$/s/^  \(genie[^:]*\)Enabled:.*/\1/p')
for old in "${keys[@]}"; do
    if [ "$old" != "$id" ]; then
        kwriteconfig6 --file kwinrc --group Plugins --key "${old}Enabled" false
        qdbus org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect "$old" >/dev/null 2>&1 || true
    fi
done

qdbus org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect "$id" >/dev/null 2>&1 || true

for file in "$plug"/genie*.so; do
    [ "$file" = "$plug/$id.so" ] || sudo rm -f "$file"
done
echo "genie effect loaded as '$id' (Name: Genie)"
