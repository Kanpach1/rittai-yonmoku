const assert = require('assert');
const { Game, LINES, bestMove } = require('../src/engine');

assert.strictEqual(LINES.length, 76, '4x4x4 の勝ちラインは76本');

// 縦
let g = new Game();
[0, 1, 0, 1, 0, 1, 0].forEach((c) => g.play(c));
assert(g.lastMoverWon(), '縦4つで勝ち');

// 横
g = new Game();
[0, 4, 1, 5, 2, 6, 3].forEach((c) => g.play(c));
assert(g.lastMoverWon(), '横4つで勝ち');

// undo
g = new Game(); g.play(5); g.play(5); g.undo(); g.undo();
assert.strictEqual(g.moves, 0); assert.strictEqual(g.lo[0], 0);

// 3つ並びなら勝ちを取る
g = new Game();
[0, 4, 1, 5, 2, 6].forEach((c) => g.play(c));
assert.strictEqual(bestMove(g, { timeMs: 300 }).move, 3, '即勝ちを選ぶ');

// 相手の3つ並びを止める(先手の 0,1,2 に対し後手番)
g = new Game();
[0, 4, 1, 5, 2].forEach((c) => g.play(c));
assert.strictEqual(bestMove(g, { timeMs: 300 }).move, 3, '相手の勝ちを防ぐ');

console.log('all tests passed');
