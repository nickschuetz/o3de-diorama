# Security Policy

## Supported versions

Diorama is pre-1.0 (beta). Security fixes land on the latest `0.x` release line;
older `0.x` tags are not separately patched.

| Version | Supported |
| --- | --- |
| latest `0.x` (currently `0.2.x`) | yes |
| older `0.x` | no |

## Reporting a vulnerability

Please **do not open a public issue** for a security vulnerability.

Report it privately through GitHub's **Report a vulnerability** flow on this
repository (the **Security** tab -> *Report a vulnerability* / private security
advisories). That opens a private channel with the maintainers.

When reporting, include where possible: the affected version/commit, a
description and impact, and steps to reproduce (a minimal scene, script, or
malformed asset). You can expect an acknowledgment within a few days, and we will
keep you updated as we investigate and fix.

## Scope

Diorama is an O3DE gem. It vendors no third-party code; its dependencies are the
O3DE engine and a few O3DE gems (see [`sbom.spdx.json`](sbom.spdx.json)).

- Issues in **Diorama's own code** (rendering, components, the asset builders, the
  bus APIs, asset parsing such as the native `.aseprite` reader) belong here.
- Issues in the **O3DE engine, Atom, or the engine's third-party libraries**
  should be reported to [o3de/o3de](https://github.com/o3de/o3de) instead. We
  treat asset-sourced data as untrusted and validate/bound it at load, so reports
  about malformed assets feeding Diorama's parsers are in scope and welcome.
