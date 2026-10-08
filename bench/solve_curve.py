"""終盤の読み切りにかかる時間を、手数ごとに測る(完全解析 E-4 の見積もりと、対局で何手目から使えるかの判断用)。

対戦場の棋譜から、各手数の局面をいくつか取り、`build/score4 solve` で読み切る。
あわせて、通常の探索が勝ち/負けを読み切った局面では結果が一致するかを確かめる。

    python3 bench/solve_curve.py bench/results/base_1s.csv --plies 48 44 40 36 --per 10 --cap 30000
"""
import argparse
import csv
import statistics
import subprocess
import sys

WIN = 100000.0


def run(args):
    return subprocess.run(args, capture_output=True, text=True).stdout.strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--plies", type=int, nargs="+", default=[48, 44, 40, 36, 32])
    ap.add_argument("--per", type=int, default=10)
    ap.add_argument("--cap", type=int, default=30000, help="1局面の上限(ミリ秒)")
    ap.add_argument("--check-ms", type=int, default=1000, help="突き合わせ用の通常探索の時間")
    args = ap.parse_args()
    games = [r["moves"].split() for r in csv.DictReader(open(args.csv, encoding="utf8"))]
    mismatch = 0
    for ply in args.plies:
        pos = [g[:ply] for g in games if len(g) > ply][:args.per]
        times, solved, results = [], 0, []
        for p in pos:
            s = ",".join(p)
            out = run(["build/score4", "solve", s, str(args.cap)]).split()
            ok = out[1] != "?"
            ms = float(out[out.index("ms") + 1])
            if ok:
                solved += 1
                times.append(ms)
                v = int(out[1])
                results.append(v)
                # 通常の探索が読み切っていれば突き合わせる
                th = run(["build/score4", "think", s, str(args.check_ms)]).splitlines()
                last = [l for l in th if "score" in l]
                if last:
                    sc = float(last[-1].split("score")[1].split()[0])
                    if (sc > WIN - 200 and v != 1) or (sc < -(WIN - 200) and v != -1):
                        mismatch += 1
                        print("  不一致: %s 読み切り %+d / 探索 %.0f" % (s, v, sc), flush=True)
        dist = "勝%d 分%d 負%d" % (results.count(1), results.count(0), results.count(-1))
        if times:
            print("%2d手目(残り%2d): 読み切り %d/%d  中央値 %.0fms  最大 %.0fms  結果 %s"
                  % (ply, 64 - ply, solved, len(pos), statistics.median(times), max(times), dist), flush=True)
        else:
            print("%2d手目(残り%2d): 読み切り 0/%d(上限 %dms)" % (ply, 64 - ply, len(pos), args.cap), flush=True)
    print("通常の探索との不一致: %d" % mismatch)


if __name__ == "__main__":
    sys.exit(main())
