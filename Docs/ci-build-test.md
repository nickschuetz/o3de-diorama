# Build/Test CI

The `build-test` workflow compiles the Diorama gem through a host O3DE project
and runs its unit tests. Unlike the `lint` workflow (always-on, fast), this one
needs the full O3DE 26.05 SDK and a host project. Both legs run entirely on
**free GitHub-hosted runners** (the repository is public), so there is no
self-hosted hardware and untrusted pull-request code never touches a local
machine. They only differ in how they obtain the SDK:

- **Linux** runs inside a `container:` whose image (`ci/Containerfile`: Fedora 44
  + the O3DE SDK installed from the `hellaenergy/o3de` COPRs) already carries the
  SDK and a host project. A stock Ubuntu runner cannot be used directly: the SDK
  is an fc44 RPM that needs glibc 2.43 and Fedora system 3rdParty libs, so the
  container matches the SDK's build environment exactly.
- **Windows** runs on `windows-latest` and installs the official O3DE 26.05
  Windows SDK at runtime, then creates a throwaway host project. Windows is
  O3DE's primary platform, so this is the leg that confirms the gem builds and
  tests there.

Both legs are opt-in by label so an ordinary PR is gated by lint alone; a
maintainer adds a label (or runs a manual dispatch) to request a build.

## The two legs

| Leg     | Runner          | SDK source                         | Trigger label    | Script                      |
| ------- | --------------- | ---------------------------------- | ---------------- | --------------------------- |
| Linux   | `ubuntu-latest` + `ci-fedora` container | baked from COPR  | `ci:build-linux` | `scripts/ci_build_test.sh`  |
| Windows | `windows-latest` | installed at runtime              | `ci:build`       | `scripts/ci_build_test.ps1` |

The labels are distinct on purpose, so adding one does not accidentally trigger
the other platform.

Both scripts do the same four steps:

1. Register this gem as an external subdirectory with the engine.
2. Configure the host project (Linux: `Ninja Multi-Config`; Windows:
   auto-detected Visual Studio generator, overridable with `CMAKE_GENERATOR`).
3. Build `Diorama`, `Diorama.Editor`, and `Diorama.Tests`.
4. Run the unit tests through `AzTestRunner` and fail on any test failure.

Neither leg verifies on-screen rendering: build verification needs no GPU, but
visual confirmation needs an interactive editor/GPU session and is done by a
human, not in CI.

## Linux leg (GitHub-hosted container)

The image is built and pushed to this repo's GitHub Container Registry by the
`ci-image` workflow (`.github/workflows/ci-image.yml`), entirely on GitHub. It
runs on a manual dispatch or automatically when `ci/Containerfile` changes on
`main`, publishing `ghcr.io/<owner>/<repo>/ci-fedora:latest`. The build-test
Linux leg then pulls that image and runs `scripts/ci_build_test.sh` in it, with
`O3DE_ENGINE_PATH=/opt/O3DE/26.05.0` and `DIORAMA_PROJECT=/opt/diorama-host`
(both baked into the image).

To change what the Linux leg builds in (SDK version, deps, host project), edit
`ci/Containerfile`; merging that change to `main` republishes the image.

While the `ci-fedora` package is private the leg authenticates the pull with the
built-in `GITHUB_TOKEN`. Making the package public (its ghcr package settings)
removes that need and lets fork PRs pull it too.

**First bring-up:** Actions -> `ci-image` -> **Run workflow** (or merge a change
to `ci/Containerfile`). Once it has published, add the `ci:build-linux` label to
a PR (or dispatch `build-test` with `leg=linux`).

## Windows leg (GitHub-hosted, runtime install)

The leg runs on `windows-latest` and:

1. Downloads the official installer
   (`https://o3debinaries.org/main/Latest/Windows/o3de_installer_2605_0.exe`)
   and installs it unattended. The installer is a WiX bootstrapper, so
   `/quiet /norestart` performs a silent install.
2. Discovers the engine path under `C:\O3DE` (the directory containing
   `scripts\o3de.bat`) rather than hard-coding it, and exports it.
3. Creates a throwaway host project at `C:\diorama-host` with `o3de.bat
   create-project --template-name MinimalProject`.
4. Runs `scripts/ci_build_test.ps1`. `windows-latest` ships Visual Studio with
   the C++ workload, which the script auto-detects via `vswhere`.

No repository variables or secrets are involved: the runner is a disposable VM,
the SDK comes from the public installer, and the only token used anywhere in
this workflow is the built-in `GITHUB_TOKEN`.

**Trigger:** add the `ci:build` label to a PR (or dispatch `build-test` with
`leg=windows`, the default).

## Triggering it

- **Manually**: Actions -> `build-test` -> Run workflow. The `leg` input
  (`both` / `linux` / `windows`, default `windows`) picks which leg(s) run.
- **On a PR**: add `ci:build` for the **Windows** leg or `ci:build-linux` for the
  **Linux** leg (each re-runs on every push while its label is present). Both can
  be applied together.

## Running the scripts locally

The same scripts run on a developer machine that already has the SDK.

Linux:

```bash
O3DE_ENGINE_PATH=/opt/O3DE/26.05.0 \
DIORAMA_PROJECT=/path/to/HostProject \
./scripts/ci_build_test.sh
```

Windows (PowerShell):

```powershell
$env:O3DE_ENGINE_PATH = "C:\O3DE\26.05.0"
$env:DIORAMA_PROJECT  = "C:\projects\HostProject"
.\scripts\ci_build_test.ps1
```

Optional overrides: `BUILD_CONFIG` (default `profile`), `BUILD_DIR`,
`TEST_FILTER`, `GEM_PATH`. Windows also accepts `CMAKE_GENERATOR` and
`AZ_TEST_RUNNER` (full path to `AzTestRunner.exe`, if it is not at the derived
default).

The Windows host build is **validated** (VS2026 / MSVC; the full unit suite
passes). The script's path assumptions (`AzTestRunner.exe` location, CMake
generator) auto-resolve via `vswhere`; set the overrides above only if your SDK
is laid out differently.
