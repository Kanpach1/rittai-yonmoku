"""対戦場で A が負けた局について、どこで負けが決まったか(転機)を調べる。

A の手を最後から遡り、その手の直後の局面を相手側から深めに読んで「相手の勝ちを読み切れる」かを確かめる。
読み切れる最初の A の手を転機とし、その直前に負けを避ける手があったか(A 側から読んで負けにならないか)も見る。

    python3 bench/analyze_losses.py bench/results/base_1s.csv --ms 1500
"""
import argparse
import csv
import json
import os
import sys

from s4lib import EngineProc

WIN = 100000.0
LOST = WIN - 200   # これ以上なら勝ち(負け)を読み切っている


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--ms", type=int, default=1500)
    ap.add_argument("--engine", default="build/score4 serve sym")
    ap.add_argument("--out", default=None, help="結果の JSON(省略時は CSV と同じ名前で .losses.json)")
    args = ap.parse_args()
    eng = EngineProc(args.engine.split())
    rows = [r for r in csv.DictReader(open(args.csv, encoding="utf8")) if r["result_a"] == "0"]
    out = []
    for r in rows:
        moves = [int(x) for x in r["moves"].split()]
        a_first = r["a_first"] == "1"
        a_plies = [i for i in range(3, len(moves)) if (i % 2 == 0) == a_first]
        turning, last_score = None, None
        for i in reversed(a_plies):
            _, _, line, _ = eng.think(moves[:i + 1], args.ms)
            score = float(line.split()[3])        # 相手(手番)から見た評価
            if score >= LOST:
                turning, last_score = i, score
            else:
                break
        alt = None
        if turning is not None:
            # 転機の直前の局面を A 側から読む。負けを読み切っていなければ、避ける手があった可能性
            _, _, line, _ = eng.think(moves[:turning], args.ms * 2)
            f = line.split()
            alt = {"move": int(f[1]), "score": float(f[3]), "played": moves[turning]}
        rec = {"opening": int(r["opening"]), "a_first": a_first, "plies": len(moves), "end": r["end"],
               "turning_ply": turning, "plies_to_end": (len(moves) - turning) if turning is not None else None,
               "loss_in": (WIN - last_score) if last_score else None, "alt": alt}
        out.append(rec)
        print(json.dumps(rec, ensure_ascii=False), flush=True)
    eng.close()
    path = args.out or os.path.splitext(args.csv)[0] + ".losses.json"
    json.dump(out, open(path, "w", encoding="utf8"), ensure_ascii=False, indent=1)
    # まとめ
    tp = [o["turning_ply"] for o in out if o["turning_ply"] is not None]
    avoid = [o for o in out if o["alt"] and o["alt"]["score"] > -LOST]
    print("負け %d 局 / 転機を特定 %d 局" % (len(out), len(tp)), file=sys.stderr)
    if tp:
        b = {"序盤(〜19手)": 0, "中盤(20〜39手)": 0, "終盤(40手〜)": 0}
        for t in tp:
            b["序盤(〜19手)" if t < 19 else "中盤(20〜39手)" if t < 39 else "終盤(40手〜)"] += 1
        print("転機の時期: %s" % "、".join("%s %d" % kv for kv in b.items()), file=sys.stderr)
        print("転機から終局まで 平均 %.1f 手 / 転機の直前に避ける手がありそうだった局 %d" %
              (sum(o["plies_to_end"] for o in out if o["plies_to_end"]) / len(tp), len(avoid)), file=sys.stderr)


if __name__ == "__main__":
    sys.exit(main())
