// Tests of web/i18n.js: both languages have the same texts, and every text app.js uses exists.
// Run: node --test tests/web/*.test.cjs
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

const I18N = require('../../web/i18n.js');
const Layout = require('../../web/layout.js');
const app = fs.readFileSync(path.join(__dirname, '..', '..', 'web', 'app.js'), 'utf8');
const { de, en } = I18N.dict;

function kind(v) { return Array.isArray(v) ? 'array' : typeof v; }
function placeholders(v) {
  const s = new Set();
  JSON.stringify(v).replace(/\{(\w+)\}/g, (m, k) => { s.add(k); return m; });
  return [...s].sort();
}

test('German and English have the same keys', () => {
  assert.deepStrictEqual(Object.keys(de).sort(), Object.keys(en).sort());
});

test('each text has the same shape and placeholders in both languages', () => {
  for (const k of Object.keys(en)) {
    assert.strictEqual(kind(de[k]), kind(en[k]), k);
    if (Array.isArray(en[k])) assert.strictEqual(de[k].length, en[k].length, k);
    if (kind(en[k]) === 'object') assert.deepStrictEqual(Object.keys(de[k]).sort(), Object.keys(en[k]).sort(), k);
    assert.deepStrictEqual(placeholders(de[k]), placeholders(en[k]), k);
    assert.ok(JSON.stringify(de[k]).length > 2 && JSON.stringify(en[k]).length > 2, k + ' is empty');
  }
});

test('every t() / tn() key used in app.js exists', () => {
  const used = new Set();
  app.replace(/\bt\('([A-Za-z0-9_]+)'(?=[,)])/g, (m, k) => { used.add(k); return m; });
  app.replace(/\btn\('([A-Za-z0-9_]+)'(?=[,)])/g, (m, k) => { used.add(k); return m; });
  assert.ok(used.size > 200, 'found ' + used.size + ' keys');
  for (const k of used) {
    assert.ok(k in en, 'missing in en: ' + k);
    assert.ok(k in de, 'missing in de: ' + k);
  }
});

test('plural texts (tn) are [one, many] pairs', () => {
  app.replace(/\btn\('([A-Za-z0-9_]+)'(?=[,)])/g, (m, k) => {
    assert.ok(Array.isArray(en[k]) && en[k].length === 2, k);
    return m;
  });
});

test('keys built at run time exist: field labels, structure names, layout errors', () => {
  const fields = /var FIELDS = \{([\s\S]*?)\};/.exec(app)[1].match(/(\w+):/g).map((x) => x.slice(0, -1));
  assert.ok(fields.length > 20);
  for (const f of fields) assert.ok(('f_' + f) in en && ('f_' + f) in de, 'f_' + f);
  for (const s of Layout.GENERIC) assert.ok(('st_' + s.key) in en && ('st_' + s.key) in de, 'st_' + s.key);
  for (const c of ['count', 'outside', 'overlap', 'unknown', 'duplicate', 'size', 'gap']) assert.ok(('v_' + c) in en, 'v_' + c);
});

test('layout error parameters fill the translated texts', () => {
  const W = [{ key: 'clock', name: 'Uhr', minW: 2, minH: 1 }];
  const r = Layout.validate({ boxes: [{ c: 0, r: 0, w: 1, h: 1, widget: 'clock' }, { c: 0, r: 0, w: 1, h: 1, widget: null }] }, W, { cols: 4, rows: 3 });
  I18N.setLang('de');
  for (const e of r.errors) {
    const text = e.code === 'gap' ? I18N.tn('v_gap', e.p.free) : I18N.t('v_' + e.code, e.p);
    assert.ok(!/\{\w+\}/.test(text), text);
  }
});

test('t, tn and num', () => {
  I18N.setLang('de');
  assert.strictEqual(I18N.t('tabHome'), 'Start');
  assert.strictEqual(I18N.t('stepOf', { n: 1, m: 6 }), 'Schritt 1 von 6');
  assert.strictEqual(I18N.tn('saveN', 1), '1 Änderung speichern und neu starten');
  assert.strictEqual(I18N.tn('saveN', 3), '3 Änderungen speichern und neu starten');
  assert.strictEqual(I18N.num(3.912, 2), '3,91');
  assert.strictEqual(I18N.t('fmtDate', { wd: 'Fr', d: 20, mon: 'November' }), 'Fr, 20. November');
  assert.deepStrictEqual(I18N.t('days').slice(5), ['Fr', 'Sa']);
  I18N.setLang('en');
  assert.strictEqual(I18N.t('tabHome'), 'Home');
  assert.strictEqual(I18N.num(3.912, 2), '3.91');
  assert.strictEqual(I18N.t('fmtDate', { wd: 'Fri', d: 20, mon: 'November' }), 'Fri, 20 November');
  I18N.setLang('xx');
  assert.strictEqual(I18N.lang(), 'de');
});

test('German wording the owner asked for', () => {
  assert.deepStrictEqual([de.tabHome, de.tabScreen, de.tabData, de.tabDevice], ['Start', 'Bildschirm', 'Daten', 'Gerät']);
  assert.strictEqual(de.sToday, 'Heute und morgen');
  assert.ok(/EUNL\.DE:MSCI World,PPFB\.DE:Gold/.test(de.stocksNote) && /EUNL\.DE:MSCI World,PPFB\.DE:Gold/.test(en.stocksNote));
});
