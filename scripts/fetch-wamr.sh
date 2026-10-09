#!/bin/sh
# WAMR (github.com/wasm-micro-runtime/wasm-micro-runtime, Apache-2.0 WITH LLVM-exception) for STU_WASM:
# third_party/wamr/PIN's commit, exported from a local clone (WAMR_REPO, default ../wasm-micro-runtime) with
# the patches of third_party/wamr/patches applied in order, into build/wamr. Each patch must match PIN's
# sha256 and the result must be PIN's tree, or nothing is written. The commit must be on the clone's origin.
# The work is done in a private object store, so the build never writes into that clone.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PIN=$ROOT/third_party/wamr/PIN
REPO=${WAMR_REPO:-$ROOT/../wasm-micro-runtime}
OUT=$ROOT/build/wamr
COMMIT=$(awk '$1 == "commit" {print $2}' "$PIN")
TREE=$(awk '$1 == "tree" {print $2}' "$PIN")
if [ -f "$OUT/.pin" ] && [ "$(cat "$OUT/.pin")" = "$TREE" ]; then
    exit 0
fi
case "$COMMIT$TREE" in *[!0-9a-f]*|"") echo "fetch-wamr: PIN has no commit or tree" >&2; exit 1;; esac
HEAD=$(git -C "$REPO" symbolic-ref -q refs/remotes/origin/HEAD) || { echo "fetch-wamr: $REPO has no origin/HEAD" >&2; exit 1; }
git -C "$REPO" merge-base --is-ancestor "$COMMIT" "$HEAD" || { echo "fetch-wamr: $COMMIT is not on $HEAD" >&2; exit 1; }
awk '$1 == "patch" {print $2, $3}' "$PIN" | while read -r name sum; do
    got=$(sha256sum "$ROOT/third_party/wamr/patches/$name" | cut -c1-64)
    [ "$got" = "$sum" ] || { echo "fetch-wamr: $name is $got, PIN says $sum" >&2; exit 1; }
done
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
GITDIR=$(git -C "$REPO" rev-parse --absolute-git-dir)
export GIT_DIR="$GITDIR" GIT_INDEX_FILE="$TMP/index" GIT_OBJECT_DIRECTORY="$TMP/objects"
export GIT_ALTERNATE_OBJECT_DIRECTORIES="$GITDIR/objects"
mkdir -p "$TMP/objects"
git read-tree "$COMMIT"
awk '$1 == "patch" {print $2}' "$PIN" | while read -r name; do
    git apply --cached "$ROOT/third_party/wamr/patches/$name" || { echo "fetch-wamr: $name does not apply" >&2; exit 1; }
done
GOT=$(git write-tree)
[ "$GOT" = "$TREE" ] || { echo "fetch-wamr: the patched tree is $GOT, PIN says $TREE" >&2; exit 1; }
rm -rf "$OUT"
mkdir -p "$OUT"
git checkout-index -a --prefix="$OUT/"
echo "$TREE" > "$OUT/.pin"
echo "wamr $COMMIT + $(grep -c '^patch' "$PIN") patches = tree $TREE"
