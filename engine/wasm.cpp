// WebAssembly 用の入口。標準ライブラリなしでビルドする（tools/build_wasm.sh）。
// JS からは env.now / env.rand / env.progress を渡す。
#include "score4.h"

extern "C" {
__attribute__((import_module("env"), import_name("now"))) double js_now();
__attribute__((import_module("env"), import_name("rand"))) double js_rand();
__attribute__((import_module("env"), import_name("progress"))) void js_progress(int, int, double, double);
double s4_now() { return js_now(); }
double s4_rand() { return js_rand(); }
void s4_progress(int m, int d, double s, double n) { js_progress(m, d, s, n); }

// clang がループを置き換えて呼ぶことがあるので自前で用意する
void *memset(void *p, int v, unsigned long n) { unsigned char *q = (unsigned char *)p; while (n--) *q++ = (unsigned char)v; return p; }
void *memcpy(void *d, const void *s, unsigned long n) { unsigned char *q = (unsigned char *)d; const unsigned char *r = (const unsigned char *)s; while (n--) *q++ = *r++; return d; }
}

static s4::Engine E;
const int TT_BITS = 19;
static s4::TTE TT[1u << TT_BITS];
static unsigned char MOVES[64];

extern "C" {
__attribute__((export_name("init"))) void init() { s4::initTables(); E.setTT(TT, TT_BITS); E.reset(); }
__attribute__((export_name("moves_ptr"))) unsigned char *moves_ptr() { return MOVES; }
// MOVES[0..n) の棋譜を並べる。不正な手があれば -1
__attribute__((export_name("load"))) int load(int n) {
  E.reset();
  for (int i = 0; i < n; i++) { int p = MOVES[i]; if (p > 15 || !E.canPlay(p)) return -1; E.play(p); }
  return 0;
}
__attribute__((export_name("think"))) int think(double timeMs, int maxDepth, double margin, int report) {
  return E.think(timeMs, maxDepth, margin, report != 0);
}
__attribute__((export_name("res_score"))) double res_score() { return E.resScore; }
__attribute__((export_name("res_depth"))) int res_depth() { return E.resDepth; }
__attribute__((export_name("res_nodes"))) double res_nodes() { return E.nodes; }
}
