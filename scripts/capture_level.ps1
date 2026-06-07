<#
.SYNOPSIS
  Headless-friendly capture of a Diorama level on Windows: the project GameLauncher
  renders the level on the real GPU (in the logged-in desktop session) and ffmpeg
  records it. The Windows counterpart of scripts/capture_level.sh.

.DESCRIPTION
  Unlike Linux there is no Xvfb: Windows GPU rendering needs a logged-in desktop
  session with a display (a physical monitor or a dummy/virtual display). Run this
  from that session (e.g. the same interactive session the self-hosted runner uses).
  RHI on Windows defaults to DX12 (no lavapipe trap); override with $env:CAP_RHI.

  Flow mirrors the bash script: point the autoexec LoadLevel at the target, run
  AssetProcessorBatch to build it into the cache, launch the GameLauncher, wait for
  the level to load, settle, capture, then restore LoadLevel to "defaultlevel".

.PARAMETER Level
  Level name to load and capture.

.PARAMETER Out
  Output file. Use a .mp4 for video (default) or a .png with -Still for one frame.

.PARAMETER Seconds
  Video duration in seconds (ignored with -Still). Default 8.

.PARAMETER Still
  Capture a single PNG frame instead of a video. This is the CI-friendly mode
  (deterministic, one artifact to assert "not blank" against).

.NOTES
  Env overrides:
    DIORAMA_PROJECT  host project (default C:\projects\DioramaSandbox)
    O3DE_ENGINE_PATH engine/SDK    (default C:\O3DE\26.05.0)
    CAP_W, CAP_H     render size    (default 1280 720)
    CAP_RHI          dx12 (default) or vulkan
    CAP_WINDOW       GameLauncher window title for gdigrab (default DioramaSandbox)
    CAP_MODE         window (default; grab just the launcher window) or desktop
    CAP_LAUNCHER     full path to the GameLauncher exe (overrides the derived path)

  The level MUST contain a camera that becomes the active view (put
  make_active_camera.lua on it, or use scripts/prep_demo_camera.py), exactly as on
  Linux; without an active view the launcher renders gray. See
  Docs/howto/12-recording-demos.md.

  Requires ffmpeg on PATH (winget install Gyan.FFmpeg, or scoop install ffmpeg).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Level,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$Seconds = 8,
    [switch]$Still
)

$ErrorActionPreference = 'Stop'
function Fail($m) { Write-Error "capture_level: $m"; exit 1 }

$Proj    = if ($env:DIORAMA_PROJECT)  { $env:DIORAMA_PROJECT }  else { 'C:\projects\DioramaSandbox' }
$Engine  = if ($env:O3DE_ENGINE_PATH) { $env:O3DE_ENGINE_PATH } else { 'C:\O3DE\26.05.0' }
$W       = if ($env:CAP_W) { [int]$env:CAP_W } else { 1280 }
$H       = if ($env:CAP_H) { [int]$env:CAP_H } else { 720 }
$Rhi     = if ($env:CAP_RHI) { $env:CAP_RHI } else { 'dx12' }
$WinName = if ($env:CAP_WINDOW) { $env:CAP_WINDOW } else { 'DioramaSandbox' }
$Mode    = if ($env:CAP_MODE) { $env:CAP_MODE } else { 'window' }

$Launcher = if ($env:CAP_LAUNCHER) { $env:CAP_LAUNCHER }
            else { Join-Path $Proj 'build\windows\bin\profile\DioramaSandbox.GameLauncher.exe' }
$ApBatch  = Join-Path $Engine 'bin\Windows\profile\Default\AssetProcessorBatch.exe'
$GameLog  = Join-Path $Proj 'user\log\Game.log'
$LL       = Join-Path $Proj 'Registry\load_level.setreg'

if (-not (Test-Path $Launcher)) { Fail "GameLauncher not found at $Launcher (build it, or set CAP_LAUNCHER)" }
if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) { Fail 'ffmpeg not on PATH (winget install Gyan.FFmpeg)' }
if (-not (Test-Path $LL)) { Fail "no load_level.setreg at $LL" }

# Set the autoexec LoadLevel to our target FIRST, then process: the launcher merges
# the live setreg AND a cached bootstrap setreg that AssetProcessorBatch bakes from
# it; if they disagree the cached one wins and the wrong level loads. (Same gotcha as
# the Linux script.) Use pwsh's -AsHashtable for easy nested mutation.
function Set-LoadLevel($name) {
    $d = Get-Content $LL -Raw | ConvertFrom-Json -AsHashtable
    if (-not $d.O3DE) { $d.O3DE = @{} }
    if (-not $d.O3DE.Autoexec) { $d.O3DE.Autoexec = @{} }
    if (-not $d.O3DE.Autoexec.ConsoleCommands) { $d.O3DE.Autoexec.ConsoleCommands = @{} }
    $d.O3DE.Autoexec.ConsoleCommands.LoadLevel = $name
    ($d | ConvertTo-Json -Depth 32) | Set-Content -Path $LL -Encoding utf8
}

# Hard-kill any prior launcher so it does not share/clobber Game.log.
Get-Process -Name 'DioramaSandbox.GameLauncher' -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

Write-Host "== capture: level=$Level out=$Out mode=$Mode rhi=$Rhi $W`x$H =="
Set-LoadLevel $Level

if (Test-Path $ApBatch) {
    Write-Host '== AssetProcessorBatch (build level + changed assets) =='
    & $ApBatch "--project-path=$Proj" *> "$env:TEMP\diorama_apbatch.log"
}

# Fresh Game.log so the load-detection below sees only this run.
New-Item -ItemType Directory -Force -Path (Split-Path $GameLog) | Out-Null
Set-Content -Path $GameLog -Value '' -ErrorAction SilentlyContinue

$launchArgs = @("--project-path=$Proj", "--rhi=$Rhi", '--r_fullscreen=0', "--r_width=$W", "--r_height=$H")
$proc = Start-Process -FilePath $Launcher -ArgumentList $launchArgs -PassThru

# Wait (up to ~3 min) for "<Level> loaded" in Game.log, bailing if the launcher dies.
$loaded = $false
for ($i = 0; $i -lt 60; $i++) {
    if ($proc.HasExited) { Write-Host 'launcher exited before level load'; break }
    if ((Test-Path $GameLog) -and (Select-String -Path $GameLog -Pattern "$Level.*loaded" -Quiet -ErrorAction SilentlyContinue)) {
        $loaded = $true; break
    }
    Start-Sleep -Seconds 3
}
Start-Sleep -Seconds 8   # settle: reload + animation populate

$rc = 1
if (-not $proc.HasExited) {
    if (-not $loaded) { Write-Host "WARN: '$Level loaded' not seen; capturing anyway" }
    # gdigrab grabs the desktop (whole screen) or a window by title. Window mode
    # captures just the launcher regardless of position; switch to desktop if the
    # window title does not match (set CAP_MODE=desktop or CAP_WINDOW=<title>).
    $grabInput = if ($Mode -eq 'desktop') { 'desktop' } else { "title=$WinName" }
    if ($Still) {
        & ffmpeg -y -f gdigrab -draw_mouse 0 -i $grabInput -frames:v 1 $Out *> "$env:TEMP\diorama_ffmpeg.log"
    } else {
        & ffmpeg -y -f gdigrab -framerate 30 -draw_mouse 0 -i $grabInput -t $Seconds -pix_fmt yuv420p $Out *> "$env:TEMP\diorama_ffmpeg.log"
    }
    $rc = $LASTEXITCODE
    Write-Host "ffmpeg rc=$rc"
} else {
    Write-Host "launcher not running at capture time (see $env:TEMP\diorama_launcher.log)"
}

if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
Set-LoadLevel 'defaultlevel'
if (Test-Path $ApBatch) { & $ApBatch "--project-path=$Proj" *> "$env:TEMP\diorama_apbatch_restore.log" }
Write-Host 'restored load_level -> defaultlevel'

if (Test-Path $Out) { Get-Item $Out | Format-List Name, Length, LastWriteTime }
exit $rc
