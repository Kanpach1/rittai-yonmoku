// index.html の orderWinCells(勝利線を光らせる順)を取り出してテストする
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const html = fs.readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8');
const engineSrc = html.match(/<script id="engineSrc">([\s\S]*?)<\/script>/)[1];
const fnSrc = html.match(/function orderWinCells\(cells, lastC\)\{[\s\S]*?\n  \}/)[0];
const ctx = { module: { exports: {} }, performance: { now: () => Date.now() }, Math, Date, atob };
vm.createContext(ctx);
vm.runInContext(engineSrc + '\n' + fnSrc + '\nthis.orderWinCells = orderWinCells;', ctx);
const { LINES } = ctx.module.exports;
const orderWinCells = ctx.orderWinCells;

const lv = (c) => c >> 4;
let flat = 0, cross = 0;
for (let li = 0; li < LINES.length / 4; li++) {
  const line = [LINES[li * 4], LINES[li * 4 + 1], LINES[li * 4 + 2], LINES[li * 4 + 3]];
  if (lv(line[0]) !== lv(line[3])) {
    // 段をまたぐ線: 最後の玉がどこでも、下の段から
    cross++;
    for (const last of [-1, ...line]) {
      const o = orderWinCells(line.slice(), last);
      assert(lv(o[0]) < lv(o[3]), `段をまたぐ線は下の段から: ${line}`);
    }
    continue;
  }
  flat++;
  // 同じ段の線: 最後の玉から遠い方の端から光り始め、並びは線に沿ったまま
  for (let k = 0; k < 4; k++) {
    const o = orderWinCells(line.slice(), line[k]);
    const kk = o.indexOf(line[k]);
    assert(kk >= 2, `最後の玉(${k}番目)から遠い端から始まる: ${line} → ${o}`);
    assert(o.join() === line.join() || o.join() === line.slice().reverse().join(), '並びは線に沿ったまま');
  }
  // 最後の玉が線上にないとき(通常は起きない)は並びを変えない
  assert.strictEqual(orderWinCells(line.slice(), -1).join(), line.join());
}
assert.strictEqual(flat, 40, '同じ段の線は 4段 × 10本');
assert.strictEqual(cross, 36, '段をまたぐ線は 36本');
console.log(`ok winorder: 同じ段 ${flat}本 / 段をまたぐ ${cross}本`);
