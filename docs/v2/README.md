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

## 検証記録

- Chromium 151 で 371,225,018 バイトの人工ファイルと 1 バイトの境界ケースを検証。
  大容量ケースは 64 回（計 4 MiB）のサンプルが一致し、WASM 線形メモリは
  16,973,824 バイトのままだった。OPFS の 64 KiB 往復照合、ログの再読み込みも成功。
  アーカイブの展開、ゲーム実行、iPhone Safari でのメモリ使用量は未検証。
- 公開中の Libretro JS に WasmFS、OPFS 同期アクセス、AudioWorklet の実装を確認。
  WORKERFS の名前は見つからなかった。別ビルドで検証した今回の WORKERFS を
  既存コアでそのまま使える根拠はない。
- V40/V41 の最終ログは、RetroArch の `platform_emscripten_mount_filesystems`
  で `wasmfs_create_opfs_backend()` の直前に出力される。同じ関数には親ディレクトリ
  作成と `wasmfs_create_directory()` もあり、既存ログから停止した呼び出しは特定できない。
  このコアはフルパスを要求し、mkxp-z は PhysicsFS でそのパスを開く。
- ArkRPG の [公式サイト](https://arkrpg.tech/) は主に MV/MZ の WKWebView/WebGL を説明。
  XP/VX/VX Ace の mkios は App Store で確認した。App Store の変更履歴にはマップ遷移時の
  メモリ増加抑制、ZIP の文字コード判定、フォントとスクリプト互換性の改善が記載されている。
  外伝で使われる処理や修正内容への対応関係は公開情報から判定できない。
- 独立 Web 移植候補のローカルビルドは Emscripten 起動と依存ソース取得まで進んだ。
  libsigc++ の構成段階で `mm-common-prepare` が見つからず停止した。
  既存レシピは Docker と Ruby/Rake を使い、今回ビルドできた WASM は I/O 診断のみ。

## 最小 RGSS2 ゲーム

```sh
python3 docs/v2/make-rgss2-smoke.py rgss2-smoke.mkxpz
```

Chromium の隔離されたローカル環境で公開 V44 と配布コアを実行し、
作成したゲームの描画と方向キーによる移動を確認した。V44 の起動画面の
RTP チェックを満たすため、自作の透明画像 1 枚を含むテスト用 ZIP を選択した。
ゲーム自体には RTP 参照がない。ゲーム内のセーブ・ロードで保存位置が復元し、V44 の端末保存ログにテスト用セーブが記録された。実機、ブラウザ再起動後のセーブ復元、BGM は未検証。

実行ログ上の CRuby は 3.3.10 (`wasm32-wasip2`)。RetroArch の画面スリープ防止リクエストから
`NotAllowedError: Wake Lock permission request denied` が出た試行でも描画は開始した。
この API を無効にしたローカル試行では同じ警告が消え、描画とキーボード操作が通った。
外伝の Safari ログには呼び出し元がなく、同じ例外かどうかは未確認。

## PhysicsFS の ZIP 検証

PhysicsFS 3.2.0 を固定コミットからビルドし、Worker 内の WORKERFS から
ZIP をマウントする。ルートまたは直下のフォルダの Game.ini を確認し、
`[Game]` の `Scripts` が指すファイルを分割して読み、Marshal 4.8 の
ヘッダーを確認する。対応範囲は ZIP に直接入っているスクリプト。
Game.rgss2a の中のスクリプトと Ruby の実行はこの診断の対象外。

Chromium 151 で STORE のルート配置、DEFLATE の日本語フォルダ内配置、
および不正ファイルのエラー表示を確認した。実機の元ZIPでの PhysicsFS
読み込みは未検証。WORKERFS と OPFS の診断は引き続き個別に出力する。
