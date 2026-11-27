import { WASI, File, OpenFile, PreopenDirectory, ConsoleStdout } from './ruby/shim/dist/index.js';

export async function checkScripts(bytes, log) {
  async function fetchAsset(path) {
    const response = await fetch(new URL(path, import.meta.url));
    if (!response.ok) throw new Error(path + ' HTTP ' + response.status);
    return response;
  }
  const [wasm, script] = await Promise.all([
    fetchAsset('./ruby/ruby.wasm').then(response => WebAssembly.compileStreaming(response)),
    fetchAsset('./check-scripts.rb').then(response => response.text()),
  ]);
  const wasi = new WASI(['ruby', '--disable-gems', 'check-scripts.rb'], [], [
    new OpenFile(new File(bytes, { readonly: true })),
    ConsoleStdout.lineBuffered(log), ConsoleStdout.lineBuffered(log),
    new PreopenDirectory('.', [['check-scripts.rb', new File(new TextEncoder().encode(script), { readonly: true })]]),
  ], { debug: false });
  const instance = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: wasi.wasiImport });
  const status = wasi.start(instance);
  if (status !== 0) throw new Error('CRuby 検証終了コード ' + status);
}
