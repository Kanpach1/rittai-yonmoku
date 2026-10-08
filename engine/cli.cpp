// ネイティブ版（PCでの研究・ベンチマーク用）。
//   ./score4 think "12,3,15" 1000   … 局面を読んで最善手を表示
//   ./score4 bench 3000             … 初期局面から決めた手順で速度を測る
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include "score4.h"

static auto T0 = std::chrono::steady_clock::now();
extern "C" double s4_now() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count(); }
static std::mt19937_64 RNG(12345);
extern "C" double s4_rand() { return std::uniform_real_distribution<double>(0, 1)(RNG); }
static bool verbose = true;
extern "C" void s4_progress(int m, int d, double s, double n) {
  if (verbose) std::printf("  depth %2d  move %2d  score %9.1f  nodes %.0f  %.0f ms\n", d, m, s, n, s4_now());
}

static s4::Engine E;
static const int TT_BITS = 22;

static bool loadMoves(const char *s) {
  E.reset();
  const char *q = s;
  while (*q) {
    while (*q && (*q < '0' || *q > '9')) q++;
    if (!*q) break;
    int p = std::atoi(q);
    while (*q >= '0' && *q <= '9') q++;
    if (p > 15 || !E.canPlay(p)) return false;
    E.play(p);
  }
  return true;
}

int main(int argc, char **argv) {
  s4::initTables();
  E.setTT(new s4::TTE[1u << TT_BITS], TT_BITS);
  if (argc >= 3 && !std::strcmp(argv[1], "think")) {
    double ms = argc >= 4 ? std::atof(argv[3]) : 1000;
    if (!loadMoves(argv[2])) { std::fprintf(stderr, "棋譜が不正です\n"); return 1; }
    double t = s4_now();
    int m = E.think(ms, 60, 0, true);
    double el = s4_now() - t;
    std::printf("bestmove %d  score %.1f  depth %d  nodes %.0f  %.0f knps\n", m, E.resScore, E.resDepth, E.nodes, E.nodes / (el > 1 ? el : 1));
    return 0;
  }
  // 対称変換が本当に盤の対称（勝ちラインを勝ちラインに移す）になっているか
  if (argc >= 2 && !std::strcmp(argv[1], "selftest")) {
    for (int s = 0; s < 8; s++)
      for (int l = 0; l < s4::NL; l++) {
        u64 m = s4::symApply(s4::G.line[l], s);
        bool found = false;
        for (int j = 0; j < s4::NL; j++) if (s4::G.line[j] == m) found = true;
        if (!found) { std::printf("NG: sym %d line %d\n", s, l); return 1; }
      }
    std::printf("selftest ok\n");
    return 0;
  }
  // A と B を対戦させる: match <ms> <開局の数> <seed> <Aの設定> <Bの設定>
  // 設定は "base" か、"sym" などの改良名を + でつないだもの。各開局（最初の2手をランダム）を先後入れ替えて2局ずつ
  if (argc >= 7 && !std::strcmp(argv[1], "match")) {
    double ms = std::atof(argv[2]);
    int pairs = std::atoi(argv[3]);
    std::mt19937 orng(std::atoi(argv[4]));
    s4::Engine A, B;
    A.setTT(new s4::TTE[1u << TT_BITS], TT_BITS);
    B.setTT(new s4::TTE[1u << TT_BITS], TT_BITS);
    auto conf = [](s4::Engine &e, const char *s) { e.optSym = std::strstr(s, "sym") != nullptr;
      e.optKiller = std::strstr(s, "killer") != nullptr; e.optLMR = std::strstr(s, "lmr") != nullptr; };
    conf(A, argv[5]); conf(B, argv[6]);
    verbose = false;
    int wB = 0, lB = 0, dr = 0;
    for (int g = 0; g < pairs; g++) {
      int o1 = orng() % 16, o2 = orng() % 16;
      for (int col = 0; col < 2; col++) {
        s4::Engine *pl[2] = { col ? &B : &A, col ? &A : &B };
        int moves[64], nm = 0, winner = -1;
        moves[nm++] = o1; moves[nm++] = o2;
        while (nm < 64) {
          s4::Engine &e = *pl[nm % 2];
          e.reset();
          for (int i = 0; i < nm; i++) e.play(moves[i]);
          int m = e.think(ms, 60, 0, false);
          u64 thr = s4::threatCells(e.bb[e.side]) & ~e.occ;
          bool won = (thr >> e.cellOf(m)) & 1;
          moves[nm++] = m;
          if (won) { winner = (nm - 1) % 2; break; }
        }
        // winner: 0=先手, 1=後手。B が先手なのは col==1
        if (winner < 0) dr++;
        else if ((winner == 0) == (col == 1)) wB++;
        else lB++;
      }
    }
    std::printf("B(%s) vs A(%s): %d勝 %d敗 %d分\n", argv[6], argv[5], wB, lB, dr);
    return 0;
  }
  // 標準入力の各行の棋譜について、静的評価を1行ずつ出す（JS版との突き合わせ用）
  if (argc >= 2 && !std::strcmp(argv[1], "eval")) {
    char buf[1024];
    while (std::fgets(buf, sizeof buf, stdin)) {
      if (!loadMoves(buf)) { std::printf("x\n"); continue; }
      std::printf("%.6f\n", E.evaluate());
    }
    return 0;
  }
  if (argc >= 2 && !std::strcmp(argv[1], "bench")) {
    double ms = argc >= 3 ? std::atof(argv[2]) : 3000;
    const char *pos[] = {"", "12,3,15,0", "12,3,15,0,13,14,1,5", "5,6,9,10,0,15"};
    double tn = 0, tt = 0;
    for (const char *p : pos) {
      loadMoves(p);
      double t = s4_now();
      verbose = false;
      int m = E.think(ms, 60, 0, false);
      double el = s4_now() - t;
      tn += E.nodes; tt += el;
      std::printf("[%s] move %d depth %d score %.1f nodes %.0f\n", p, m, E.resDepth, E.resScore, E.nodes);
    }
    std::printf("合計 %.0f knps\n", tn / tt);
    return 0;
  }
  std::fprintf(stderr, "使い方: %s think <棋譜> [ms] | bench [ms]\n", argv[0]);
  return 1;
}
