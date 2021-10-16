#!/bin/bash

set -e

ancestor_horizon=28  # days (4 weeks)

# Wrap everything in a subshell so we can propagate the exit status.
(

source .gitlab-ci/search-common-ancestor.sh

git diff -U0 --no-color "${newest_common_ancestor_sha}" libgweather/*.c libgweather/tests/*.c libgweather/tools/*.c | clang-format-diff -p1

)
exit_status=$?

exit ${exit_status}
