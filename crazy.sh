#!/usr/bin/env bash


git diff --stat
echo
git diff --numstat | awk '
{
  added += $1
  removed += $2
}
END {
  printf "Total lines added:   %d\n", added
  printf "Total lines removed: %d\n", removed
  printf "Net change:          %+d\n", added - removed
}'
