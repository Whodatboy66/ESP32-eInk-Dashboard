// Tests of web/layout.js (the setup app's layout editor logic). Run: node --test tests/web/*.test.cjs
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const path = require('node:path');
const fs = require('node:fs');

const Layout = require('../../web/layout.js');
const vectors = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'vectors', 'layouts.json'), 'utf8'));
const W = vectors.widgets;
const GRID = { cols: 4, rows: 3 };

function box(c, r, w, h, widget) { return { c, r, w, h, widget: widget === undefined ? null : widget }; }
function codes(result) { return result.errors.map((e) => e.code); }

test('vectors: widget table as specified', () => {
  const t = Layout.table(W);
  assert.deepStrictEqual([t.clock.minW, t.clock.minH], [1, 1]);  // stage 7: 64 px digits in one cell
  assert.deepStrictEqual([t.month.minW, t.month.minH], [1, 1]);  // stage 7: a design for every size
  assert.deepStrictEqual([t.notes.minW, t.notes.minH], [1, 1]);
  assert.strictEqual(W.length, 10);
  assert.ok(vectors.cases.length >= 12);
});

test('vectors: every case gives the expected result', () => {
  for (const v of vectors.cases) {
    const r = Layout.validate({ statusBar: v.statusBar, boxes: v.boxes }, W, GRID);
    assert.strictEqual(r.ok, v.valid, `${v.name}: ${JSON.stringify(r.errors)}`);
    if (!v.valid && v.rule) assert.ok(codes(r).includes(v.rule), `${v.name}: expected rule ${v.rule}, got ${codes(r)}`);
  }
});

test('vectors: every rule has an invalid case and there are valid ones', () => {
  const rules = new Set(vectors.cases.filter((v) => !v.valid).map((v) => v.rule));
  for (const r of ['outside', 'overlap', 'gap', 'count', 'duplicate', 'unknown', 'size']) assert.ok(rules.has(r), r);
  assert.ok(vectors.cases.some((v) => v.valid));
});

test('fits', () => {
  const t = Layout.table(W);
  assert.strictEqual(Layout.fits(t.clock, box(2, 0, 2, 1)), true);
  assert.strictEqual(Layout.fits(t.clock, box(0, 0, 1, 3)), true);
  assert.strictEqual(Layout.fits(t.stocks, box(0, 0, 1, 3)), false);
  assert.strictEqual(Layout.fits(t.month, box(0, 0, 2, 2)), true);
  assert.strictEqual(Layout.fits(t.month, box(0, 0, 4, 1)), true);
  assert.strictEqual(Layout.fits(t.stocks, box(0, 0, 1, 1)), false);
  assert.strictEqual(Layout.fits(t.notes, box(0, 0, 1, 1)), true);
  assert.strictEqual(Layout.fits(undefined, box(0, 0, 4, 3)), false);
  // a table passed by the caller wins: no copy of the sizes inside layout.js
  assert.strictEqual(Layout.fits({ key: 'clock', minW: 4, minH: 2 }, box(0, 0, 2, 1)), false);
});

test('split: 2 x 2 gives two 2 x 1, top first, widget stays on top when it fits', () => {
  const out = Layout.split([box(0, 1, 2, 2, 'agenda'), box(2, 1, 2, 2)], 0, W);
  assert.deepStrictEqual(out.slice(0, 2), [box(0, 1, 2, 1, 'agenda'), box(0, 2, 2, 1, null)]);
  assert.strictEqual(out.length, 3);
});

test('split: 4 x 3 gives two 2 x 3', () => {
  const out = Layout.split([box(0, 0, 4, 3, 'agenda')], 0, W);
  assert.deepStrictEqual(out, [box(0, 0, 2, 3, 'agenda'), box(2, 0, 2, 3, null)]);
});

test('split: 3 rows give 2 + 1, larger part first', () => {
  const out = Layout.split([box(0, 0, 2, 3), box(2, 0, 2, 3)], 0, W);
  assert.deepStrictEqual(out.slice(0, 2), [box(0, 0, 2, 2), box(0, 2, 2, 1)]);
  const odd = Layout.split([box(0, 0, 3, 1), box(3, 0, 1, 1)], 0, W);
  assert.deepStrictEqual(odd.slice(0, 2), [box(0, 0, 2, 1), box(2, 0, 1, 1)]);
});

test('split: widget stays in the first part when it fits there, or is dropped', () => {
  // 3 x 2 -> 2 x 2 + 1 x 2: the month (2 x 2) stays in the first part
  let out = Layout.split([box(0, 0, 3, 2, 'month')], 0, W);
  assert.deepStrictEqual(out, [box(0, 0, 2, 2, 'month'), box(2, 0, 1, 2, null)]);
  // 2 x 3 -> 2 x 2 + 2 x 1: the month stays first
  out = Layout.split([box(0, 0, 2, 3, 'month'), box(2, 0, 2, 3)], 0, W);
  assert.strictEqual(out[0].widget, 'month');
  // 3 x 1 -> 2 x 1 + 1 x 1: the clock (2 x 1) stays first
  out = Layout.split([box(0, 0, 3, 1, 'clock')], 0, W);
  assert.deepStrictEqual(out.map((b) => b.widget), ['clock', null]);
  // 1 x 3 -> 1 x 2 + 1 x 1: the note stays first (first part preferred when both fit)
  out = Layout.split([box(0, 0, 1, 3, 'notes')], 0, W);
  assert.deepStrictEqual(out.map((b) => b.widget), ['notes', null]);
  // 2 x 2 -> 2 x 1 + 2 x 1: the month (1 x 1 since stage 7) stays in the first part
  out = Layout.split([box(0, 0, 2, 2, 'month')], 0, W);
  assert.deepStrictEqual(out.map((b) => b.widget), ['month', null]);
  // 2 x 1 -> 1 x 1 + 1 x 1: the agenda (2 x 1) fits neither part
  out = Layout.split([box(0, 0, 2, 1, 'agenda'), box(2, 0, 2, 1)], 0, W);
  assert.deepStrictEqual(out.slice(0, 2), [box(0, 0, 1, 1, null), box(1, 0, 1, 1, null)]);
  // Note: with "larger part first" the second part is never larger than the first in either
  // direction, so a widget that only fits the second part can't happen with min sizes alone;
  // the code still moves it in that case (rule of the plan).
});

test('split: 1 x 1 is refused', () => {
  const boxes = [box(0, 0, 1, 1, 'weather')];
  assert.strictEqual(Layout.canSplit(boxes[0]), false);
  assert.strictEqual(Layout.split(boxes, 0, W), null);
  assert.strictEqual(Layout.canSplit(box(0, 0, 1, 2)), true);
});

test('split: the input is not changed and the result stays valid', () => {
  const boxes = [box(0, 0, 4, 3, 'agenda')];
  const out = Layout.split(boxes, 0, W);
  assert.deepStrictEqual(boxes, [box(0, 0, 4, 3, 'agenda')]);
  assert.ok(Layout.validate({ boxes: out }, W, GRID).ok);
});

test('merge: neighbours sharing a full edge only', () => {
  const four = [box(0, 0, 2, 1, 'clock'), box(2, 0, 2, 1, 'weather'), box(0, 1, 2, 2, 'agenda'), box(2, 1, 2, 2, 'month')];
  assert.deepStrictEqual(Layout.mergePartners(four, 0), [1, 2]);    // right, then below
  assert.strictEqual(Layout.mergePartner(four, 2), 3);
  assert.deepStrictEqual(Layout.mergePartners(four, 3), [2, 1]);    // left, then above
  assert.strictEqual(Layout.merge(four, 0, 3), null);               // only a corner touches
  const side = [box(0, 0, 2, 3), box(2, 0, 2, 1), box(2, 1, 2, 1), box(2, 2, 2, 1)];
  assert.deepStrictEqual(Layout.mergePartners(side, 2), [3, 1]);    // below first, then above
  assert.strictEqual(Layout.mergePartner(side, 0), -1);            // touches all three, no full edge
  assert.strictEqual(Layout.merge(side, 0, 1), null);
  assert.strictEqual(Layout.merge(side, 1, 0), null);
  assert.strictEqual(Layout.direction(side[2], side[1]), 'above');
});

test('merge: selected widget wins, the neighbour\'s when the selected box is empty', () => {
  const two = [box(0, 0, 2, 3, 'agenda'), box(2, 0, 2, 3, 'month')];
  assert.deepStrictEqual(Layout.merge(two, 0, 1), [box(0, 0, 4, 3, 'agenda')]);
  assert.deepStrictEqual(Layout.merge(two, 1, 0), [box(0, 0, 4, 3, 'month')]);
  const empty = [box(0, 0, 4, 1, null), box(0, 1, 4, 2, 'clock')];
  assert.deepStrictEqual(Layout.merge(empty, 0, 1), [box(0, 0, 4, 3, 'clock')]);
  const both = [box(0, 0, 4, 1), box(0, 1, 4, 2)];
  assert.deepStrictEqual(Layout.merge(both, 1, 0), [box(0, 0, 4, 3, null)]);
});

test('merge: the merged box takes the lower index and the rest keeps its order', () => {
  const six = Layout.fromStructure(Layout.GENERIC.find((s) => s.key === 'six'), W);
  const out = Layout.merge(six, 3, 1);  // box 4 (stocks) with box 2 above (weather)
  assert.strictEqual(out.length, 5);
  assert.deepStrictEqual(out[1], box(2, 0, 2, 2, 'stocks'));
  assert.deepStrictEqual(out.map((b) => b.widget), ['clock', 'stocks', 'agenda', 'tracking', 'printer']);
  assert.ok(Layout.validate({ boxes: out }, W, GRID).ok);
});

test('split then merge gives the box back', () => {
  const start = [box(0, 0, 4, 3, 'agenda')];
  const s = Layout.split(start, 0, W);
  assert.deepStrictEqual(Layout.merge(s, 0, 1), start);
});

test('validate: rules', () => {
  const ok = Layout.validate({ boxes: [box(0, 0, 4, 3, 'agenda')] }, W, GRID);
  assert.deepStrictEqual(ok, { ok: true, errors: [] });
  assert.deepStrictEqual(codes(Layout.validate({ boxes: [box(0, 0, 4, 3), box(4, 0, 1, 1)] }, W, GRID)), ['outside']);
  assert.deepStrictEqual(codes(Layout.validate({ boxes: [box(0, 0, 4, 3), box(0, 0, 1, 1)] }, W, GRID)), ['overlap']);
  assert.deepStrictEqual(codes(Layout.validate({ boxes: [box(0, 0, 4, 2)] }, W, GRID)), ['gap']);
  assert.deepStrictEqual(codes(Layout.validate({ boxes: [box(0, 0, 2, 3, 'clock'), box(2, 0, 2, 3, 'clock')] }, W, GRID)), ['duplicate']);
  assert.deepStrictEqual(codes(Layout.validate({ boxes: [box(0, 0, 4, 3, 'radio')] }, W, GRID)), ['unknown']);
  assert.deepStrictEqual(codes(Layout.validate({ boxes: [box(0, 0, 1, 3, 'stocks'), box(1, 0, 3, 3)] }, W, GRID)), ['size']);
  assert.deepStrictEqual(codes(Layout.validate({ boxes: [box(0, 0, 1.5, 3), box(1, 0, 3, 3)] }, W, GRID)).slice(0, 1), ['outside']);
  const thirteen = [];
  for (let i = 0; i < 12; i++) thirteen.push(box(i % 4, Math.floor(i / 4), 1, 1));
  thirteen.push(box(0, 0, 1, 1));
  assert.ok(codes(Layout.validate({ boxes: thirteen }, W, GRID)).includes('count'));
  // empty widget values are fine
  assert.ok(Layout.validate({ boxes: [box(0, 0, 4, 3, '')] }, W, GRID).ok);
  // errors carry a readable text and the box index
  const e = Layout.validate({ boxes: [box(0, 0, 1, 1, 'stocks'), box(1, 0, 3, 1), box(0, 1, 4, 2)] }, W, GRID).errors[0];
  assert.strictEqual(e.box, 0);
  assert.match(e.text, /needs 2 × 1/);
});

test('validate: the caller\'s widget table and grid are used', () => {
  const T = [{ key: 'clock', minW: 4, minH: 1 }];
  assert.deepStrictEqual(codes(Layout.validate({ boxes: [box(0, 0, 2, 3, 'clock'), box(2, 0, 2, 3)] }, T, GRID)), ['size']);
  assert.ok(Layout.validate({ boxes: [box(0, 0, 2, 2)] }, W, { cols: 2, rows: 2 }).ok);
});

test('generic structures are valid, cover all 12 cells and keep their suggested widgets', () => {
  const names = Layout.GENERIC.map((s) => s.name);
  assert.deepStrictEqual(names, ['Full', 'Halves', 'Top + main', 'Top + 2', 'Four', 'Main + side', 'Six', 'Big + 2 small']);
  for (const s of Layout.GENERIC) {
    assert.ok(Layout.validate(s, W, GRID).ok, s.name);
    const area = s.boxes.reduce((n, b) => n + b.w * b.h, 0);
    assert.strictEqual(area, 12, s.name);
    const boxes = Layout.fromStructure(s, W);
    assert.ok(Layout.validate({ boxes }, W, GRID).ok, s.name);
    assert.deepStrictEqual(boxes.map((b) => b.widget), s.boxes.map((b) => b.widget), s.name);
  }
});

test('fromStructure drops widgets that are unknown, too big or used twice', () => {
  const s = { boxes: [box(0, 0, 1, 3, 'stocks'), box(1, 0, 3, 1, 'agenda'), box(1, 1, 3, 1, 'agenda'), box(1, 2, 3, 1, 'radio')] };
  assert.deepStrictEqual(Layout.fromStructure(s, W).map((b) => b.widget), [null, 'agenda', null, null]);
});

test('structures: device entries replace or extend the generic ones, Classic last', () => {
  const list = Layout.structures([
    { key: 'planner', name: 'Planner', boxes: [box(0, 0, 2, 1, 'clock')] },
    { key: 'full', name: 'Full (device)', boxes: [box(0, 0, 4, 3, 'month')] },
    { key: 'classic', name: 'Classic' }
  ]);
  assert.strictEqual(list.filter((s) => s.key === 'full').length, 1);
  assert.strictEqual(list.find((s) => s.key === 'full').name, 'Full (device)');
  assert.ok(list.find((s) => s.key === 'planner'));
  assert.strictEqual(list[list.length - 1].key, 'classic');
  assert.strictEqual(list[list.length - 1].classic, true);
  assert.strictEqual(list.filter((s) => s.key === 'classic').length, 1);
  assert.strictEqual(Layout.structures().length, Layout.GENERIC.length + 1);
});

test('sameBoxes', () => {
  assert.ok(Layout.sameBoxes([box(0, 0, 4, 3, 'agenda')], [box(0, 0, 4, 3, 'agenda')]));
  assert.ok(!Layout.sameBoxes([box(0, 0, 4, 3, 'agenda')], [box(0, 0, 4, 3, null)]));
});
