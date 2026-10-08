# 立体四目

4×4×4の盤に石を落として四つ並べる、重力ありの立体四目並べ。
最強AIとの対戦と二人対戦ができる対戦環境と、このゲームの完全解析を目指すプロジェクトです。

## 遊び方

`index.html` をブラウザで開くだけで遊べます(ビルド不要、1ファイル完結)。

- 3D盤面: ドラッグで回転、ピンチ/ホイールで拡大縮小。棒をタップするとその棒に玉が落ちます。
- AI: 初級・中級・上級・最強の4段階(αβ探索、上級以上は定石つき)と、試験中の MCTS AI。二人対局もできます。
- 待った、終局後の振り返り(転機の手の表示とやり直し)、棋譜のコピー/読み込み。
- 対局は自動で保存され、ページを開き直しても「前回の続きから」で再開できます。相手ごとの戦績も残ります(この端末のブラウザ内のみ)。

## 構成

- `index.html` … 画面・3D表示(three.js r128)・探索エンジンをまとめた本体。
  - `<script id="engineSrc">` の中がエンジン(反復深化 + PVS + 置換表、決勝点の偶奇を考慮した評価関数)。
    Web Worker でも同じソースを動かしています。
  - `E.mcts` は [score4_mcts](https://github.com/mikanakim/score4_mcts) の `mcts2.cpp` の移植(UCB1 C=0.2、訪問10回で展開、決勝点を打つ/止めるプレイアウト)。
    C++版は12スレッドのルート並列ですが、こちらは1スレッドです。
  - 座標は `cell = 棒 + 16*段`、`棒 = x + 4*y`([score4_AI](https://github.com/mikanakim/score4_AI) と同じ)。
- `engine/` … C++ の探索エンジン(ビットボード)。上級・最強・ヒント・振り返りの探索はこれを WebAssembly にして使います。
  - `score4.h` … 本体。探索と評価は JS エンジンと同じ式で、決勝点をビット演算で求めるぶん速い(JS の約2.3倍)。
    置換表で盤の対称な局面(8通り)を同一視する改良を入れています。
  - `wasm.cpp` … WebAssembly 用の入口。`cli.cpp` … PC 用のネイティブ版(ベンチマーク・対戦・局面検討)。

  ```sh
  ./tools/build_wasm.sh                                   # WebAssembly を作って index.html に埋め込む(clang + wasm-ld)
  g++ -O3 -march=native -std=c++17 -o score4 engine/cli.cpp
  ./score4 think "12,3,15,0" 3000                          # 局面を読む
  ./score4 match 100 20 1 base sym                         # 改良の効果を対戦で確かめる
  ```

- `bench/` … 既存最強AI(score4_AI)との対戦で強さを測る(requirements 5.1)。
  - `build.sh` … `build/score4`(このエンジン)と `build/baseline_driver`(score4_AI を同じ命令で動かす包み。`third_party/baseline_ai` が必要)をビルド。
  - `openings.txt` … 開始局面50個(3手目までの合法局面を対称で同一視した321個から選んだもの。`gen_openings.py` で作り直せる)。
  - `arena.py` … 50局面×先後入れ替えの100局を指し、得点率・95%区間・思考時間を出す。結果は1局ごとにCSVへ追記し、止めても続きから再開できる。

  ```sh
  ./bench/build.sh
  caffeinate -i python3 bench/arena.py --ms 1000 --out bench/results/base_1s.csv   # 1手1秒、同時2局(約40分)
  ```

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
- [x] MCTSの並列化(複数Workerでのルート並列)
- [x] C++ ビットボードエンジン + WebAssembly 化(JS版に同じ持ち時間で 17勝7敗)
- [x] 置換表の対称性(改良前に 26勝14敗)
- [ ] αβ の並列化(ネイティブは複数スレッド、Web は複数 Worker)
- [ ] score4_AI(C++)との対戦で強さを測る、評価関数の改良
- [ ] 完全解析(先手必勝/後手必勝/引き分けの判定)
