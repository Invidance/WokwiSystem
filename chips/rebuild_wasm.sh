#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
wokwi-cli chip compile bmp280.chip.c -o bmp280.chip.wasm
