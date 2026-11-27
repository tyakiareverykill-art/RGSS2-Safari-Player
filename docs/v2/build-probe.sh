#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

physicsfs_revision=eb3383b532c5f74bfeb42ec306ba2cf80eed988c
build_dir=$(mktemp -d)
trap 'rm -rf -- "$build_dir"' EXIT
git -C "$build_dir" init --quiet
git -C "$build_dir" fetch --quiet --depth 1 https://github.com/icculus/physfs.git "$physicsfs_revision"
git -C "$build_dir" checkout --quiet --detach FETCH_HEAD
src="$build_dir/src"

emcc -O2 io-probe.c zip-probe.c "$src"/*.c -I"$src" --no-entry -lworkerfs.js \
  -DPHYSFS_SUPPORTS_7Z=0 -DPHYSFS_SUPPORTS_GRP=0 -DPHYSFS_SUPPORTS_WAD=0 \
  -DPHYSFS_SUPPORTS_HOG=0 -DPHYSFS_SUPPORTS_MVL=0 -DPHYSFS_SUPPORTS_QPAK=0 \
  -DPHYSFS_SUPPORTS_SLB=0 -DPHYSFS_SUPPORTS_ISO9660=0 -DPHYSFS_SUPPORTS_VDF=0 \
  -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=134217728 \
  -sENVIRONMENT=worker -sMODULARIZE=1 -sEXPORT_NAME=createIOProbe \
  -sEXPORTED_FUNCTIONS=_probe_archive,_probe_zip -sEXPORTED_RUNTIME_METHODS=FS,WORKERFS \
  -o io-probe.js
cp "$build_dir/LICENSE.txt" physicsfs-license.txt
