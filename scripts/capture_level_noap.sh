#!/bin/bash
#
# Capture a Diorama level headless WITHOUT running the AssetProcessor. Use this when
# the level + its assets are ALREADY in the cache (e.g. processed by the editor's
# live AP) and the standalone AssetProcessorBatch is unreliable on this host. It
# points the cached bootstrap autoexec LoadLevel straight at the target (the value
# AssetProcessorBatch would normally re-bake), launches the GameLauncher, and grabs
# frames with ffmpeg.
#
# Usage: scripts/capture_level_noap.sh <LevelName> <out.mp4> [seconds]
set -u
LEVEL="${1:?usage: capture_level_noap.sh <LevelName> <out.mp4> [seconds]}"
OUT="${2:?usage}"
SECS="${3:-4}"
PROJ="${DIORAMA_PROJECT:-$HOME/PROJECTS/DioramaSandbox}"
DISP="${CAP_DISPLAY:-:210}"
W="${CAP_W:-1280}"; H="${CAP_H:-720}"
LAUNCHER="$PROJ/build/linux/bin/profile/DioramaSandbox.GameLauncher"
GAMELOG="$PROJ/user/log/Game.log"

for p in $(pgrep -f DioramaSandbox.GameLauncher); do kill -9 "$p" 2>/dev/null; done
for p in $(pgrep -f "Xvfb ${DISP}"); do kill -9 "$p" 2>/dev/null; done
sleep 3
rm -f "/tmp/.X${DISP#:}-lock"

# Point LoadLevel at the target in the live registry AND every cached bootstrap
# (the cached value is the one the launcher honors last, so it must agree).
python3 - "$PROJ" "$LEVEL" <<'PY'
import json, sys, glob, os
proj, level = sys.argv[1], sys.argv[2]
files = [os.path.join(proj, "Registry", "load_level.setreg")]
files += glob.glob(os.path.join(proj, "Cache", "linux", "bootstrap.*.setreg"))
for f in files:
    try:
        d = json.load(open(f))
    except Exception:
        continue
    d.setdefault("O3DE", {}).setdefault("Autoexec", {}).setdefault("ConsoleCommands", {})["LoadLevel"] = level
    json.dump(d, open(f, "w"))
    print("set LoadLevel=%s in %s" % (level, os.path.basename(f)))
PY

Xvfb "$DISP" -screen 0 "${W}x${H}x24" -nolisten tcp -nocursor >/tmp/diorama_xvfb.log 2>&1 & XPID=$!
sleep 3
: > "$GAMELOG" 2>/dev/null || true
# r_displayInfo=0 hides Atom's top-right RHI/FPS/VRAM dev overlay so the frame is clean.
DISPLAY="$DISP" "$LAUNCHER" --project-path="$PROJ" --rhi=vulkan --r_fullscreen=0 --r_width="$W" --r_height="$H" --r_displayInfo=0 >/tmp/diorama_launcher.log 2>&1 & GPID=$!

for i in $(seq 1 40); do
  grep -qiE "$LEVEL.*loaded" "$GAMELOG" 2>/dev/null && break
  kill -0 $GPID 2>/dev/null || { echo "launcher exited early"; break; }
  sleep 3
done
sleep 6

if kill -0 $GPID 2>/dev/null; then
  # Lossless single-frame PNG (no yuv420p fringing on high-contrast pixel art).
  PNG="${OUT%.*}.png"
  DISPLAY="$DISP" ffmpeg -y -f x11grab -draw_mouse 0 -video_size "${W}x${H}" -i "$DISP" -frames:v 1 "$PNG" >/tmp/diorama_ffmpeg.log 2>&1
  echo "ffmpeg rc=$? -> $PNG"
else
  echo "launcher not running at capture time (see /tmp/diorama_launcher.log)"
fi
kill -9 $GPID 2>/dev/null; kill -9 $XPID 2>/dev/null
ls -la "$OUT" 2>/dev/null
