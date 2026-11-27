#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
emcc -O2 io-probe.c --no-entry -lworkerfs.js \
  -sENVIRONMENT=worker -sMODULARIZE=1 -sEXPORT_NAME=createIOProbe \
  -sEXPORTED_FUNCTIONS=_probe_archive -sEXPORTED_RUNTIME_METHODS=FS,WORKERFS \
  -o io-probe.js
