#!/usr/bin/env python3
from __future__ import annotations

import datetime as dt
import json
import pathlib
import sys
import urllib.parse

def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_distribution_sbom.py <manifest.json> <output.spdx.json>")

    manifest_path = pathlib.Path(sys.argv[1]).resolve()
    output_path = pathlib.Path(sys.argv[2]).resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    version = manifest["version"]
    commit = manifest["source_commit"]
    package_hash = manifest["package_sha256"]
    namespace = (
        "https://github.com/rastogivaibhav/graphenedb_v1/"
        f"spdx/{urllib.parse.quote(version, safe='')}/{commit}/{package_hash}"
    )

    files = []
    relationships = [
        {
            "spdxElementId": "SPDXRef-DOCUMENT",
            "relationshipType": "DESCRIBES",
            "relatedSpdxElement": "SPDXRef-Package-GrapheneDB",
        }
    ]

    for index, item in enumerate(manifest.get("files", []), start=1):
        spdx_id = f"SPDXRef-File-{index}"
        files.append({
            "fileName": item["path"],
            "SPDXID": spdx_id,
            "checksums": [
                {"algorithm": "SHA256", "checksumValue": item["sha256"]}
            ],
            "licenseConcluded": "NOASSERTION",
            "copyrightText": "NOASSERTION",
        })
        relationships.append({
            "spdxElementId": "SPDXRef-Package-GrapheneDB",
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": spdx_id,
        })

    document = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"GrapheneDB-{version}",
        "documentNamespace": namespace,
        "creationInfo": {
            "created": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
            "creators": ["Tool: GrapheneDB generate_distribution_sbom.py"],
        },
        "packages": [
            {
                "name": "GrapheneDB",
                "SPDXID": "SPDXRef-Package-GrapheneDB",
                "versionInfo": version,
                "downloadLocation": "https://github.com/rastogivaibhav/graphenedb_v1",
                "filesAnalyzed": True,
                "licenseConcluded": "Apache-2.0",
                "licenseDeclared": "Apache-2.0",
                "copyrightText": "Copyright 2026 Vaibhav Rastogi",
                "checksums": [
                    {"algorithm": "SHA256", "checksumValue": package_hash}
                ],
                "externalRefs": [
                    {
                        "referenceCategory": "OTHER",
                        "referenceType": "vcs",
                        "referenceLocator": (
                            "git+https://github.com/rastogivaibhav/graphenedb_v1.git@"
                            + commit
                        ),
                    }
                ],
                "supplier": "Person: Vaibhav Rastogi",
            }
        ],
        "files": files,
        "relationships": relationships,
        "annotations": [
            {
                "annotationType": "OTHER",
                "annotator": "Tool: GrapheneDB generate_distribution_sbom.py",
                "annotationDate": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
                "comment": (
                    "Default GrapheneDB package does not bundle FAISS. Optional "
                    "FAISS linkage is externally supplied and documented in "
                    "THIRD_PARTY_NOTICES.md."
                ),
            }
        ],
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(output_path)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
