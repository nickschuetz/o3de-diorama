# Build/Test CI on a Self-Hosted Runner

The `build-test` workflow compiles the Diorama gem through a host O3DE project
and runs its unit tests. Unlike the `lint` workflow (which runs on free
GitHub-hosted runners), this one needs the full O3DE 26.05 SDK, a host project,
and the engine's 3rdParty packages, none of which fit on a hosted runner. It
therefore runs only on a **self-hosted runner** that already has the engine.

This is opt-in by design: the always-on gate is lint. Build/test runs when a
maintainer triggers it manually or labels a PR, so contributors without the
hardware are never blocked.

**Current posture (on-demand).** No always-on self-hosted runner is registered.
The practical pre-merge gate for a code change is running
`scripts/ci_build_test.sh` locally (see [Running the script locally](#running-the-script-locally)),
which performs the same build and tests as the workflow. The `ci:build` label
and the `O3DE_ENGINE_PATH` / `DIORAMA_PROJECT` repository variables are already
configured, so the workflow can run as soon as a runner is brought online on
demand. A persistent runner is intentionally avoided while the repository is
public-facing, because a self-hosted runner that executes pull-request code is a
security risk for fork contributions; bring one online only for trusted runs and
take it offline afterward.

The workflow has two legs, gated identically: a **Linux** leg
(`runs-on: [self-hosted, o3de]`, `scripts/ci_build_test.sh`) and a **Windows**
leg (`runs-on: [self-hosted, o3de, windows]`, `scripts/ci_build_test.ps1`). Each
leg only runs when a matching runner is online, so you can enable just one, or
both. Windows is O3DE's primary platform, so the Windows leg is the one that
confirms the gem builds and tests there, not only on Linux.

## What the runner needs

- A machine with O3DE **26.05** installed (the SDK). On Linux that means
  `scripts/o3de.sh` and `bin/Linux/<config>/Default/AzTestRunner`; on Windows,
  `scripts\o3de.bat` and `bin\Windows\<config>\Default\AzTestRunner.exe`.
- The engine 3rdParty packages (default `~/.o3de/3rdParty`).
- A host O3DE project to build the gem through (any project with the standard
  layout; this repo is developed against a `DioramaSandbox` project).
- Build tooling matching the engine: CMake plus, on Linux, Ninja
  (`ninja-build`) and a clang toolchain; on Windows, Visual Studio 2022 or 2026
  (the C++ workload) and its CMake. The Windows host build has been verified
  (VS2026 / MSVC); `scripts/ci_build_test.ps1` auto-detects the newest installed
  Visual Studio generator.

## One-time setup

1. **Register a self-hosted runner** on the repository (GitHub: Settings →
   Actions → Runners → New self-hosted runner). For a Linux runner give it the
   labels `self-hosted` and `o3de` (the Linux leg targets
   `runs-on: [self-hosted, o3de]`). For a Windows runner add a third label,
   `windows` (the Windows leg targets `runs-on: [self-hosted, o3de, windows]`).

2. **Tell the workflow where the engine and project are.** Set these as
   repository variables (Settings → Secrets and variables → Actions →
   Variables), or export them in the runner's service environment:

   | Variable           | Example                              |
   | ------------------ | ------------------------------------ |
   | `O3DE_ENGINE_PATH` | `/opt/O3DE/26.05.0`                  |
   | `DIORAMA_PROJECT`  | `/home/runner/projects/DioramaSandbox` |

   The build script reads both. `GEM_PATH` defaults to the checked-out repo.

   If one runner is Linux and another is Windows and their paths differ, set the
   Windows-specific variables `O3DE_ENGINE_PATH_WINDOWS` and
   `DIORAMA_PROJECT_WINDOWS` (e.g. `C:\O3DE\26.05.0` and
   `C:\projects\DioramaSandbox`); the Windows leg prefers those and falls back to
   the common variables when they are unset.

## Triggering it

- **Manually**: Actions → `build-test` → Run workflow.
- **On a PR**: add the `ci:build` label. The job runs on label, and again on
  each push to the PR while the label is present.

## What it does

`scripts/ci_build_test.sh` (Linux) and `scripts/ci_build_test.ps1` (Windows) are
the two build scripts the workflow calls, and which you can run locally with the
same environment variables. Both do the same four steps:

1. Register this gem as an external subdirectory with the engine.
2. Configure the host project (Linux: `Ninja Multi-Config`; Windows:
   `Visual Studio 17 2022`, overridable with `CMAKE_GENERATOR`).
3. Build `Diorama`, `Diorama.Editor`, and `Diorama.Tests`.
4. Run the unit tests through `AzTestRunner` and fail on any test failure.

## Running the script locally

The same scripts work on a developer machine.

Linux:

```bash
O3DE_ENGINE_PATH=/opt/O3DE/26.05.0 \
DIORAMA_PROJECT=/path/to/DioramaSandbox \
./scripts/ci_build_test.sh
```

Windows (PowerShell):

```powershell
$env:O3DE_ENGINE_PATH = "C:\O3DE\26.05.0"
$env:DIORAMA_PROJECT  = "C:\projects\DioramaSandbox"
.\scripts\ci_build_test.ps1
```

Optional overrides: `BUILD_CONFIG` (default `profile`), `BUILD_DIR`,
`TEST_FILTER`, `GEM_PATH`. Windows also accepts `CMAKE_GENERATOR` and
`AZ_TEST_RUNNER` (full path to `AzTestRunner.exe`, if it is not at the derived
default).

## First Windows bring-up

The Windows leg and `ci_build_test.ps1` were authored on Linux and have not yet
run on a real Windows host, so two path assumptions may need adjusting the first
time: the `AzTestRunner.exe` location and the CMake generator. Resolve them
interactively **before** turning the CI leg loose, by running the script
directly on the Windows machine:

1. **Install the prerequisites**: O3DE 26.05 SDK, the 3rdParty packages, a host
   project (e.g. `DioramaSandbox`), Visual Studio 2022 with the C++ workload,
   and CMake. Open a *Developer PowerShell for VS 2022* so the MSVC toolchain is
   on `PATH`.

2. **Run the script by hand** with your paths:

   ```powershell
   $env:O3DE_ENGINE_PATH = "C:\O3DE\26.05.0"
   $env:DIORAMA_PROJECT  = "C:\projects\DioramaSandbox"
   .\scripts\ci_build_test.ps1
   ```

3. **Fix the two likely snags** if the script fails late:
   - *Wrong generator*: if configure fails or you use a different Visual Studio
     version, set `$env:CMAKE_GENERATOR` (e.g. `"Visual Studio 16 2019"`).
   - *Runner not found*: the script derives
     `bin\Windows\<config>\Default\AzTestRunner.exe` from the engine path. If
     your SDK lays it out differently, find `AzTestRunner.exe` under the engine
     and set `$env:AZ_TEST_RUNNER` to its full path. If the test `.dll` is not at
     `bin\<config>\Diorama.Tests.dll` in the build tree, note the actual path;
     that location is currently hard-coded in the script and may need a tweak.

4. **Confirm green**, then capture whatever overrides you needed.

5. **Wire up CI**: register the runner with labels `self-hosted`, `o3de`,
   `windows`; set the repository variables (including the overrides from step 4,
   and `O3DE_ENGINE_PATH_WINDOWS` / `DIORAMA_PROJECT_WINDOWS` if the paths differ
   from the Linux runner's); then add the `ci:build` label to a PR to trigger
   both legs.

## Not covered

This runs the unit tests, which cover the UV, animation, and batch-planning
logic. It does **not** verify on-screen rendering: that needs an interactive
editor/GPU session and is confirmed by a human, not in CI.
