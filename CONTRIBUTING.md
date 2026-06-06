# Contributing to Diorama

Thanks for your interest! Diorama is a world-space 2D/2.5D gem for the
[Open 3D Engine](https://github.com/o3de/o3de). Contributions, issues, and ideas
are welcome.

## Ground rules

- Be respectful; see the [Code of Conduct](CODE_OF_CONDUCT.md).
- Keep the runtime client module free of Qt and AzToolsFramework. Editor-only code
  lives in the editor module.
- Treat asset-sourced data as untrusted: validate and bound it in builders and at
  load. No unchecked sizes feed GPU buffers.
- No per-frame allocations in the render loop.
- Match the surrounding code style and the existing SPDX file headers
  (`SPDX-License-Identifier: Apache-2.0 OR MIT`).

## Building and testing

Building Diorama or running its tests needs the full O3DE SDK plus a host project
to build through. The helper scripts do the whole register / configure / build /
test flow:

```bash
# Linux
export O3DE_ENGINE_PATH=/path/to/o3de         # has scripts/o3de.sh
export DIORAMA_PROJECT=/path/to/YourProject   # has project.json
scripts/ci_build_test.sh
```

```powershell
# Windows (auto-detects the newest installed Visual Studio)
$env:O3DE_ENGINE_PATH = "C:\O3DE\26.05"
$env:DIORAMA_PROJECT  = "C:\path\to\YourProject"
powershell -ExecutionPolicy Bypass -File scripts\ci_build_test.ps1
```

Both configure the project, build `Diorama` / `Diorama.Editor` / `Diorama.Tests`,
and run the unit tests through `AzTestRunner`. Set `TEST_FILTER` to narrow the run.

## Before you open a pull request

- **Format** C++ with `clang-format` **18.1.8** (CI pins this exact version; newer
  versions format differently). The `lint` workflow checks formatting, SPDX
  headers, whitespace/EOF hygiene, JSON validity, and SBOM freshness.
- **Run the tests** locally with `ci_build_test.sh` / `.ps1` and make sure they
  pass (CI cannot build the gem on hosted runners, so this is the build gate).
- **Update docs in the same change** as the code: the README feature table, the
  relevant how-to under `Docs/`, and `CHANGELOG.md`.
- If you change the gem version or its dependencies, regenerate the SBOM
  (`python3 scripts/gen_sbom.py`) and commit `sbom.spdx.json` (CI rejects a stale
  one).
- Keep AI/human parity: a new runtime knob should be reachable both from the
  Inspector and from the typed request bus.

## Upstream first

Diorama complements the engine. If you find something in O3DE (Atom, AzCore, asset
builders, ...) that is broken or improvable while working here, please flag and
contribute it back to [o3de/o3de](https://github.com/o3de/o3de) rather than
patching around it.

## Reporting bugs and requesting features

Use the issue templates. For security vulnerabilities, do **not** open a public
issue; follow [SECURITY.md](SECURITY.md).

## License

By contributing, you agree that your contributions are dual licensed under
Apache-2.0 OR MIT, matching the rest of the project.
