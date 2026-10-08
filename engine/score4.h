// 立体四目 (4x4x4, 重力あり) の探索エンジン。
// 標準ライブラリに依存しない（WebAssembly でもそのまま動かすため）。
// 座標: cell = peg + 16*z, peg = x + 4*y （score4_AI・index.html と同じ）。
// 盤面は先手・後手それぞれ 64bit のビットボードで持つ。
//
// 探索と評価は index.html の JS エンジン (Score4.Engine) と同じ考え方:
//   反復深化 + PVS + 置換表 + history、決勝点の強制手は深さを減らさず延長、
//   評価は「揃えるまでの最短手数」の逆数と、列ごとの決勝点の偶奇。
// ここではビット演算で決勝点を求めるぶん速い。
#pragma once

typedef unsigned long long u64;
typedef unsigned int u32;

// 環境ごとに用意する関数（WebAssembly では JS から渡す）
extern "C" double s4_now();                 // ミリ秒
extern "C" double s4_rand();                // [0, 1)
extern "C" void s4_progress(int move, int depth, double score, double nodes);

namespace s4 {

static inline int pc(u64 x) { return __builtin_popcountll(x); }
static inline int ctz(u64 x) { return __builtin_ctzll(x); }

const double WIN = 100000.0;
const double INF = 1e9;
const int NL = 76;

// ---------------- 盤の幾何 ----------------
struct Geo {
  u64 line[NL];
  signed char lineCell[NL][4];
  // 決勝点をビット演算で求めるための表: 13方向 × 位置t(0..3)
  int dirShift[13];
  u64 posMask[13][4];
  int center[16];
  // below[l]: ラインの各マスより下のマス（縦以外のラインは列が重ならない）。vert[l]: 縦のライン
  u64 below[NL];
  bool vert[NL];
  void init() {
    int n = 0, nd = 0;
    for (int dx = -1; dx <= 1; dx++)
      for (int dy = -1; dy <= 1; dy++)
        for (int dz = -1; dz <= 1; dz++) {
          if (!dx && !dy && !dz) continue;
          int f = dx ? dx : (dy ? dy : dz);
          if (f < 0) continue;
          dirShift[nd] = dx + 4 * dy + 16 * dz;   // 16,-12,4,20,-19,-3,13,-15,1,17,-11,5,21
          for (int t = 0; t < 4; t++) posMask[nd][t] = 0;
          for (int x = 0; x < 4; x++)
            for (int y = 0; y < 4; y++)
              for (int z = 0; z < 4; z++) {
                int ex = x + 3 * dx, ey = y + 3 * dy, ez = z + 3 * dz;
                if (ex < 0 || ex > 3 || ey < 0 || ey > 3 || ez < 0 || ez > 3) continue;
                u64 m = 0;
                for (int k = 0; k < 4; k++) {
                  int c = (x + k * dx) + 4 * (y + k * dy) + 16 * (z + k * dz);
                  lineCell[n][k] = (signed char)c;
                  m |= 1ULL << c;
                  posMask[nd][k] |= 1ULL << c;
                }
                line[n++] = m;
              }
          nd++;
        }
    for (int l = 0; l < NL; l++) {
      vert[l] = (lineCell[l][0] & 15) == (lineCell[l][1] & 15);
      below[l] = 0;
      for (int i = 0; i < 4; i++)
        for (int c = lineCell[l][i] - 16; c >= 0; c -= 16) below[l] |= 1ULL << c;
    }
    for (int p = 0; p < 16; p++) {
      double cx = p % 4 - 1.5, cy = p / 4 - 1.5;
      center[p] = (cx * cx + cy * cy) < 1 ? 3 : ((cx > 1 || cx < -1) && (cy > 1 || cy < -1) ? 2 : 0);
    }
  }
};
static Geo G;

template <int K> static inline u64 sh(u64 b) {
  if constexpr (K >= 0) return b >> K; else return b << -K;
}
// 方向 d（ずらし幅 S）で、b があと1つで揃うマス
template <int S> static inline u64 dirThreat(u64 b, const u64 *M) {
  u64 a0 = sh<S>(b), a1 = sh<2 * S>(b), a2 = sh<3 * S>(b);
  u64 m1 = sh<-S>(b), m2 = sh<-2 * S>(b), m3 = sh<-3 * S>(b);
  return (a0 & a1 & a2 & M[0]) | (m1 & a0 & a1 & M[1]) | (m2 & m1 & a0 & M[2]) | (m3 & m2 & m1 & M[3]);
}
// b の玉があと1つで揃うマス（空きかどうかは見ない）。方向の順は Geo::init と同じ
static inline u64 threatCells(u64 b) {
  const u64(*M)[4] = G.posMask;
  return dirThreat<16>(b, M[0]) | dirThreat<-12>(b, M[1]) | dirThreat<4>(b, M[2]) | dirThreat<20>(b, M[3]) |
         dirThreat<-19>(b, M[4]) | dirThreat<-3>(b, M[5]) | dirThreat<13>(b, M[6]) | dirThreat<-15>(b, M[7]) |
         dirThreat<1>(b, M[8]) | dirThreat<17>(b, M[9]) | dirThreat<-11>(b, M[10]) | dirThreat<5>(b, M[11]) |
         dirThreat<21>(b, M[12]);
}
// 今すぐ置けるマス（各列の一番下の空き）
static inline u64 groundOf(u64 occ) { return ((occ << 16) | 0xFFFFULL) & ~occ; }

// ---------------- 置換表 ----------------
struct TTE { u64 b0, b1; float v; signed char d, f, m, pad; };
static inline u32 ttIndex(u64 b0, u64 b1, int bits) {
  u64 h = b0 * 0x9E3779B97F4A7C15ULL ^ (b1 + 0x632BE59BD9B4E019ULL) * 0xC2B2AE3D27D4EB4FULL;
  h ^= h >> 29;
  return (u32)(h >> (64 - bits));
}

// ---------------- 盤の対称性（4×4 の正方形の8通りの回転・裏返し） ----------------
// 各段 16bit を同時に変換する。s の bit0: x反転, bit1: y反転, bit2: 最後に転置
static inline u64 flipX(u64 b) {
  b = ((b >> 1) & 0x5555555555555555ULL) | ((b & 0x5555555555555555ULL) << 1);
  return ((b >> 2) & 0x3333333333333333ULL) | ((b & 0x3333333333333333ULL) << 2);
}
static inline u64 flipY(u64 b) {
  b = ((b >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((b & 0x0F0F0F0F0F0F0F0FULL) << 4);
  return ((b >> 8) & 0x00FF00FF00FF00FFULL) | ((b & 0x00FF00FF00FF00FFULL) << 8);
}
static inline u64 transp(u64 b) {
  u64 t = (b ^ (b >> 3)) & 0x0A0A0A0A0A0A0A0AULL; b ^= t ^ (t << 3);
  t = (b ^ (b >> 6)) & 0x00CC00CC00CC00CCULL; b ^= t ^ (t << 6);
  return b;
}
static inline u64 symApply(u64 b, int s) {
  if (s & 1) b = flipX(b);
  if (s & 2) b = flipY(b);
  if (s & 4) b = transp(b);
  return b;
}
struct Sym {
  signed char fwd[8][16], inv[8][16];
  void init() {
    for (int s = 0; s < 8; s++)
      for (int p = 0; p < 16; p++) {
        int q = ctz(symApply(1ULL << p, s));
        fwd[s][p] = (signed char)q; inv[s][q] = (signed char)p;
      }
  }
};
static Sym SY;

// 表を作る。最初に一度だけ呼ぶ
static inline void initTables() { G.init(); SY.init(); }

// ---------------- 探索 ----------------
struct Engine {
  TTE *tt = 0;    // 置換表（呼び出し側が用意する）
  int ttBits = 0;
  // 改良の切り替え（対戦で効果を確かめるため）
  bool optSym = true;     // 置換表で対称な局面を同一視する（対戦で勝率65%）
  bool optKiller = false; // 同じ深さでβカットした手を先に読む（効果なし）
  bool optLMR = false;    // 後ろの順番の手は1手浅く読み、良さそうなら読み直す（効果なし）
  signed char killer[70][2];
  void setTT(TTE *p, int bits) { tt = p; ttBits = bits; }
  u64 bb[2];      // [0]=先手, [1]=後手
  u64 occ;
  int side;       // 手番 0/1
  int n;          // 打たれた玉の数
  int hist[32];
  double nodes, deadline;
  bool stop;
  // 結果
  int resMove, resDepth;
  double resScore;
  double rootScore[16];
  bool rootDone[16];

  void reset() { bb[0] = bb[1] = occ = 0; side = 0; n = 0; }
  int height(int p) const { return pc(occ & (0x0001000100010001ULL << p)); }
  bool canPlay(int p) const { return !(occ & (1ULL << (p + 48))); }
  int cellOf(int p) const { return p + 16 * height(p); }
  void play(int p) { u64 c = 1ULL << cellOf(p); bb[side] |= c; occ |= c; side ^= 1; n++; }
  void playCell(int c) { u64 m = 1ULL << c; bb[side] |= m; occ |= m; side ^= 1; n++; }
  void undoCell(int c) { u64 m = ~(1ULL << c); side ^= 1; bb[side] &= m; occ &= m; n--; }

  // 静的評価（手番側から見た値）。JS版 Engine.evaluate と同じ式
  double evaluate() const {
    return evaluate(threatCells(bb[0]) & ~occ, threatCells(bb[1]) & ~occ);
  }
  // t0, t1: 先手・後手の決勝点（空きマスのみ）
  double evaluate(u64 t0, u64 t1) const {
    int me = side;
    double s[2] = {0, 0};
    u64 empty = ~occ;
    for (int l = 0; l < NL; l++) {
      u64 m = G.line[l], x = occ & m;
      if (!x) continue;
      int a = pc(bb[0] & m), b = pc(x) - a;
      if (a && b) continue;
      int k = a ? 0 : 1, cnt = a ? a : b;
      if (cnt >= 3) continue;
      // 土台に必要な玉の数（ラインの各マスの下にある空きマスの数）
      int sup = G.vert[l] ? (cnt == 1 ? 3 : 1) : pc(empty & G.below[l]);
      int plies = 2 * (4 - cnt) - (k == me ? 1 : 0) + sup;
      s[k] += 100.0 / plies;
    }
    u64 thr[2] = { t0, t1 }, ta = t0 | t1;
    for (int p = 0; p < 16; p++) {
      if (!(ta & (0x0001000100010001ULL << p))) continue;
      int lowest = 0, prev = 0;
      for (int z = height(p); z < 4; z++) {
        int c = p + 16 * z;
        int f = (int)((thr[0] >> c) & 1) | ((int)((thr[1] >> c) & 1) << 1);
        if (f) {
          for (int kk = 0; kk < 2; kk++) {
            int bit = 1 << kk;
            if (!(f & bit)) continue;
            double w;
            if (!lowest) {
              bool good = (kk == 0) ? (z % 2 == 0) : (z % 2 == 1);
              w = good ? 260 : 130;
            } else if ((lowest & (1 << (1 - kk))) && !(lowest & bit)) {
              w = 10;
            } else {
              w = 70;
              if (prev & bit) w += 1200;
            }
            s[kk] += w;
          }
          if (!lowest) lowest = f;
        }
        prev = f;
      }
    }
    double v = s[0] - s[1];
    return me == 0 ? v : -v;
  }

  double search(int depth, double alpha, double beta, int ply) {
    nodes += 1;
    if (((long long)nodes & 4095) == 0 && s4_now() > deadline) stop = true;
    if (stop) return 0;
    if (n == 64) return 0;
    int k = side, o = side ^ 1;
    u64 ground = groundOf(occ);
    u64 myT = threatCells(bb[k]) & ~occ, opT = threatCells(bb[o]) & ~occ;
    if (myT & ground) return WIN - ply;
    u64 og = opT & ground;
    if (og) {
      if (og & (og - 1)) return -(WIN - ply - 1);
      // 強制手: 深さを減らさずに読む
      int c = ctz(og);
      playCell(c);
      double fv = ply > 60 ? -evaluate() : -search(depth, -beta, -alpha, ply + 1);
      undoCell(c);
      return fv;
    }
    if (depth <= 0) return k == 0 ? evaluate(myT, opT) : evaluate(opT, myT);

    u64 k0 = bb[0], k1 = bb[1];
    int sym = 0;
    if (optSym) {
      // 8通りのうち (b0, b1) が最小になる向きを代表にする
      for (int s = 1; s < 8; s++) {
        u64 a0 = symApply(bb[0], s), a1 = symApply(bb[1], s);
        if (a0 < k0 || (a0 == k0 && a1 < k1)) { k0 = a0; k1 = a1; sym = s; }
      }
    }
    TTE &e = tt[ttIndex(k0, k1, ttBits)];
    int ttMove = -1;
    if (e.f && e.b0 == k0 && e.b1 == k1) {
      ttMove = SY.inv[sym][(int)e.m];
      if (e.d >= depth) {
        double ev = e.v;
        if (e.f == 3) return ev;
        if (e.f == 1 && ev > alpha) alpha = ev;
        else if (e.f == 2 && ev < beta) beta = ev;
        if (alpha >= beta) return ev;
      }
    }

    // 着手の並べ替え
    int mv[16] = {0}, cs[16];
    double sc[16];
    int cnt = 0;
    u64 bad = ground & (opT >> 16);   // 真上が相手の決勝点になるマス
    for (u64 g = ground; g; g &= g - 1) {
      int c = ctz(g), p = c & 15;
      double s;
      if (p == ttMove) s = 1e9;
      else {
        s = hist[k * 16 + p] + G.center[p] * 4;
        if (optKiller) { if (p == killer[ply][0]) s += 2e6; else if (p == killer[ply][1]) s += 1e6; }
        if (bad & (1ULL << c)) s -= 1e7;
      }
      int j = cnt++;
      while (j > 0 && sc[j - 1] < s) { mv[j] = mv[j - 1]; cs[j] = cs[j - 1]; sc[j] = sc[j - 1]; j--; }
      mv[j] = p; cs[j] = c; sc[j] = s;
    }

    double best = -INF, a0 = alpha;
    int bestMove = mv[0];
    for (int i = 0; i < cnt; i++) {
      int c = cs[i];
      playCell(c);
      double v;
      if (i == 0) v = -search(depth - 1, -beta, -alpha, ply + 1);
      else {
        if (optLMR && i >= 3 && depth >= 3 && !(bad & (1ULL << c))) {
          v = -search(depth - 2, -alpha - 1, -alpha, ply + 1);
          if (v > alpha) v = -search(depth - 1, -alpha - 1, -alpha, ply + 1);
        } else v = -search(depth - 1, -alpha - 1, -alpha, ply + 1);
        if (v > alpha && v < beta) v = -search(depth - 1, -beta, -alpha, ply + 1);
      }
      undoCell(c);
      if (stop) return 0;
      if (v > best) {
        best = v; bestMove = mv[i];
        if (v > alpha) {
          alpha = v;
          if (alpha >= beta) {
            hist[k * 16 + mv[i]] += depth * depth;
            if (optKiller && killer[ply][0] != mv[i]) { killer[ply][1] = killer[ply][0]; killer[ply][0] = (signed char)mv[i]; }
            break;
          }
        }
      }
    }
    e.b0 = k0; e.b1 = k1; e.d = (signed char)depth;
    e.f = best <= a0 ? 2 : (best >= beta ? 1 : 3);
    e.v = (float)best; e.m = SY.fwd[sym][bestMove];
    return best;
  }

  // 反復深化。JS版 Engine.think（定石・ランダム以外）と同じ手順
  int think(double timeMs, int maxDepth, double margin, bool report) {
    int legal[16], nl = 0;
    for (int p = 0; p < 16; p++) if (canPlay(p)) legal[nl++] = p;
    resDepth = 0; resScore = 0; resMove = -1; nodes = 0;
    if (!nl) return -1;
    u64 myT = threatCells(bb[side]) & ~occ, ground = groundOf(occ);
    if (myT & ground) { resMove = ctz(myT & ground) & 15; resDepth = 1; resScore = WIN - 1; return resMove; }
    for (u32 i = 0, sz = 1u << ttBits; i < sz; i++) tt[i].f = 0;
    for (int i = 0; i < 32; i++) hist[i] = 0;
    for (int i = 0; i < 70; i++) killer[i][0] = killer[i][1] = -1;
    for (int i = 0; i < 16; i++) { rootDone[i] = false; rootScore[i] = -INF; }
    double start = s4_now();
    deadline = start + timeMs;
    stop = false;
    int order[16];
    for (int i = 0; i < nl; i++) order[i] = legal[i];
    // 中央寄りを先に（安定ソート）
    for (int i = 1; i < nl; i++) { int x = order[i], j = i; while (j > 0 && G.center[order[j - 1]] < G.center[x]) { order[j] = order[j - 1]; j--; } order[j] = x; }
    double scores[16];
    int bestMove = order[0];
    double bestScore = -INF;
    bool haveLast = false;
    double lastScores[16];
    if (maxDepth <= 0) maxDepth = 60;
    for (int d = 1; d <= maxDepth; d++) {
      double alpha = -INF, iterScore = -INF;
      int iterBest = -1, completed = 0;
      for (int i = 0; i < nl; i++) {
        int p = order[i], c = cellOf(p);
        playCell(c);
        double v = -search(d - 1, -INF, -(alpha - margin), 1);
        undoCell(c);
        if (stop) break;
        scores[p] = v; completed++;
        if (v > iterScore) { iterScore = v; iterBest = p; }
        if (v > alpha) alpha = v;
      }
      if (stop) {
        if (completed > 0) { bestMove = iterBest; bestScore = iterScore; }
        break;
      }
      bestMove = iterBest; bestScore = iterScore; resDepth = d;
      for (int i = 0; i < nl; i++) lastScores[order[i]] = scores[order[i]];
      haveLast = true;
      if (report) s4_progress(bestMove, d, bestScore, nodes);
      for (int i = 1; i < nl; i++) { int x = order[i], j = i; while (j > 0 && scores[order[j - 1]] < scores[x]) { order[j] = order[j - 1]; j--; } order[j] = x; }
      if (bestScore > WIN - 200 || bestScore < -(WIN - 200)) break;
      if (n + d >= 64) break;
    }
    // 最善とほぼ同価値の手からランダムに選ぶ（読み切れているときは揺らさない）
    if (margin > 0 && haveLast && bestScore < WIN - 200 && bestScore > -(WIN - 200)) {
      int cand[16], nc = 0;
      for (int i = 0; i < nl; i++) if (lastScores[legal[i]] >= bestScore - margin) cand[nc++] = legal[i];
      if (nc > 1) { int r = (int)(s4_rand() * nc); if (r >= nc) r = nc - 1; bestMove = cand[r]; }
    }
    if (haveLast) for (int i = 0; i < nl; i++) { rootDone[legal[i]] = true; rootScore[legal[i]] = lastScores[legal[i]]; }
    resMove = bestMove; resScore = bestScore;
    return bestMove;
  }
};

}  // namespace s4
