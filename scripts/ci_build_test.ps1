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
$Generator = if ($env:CMAKE_GENERATOR) { $env:CMAKE_GENERATOR } else { 'Visual Studio 17 2022' }

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
Write-Host '== register gem =='
& $O3de register --external-subdirectory $GemPath
if ($LASTEXITCODE -ne 0) { Fail 'gem registration failed' }

# Configure. The Visual Studio generator is O3DE's documented Windows default;
# tests are gated by PAL_TRAIT_DIORAMA_TEST_SUPPORTED, now TRUE on Windows.
Write-Host '== configure =='
cmake -B $BuildDir -S $env:DIORAMA_PROJECT -G $Generator -DLY_UNITY_BUILD=ON
if ($LASTEXITCODE -ne 0) { Fail 'configure failed' }

# Build the gem, editor module, and test library.
Write-Host '== build =='
cmake --build $BuildDir --config $BuildConfig --target Diorama Diorama.Editor Diorama.Tests
if ($LASTEXITCODE -ne 0) { Fail 'build failed' }

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
