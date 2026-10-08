// index.html に内蔵されたエンジン(<script id="engineSrc">)を取り出してテストする
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const html = fs.readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8');
const src = html.match(/<script id="engineSrc">([\s\S]*?)<\/script>/)[1];
const ctx = { module: { exports: {} }, performance: { now: () => Date.now() }, Math, Date };
vm.createContext(ctx);
vm.runInContext(src + '\nthis.BOOK = BOOK;', ctx);
const Score4 = ctx.module.exports, BOOK = ctx.BOOK;
const { Engine, LINES, WIN } = Score4;

assert.strictEqual(LINES.length / 4, 76, '勝ちラインは76本');

// 定石: 盤面として成り立ち、指し手が合法であること
let n = 0;
for (const [key, mv] of Object.entries(BOOK)) {
  assert.strictEqual(key.length, 64);
  const c1 = [...key].filter((c) => c === '1').length, c2 = [...key].filter((c) => c === '2').length;
  assert(c1 === c2 || c1 === c2 + 1, `玉の数が不正: ${key}`);
  for (let i = 16; i < 64; i++) if (key[i] !== '0') assert(key[i - 16] !== '0', `宙に浮いた玉: ${key}`);
  assert(mv >= 0 && mv < 16 && key[mv + 48] === '0', `満杯の棒を指している: ${key}`);
  n++;
}
assert(n > 100, '定石が読み込まれている');

const e = new Engine();
// 定石: 初手は12番
e.reset();
assert.strictEqual(e.think({ book: true, timeMs: 100 }).move, 12);

// 即勝ち: 先手が 0,1,2 に並べている
e.load([0, 4, 1, 5, 2, 6]);
let r = e.think({ timeMs: 300, maxDepth: 60 });
assert.strictEqual(r.move, 3); assert(r.score > WIN - 200);

// 相手の決勝点を止める
e.load([0, 4, 1, 5, 2]);
assert.strictEqual(e.think({ timeMs: 300, maxDepth: 60 }).move, 3);

// play/undo で状態が戻る
e.load([5, 6, 5, 9]);
const before = e.bookKey(), h1 = e.h1;
e.play(10); e.undo(10);
assert.strictEqual(e.bookKey(), before); assert.strictEqual(e.h1, h1);

// MCTS: 即勝ち・防御、探索後に盤面が元に戻る
e.load([0, 4, 1, 5, 2, 6]);
assert.strictEqual(e.think({ algo: 'mcts', timeMs: 200 }).move, 3);
e.load([0, 4, 1, 5, 2]);
assert.strictEqual(e.think({ algo: 'mcts', timeMs: 200 }).move, 3);
e.load([5, 6, 9]);
const key = e.bookKey();
r = e.think({ algo: 'mcts', timeMs: 300 });
assert(r.nodes > 100 && r.move >= 0 && r.move < 16);
assert.strictEqual(e.bookKey(), key); assert.strictEqual(e.stack.length, 3);

console.log(`all tests passed (定石 ${n} 局面)`);
