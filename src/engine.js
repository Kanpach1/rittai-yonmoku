// 立体四目エンジン: 4x4x4, 重力あり
// セル番号 = column*4 + z (column = y*4 + x, z = 高さ)。0-31が lo、32-63が hi。
(function (root) {
  'use strict';

  const LINES = [];
  (function buildLines() {
    const dirs = [];
    for (let dx = -1; dx <= 1; dx++)
      for (let dy = -1; dy <= 1; dy++)
        for (let dz = -1; dz <= 1; dz++) {
          if (dx === 0 && dy === 0 && dz === 0) continue;
          // 重複排除: 最初の非ゼロ成分が正のもののみ
          const first = dx !== 0 ? dx : dy !== 0 ? dy : dz;
          if (first > 0) dirs.push([dx, dy, dz]);
        }
    for (const [dx, dy, dz] of dirs)
      for (let x = 0; x < 4; x++)
        for (let y = 0; y < 4; y++)
          for (let z = 0; z < 4; z++) {
            const ex = x + 3 * dx, ey = y + 3 * dy, ez = z + 3 * dz;
            if (ex < 0 || ex > 3 || ey < 0 || ey > 3 || ez < 0 || ez > 3) continue;
            const cells = [];
            for (let i = 0; i < 4; i++) cells.push(cell(x + i * dx, y + i * dy, z + i * dz));
            LINES.push(cells);
          }
  })();

  function cell(x, y, z) { return (y * 4 + x) * 4 + z; }

  const LINE_MASKS = LINES.map((cells) => {
    let lo = 0, hi = 0;
    for (const c of cells) { if (c < 32) lo |= 1 << c; else hi |= 1 << (c - 32); }
    return { lo, hi };
  });

  class Game {
    constructor() {
      this.lo = [0, 0]; // プレイヤー0/1 の盤面
      this.hi = [0, 0];
      this.heights = new Array(16).fill(0);
      this.turn = 0;
      this.moves = 0;
      this.history = [];
    }
    clone() {
      const g = new Game();
      g.lo = this.lo.slice(); g.hi = this.hi.slice();
      g.heights = this.heights.slice();
      g.turn = this.turn; g.moves = this.moves; g.history = this.history.slice();
      return g;
    }
    canPlay(col) { return col >= 0 && col < 16 && this.heights[col] < 4; }
    legalMoves() {
      const r = [];
      for (let c = 0; c < 16; c++) if (this.heights[c] < 4) r.push(c);
      return r;
    }
    play(col) {
      const z = this.heights[col]++;
      const c = col * 4 + z;
      if (c < 32) this.lo[this.turn] |= 1 << c; else this.hi[this.turn] |= 1 << (c - 32);
      this.history.push(col);
      this.turn ^= 1; this.moves++;
      return z;
    }
    undo() {
      const col = this.history.pop();
      this.turn ^= 1; this.moves--;
      const z = --this.heights[col];
      const c = col * 4 + z;
      if (c < 32) this.lo[this.turn] &= ~(1 << c); else this.hi[this.turn] &= ~(1 << (c - 32));
    }
    owns(p, c) {
      return c < 32 ? ((this.lo[p] >>> c) & 1) === 1 : ((this.hi[p] >>> (c - 32)) & 1) === 1;
    }
    // プレイヤー p が四つ揃っていれば、その線のセル配列を返す
    winningLine(p) {
      const lo = this.lo[p], hi = this.hi[p];
      for (let i = 0; i < LINE_MASKS.length; i++) {
        const m = LINE_MASKS[i];
        if ((lo & m.lo) === m.lo && (hi & m.hi) === m.hi) return LINES[i];
      }
      return null;
    }
    // 直前の手を打ったプレイヤーが勝ったか
    lastMoverWon() { return this.winningLine(this.turn ^ 1) !== null; }
    isFull() { return this.moves === 64; }
  }

  // ---- AI ----
  const WIN = 100000;
  const CELL_WEIGHT = (function () {
    // 多くの線に含まれるセルほど価値が高い
    const w = new Array(64).fill(0);
    for (const l of LINES) for (const c of l) w[c]++;
    return w;
  })();

  function popcount(x) {
    x = x - ((x >>> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >>> 2) & 0x33333333);
    return (((x + (x >>> 4)) & 0x0f0f0f0f) * 0x01010101) >>> 24;
  }

  // 手番側から見た静的評価
  function evaluate(g) {
    const me = g.turn, op = me ^ 1;
    let score = 0;
    for (const m of LINE_MASKS) {
      const mine = popcount(g.lo[me] & m.lo) + popcount(g.hi[me] & m.hi);
      const theirs = popcount(g.lo[op] & m.lo) + popcount(g.hi[op] & m.hi);
      if (mine && theirs) continue;
      if (mine) score += [0, 1, 6, 40][mine];
      else if (theirs) score -= [0, 1, 6, 40][theirs];
    }
    return score;
  }

  const ORDER = [5, 6, 9, 10, 0, 3, 12, 15, 1, 2, 4, 7, 8, 11, 13, 14]; // 中央優先

  function orderedMoves(g) {
    return ORDER.filter((c) => g.heights[c] < 4);
  }

  function search(g, depth, alpha, beta, ctx) {
    if (++ctx.nodes % 2048 === 0 && Date.now() > ctx.deadline) ctx.timeout = true;
    if (ctx.timeout) return 0;
    if (g.isFull()) return 0;
    if (depth === 0) return evaluate(g);
    for (const c of orderedMoves(g)) { // 即勝ち
      g.play(c);
      const won = g.lastMoverWon();
      g.undo();
      if (won) return WIN + depth;
    }
    let best = -Infinity;
    for (const c of orderedMoves(g)) {
      g.play(c);
      const s = -search(g, depth - 1, -beta, -alpha, ctx);
      g.undo();
      if (ctx.timeout) return 0;
      if (s > best) best = s;
      if (best > alpha) alpha = best;
      if (alpha >= beta) break;
    }
    return best;
  }

  // 反復深化。timeMs 内で最も深く読んだ結果の手を返す
  function bestMove(game, opts) {
    opts = opts || {};
    const maxDepth = opts.maxDepth || 12;
    const timeMs = opts.timeMs || 800;
    const g = game.clone();
    const ctx = { nodes: 0, deadline: Date.now() + timeMs, timeout: false };
    let best = orderedMoves(g)[0], bestScore = 0, reached = 0;
    for (let d = 1; d <= maxDepth; d++) {
      let curBest = null, curScore = -Infinity, alpha = -Infinity;
      for (const c of orderedMoves(g)) {
        g.play(c);
        const won = g.lastMoverWon();
        const s = won ? WIN + d : -search(g, d - 1, -Infinity, -alpha, ctx);
        g.undo();
        if (ctx.timeout) break;
        if (s > curScore) { curScore = s; curBest = c; }
        if (s > alpha) alpha = s;
      }
      if (ctx.timeout) break;
      best = curBest; bestScore = curScore; reached = d;
      if (Math.abs(curScore) >= WIN) break;
    }
    return { move: best, score: bestScore, depth: reached, nodes: ctx.nodes };
  }

  const api = { Game, LINES, bestMove, cell };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.Rittai = api;
})(typeof self !== 'undefined' ? self : this);
