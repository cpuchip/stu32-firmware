#!/bin/sh
# host-commit: the repository's short HEAD, "+dirty" when a tracked file differs, as gen_commit.cmake writes it; the
# device scripts run it on the host and pass it into the build container as AB_CORE_COMMIT_HOST, for a worktree whose
# .git points outside the container's mount (abacus #6997: package.sh's images read CORE "unknown").
#   sh scripts/host-commit.sh <repo>
h=$(git -C "${1:-.}" rev-parse --short HEAD 2>/dev/null) || exit 0
[ -z "$(git -C "${1:-.}" status --porcelain --untracked-files=no 2>/dev/null)" ] || h="$h+dirty"
echo "$h"
