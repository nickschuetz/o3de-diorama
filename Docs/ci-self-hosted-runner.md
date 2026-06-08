# Build/Test CI

The `build-test` workflow compiles the Diorama gem through a host O3DE project
and runs its unit tests. Unlike the `lint` workflow (which runs on free
GitHub-hosted runners), this one needs the full O3DE 26.05 SDK, a host project,
and the engine's 3rdParty packages. Its two legs solve that two different ways:

- **Linux** runs on a free **GitHub-hosted** runner inside a `container:` whose
  image (`ci/Containerfile`: Fedora 44 + the O3DE SDK from COPR) already carries
  the SDK and a host project. Untrusted PR code runs on GitHub's throwaway VMs,
  not on local hardware, so there is no fork-PR security risk and nothing to
  bring online.
- **Windows** runs on a **self-hosted** runner that has the SDK installed.
  Windows is O3DE's primary platform and headless GPU work needs the real
  hardware, so this leg is the active gate.

This is opt-in by design: the always-on gate is lint. Build/test runs when a
maintainer triggers it manually or labels a PR, so contributors are never
blocked.

## The two legs

| Leg     | Home                          | Trigger label    | Script                     |
| ------- | ----------------------------- | ---------------- | -------------------------- |
| Linux   | GitHub-hosted + `ci-fedora` container | `ci:build-linux` | `scripts/ci_build_test.sh` |
| Windows | self-hosted (`o3de`+`windows`) | `ci:build`       | `scripts/ci_build_test.ps1` |

The labels are distinct on purpose. **A leg whose `if` condition is true but
whose `runs-on` labels match no online runner does NOT skip; it queues until a
matching runner appears (or the run is cancelled).** Keeping `ci:build` for
Windows and `ci:build-linux` for Linux means an ordinary `ci:build` never leaves
a job hung waiting for a runner that is not online.

### Linux leg (GitHub-hosted container)

The image is built and pushed to this repo's GitHub Container Registry by the
`ci-image` workflow (`.github/workflows/ci-image.yml`), entirely on GitHub. It
runs on a manual dispatch or automatically when `ci/Containerfile` changes on
`main`, publishing `ghcr.io/<owner>/<repo>/ci-fedora:latest`. The build-test
Linux leg then pulls that image and runs `scripts/ci_build_test.sh` in it, with
`O3DE_ENGINE_PATH=/opt/O3DE/26.05.0` and `DIORAMA_PROJECT=/opt/diorama-host`
(both baked into the image). No repository variables and no self-hosted runner
are needed.

While the package is private the leg authenticates the pull with the built-in
`GITHUB_TOKEN`. Making the `ci-fedora` package public (ghcr package settings)
removes that need and lets fork PRs pull it too.

To bring the image up the first time: Actions -> `ci-image` -> **Run workflow**
(or merge a change to `ci/Containerfile`). Once it has published, add the
`ci:build-linux` label to a PR (or dispatch `build-test` with `leg=linux`).

A manual run (Actions -> build-test -> **Run workflow**) takes a `leg` input
(`both` / `linux` / `windows`, default `windows`) so you can exercise a single
platform. The Windows leg still needs its self-hosted runner online; the Linux
leg runs on a GitHub-hosted container and is always available.

The rest of this document covers the **self-hosted Windows** runner. The Linux
leg needs no host setup (see [Linux leg](#linux-leg-github-hosted-container)
above); to change what it builds in, edit `ci/Containerfile`.

## What the Windows runner needs

- A machine with O3DE **26.05** installed (the SDK): `scripts\o3de.bat` and
  `bin\Windows\<config>\Default\AzTestRunner.exe`.
- The engine 3rdParty packages (default `~/.o3de/3rdParty`).
- A host O3DE project to build the gem through (any project with the standard
  layout; this repo is developed against a `DioramaSandbox` project).
- Visual Studio 2022 or 2026 (the C++ workload) and its CMake. The Windows host
  build has been verified (VS2026 / MSVC); `scripts/ci_build_test.ps1`
  auto-detects the newest installed Visual Studio generator.

## One-time setup (Windows runner)

1. **Register a self-hosted runner** on the repository (GitHub: Settings →
   Actions → Runners → New self-hosted runner). Add the custom label `o3de`; the
   runner adds `self-hosted` and the `Windows` OS label automatically, so it
   matches the Windows leg (`[self-hosted, o3de, windows]`) with no extra labels.
   Configure unattended and install as a service, for example:

   ```powershell
   ./config.cmd --url https://github.com/<owner>/<repo> --token <TOKEN> `
     --labels o3de --name win-runner --unattended --runasservice
   ```

   (`--labels o3de,windows` also works; the explicit `windows` simply dedupes
   into the built-in `Windows` system label.)

2. **Tell the workflow where the engine and project are.** Set these as
   repository variables (Settings → Secrets and variables → Actions →
   Variables), or export them in the runner's service environment:

   | Variable           | Example                                |
   | ------------------ | -------------------------------------- |
   | `O3DE_ENGINE_PATH` | `C:\O3DE\26.05.0`                      |
   | `DIORAMA_PROJECT`  | `C:\projects\DioramaSandbox`           |

   The build script reads both. `GEM_PATH` defaults to the checked-out repo. The
   Windows leg also accepts `O3DE_ENGINE_PATH_WINDOWS` /
   `DIORAMA_PROJECT_WINDOWS` and prefers those when set, falling back to the
   common variables otherwise.

## Triggering it

- **Manually**: Actions → `build-test` → Run workflow. The `leg` input defaults
  to `windows`; choose `linux`/`both` to include the Linux container leg.
- **On a PR**: add the `ci:build` label to run the **Windows** leg, or
  **`ci:build-linux`** to run the **Linux** leg (each re-runs on every push while
  its label is present). Both can be applied together.

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

The Windows leg is **validated**: `ci_build_test.ps1` builds the gem and runs the
unit suite green on a Windows host (VS2026 / MSVC; all sprite UV / animation /
batch-plan tests pass). The script's path assumptions (`AzTestRunner.exe`
location, CMake generator) auto-resolve via `vswhere`. The one non-obvious snag
is the **runner service account** (below). To bring up a new Windows host, or to
debug a failure, run the script directly first:

> **Runner service account.** O3DE's Python is set up per Windows user profile
> (`%USERPROFILE%\.o3de\Python`). If the runner is installed as a service under
> the default `NT AUTHORITY\NetworkService` account, `o3de register` fails with
> *"Python has not been setup completely for O3DE"* because that account has no
> O3DE venv. Run the runner as the **same user that ran `get_python.bat`**:
> either interactively with `run.cmd` from that user's session, or install the
> service with `config.cmd ... --runasservice --windowslogonaccount "<host>\<user>"
> --windowslogonpassword "<pw>"`. Running interactively in a desktop session also
> gives the runner GPU access, which a session-0 service never has, so it is the
> better choice if you later add rendering tests.

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
   via `O3DE_ENGINE_PATH_WINDOWS` / `DIORAMA_PROJECT_WINDOWS`); then add the
   `ci:build` label to a PR to trigger the Windows leg. (The Linux leg is
   independent and always available via `ci:build-linux`.)

## Not covered

This runs the unit tests, which cover the UV, animation, and batch-planning
logic. It does **not** verify on-screen rendering: that needs an interactive
editor/GPU session and is confirmed by a human, not in CI.
