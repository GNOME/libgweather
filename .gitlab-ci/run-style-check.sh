#!/bin/bash

set -e

ancestor_horizon=28  # days (4 weeks)

# Wrap everything in a subshell so we can propagate the exit status.
(

source .gitlab-ci/search-common-ancestor.sh

git diff -U0 --no-color "${newest_common_ancestor_sha}" libgweather/*.c libgweather/tests/*.c libgweather/tools/*.c | clang-format-diff -p1 > format-diff.log

)
exit_status=$?

format_diff="$(<format-diff.log)"

if [ -n "${format_diff}" ]; then
  echo \`\`\`diff > format-diff.log
  echo "${format_diff}" >> format-diff.log
  echo \`\`\` >> format-diff.log
  exit 1
  #[ -z "$CI_MERGE_REQUEST_IID" ] && curl \
  #  --request POST \
  #  --header "Private-Token: $FORMAT_PRIVATE_TOKEN" \
  #  -d @format-diff.log \
  #  https://gitlab.your-server.com/api/v4/projects/$CI_PROJECT_ID/merge_requests/$CI_MERGE_REQUEST_IID/notes \
  #  --insecure
fi

exit ${exit_status}
