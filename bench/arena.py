"""2つのエンジンを対戦させて得点率を出す(requirements 5.1)。

各開始局面で先後を入れ替えて2局ずつ。勝ち=1点、引き分け=0.5点。
結果は1局ごとに CSV へ追記するので、途中で止めても同じコマンドで続きから再開できる。

    ./bench/build.sh
    caffeinate -i python3 bench/arena.py --ms 1000 --out bench/results/base_1s.csv

A は build/score4(定石つき = ゲームの「最強」と同じ使い方)、B は既存最強AI(score4_AI)。
"""
import argparse
import csv
import math
import os
import sys
import threading
import time

from s4lib import Board, EngineProc, load_book

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE_BOOK = os.path.join(ROOT, "third_party/baseline_ai/score4/kifu/opening_book_with_human_made_book.csv")
FIELDS = ["opening", "a_first", "result_a", "plies", "end", "a_avg_ms", "a_max_ms", "b_avg_ms", "b_max_ms", "a_book", "b_book", "moves"]


def load_openings(path):
    out = []
    for line in open(path, encoding="utf8"):
        line = line.split("#")[0].strip()
        if line:
            out.append([int(x) for x in line.split()])
    return out


def play_game(opening, a_first, A, B, ms, timeout_ms):
    """A から見た結果(1/0.5/0)と記録"""
    b = Board(opening)
    times = {"A": [], "B": []}
    books = {"A": 0, "B": 0}
    end = "draw"
    result_a = 0.5
    while len(b.moves) < 64:
        first_to_move = len(b.moves) % 2 == 0
        who = "A" if first_to_move == a_first else "B"
        eng = A if who == "A" else B
        try:
            m, el, _, book = eng.think(b.moves, ms)
        except Exception as e:   # 応答なし・不正な返事は負け
            end = "%s error: %s" % (who, e)
            result_a = 0 if who == "A" else 1
            break
        if book:
            books[who] += 1
        else:
            times[who].append(el * 1000)
        if el * 1000 > timeout_ms or not b.legal(m):
            end = "%s %s" % (who, "timeout" if b.legal(m) else "illegal")
            result_a = 0 if who == "A" else 1
            break
        if b.play(m):
            end = "%s win" % who
            result_a = 1 if who == "A" else 0
            break
    avg = lambda v: round(sum(v) / len(v)) if v else 0
    return {"result_a": result_a, "plies": len(b.moves), "end": end,
            "a_avg_ms": avg(times["A"]), "a_max_ms": round(max(times["A"] or [0])),
            "b_avg_ms": avg(times["B"]), "b_max_ms": round(max(times["B"] or [0])),
            "a_book": books["A"], "b_book": books["B"], "moves": " ".join(map(str, b.moves))}


def summarize(rows):
    n = len(rows)
    if not n:
        return "まだ対局がありません"
    s = [float(r["result_a"]) for r in rows]
    w, d, l = s.count(1.0), s.count(0.5), s.count(0.0)
    mean = sum(s) / n
    sd = math.sqrt(sum((x - mean) ** 2 for x in s) / (n - 1)) if n > 1 else 0
    half = 1.96 * sd / math.sqrt(n)
    elo = -400 * math.log10(1 / mean - 1) if 0 < mean < 1 else float("inf") * (1 if mean >= 1 else -1)
    am = [float(r["a_max_ms"]) for r in rows]
    bm = [float(r["b_max_ms"]) for r in rows]
    aa = [float(r["a_avg_ms"]) for r in rows if float(r["a_avg_ms"]) > 0]
    ba = [float(r["b_avg_ms"]) for r in rows if float(r["b_avg_ms"]) > 0]
    ends = {}
    for r in rows:
        k = r["end"].split(":")[0]
        ends[k] = ends.get(k, 0) + 1
    return ("%d局: A %d勝 %d敗 %d分  得点率 %.1f%%(95%%区間 %.1f〜%.1f%%)  Elo差 %+.0f\n"
            "終わり方: %s\n"
            "1手の思考時間  A 平均 %.0fms・最大 %.0fms / B 平均 %.0fms・最大 %.0fms"
            % (n, w, l, d, mean * 100, max(0, mean - half) * 100, min(1, mean + half) * 100, elo,
               "、".join("%s %d" % kv for kv in sorted(ends.items())),
               sum(aa) / max(1, len(aa)), max(am), sum(ba) / max(1, len(ba)), max(bm)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ms", type=int, default=1000, help="1手の持ち時間(ミリ秒)")
    ap.add_argument("--openings", default=os.path.join(ROOT, "bench/openings.txt"))
    ap.add_argument("--limit", type=int, default=0, help="開始局面を先頭からこの数だけ使う(試運転用)")
    ap.add_argument("--out", required=True, help="結果の CSV(あれば続きから)")
    ap.add_argument("-j", "--jobs", type=int, default=2, help="同時に進める対局数(各エンジンは1スレッド)")
    ap.add_argument("--a", default="build/score4 serve sym", help="A のコマンド")
    ap.add_argument("--b", default="build/baseline_driver " + BASE_BOOK, help="B のコマンド")
    ap.add_argument("--no-book-a", action="store_true", help="A に index.html の定石を使わせない")
    ap.add_argument("--timeout-factor", type=float, default=10, help="持ち時間のこの倍を超えたら負け")
    args = ap.parse_args()

    openings = load_openings(args.openings)
    if args.limit:
        openings = openings[:args.limit]
    book = None if args.no_book_a else load_book(os.path.join(ROOT, "index.html"))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)

    done = {}
    if os.path.exists(args.out):
        for r in csv.DictReader(open(args.out, encoding="utf8")):
            done[(int(r["opening"]), r["a_first"] == "1")] = r
    todo = [(i, af) for i in range(len(openings)) for af in (True, False) if (i, af) not in done]
    print("開始局面 %d 個 × 先後 = %d 局(済み %d、残り %d)、持ち時間 %dms、同時 %d 局"
          % (len(openings), len(openings) * 2, len(done), len(todo), args.ms, args.jobs), flush=True)

    lock = threading.Lock()
    new_file = not os.path.exists(args.out)
    out = open(args.out, "a", newline="", encoding="utf8")
    wr = csv.DictWriter(out, fieldnames=FIELDS)
    if new_file:
        wr.writeheader(); out.flush()
    rows = list(done.values())
    t0 = time.time()

    def worker():
        A = EngineProc(args.a.split(), book)
        B = EngineProc(args.b.split())
        try:
            while True:
                with lock:
                    if not todo:
                        return
                    i, af = todo.pop(0)
                r = play_game(openings[i], af, A, B, args.ms, args.ms * args.timeout_factor)
                r.update({"opening": i, "a_first": 1 if af else 0})
                with lock:
                    wr.writerow(r); out.flush(); rows.append(r)
                    print("[%d/%d] 局面%2d A%s → %s(%s手 %s)  %.0f分経過"
                          % (len(rows), len(openings) * 2, i, "先手" if af else "後手",
                             {1: "A勝ち", 0: "A負け", 0.5: "引き分け"}[r["result_a"]], r["plies"], r["end"], (time.time() - t0) / 60), flush=True)
        finally:
            A.close(); B.close()

    ths = [threading.Thread(target=worker) for _ in range(max(1, args.jobs))]
    for th in ths:
        th.start()
    for th in ths:
        th.join()
    out.close()
    print(summarize(rows))


if __name__ == "__main__":
    sys.exit(main())
