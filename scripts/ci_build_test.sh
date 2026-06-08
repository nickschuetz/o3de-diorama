#!/usr/bin/env bash
#
# Build the Diorama gem through a host O3DE project and run its unit tests.
#
# Building Diorama requires the full O3DE SDK (multi-GB) plus a host project to
# build through, which GitHub-hosted runners do not have. This script is meant
# for a self-hosted runner (or a developer machine) that already has the engine
# and a project configured. It is deliberately path-agnostic: all locations come
# from environment variables so it is not tied to one machine.
#
# Required environment:
#   O3DE_ENGINE_PATH   Path to the O3DE engine/SDK (has scripts/o3de.sh).
#   DIORAMA_PROJECT    Path to a host project to build the gem through.
#   GEM_PATH           Path to this gem checkout (defaults to the repo root).
#
# Optional:
#   BUILD_CONFIG       profile (default) | debug | release.
#   BUILD_DIR          Build tree (default: $DIORAMA_PROJECT/build/linux).
#   TEST_FILTER        gtest filter (default: all Diorama tests).
#
# Exit status is non-zero if configure, build, or any test fails.

set -euo pipefail

GEM_PATH="${GEM_PATH:-$(cd "$(dirname "$0")/.." && pwd)}"
BUILD_CONFIG="${BUILD_CONFIG:-profile}"

fail() {
    echo "ci_build_test: $1" >&2
    exit 1
}

[ -n "${O3DE_ENGINE_PATH:-}" ] || fail "O3DE_ENGINE_PATH is not set"
[ -n "${DIORAMA_PROJECT:-}" ] || fail "DIORAMA_PROJECT is not set"
[ -x "$O3DE_ENGINE_PATH/scripts/o3de.sh" ] || fail "no scripts/o3de.sh under O3DE_ENGINE_PATH ($O3DE_ENGINE_PATH)"
[ -f "$DIORAMA_PROJECT/project.json" ] || fail "DIORAMA_PROJECT has no project.json ($DIORAMA_PROJECT)"

BUILD_DIR="${BUILD_DIR:-$DIORAMA_PROJECT/build/linux}"
O3DE="$O3DE_ENGINE_PATH/scripts/o3de.sh"

echo "== Diorama CI build/test =="
echo "  engine : $O3DE_ENGINE_PATH"
echo "  project: $DIORAMA_PROJECT"
echo "  gem    : $GEM_PATH"
echo "  config : $BUILD_CONFIG"
echo "  build  : $BUILD_DIR"

# Register the gem as an external subdirectory so the project can find it.
# Idempotent: re-registering an existing path is a no-op.
echo "== register + enable gem =="
# Register makes the gem discoverable; enable adds it to the project so its
# targets (Diorama, Diorama.Editor, Diorama.Tests) are generated at configure.
# Registering alone is not enough on a project that does not already enable the
# gem (e.g. a fresh host project). Both are idempotent.
"$O3DE" register --external-subdirectory "$GEM_PATH"
"$O3DE" enable-gem --gem-path "$GEM_PATH" --project-path "$DIORAMA_PROJECT"

# Configure. Ninja Multi-Config matches the engine's expected generator; tests
# are gated by PAL_TRAIT_DIORAMA_TEST_SUPPORTED, which is TRUE on Linux.
echo "== configure =="
cmake -B "$BUILD_DIR" -S "$DIORAMA_PROJECT" -G "Ninja Multi-Config" \
    -DLY_UNITY_BUILD=ON

# Build the gem, editor module, and test library.
echo "== build =="
cmake --build "$BUILD_DIR" --config "$BUILD_CONFIG" \
    --target Diorama Diorama.Editor Diorama.Tests -j

# Run the unit tests directly through AzTestRunner so the result is explicit and
# does not depend on ctest discovery. The test library lands in the project's
# bin directory for the chosen config.
TEST_LIB="$BUILD_DIR/bin/$BUILD_CONFIG/libDiorama.Tests.so"
[ -f "$TEST_LIB" ] || fail "test library not found after build: $TEST_LIB"

RUNNER="$O3DE_ENGINE_PATH/bin/Linux/$BUILD_CONFIG/Default/AzTestRunner"
[ -x "$RUNNER" ] || fail "AzTestRunner not found at $RUNNER"

echo "== run tests =="
"$RUNNER" "$(readlink -f "$TEST_LIB")" AzRunUnitTests \
    --gtest_filter="${TEST_FILTER:-*}"

echo "== all tests passed =="
