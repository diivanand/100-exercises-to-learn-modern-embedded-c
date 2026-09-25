#!/usr/bin/env bash
#
# Course integrity check. Run it after
# editing an exercise. It asserts:
#
#   1. Every reference solution compiles and passes. A solution that does not
#      is worse than no solution.
#   2. Every host exercise STARTER fails -- either to compile or to pass. An
#      exercise that already passes teaches nothing, and is the most common
#      way for one of these to rot.
#   3. Every starter that fails to *compile* says so in its header comment, so
#      a learner is never staring at an error the course did not warn them
#      about -- and, conversely, no header promises a compile error that the
#      starter does not produce.
#   4. If arm-none-eabi-gcc is on the PATH, the NUCLEO chapters (15-17) are
#      cross-COMPILED, starters and solutions both. They cannot be run here
#      (that needs the board), but compiling catches the large majority of
#      mistakes. Starters in these chapters are exempt from "must fail" -- a
#      wrong register write cannot be observed without the hardware.
#
# Usage: scripts/check-course.sh

set -uo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/cmake-build-check"
TARGET_BUILD="${ROOT}/cmake-build-check-target"
CC_HOST="${MEC_CC:-clang}"

readonly RED=$'\033[31m' GREEN=$'\033[32m' YELLOW=$'\033[33m' BOLD=$'\033[1m'
readonly RESET=$'\033[0m'

# macOS has no coreutils `timeout`. Some exercise starters loop for ever on
# purpose (a poll whose flag the optimiser hoisted), so every starter is run
# with a deadline. A timeout counts as a failure, which is exactly what it is.
run_with_deadline() {
  local seconds="$1"
  shift
  perl -e 'alarm shift; exec @ARGV or exit 127' "${seconds}" "$@"
}

failures=0
note() { printf '  %sFAIL%s %s\n' "${RED}" "${RESET}" "$*"; failures=$((failures + 1)); }
ok() { printf '  %s ok %s %s\n' "${GREEN}" "${RESET}" "$*"; }

is_target_chapter() {
  # exercises/15_bare_metal/01_x -> chapter number 15 -> target track
  local id="$1"
  [[ $((10#${id%%_*})) -ge 15 ]]
}

printf '%s==> configuring (host)%s\n' "${BOLD}" "${RESET}"
# Always reconfigure. CMake is idempotent here, and reusing a directory that
# was configured with different options is how this script starts reporting
# failures that are really its own.
cmake -S "${ROOT}" -B "${BUILD}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER="${CC_HOST}" \
  -DMEC_BUILD_SOLUTIONS=ON -DMEC_BUILD_EXERCISES=ON >/dev/null || exit 1

printf '\n%s==> solutions must build and pass%s\n' "${BOLD}" "${RESET}"
if ! cmake --build "${BUILD}" --target solutions -- -k 0 >/dev/null 2>&1; then
  printf '%s\n' "$(cmake --build "${BUILD}" --target solutions -- -k 0 2>&1 |
    grep -E '^FAILED' | sed 's/.*dir\//    /')"
  note "one or more solutions do not compile"
fi
if ctest --test-dir "${BUILD}" -R '^solution/' --output-on-failure >/tmp/mec-sol.log 2>&1; then
  ok "all solutions pass"
else
  grep -E '^\s+[0-9]+ - ' /tmp/mec-sol.log | sed 's/^/    /'
  note "some solutions fail their own tests"
fi

printf '\n%s==> exercise starters must NOT pass%s\n' "${BOLD}" "${RESET}"
# Remove the previously linked binaries first: a starter that used to compile
# and no longer does would otherwise be judged by a stale executable.
rm -rf "${BUILD}/bin/exercises"
cmake --build "${BUILD}" --target exercises -- -k 0 >/tmp/mec-ex-build.log 2>&1

uncompilable=()
while IFS= read -r line; do
  # FAILED: exercises/CMakeFiles/ex_03_06_function_pointers.dir/...
  if [[ "${line}" =~ ex_([0-9]{2}_[0-9]{2}_[a-z0-9_]+)\.dir ]]; then
    uncompilable+=("${BASH_REMATCH[1]}")
  fi
done < <(grep -E '^FAILED' /tmp/mec-ex-build.log)

passing=()
while IFS= read -r dir; do
  chapter="$(basename "$(dirname "${dir}")")"
  name="$(basename "${dir}")"
  id="${chapter%%_*}_${name}"
  is_target_chapter "${id}" && continue
  binary="${BUILD}/bin/exercises/${chapter}/ex_${id}"
  [[ -x "${binary}" ]] || continue
  # Many starters crash rather than merely failing a check (reading past the
  # end of a buffer, dereferencing NULL). A crash is a failure; run the
  # binary in a subshell so the shell does not narrate the signal.
  if (run_with_deadline 20 "${binary}" >/dev/null 2>&1); then
    passing+=("${id}")
  fi
done < <(find "${ROOT}/exercises" -mindepth 2 -maxdepth 2 -type d | sort)

if [[ ${#passing[@]} -eq 0 ]]; then
  ok "every starter fails, as it should"
else
  for id in "${passing[@]}"; do
    note "starter ${id} already passes -- there is nothing to do in it"
  done
fi

printf '\n%s==> starters that do not compile must say so%s\n' "${BOLD}" "${RESET}"
if [[ ${#uncompilable[@]} -eq 0 ]]; then
  ok "no starter needs a note"
else
  for id in "${uncompilable[@]}"; do
    chapter_number="${id%%_*}"
    rest="${id#*_}"
    dir="$(find "${ROOT}/exercises/${chapter_number}"_* -maxdepth 1 -type d \
      -name "${rest}" 2>/dev/null | head -1)"
    if [[ -n "${dir}" ]] && grep -qh '^//  NOTE' "${dir}/exercise.c" 2>/dev/null; then
      ok "${id} (documented)"
    else
      note "${id} does not compile and has no NOTE in its header comment"
    fi
  done
fi

printf '\n%s==> starters that say they do not compile must not compile%s\n' "${BOLD}" "${RESET}"
# The converse of the previous check. A header that promises a compile error
# when the starter in fact builds and fails at run time sends the learner
# looking for a diagnostic that is not there.
lying=0
while IFS= read -r dir; do
  chapter="$(basename "$(dirname "${dir}")")"
  name="$(basename "${dir}")"
  id="${chapter%%_*}_${name}"
  is_target_chapter "${id}" && continue
  grep -qhiE '^//  NOTE.*(compile error|does not compile|not compile until)' \
    "${dir}/exercise.c" 2>/dev/null || continue
  if [[ " ${uncompilable[*]-} " != *" ${id} "* ]]; then
    note "${id} says it starts as a compile error, but it compiles"
    lying=$((lying + 1))
  fi
done < <(find "${ROOT}/exercises" -mindepth 2 -maxdepth 2 -type d | sort)
if [[ ${lying} -eq 0 ]]; then
  ok "every NOTE is truthful"
fi

if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  printf '\n%s==> NUCLEO chapters must cross-compile%s\n' "${BOLD}" "${RESET}"
  cmake -S "${ROOT}" -B "${TARGET_BUILD}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE="${ROOT}/cmake/arm-none-eabi.cmake" \
    -DMEC_BUILD_SOLUTIONS=ON -DMEC_BUILD_EXERCISES=ON >/dev/null || exit 1
  if cmake --build "${TARGET_BUILD}" --target solutions -- -k 0 >/tmp/mec-t-sol.log 2>&1; then
    ok "all NUCLEO solutions compile"
  else
    grep -E '^FAILED' /tmp/mec-t-sol.log | sed 's/^/    /'
    note "some NUCLEO solutions do not compile"
  fi
  cmake --build "${TARGET_BUILD}" --target exercises -- -k 0 >/tmp/mec-t-ex.log 2>&1
  target_uncompilable=()
  while IFS= read -r line; do
    if [[ "${line}" =~ ex_([0-9]{2}_[0-9]{2}_[a-z0-9_]+)\.dir ]]; then
      target_uncompilable+=("${BASH_REMATCH[1]}")
    fi
  done < <(grep -E '^FAILED' /tmp/mec-t-ex.log)
  bad=0
  for id in "${target_uncompilable[@]-}"; do
    [[ -z "${id}" ]] && continue
    chapter_number="${id%%_*}"
    rest="${id#*_}"
    dir="$(find "${ROOT}/exercises/${chapter_number}"_* -maxdepth 1 -type d \
      -name "${rest}" 2>/dev/null | head -1)"
    if [[ -z "${dir}" ]] || ! grep -qh '^//  NOTE' "${dir}/exercise.c" 2>/dev/null; then
      note "NUCLEO starter ${id} does not compile and has no NOTE"
      bad=1
    fi
  done
  [[ ${bad} -eq 0 ]] && ok "NUCLEO starters compile (or say why not)"
else
  printf '\n  %s(NUCLEO chapters skipped: no arm-none-eabi-gcc on this machine)%s\n' \
    "${YELLOW}" "${RESET}"
fi

printf '\n%s==> counting%s\n' "${BOLD}" "${RESET}"
count="$(find "${ROOT}/exercises" -mindepth 2 -maxdepth 2 -type d | wc -l | tr -d ' ')"
solution_count="$(find "${ROOT}/solutions" -mindepth 2 -maxdepth 2 -type d | wc -l | tr -d ' ')"
printf '  %s exercises, %s solutions\n' "${count}" "${solution_count}"
if [[ "${count}" != "${solution_count}" ]]; then
  note "every exercise needs a solution"
fi

printf '\n'
if [[ ${failures} -eq 0 ]]; then
  printf '%s%sthe course is consistent%s\n' "${GREEN}" "${BOLD}" "${RESET}"
  exit 0
fi
printf '%s%s%d problem(s)%s\n' "${RED}" "${BOLD}" "${failures}" "${RESET}"
exit 1
