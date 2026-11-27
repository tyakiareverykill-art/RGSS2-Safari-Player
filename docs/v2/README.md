# RGSS Safari Player V2

このディレクトリには独立したブラウザ I/O 診断を置く。WASM モジュールは
WORKERFS 上のゲームアーカイブを C のファイル API で部分読み込みし、
FileReaderSync の同一サンプルと照合する。OPFS は別の一時ファイルで
同期読み書きと内容照合を行う。ゲームの実行や既存セーブへのアクセスは行わない。

## ビルド

Emscripten 3.1.61 を有効化し、リポジトリのルートから実行する。

```sh
bash docs/v2/build-probe.sh
python3 -m http.server 8000 --directory docs
```

`http://localhost:8000/v2/` の診断で ZIP または MKXPZ を選択する。
WASM の表示値は線形メモリ容量。Safari プロセス全体の使用量は含まない。

## 上流の確認

- GitHub の main は V44 (`8f4f571`) へ更新済み。V43 (`b7a81fd`) は履歴にある。
  V44 の Preload は Bitmap の初期化と Audio の play 呼び出しを記録する。
- 現行 Pages の定義は [zjl4/mkxp-z-libretro-emscripten v0.1.0](https://github.com/zjl4/mkxp-z-libretro-emscripten/tree/v0.1.0) からコアを取得する。
  タグのビルド定義は Emscripten 4.0.8、RetroArch スレッド、AudioWorklet、WasmFS を使用する。
  V40/V41 で停止した OPFS マウントの原因は未特定。今回の小容量 OPFS 同期テストとは別経路。
- [ArkRPG](https://apps.apple.com/jp/app/arkrpg-rpg-maker-game-player/id6757788340) の公開説明には
  mkios、Metal、AVFoundation/CoreAudio、Ruby 1.8・1.9・3.1 が記載されている。
  ZIP の読み込み実装、キャッシュ、イベント停止回避の詳細は公開説明から確認できない。
- [mkxp-z Apple mobile](https://github.com/rep0mancer/mkxp-z-apple-mobile) は公開された別の候補。
  Ruby の世代別ビルド、セッション管理、ANGLE、FluidSynth を確認。
  Apple SDK に依存する描画と音声は Web 向けの実装が必要。
  個別ファイルの GPLv3 表記と ADDITIONAL-TERMS.md は再利用前に確認する。
- [mkxp-z](https://github.com/mkxp-z/mkxp-z) は CRuby ベースの候補。
  [mkxp-web-jspi](https://github.com/Dim145/mkxp-web-jspi) は SDL・PhysicsFS を使う
  既存の独立 Web 移植例。mruby を使用するため、V43/V44 の CRuby の互換性を引き継げるかは未検証。

外伝の停止箇所の特定には、同一セーブからの再現とゲーム側の進行指標が必要。
画面の requestAnimationFrame と WASM の I/O 結果だけではイベント実行や停止原因は判定できない。
