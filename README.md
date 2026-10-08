# 立体四目

4×4×4の盤に石を落として四つ並べる、重力ありの立体四目並べ。
最強AIとの対戦と二人対戦ができる対戦環境と、このゲームの完全解析を目指すプロジェクトです。

## 遊び方

`index.html` をブラウザで開くだけで遊べます(ビルド不要、1ファイル完結)。

- 3D盤面: ドラッグで回転、ピンチ/ホイールで拡大縮小。棒をタップするとその棒に玉が落ちます。
- AI: 初級・中級・上級・最強の4段階(αβ探索、上級以上は定石つき)と、試験中の MCTS AI。二人対局もできます。
- 見取り図(平面図)、待った、終局後の振り返り(転機の手の表示とやり直し)、棋譜のコピー/読み込み。

## 構成

- `index.html` … 画面・3D表示(three.js r128)・探索エンジンをまとめた本体。
  - `<script id="engineSrc">` の中がエンジン(反復深化 + PVS + 置換表、決勝点の偶奇を考慮した評価関数)。
    Web Worker でも同じソースを動かしています。
  - `E.mcts` は [score4_mcts](https://github.com/mikanakim/score4_mcts) の `mcts2.cpp` の移植(UCB1 C=0.2、訪問10回で展開、決勝点を打つ/止めるプレイアウト)。
    C++版は12スレッドのルート並列ですが、こちらは1スレッドです。
  - 座標は `cell = 棒 + 16*段`、`棒 = x + 4*y`([score4_AI](https://github.com/mikanakim/score4_AI) と同じ)。
- `tools/gen_book.py` … score4_AI の定石CSVから `index.html` の `var BOOK=...;` を作り直します。

  ```sh
  python3 tools/gen_book.py path/to/score4_AI/score4/kifu/opening_book_with_human_made_book.csv index.html
  ```

- `test/engine.test.js` … `index.html` からエンジンを取り出してテストします(`node test/engine.test.js`)。

## 参考

- [mikanakim/score4_AI](https://github.com/mikanakim/score4_AI) … C++ の αβ 探索AI。定石データの出典。
- [mikanakim/score4_mcts](https://github.com/mikanakim/score4_mcts) … C++ の MCTS AI。

## ロードマップ

- [x] MCTSの移植(1スレッド)
- [ ] MCTSの並列化(複数Workerでのルート並列)、ビットボード化による高速化
- [ ] 完全解析(先手必勝/後手必勝/引き分けの判定)
