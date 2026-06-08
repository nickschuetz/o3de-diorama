<#
.SYNOPSIS
    Build the Diorama gem through a host O3DE project and run its unit tests on Windows.

.DESCRIPTION
    Windows counterpart of ci_build_test.sh. Building Diorama requires the full
    O3DE SDK (multi-GB) plus a host project to build through, which GitHub-hosted
    runners do not have. This script is meant for a self-hosted Windows runner (or
    a developer machine) that already has the engine and a project configured. It
    is deliberately path-agnostic: all locations come from environment variables
    so it is not tied to one machine.

    Required environment:
      O3DE_ENGINE_PATH   Path to the O3DE engine/SDK (has scripts\o3de.bat).
      DIORAMA_PROJECT    Path to a host project to build the gem through.
      GEM_PATH           Path to this gem checkout (defaults to the repo root).

    Optional:
      BUILD_CONFIG       profile (default) | debug | release.
      BUILD_DIR          Build tree (default: <project>\build\windows).
      CMAKE_GENERATOR    CMake generator (default: "Visual Studio 17 2022").
      TEST_FILTER        gtest filter (default: all Diorama tests).
      AZ_TEST_RUNNER     Full path to AzTestRunner.exe (default: derived from the engine).

    Exit status is non-zero if configure, build, or any test fails.
#>

$ErrorActionPreference = 'Stop'

function Fail($message) {
    Write-Error "ci_build_test: $message"
    exit 1
}

$GemPath = if ($env:GEM_PATH) { $env:GEM_PATH } else { (Resolve-Path (Join-Path $PSScriptRoot '..')).Path }
$BuildConfig = if ($env:BUILD_CONFIG) { $env:BUILD_CONFIG } else { 'profile' }

# Pick the CMake generator: explicit override wins; otherwise auto-detect the
# newest installed Visual Studio (via vswhere) so VS2026 / VS2022 / VS2019 boxes
# all work without the caller hand-setting CMAKE_GENERATOR.
function Get-VsGenerator {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    $ver = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion 2>$null
    if (-not $ver) { return $null }
    switch ([int]($ver.Split('.')[0])) {
        18 { 'Visual Studio 18 2026' }
        17 { 'Visual Studio 17 2022' }
        16 { 'Visual Studio 16 2019' }
        default { $null }
    }
}
$Generator = if ($env:CMAKE_GENERATOR) { $env:CMAKE_GENERATOR }
             elseif ($g = Get-VsGenerator) { $g }
             else { 'Visual Studio 17 2022' }

if (-not $env:O3DE_ENGINE_PATH) { Fail 'O3DE_ENGINE_PATH is not set' }
if (-not $env:DIORAMA_PROJECT) { Fail 'DIORAMA_PROJECT is not set' }

$O3de = Join-Path $env:O3DE_ENGINE_PATH 'scripts\o3de.bat'
if (-not (Test-Path $O3de)) { Fail "no scripts\o3de.bat under O3DE_ENGINE_PATH ($env:O3DE_ENGINE_PATH)" }
if (-not (Test-Path (Join-Path $env:DIORAMA_PROJECT 'project.json'))) { Fail "DIORAMA_PROJECT has no project.json ($env:DIORAMA_PROJECT)" }

$BuildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path $env:DIORAMA_PROJECT 'build\windows' }

Write-Host '== Diorama CI build/test (Windows) =='
Write-Host "  engine : $env:O3DE_ENGINE_PATH"
Write-Host "  project: $env:DIORAMA_PROJECT"
Write-Host "  gem    : $GemPath"
Write-Host "  config : $BuildConfig"
Write-Host "  build  : $BuildDir"
Write-Host "  gen    : $Generator"

# Register the gem as an external subdirectory so the project can find it.
# Idempotent: re-registering an existing path is a no-op.
Write-Host '== register + enable gem =='
# Register makes the gem discoverable; enable adds it to the project so its
# targets (Diorama, Diorama.Editor, Diorama.Tests) are generated at configure.
# Registering alone is not enough on a project that does not already enable the
# gem (e.g. a fresh host project). Both are idempotent.
& $O3de register --external-subdirectory $GemPath
if ($LASTEXITCODE -ne 0) { Fail 'gem registration failed' }
& $O3de enable-gem --gem-path $GemPath --project-path $env:DIORAMA_PROJECT
if ($LASTEXITCODE -ne 0) { Fail 'gem enable failed' }

# Configure with the chosen generator. tests are gated by
# PAL_TRAIT_DIORAMA_TEST_SUPPORTED, now TRUE on Windows.
# Clear a stale cache first: CMake errors out if a prior configure used a
# different generator (e.g. switching a box from VS2022 to VS2026).
$cache = Join-Path $BuildDir 'CMakeCache.txt'
if (Test-Path $cache) {
    $prev = (Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:' -ErrorAction SilentlyContinue).Line
    if ($prev -and ($prev -notlike "*$Generator*")) {
        Write-Host "  clearing stale CMake cache ($prev)"
        Remove-Item -Force $cache -ErrorAction SilentlyContinue
        Remove-Item -Recurse -Force (Join-Path $BuildDir 'CMakeFiles') -ErrorAction SilentlyContinue
    }
}
Write-Host "== configure (generator: $Generator) =="
cmake -B $BuildDir -S $env:DIORAMA_PROJECT -G $Generator -DLY_UNITY_BUILD=ON
if ($LASTEXITCODE -ne 0) { Fail 'configure failed' }

# Build the gem, editor module, and test library. With the Visual Studio
# generator, `cmake --build --target <t>` assumes <t>.vcxproj sits at the build
# root, which is false for a gem registered as an external subdirectory: its
# source is outside the host project, so CMake generates the .vcxproj in a nested
# binary directory and the build aborts with MSB1009 ("project file does not
# exist"). Locate each target's .vcxproj in the build tree and build it directly
# with MSBuild instead.
Write-Host '== build =='
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$MSBuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null | Select-Object -First 1
if (-not $MSBuild) { Fail 'MSBuild not found via vswhere' }
Write-Host "  msbuild: $MSBuild"
foreach ($target in @('Diorama', 'Diorama.Editor', 'Diorama.Tests')) {
    $proj = Get-ChildItem -Path $BuildDir -Recurse -Filter "$target.vcxproj" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $proj) { Fail "could not find $target.vcxproj under $BuildDir" }
    Write-Host "building $($proj.FullName)"
    & $MSBuild $proj.FullName /nologo /m /p:Configuration=$BuildConfig /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { Fail "build failed: $target" }
}

# Run the unit tests directly through AzTestRunner so the result is explicit and
# does not depend on ctest discovery. The Windows MODULE target builds as a .dll
# (no "lib" prefix) under the project's bin directory for the chosen config.
$TestLib = Join-Path $BuildDir "bin\$BuildConfig\Diorama.Tests.dll"
if (-not (Test-Path $TestLib)) { Fail "test library not found after build: $TestLib" }

$Runner = if ($env:AZ_TEST_RUNNER) { $env:AZ_TEST_RUNNER } else { Join-Path $env:O3DE_ENGINE_PATH "bin\Windows\$BuildConfig\Default\AzTestRunner.exe" }
if (-not (Test-Path $Runner)) { Fail "AzTestRunner not found at $Runner (set AZ_TEST_RUNNER to override)" }

$Filter = if ($env:TEST_FILTER) { $env:TEST_FILTER } else { '*' }

Write-Host '== run tests =='
& $Runner $TestLib AzRunUnitTests "--gtest_filter=$Filter"
if ($LASTEXITCODE -ne 0) { Fail 'unit tests failed' }

Write-Host '== all tests passed =='
