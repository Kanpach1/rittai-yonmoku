// 既存最強AI(mikanakim/score4_AI、third_party/baseline_ai)を、対戦場と同じ命令で動かすための包み。
// third_party のソースは書き換えず、main.cpp 以外とリンクする(bench/build.sh)。
//   position <棋譜>  … 初期局面から棋譜を並べる
//   go <ms>          → "bestmove <棒>"
//   quit
// 思考の条件は main.cpp の「AI vs AI」と同じフラグ(連鎖検出は8手目から、序盤6手まではpot4line)。
// 深さの上限は外し、時間(time_limit)だけで止める。定石は使う(本来の設定のまま)。
// 注意: 持ち時間が短いと、score4_AI 自身が時間切れで打ち切った読みから埋まった棒を返すことがある(そのまま返す)。
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include "global.h"
#include "hash.h"
#include "time.h"

void init_zobrist_table();
map<vector<int>, OpeningMove> loadOpeningBook(const string &filename);
bool move(int player, int idx);
void kifu_input(int idx);
void BestPlayer(int player, int depth, bool use_book);

static void resetState(){
  teban = 0; allMoves = 0; playerBoard = {0, 0};
  kifu.clear(); kifu_for_book.assign(64, 0);
  kst = {0, 0}; kst_history.clear();
  transposition_table.clear(); current_hash = 0;
  rensa_start = false; game_start = false; is_ai2 = false; time_over = false;
  ai_sente = true;   // main.cpp の AI vs AI と同じ(評価関数に効く)
}

int main(int argc, char **argv){
  // score4_AI は思考の経過を cout に大量に出すので捨てる。命令への返事は printf で返す
  static std::ostringstream sink;
  std::cout.rdbuf(sink.rdbuf());
  init_zobrist_table();
  initialize_random_generator();
  if (argc >= 2) loadOpeningBook(argv[1]);
  char buf[1024];
  while (std::fgets(buf, sizeof buf, stdin)) {
    if (!std::strncmp(buf, "position", 8)) {
      resetState();
      const char *q = buf + 8;
      while (*q) {
        while (*q && (*q < '0' || *q > '9')) q++;
        if (!*q) break;
        int p = std::atoi(q);
        while (*q >= '0' && *q <= '9') q++;
        move(teban, p); kifu_input(p);
      }
    } else if (!std::strncmp(buf, "go", 2)) {
      int tesu = __builtin_popcountll(allMoves);
      rensa_start = tesu >= 8;
      game_start = tesu <= 6;
      is_ai2 = teban == 1;
      time_limit = (int)std::atof(buf + 2);
      time_over = false;
      BestPlayer(teban, 64, true);
      std::printf("bestmove %d\n", kifu.back() & 15);
      sink.str(""); sink.clear();
    } else if (!std::strncmp(buf, "quit", 4)) break;
    std::fflush(stdout);
  }
  return 0;
}
