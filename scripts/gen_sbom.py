#!/usr/bin/env python3
"""
Generate a minimal, accurate SPDX 2.3 SBOM for the Diorama gem.

Diorama vendors no third-party code; its dependencies are entirely O3DE (the
engine plus a few O3DE gems). The transitive third-party tree (zlib, Lua, Qt,
PhysX, OpenSSL, ...) belongs to O3DE and is covered by O3DE's own SBOM/3rdParty
manifests -- so this SBOM declares Diorama and its DIRECT deps only, and points
at O3DE for the rest rather than re-listing (and implicitly vouching for) trees
it does not own.

Run: python3 scripts/gen_sbom.py  ->  writes sbom.spdx.json at the repo root.
"""
import json, os, datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
gem = json.load(open(os.path.join(ROOT, "gem.json")))
version = gem.get("version", "0.0.0")
created = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

REPO = "https://github.com/nickschuetz/o3de-diorama"
O3DE = "https://github.com/o3de/o3de"
DUAL = "Apache-2.0 OR MIT"

# Direct dependencies (from gem.json + the engine itself). All supplied by O3DE;
# Diorama links them but does not redistribute their sources.
DEPS = [
    ("O3DE-Engine", "Open 3D Engine", "26.05", O3DE,
     "The host engine (AzCore, AzFramework, Atom, asset pipeline). Provides and "
     "SBOMs the transitive 3rdParty (zlib, Lua, Qt, PhysX, OpenSSL, ...)."),
    ("Gem-Atom-RPI", "Atom_RPI (O3DE gem)", "26.05", O3DE, "O3DE rendering gem dependency."),
    ("Gem-Atom-Feature-Common", "Atom_Feature_Common (O3DE gem)", "26.05", O3DE, "O3DE rendering gem dependency."),
    ("Gem-MiniAudio", "MiniAudio (O3DE gem)", "26.05", O3DE,
     "O3DE audio gem dependency; it bundles + SBOMs its own audio libraries (miniaudio, ogg, vorbis)."),
]

packages = [{
    "SPDXID": "SPDXRef-Package-Diorama",
    "name": "Diorama",
    "versionInfo": version,
    "downloadLocation": REPO,
    "homepage": REPO,
    "filesAnalyzed": False,
    "supplier": "Person: Nick Schuetz",
    "licenseConcluded": DUAL,
    "licenseDeclared": DUAL,
    "copyrightText": "Copyright (c) Contributors to the Open 3D Engine Project.",
    "externalRefs": [{
        "referenceCategory": "PACKAGE-MANAGER",
        "referenceType": "purl",
        "referenceLocator": f"pkg:github/nickschuetz/o3de-diorama@{version}",
    }],
    "comment": "World-space 2D/2.5D gem for O3DE. Vendors no third-party code.",
}]
relationships = [{
    "spdxElementId": "SPDXRef-DOCUMENT",
    "relationshipType": "DESCRIBES",
    "relatedSpdxElement": "SPDXRef-Package-Diorama",
}]

for spid, name, ver, loc, note in DEPS:
    pid = f"SPDXRef-Package-{spid}"
    packages.append({
        "SPDXID": pid,
        "name": name,
        "versionInfo": ver,
        "downloadLocation": loc,
        "filesAnalyzed": False,
        "supplier": "Organization: The Linux Foundation (Open 3D Foundation)",
        "licenseConcluded": "NOASSERTION",
        "licenseDeclared": DUAL,
        "copyrightText": "NOASSERTION",
        "comment": note,
    })
    relationships.append({
        "spdxElementId": "SPDXRef-Package-Diorama",
        "relationshipType": "DEPENDS_ON",
        "relatedSpdxElement": pid,
    })

doc = {
    "spdxVersion": "SPDX-2.3",
    "dataLicense": "CC0-1.0",
    "SPDXID": "SPDXRef-DOCUMENT",
    "name": f"Diorama-{version}",
    "documentNamespace": f"{REPO}/sbom/{version}",
    "creationInfo": {
        "created": created,
        "creators": ["Tool: scripts/gen_sbom.py"],
        "licenseListVersion": "3.22",
    },
    "packages": packages,
    "relationships": relationships,
}

out = os.path.join(ROOT, "sbom.spdx.json")
with open(out, "w") as f:
    json.dump(doc, f, indent=2)
    f.write("\n")
print("wrote", out, f"({len(packages)} packages)")
