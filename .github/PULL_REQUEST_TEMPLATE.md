<!-- Thanks for contributing to Diorama. Keep the description focused on what and why. -->

## What this changes

A short summary of the change and the motivation.

## Checklist

- [ ] Runtime client module stays free of Qt and AzToolsFramework (editor-only code lives in the editor module).
- [ ] New runtime knobs are reachable both in the Inspector and on a typed request bus (human/AI parity).
- [ ] No per-frame allocations added to the render loop; asset-sourced data is validated and bounded.
- [ ] Code is portable (std:: math, no POSIX-only calls, explicit-endian binary parsing) and builds on Windows and Linux.
- [ ] C++ formatted with clang-format 18.1.8; SPDX headers present on new files.
- [ ] Docs updated in the same change (how-to/reference) when behavior changed.
- [ ] If the version or dependencies changed, `sbom.spdx.json` was regenerated (`python3 scripts/gen_sbom.py`).
- [ ] Unit tests added/updated and pass locally (`scripts/ci_build_test.sh`).

## How this was tested

Describe the build/test you ran (platform, config) and any captures or screenshots.
