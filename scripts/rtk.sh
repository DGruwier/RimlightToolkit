#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
COMMAND="${1:-run}"
if [[ $# -gt 0 ]]; then
  shift
fi

BUILD_DIR="${RTK_BUILD_DIR:-"$ROOT/build"}"
CONFIG="${RTK_CONFIG:-Release}"
PREVIEW_ARGS=("$@")

step() {
  printf '[rtk] %s\n' "$*"
}

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'Missing required tool %s. Install it and make sure it is on PATH.\n' "$1" >&2
    exit 1
  fi
}

check_deps() {
  step "checking dependencies"
  require_tool cmake
  step "$(cmake --version | head -n 1)"

  if command -v ninja >/dev/null 2>&1; then
    step "ninja found at $(command -v ninja)"
  else
    step "ninja not found; CMake will use the platform default generator"
  fi

  if command -v c++ >/dev/null 2>&1; then
    step "compiler candidate found at $(command -v c++)"
  elif command -v clang++ >/dev/null 2>&1; then
    step "compiler candidate found at $(command -v clang++)"
  elif command -v g++ >/dev/null 2>&1; then
    step "compiler candidate found at $(command -v g++)"
  else
    step "no compiler found directly on PATH; CMake may still find a platform toolchain"
  fi
}

configure_project() {
  if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    return
  fi

  step "configuring build in $BUILD_DIR"
  args=(-S "$ROOT" -B "$BUILD_DIR" "-DCMAKE_BUILD_TYPE=$CONFIG")
  if command -v ninja >/dev/null 2>&1; then
    args+=(-G Ninja)
  fi
  cmake "${args[@]}"
}

build_project() {
  check_deps
  configure_project
  step "building latest sources"
  cmake --build "$BUILD_DIR" --config "$CONFIG"
}

find_preview_exe() {
  local exe
  exe="$(find "$BUILD_DIR" -type f \( -name 'rtk_preview_cli' -o -name 'rtk_preview_cli.exe' \) -print | sort | tail -n 1)"
  if [[ -z "$exe" ]]; then
    printf 'Preview harness was not found under %s after build.\n' "$BUILD_DIR" >&2
    exit 1
  fi
  printf '%s\n' "$exe"
}

run_preview() {
  build_project
  local exe
  exe="$(find_preview_exe)"
  step "running $exe ${PREVIEW_ARGS[*]}"
  "$exe" "${PREVIEW_ARGS[@]}"
}

run_tests() {
  build_project
  step "running tests"
  ctest --test-dir "$BUILD_DIR" --build-config "$CONFIG" --output-on-failure
}

clean_build() {
  local resolved_build
  resolved_build="$(cd "$(dirname "$BUILD_DIR")" && pwd)/$(basename "$BUILD_DIR")"
  case "$resolved_build" in
    "$ROOT"/*)
      if [[ -d "$resolved_build" ]]; then
        step "removing $resolved_build"
        rm -rf "$resolved_build"
      fi
      ;;
    *)
      printf 'Refusing to remove build dir outside repo: %s\n' "$resolved_build" >&2
      exit 1
      ;;
  esac
}

case "$COMMAND" in
  check) check_deps ;;
  build) build_project ;;
  test) run_tests ;;
  run) run_preview ;;
  clean) clean_build ;;
  *)
    printf 'Usage: scripts/rtk.sh [check|build|test|run|clean] [preview args...]\n' >&2
    exit 2
    ;;
esac
