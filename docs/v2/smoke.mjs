const canvas = document.querySelector('canvas');
const start = document.querySelector('#start');
const status = document.querySelector('#status');
const log = document.querySelector('#log');
const lines = [];
let runner;

function report(line) {
  lines.push(String(line).slice(0, 1024));
  if (lines.length > 100) lines.shift();
  log.textContent = lines.join('\n');
  if (String(line).includes('SMOKE: frame 120')) {
    status.textContent = 'RGSS2の描画が120フレームに到達しました。';
  } else if (String(line).includes('SMOKE: save')) {
    status.textContent = '位置を保存しました。';
  } else if (String(line).includes('SMOKE: load')) {
    status.textContent = '保存した位置に戻しました。';
  }
}

// Nostalgist loads the core from a Blob URL. Emscripten also resolves
// relative worker paths, which need the original core directory.
const coreBase = new URL('../mkxpz/cores/', location.href);
const NativeURL = window.URL;
window.URL = new Proxy(NativeURL, {
  construct(target, args, newTarget) {
    try {
      return Reflect.construct(target, args, newTarget);
    } catch (error) {
      const [name, base] = args;
      if (String(base).startsWith('blob:' + location.origin + '/') &&
          (name === '.' || /^mkxp[-_]z_libretro\.(js|wasm)$/.test(name))) {
        return new NativeURL(name.replace('mkxp-z', 'mkxp_z'), coreBase);
      }
      throw error;
    }
  },
});

start.onclick = async () => {
  start.disabled = true;
  const watchdog = setTimeout(() => {
    if (!lines.some(line => line.includes('SMOKE: frame 120'))) {
      report('120フレームに未到達 / ' + status.textContent);
    }
  }, 60000);
  try {
    if (!crossOriginIsolated) {
      status.textContent = 'Safariの起動環境を準備しています。再読み込み後にもう一度押してください。';
      await navigator.serviceWorker.register('./coi-serviceworker.js');
      await navigator.serviceWorker.ready;
      location.reload();
      return;
    }
    status.textContent = 'mkxp-zコアを起動しています…';
    const [{ Nostalgist }, response] = await Promise.all([
      import('./nostalgist.mjs'),
      fetch('./rgss2-smoke.mkxpz'),
    ]);
    if (!response.ok) throw new Error('試験データ HTTP ' + response.status);
    runner = await Nostalgist.prepare({
      element: canvas,
      core: {
        name: 'mkxp-z',
        js: new URL('mkxp_z_libretro.js', coreBase).href,
        wasm: new URL('mkxp_z_libretro.wasm', coreBase).href,
      },
      rom: { fileName: 'rgss2-smoke.mkxpz', fileContent: await response.blob() },
      emscriptenModule: { print: report, printErr: report },
      retroarchCoreConfig: { 'mkxp-z_rgssVersion': 'inherit', 'mkxp-z_threadedAudio': 'disabled' },
      retroarchConfig: { audio_enable: false, audio_mute_enable: true, log_verbosity: true },
    });
    await runner.start();
    canvas.focus();
  } catch (error) {
    clearTimeout(watchdog);
    report(error);
    status.textContent = '起動できませんでした。実行ログを確認してください。';
    start.disabled = false;
  }
};

for (const button of document.querySelectorAll('[data-button]')) {
  const input = button.dataset.button;
  button.onpointerdown = event => {
    if (!runner) return;
    event.preventDefault();
    button.setPointerCapture(event.pointerId);
    runner.pressDown(input);
  };
  button.onlostpointercapture = () => runner?.pressUp(input);
}
