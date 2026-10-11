# gen_commit: writes OUT, "#define AB_CORE_COMMIT ..." with the repository's short HEAD and "+dirty"
# when a tracked file differs from it (abacus #4238: VER must not lie). Run at every build by the
# devices' CMake; the file is rewritten only when its text changes, so an unchanged tree rebuilds
# nothing. Where git cannot read the tree, AB_CORE_COMMIT_HOST (scripts/host-commit.sh) is used.   cmake -DREPO=<repo> -DOUT=<header> -P scripts/gen_commit.cmake
execute_process(COMMAND git -C "${REPO}" rev-parse --short HEAD OUTPUT_VARIABLE h
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE rc)
if(rc EQUAL 0 AND h)
    execute_process(COMMAND git -C "${REPO}" status --porcelain --untracked-files=no OUTPUT_VARIABLE d
                    ERROR_QUIET)
    if(d)
        set(h "${h}+dirty")
    endif()
elseif(NOT "$ENV{AB_CORE_COMMIT_HOST}" STREQUAL "")
    # git cannot read this tree here (a worktree's .git points outside the container's mount): the host's answer
    # (scripts/host-commit.sh, passed in by the device scripts; abacus #6997)
    set(h "$ENV{AB_CORE_COMMIT_HOST}")
else()
    set(h "unknown")
endif()
set(text "#define AB_CORE_COMMIT \"${h}\"\n")
set(old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" old)
endif()
if(NOT "${old}" STREQUAL "${text}")
    file(WRITE "${OUT}" "${text}")
endif()
