#!/bin/sh
# engine/ の C++ エンジンを WebAssembly にして、index.html の `var WASM_B64="...";` 行に埋め込む。
# 必要なもの: clang（wasm32 ターゲット）と wasm-ld。標準ライブラリは使わない。
set -e
cd "$(dirname "$0")/.."
OUT=engine/score4.wasm
clang++ --target=wasm32 -O3 -std=c++17 -nostdlib -fno-exceptions -fno-rtti \
  -Wl,--no-entry -Wl,--strip-all -Wl,-z,stack-size=1048576 \
  -o "$OUT" engine/wasm.cpp
python3 - "$OUT" index.html <<'PY'
import base64, re, sys
wasm, html = sys.argv[1], sys.argv[2]
b64 = base64.b64encode(open(wasm, "rb").read()).decode()
t = open(html, encoding="utf-8").read()
t, n = re.subn(r'^var WASM_B64=".*";$', lambda _: 'var WASM_B64="' + b64 + '";', t, count=1, flags=re.M)
assert n == 1, "index.html に var WASM_B64= の行が見つかりません"
open(html, "w", encoding="utf-8").write(t)
print("%s: %d bytes を埋め込みました" % (wasm, len(b64) * 3 // 4))
PY
