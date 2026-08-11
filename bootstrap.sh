#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="${PYTHON_BIN:-python3}"

if [[ -x "${ROOT_DIR}/../.venv/bin/python" ]]; then
    PYTHON_BIN="${ROOT_DIR}/../.venv/bin/python"
elif [[ -x "${ROOT_DIR}/.venv/bin/python" ]]; then
    PYTHON_BIN="${ROOT_DIR}/.venv/bin/python"
fi

if [[ ! -d "${ROOT_DIR}/dependencies/lvgl/.git" || \
      ! -d "${ROOT_DIR}/dependencies/spdlog/.git" || \
      ! -d "${ROOT_DIR}/dependencies/smooth_ui_toolkit/.git" || \
      ! -d "${ROOT_DIR}/dependencies/miniaudio/.git" || \
      ! -d "${ROOT_DIR}/dependencies/TinySoundFont/.git" || \
      ! -f "${ROOT_DIR}/dependencies/TinySoundFont/tsf.h" ]]; then
    "${PYTHON_BIN}" "${ROOT_DIR}/fetch_repos.py"
fi

echo "Piano bootstrap complete."
echo "Build with:"
echo "  cmake -S . -B build/sdl -DPIANO_USE_SDL=ON"
echo "  cmake --build build/sdl -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
