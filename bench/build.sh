#!/bin/sh
# 対戦場で使う2つのエンジンをビルドする: build/score4(このプロジェクトの C++ エンジン)と build/baseline_driver(既存最強AI)
set -e
cd "$(dirname "$0")/.."
mkdir -p build
c++ -O3 -std=c++17 -o build/score4 engine/cli.cpp
B=third_party/baseline_ai/score4
c++ -O3 -std=c++17 -w -iquote "$B" -o build/baseline_driver bench/baseline_driver.cpp \
  "$B"/alpha_beta.cpp "$B"/evaluation.cpp "$B"/game.cpp "$B"/hash.cpp "$B"/player.cpp "$B"/time.cpp "$B"/global.cpp
echo "built: build/score4 build/baseline_driver"
