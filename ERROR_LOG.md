# エラー記録
## 過去の報告（会話由来、今回再検証なし）
- Scripts bytes=0 → PhysFS読み込み修正後に698727 bytesとの報告。
- RPG::SE / RPG::AudioFile / RPG::MapInfo 未定義 → 互換クラスとRuby定義埋め込み後に先へ進んだとの報告。
- Graphics/System/Window.png → Safari PNG decode成功、SDL_image PNG対応後に次のエラーへ進んだとの報告。
- スクリプト101 書式指定: undefined method 'dispose' (NoMethodError)。レシーバ未特定。
- 外部RetroArch mkxp-z: Runが読み込み継続。原因未特定。
## 新規記録の形式
日付 / コミット / 環境 / 再現手順 / 生ログ / 仮説 / 確認済み事項 / 次の作業。
