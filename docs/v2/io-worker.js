const sampleCount = 64;
const sampleBytes = 65536;

function measureFile(file, read) {
  const start = performance.now();
  const length = Math.min(file.size, sampleBytes);
  let checksum = 0;
  for (let index = 0; index < sampleCount; index++) {
    const offset = Math.floor((file.size - length) * index / (sampleCount - 1));
    const bytes = read(offset, length);
    if (bytes.length !== length) throw new Error('部分読み込みのバイト数が不一致');
    checksum += bytes[0] + bytes[length - 1];
  }
  return { checksum, milliseconds: performance.now() - start };
}

function report(name, result) {
  postMessage({ text: name + ' 64回 / ' + result.milliseconds.toFixed(1) +
    ' ms / チェックサム ' + result.checksum });
}

self.onmessage = async ({ data: file }) => {
  try {
    if (!file.size) throw new Error('空のファイルです');
    const reader = new FileReaderSync();
    const baseline = measureFile(file, (offset, length) =>
      new Uint8Array(reader.readAsArrayBuffer(file.slice(offset, offset + length))));
    report('FileReaderSync', baseline);

    try {
      importScripts('./io-probe.js');
      const module = await createIOProbe();
      module.FS.mkdir('/game');
      module.FS.mount(module.WORKERFS, { blobs: [{ name: 'archive', data: file }] }, '/game');
      try {
        const before = module.HEAPU8.buffer.byteLength;
        const start = performance.now();
        const checksum = module._probe_archive();
        report('WASM / WORKERFS', { checksum, milliseconds: performance.now() - start });
        if (checksum !== baseline.checksum) throw new Error('チェックサム不一致');
        postMessage({ text: 'WORKERFS 検証成功 / WASMメモリ ' + before + ' → ' +
          module.HEAPU8.buffer.byteLength + ' バイト' });
      } finally {
        module.FS.unmount('/game');
      }
    } catch (error) {
      postMessage({ text: 'WORKERFS 検証失敗 / ' + error.message });
    }

    try {
      // Use an isolated temporary file to avoid touching V43/V44 storage.
      const root = await navigator.storage.getDirectory();
      const name = 'rgss-v2-probe-' + crypto.randomUUID();
      const handle = await root.getFileHandle(name, { create: true });
      try {
        const access = await handle.createSyncAccessHandle();
        try {
          const expected = new Uint8Array(reader.readAsArrayBuffer(file.slice(0, sampleBytes)));
          const written = access.write(expected, { at: 0 });
          access.flush();
          const actual = new Uint8Array(expected.length);
          const count = access.read(actual, { at: 0 });
          if (written !== expected.length || count !== expected.length ||
              actual.some((byte, index) => byte !== expected[index])) {
            throw new Error('書き込み内容が不一致');
          }
          postMessage({ text: 'OPFS 同期読み書き成功 / ' + count + ' バイト' });
        } finally {
          access.close();
        }
      } finally {
        await root.removeEntry(name);
      }
    } catch (error) {
      postMessage({ text: 'OPFS 検証失敗 / ' + error.message });
    }
  } catch (error) {
    postMessage({ text: error.name + ' / ' + error.message });
  } finally {
    postMessage({ done: true });
  }
};
