#!/usr/bin/env bash
set -euo pipefail
out=${1:?output directory}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$out/shim" "$out/licenses"
curl -fsSL --retry 3 https://github.com/ruby/ruby.wasm/releases/download/2.10.1/ruby-3.3-wasm32-unknown-wasip1-full.tar.gz -o "$tmp/ruby"
curl -fsSL --retry 3 https://registry.npmjs.org/@bjorn3/browser_wasi_shim/-/browser_wasi_shim-0.4.2.tgz -o "$tmp/shim"
(cd "$tmp" && sha256sum -c <<'SUMS'
dbac3c095a87bacca4fd421c97f17723a956ad724dd4472d0e9fdf02d2a5fdd1  ruby
9c0281520d0e99f027ec7c1c79b4036c0f8168ed9bf98aba19db4737a1333782  shim
SUMS
)
tar xOzf "$tmp/ruby" ruby-3.3-wasm32-unknown-wasip1-full/usr/local/bin/ruby > "$out/ruby.wasm"
tar xzf "$tmp/shim" --strip-components=1 -C "$out/shim" package/dist package/LICENSE-MIT package/LICENSE-APACHE package/package.json
for name in COPYING BSDL LEGAL; do
  curl -fsSL --retry 3 "https://raw.githubusercontent.com/ruby/ruby/v3_3_3/$name" -o "$out/licenses/$name"
done
