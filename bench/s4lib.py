"""対戦場と開始局面づくりで使う、立体四目の最小限のルール。

座標は index.html・C++ エンジン・score4_AI と同じ: cell = 棒 + 16*段、棒 = x + 4*y。
"""
import json
import re
import subprocess
import time

LINES = []
for dx in (-1, 0, 1):
    for dy in (-1, 0, 1):
        for dz in (-1, 0, 1):
            if not (dx or dy or dz):
                continue
            f = dx or dy or dz
            if f < 0:
                continue
            for x in range(4):
                for y in range(4):
                    for z in range(4):
                        ex, ey, ez = x + 3 * dx, y + 3 * dy, z + 3 * dz
                        if not (0 <= ex < 4 and 0 <= ey < 4 and 0 <= ez < 4):
                            continue
                        LINES.append(tuple((x + k * dx) + 4 * (y + k * dy) + 16 * (z + k * dz) for k in range(4)))
assert len(LINES) == 76
THRU = [[ln for ln in LINES if c in ln] for c in range(64)]

# 盤の対称(4×4の棒の並びの回転・反転 8通り)。段はそのまま
def _sym_peg(p, s):
    x, y = p % 4, p // 4
    for _ in range(s & 3):
        x, y = 3 - y, x
    if s & 4:
        x = 3 - x
    return x + 4 * y
SYM = [[_sym_peg(p, s) for p in range(16)] for s in range(8)]


class Board:
    def __init__(self, moves=()):
        self.cells = [0] * 64          # 0=空、1=先手(朱)、2=後手(象牙)
        self.h = [0] * 16
        self.moves = []
        for p in moves:
            self.play(p)

    @property
    def side(self):
        return 1 if len(self.moves) % 2 == 0 else 2

    def legal(self, p):
        return 0 <= p < 16 and self.h[p] < 4

    def play(self, p):
        """置いて、その手で四目ができたら True"""
        c = p + 16 * self.h[p]
        s = self.side
        self.cells[c] = s
        self.h[p] += 1
        self.moves.append(p)
        return any(all(self.cells[q] == s for q in ln) for ln in THRU[c])

    def key(self):
        """index.html の定石(BOOK)と同じ64文字の盤面キー"""
        return "".join(str(v) for v in self.cells)


def canonical(moves):
    """対称な局面を同一視するための代表キー"""
    b = Board(moves)
    best = None
    for s in range(8):
        k = [0] * 64
        for c in range(64):
            if b.cells[c]:
                k[SYM[s][c & 15] + (c & 48)] = b.cells[c]
        k = tuple(k)
        if best is None or k < best:
            best = k
    return best


def load_book(index_html):
    s = open(index_html, encoding="utf8").read()
    m = re.search(r"var BOOK=(\{.*?\});", s)
    return json.loads(m.group(1)) if m else {}


class EngineProc:
    """position / go で指すエンジン(build/score4 serve、build/baseline_driver)"""

    def __init__(self, cmd, book=None):
        self.cmd = cmd
        self.book = book
        self.p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

    def think(self, moves, ms):
        """(手, 掛かった秒, 返事の行, 定石だったか)"""
        if self.book is not None:
            bm = self.book.get(Board(moves).key())
            if bm is not None and Board(moves).legal(bm):
                return bm, 0.0, "book", True
        t = time.perf_counter()
        self.p.stdin.write("position " + " ".join(map(str, moves)) + "\n")
        self.p.stdin.write("go %d\n" % ms)
        self.p.stdin.flush()
        line = self.p.stdout.readline()
        el = time.perf_counter() - t
        if not line.startswith("bestmove"):
            raise RuntimeError("%s: 返事が不正 %r" % (self.cmd[0], line))
        return int(line.split()[1]), el, line.strip(), False

    def close(self):
        try:
            self.p.stdin.write("quit\n")
            self.p.stdin.flush()
            self.p.wait(timeout=5)
        except Exception:
            self.p.kill()
