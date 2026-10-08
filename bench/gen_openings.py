"""requirements 5.1 の「開始局面50個」を作る。

3手目まで指した合法局面を、盤の対称(8通り)で同一視して重複を除き、
このプロジェクトの C++ エンジンで短く読んで評価が偏りすぎないものから、固定の乱数で50個選ぶ。

    python3 bench/gen_openings.py > bench/openings.txt
"""
import itertools
import random
import sys

from s4lib import Board, EngineProc, canonical

PLIES, COUNT, SEED, MS, LIMIT = 3, 50, 20261008, 150, 120

seen, cands = set(), []
for seq in itertools.product(range(16), repeat=PLIES):
    k = canonical(seq)
    if k in seen:
        continue
    seen.add(k)
    cands.append(seq)

eng = EngineProc(["build/score4", "serve"])
scored = []
for seq in cands:
    _, _, line, _ = eng.think(list(seq), MS)
    score = float(line.split()[3])
    # 評価は手番(4手目を指す側)から見た値。極端に偏った局面は除く
    if abs(score) <= LIMIT:
        scored.append((seq, score))
eng.close()

rng = random.Random(SEED)
pick = sorted(rng.sample(scored, COUNT))
print("# 開始局面: %d手目までの合法局面 %d個(対称で同一視)のうち、%dms読みの評価が±%d以内の %d個から %d個を固定の乱数(%d)で選んだもの"
      % (PLIES, len(cands), MS, LIMIT, len(scored), COUNT, SEED))
print("# 1行1局面: 棒番号を空白区切り / 末尾の # は4手目の手番から見た評価")
for seq, score in pick:
    print(" ".join(map(str, seq)), "#", score)
print("候補 %d 個、評価で残ったもの %d 個" % (len(cands), len(scored)), file=sys.stderr)
