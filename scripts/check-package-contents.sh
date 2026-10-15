#!/usr/bin/env bash
# AI/LLM-generated file. Do not treat as human-written source documentation.
# This comment must be preserved in future generated files.
#
# Verify that every shipped asset reached the package (issue #307).
#
# The packaging lane used to list the files it expected one `grep -q` at a time,
# so adding an asset meant remembering to add a line there - and if you forgot,
# the package shipped without it and CI stayed green. That is the silent-omission
# class of #103/#126/#283. Here both sides are derived: the repo tree supplies the
# expected files, the tarball supplies the actual ones.
#
#   cmake --build build --target package
#   ./scripts/check-package-contents.sh build/FOSSynd-*-Linux.tar.gz
#
# Exit codes: 0 complete, 1 an expected file is missing, 2 usage error.
set -euo pipefail

TARBALL="${1:-}"

if [ -z "${TARBALL}" ]; then
    echo "package check: no tarball given" >&2
    echo "usage: $0 <path-to-linux-tarball>" >&2
    exit 2
fi

# Resolve both the tarball and the repo root to absolute paths before changing
# directory, so the script works regardless of the caller's working directory.
case "${TARBALL}" in
    /*) ;;
    *) TARBALL="$(pwd)/${TARBALL}" ;;
esac
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

if [ ! -f "${TARBALL}" ]; then
    echo "package check: tarball not found: ${TARBALL}" >&2
    exit 2
fi

# Strip the leading `FOSSynd-<version>-Linux/` so the paths below are the ones
# seen after `tar x` into a prefix. `set -o pipefail` makes a corrupt tarball
# (tar fails) fail the script rather than pass an empty listing.
LISTING="$(tar tzf "${TARBALL}" | sed 's#^[^/]*/##')"

MISSING=0

# require <repo path> <path inside the package>
require() {
    if echo "${LISTING}" | grep -qxF "$2"; then
        return 0
    fi
    echo "package check FAILED: $1 is not in the package (expected $2)" >&2
    MISSING=1
}

require bin/FOSSynd bin/FOSSynd

# Every mission, world, and locale in the tree must be in the package. Globs
# expand from the checkout, so a file added to the tree is required automatically.
for file in assets/data/missions/*.json; do
    require "${file}" "share/fossynd/missions/$(basename "${file}")"
done
for file in assets/data/world/*.json; do
    require "${file}" "share/fossynd/world/$(basename "${file}")"
done
for file in assets/lang/*.json; do
    require "${file}" "share/fossynd/lang/$(basename "${file}")"
done

require assets/data/weapons.json share/fossynd/weapons.json
require assets/data/agents.json share/fossynd/agents.json
require assets/data/fossynd.conf.example share/fossynd/fossynd.conf.example
require LICENSE share/doc/fossynd/LICENSE
require README.md share/doc/fossynd/README.md

if [ "${MISSING}" -ne 0 ]; then
    echo "A file that is not installed is silently lost from the package." >&2
    echo "Check the install() rules in src/CMakeLists.txt." >&2
    exit 1
fi

echo "package check OK: every shipped asset is in ${TARBALL}"
