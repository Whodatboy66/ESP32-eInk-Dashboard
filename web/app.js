// Setup app (docs/plans/STAGE5_SETUP_APP.md, API: docs/SETUP_API.md). Plain ES2017, no
// framework, no external files: the phone has no internet in setup mode. Needs layout.js and
// i18n.js (every text the app writes itself; device texts arrive already translated).
(function () {
'use strict';

var t = I18N.t, tn = I18N.tn, num = I18N.num;
var root = document.getElementById('app');
var S = { tab: 'home' };

var ICONS = { clock: '◷', weather: '☀', agenda: '≡', month: '▦', stocks: '↗', tracking: '▣',
  printer: '▤', tasks: '✓', notes: '✎', birthdays: '✱' };
// Classic is a pixel layout (src/layout/Templates.h): only drawn as a sketch here.
var CLASSIC = [['clock', 10, 0, 430, 155], ['month', 440, 25, 360, 230], ['agenda', 440, 255, 360, 145],
  ['stocks', 10, 155, 430, 130], ['weather', 10, 285, 430, 105], ['printer', 10, 390, 430, 90], ['tracking', 440, 400, 360, 80]];
// Time zone names the firmware knows (src/core/Timezone.h); POSIX rules work too. Descriptions: t('tzNames').
var TZ_LIST = ['Europe/Berlin', 'Europe/London', 'Europe/Dublin', 'Europe/Lisbon', 'Europe/Helsinki',
  'America/New_York', 'America/Chicago', 'America/Denver', 'America/Los_Angeles', 'UTC'];
var TZ_NAMES = ('Europe/Berlin Europe/Vienna Europe/Zurich Europe/Paris Europe/Amsterdam Europe/Brussels Europe/Luxembourg ' +
  'Europe/Rome Europe/Madrid Europe/Copenhagen Europe/Oslo Europe/Stockholm Europe/Warsaw Europe/Prague Europe/Budapest ' +
  'Europe/Helsinki Europe/Athens Europe/Bucharest Europe/Lisbon Europe/London Europe/Dublin UTC America/New_York ' +
  'America/Chicago America/Denver America/Los_Angeles').split(' ');
var PINS = ['epd_cs', 'epd_dc', 'epd_rst', 'epd_busy', 'epd_sck', 'epd_mosi', 'display_power', 'battery', 'demo_button'];
// Setting -> tab where its field (and a field error) is shown; the label is t('f_' + key).
var FIELDS = {
  device_name: 'device', wifi_ssid: 'device', wifi_pass: 'device', timezone: 'device', weather_loc: 'data', stocks: 'data',
  ics: 'data', bday_url: 'data', pkge_key: 'data', adb_key: 'data', mqtt_ip: 'data', mqtt_sn: 'data', mqtt_pass: 'data',
  mqtt_port: 'data', printer_name: 'data', makerworld_uid: 'data', ota_auto: 'device', ota_beta: 'device', clock_24h: 'screen',
  pin_preset: 'device', pins: 'device', clock_interval: 'screen', quiet_enabled: 'screen', quiet_from: 'screen',
  quiet_to: 'screen', battery_mah: 'device', lang: 'device', holiday_state: 'data', holidays_on: 'data', school_on: 'data',
  school_url: 'data', ics_month: 'data', cal_birthdays: 'data', interval_weather: 'data', interval_stocks: 'data',
  interval_calendar: 'data', interval_tracking: 'data', ics_names: 'data', ics_every: 'data', printer_merge: 'data', printer_idle: 'data'
};

// ---------- helpers ----------

function add(el, c) {
  if (c === null || c === undefined || c === false) return;
  if (Array.isArray(c)) { c.forEach(function (x) { add(el, x); }); return; }
  el.appendChild(typeof c === 'object' ? c : document.createTextNode(String(c)));
}

function h(tag, p) {
  var el = document.createElement(tag);
  if (p) {
    Object.keys(p).forEach(function (k) {
      var v = p[k];
      if (v === null || v === undefined || v === false) return;
      if (k === 'class') el.className = v;
      else if (k === 'html') el.innerHTML = v;
      else if (k.slice(0, 2) === 'on') el.addEventListener(k.slice(2), v);
      else if (k === 'value' || k === 'checked' || k === 'disabled' || k === 'selected') el[k] = v;
      else el.setAttribute(k, v === true ? '' : v);
    });
  }
  for (var i = 2; i < arguments.length; i++) add(el, arguments[i]);
  return el;
}

function esc(s) { return String(s).replace(/[&<>"']/g, function (c) { return '&#' + c.charCodeAt(0) + ';'; }); }
var uidN = 0;
function uid() { uidN++; return 'u' + uidN; }
function clone(o) { return o === undefined ? undefined : JSON.parse(JSON.stringify(o)); }
function same(a, b) { return JSON.stringify(a) === JSON.stringify(b); }
function has(o, k) { return Object.prototype.hasOwnProperty.call(o, k); }
function pad(n) { return (n < 10 ? '0' : '') + n; }
function lbl(text) { return h('h2', { class: 'lbl' }, text); }
function card() { var c = h('div', { class: 'card' }); for (var i = 0; i < arguments.length; i++) add(c, arguments[i]); return c; }
function kv(k, v) { return h('div', { class: 'kv' }, h('b', null, k), h('span', null, v)); }
function btn(text, fn, cls, more) {
  var p = { type: 'button', class: 'btn' + (cls ? ' ' + cls : ''), onclick: fn };
  if (more) Object.keys(more).forEach(function (k) { p[k] = more[k]; });
  return h('button', p, text);
}
function icon(key) { return h('span', { class: 'ico' + (key ? '' : ' o'), 'aria-hidden': 'true' }, key ? (ICONS[key] || '◻') + '︎' : '+'); }
function spin() { return h('span', { class: 'spin', 'aria-hidden': 'true' }); }
function focusF(f) {
  var el = f && document.querySelector('[data-f="' + f + '"]');
  if (el) { try { el.focus({ preventScroll: false }); } catch (e) { el.focus(); } }
}

var toastTimer;
function toast(msg) {
  var el = document.getElementById('toast');
  if (!el) { el = h('div', { id: 'toast', class: 'toast', role: 'status' }); document.body.appendChild(el); }
  el.textContent = msg;
  el.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(function () { el.hidden = true; }, 4000);
}

// ---------- language, dates ----------

// The app's language: the unsaved choice, else the saved setting; S.uiLang when the firmware has
// no `lang` setting (older builds) or before the state is loaded. German by default.
function langNow() {
  if (!S.state) return S.uiLang || 'de';
  if (has(S.draft, 'lang')) return S.draft.lang;
  return saved('lang') || S.uiLang || 'de';
}
function chooseLang(v) {
  if (saved('lang') === undefined) S.uiLang = v; else setVal('lang', v);
  render();
}
function monthName(m) { return t('months')[m]; }
function monShort(m) { return monthName(m).slice(0, 3); }
function dayShort(d) { return t('days')[d]; }
function hmText(d) { return pad(d.getHours()) + ':' + pad(d.getMinutes()); }
function dateTimeText(sec) { return new Date(sec * 1000).toLocaleString(t('locale')); }

// ---------- device API ----------

function httpText(status) {
  if (status === 413) return t('http413');
  if (status === 404) return t('http404');
  return t('httpErr', { s: status });
}

// Every request: JSON in and out, a timeout, and one result shape: {ok, error, ...answer}.
// Never rejects. A 401 with {login: true} opens the code screen.
function api(method, path, body, o) {
  o = o || {};
  var ctl = typeof AbortController !== 'undefined' ? new AbortController() : null;
  var init = { method: method, headers: {}, credentials: 'same-origin', cache: 'no-store' };
  if (body !== undefined) { init.headers['Content-Type'] = 'application/json'; init.body = JSON.stringify(body); }
  if (ctl) init.signal = ctl.signal;
  var timer;
  var late = new Promise(function (resolve, reject) {
    timer = setTimeout(function () { if (ctl) ctl.abort(); reject(new Error('timeout')); }, o.timeout || 10000);
  });
  return Promise.race([fetch(path, init).then(function (res) {
    var ct = res.headers.get('Content-Type') || '';
    if (o.blob && res.ok && ct.indexOf('json') < 0) return res.blob().then(function (b) { return { ok: true, blob: b, status: res.status }; });
    return res.text().then(function (txt) {
      var d = null;
      try { d = txt ? JSON.parse(txt) : {}; } catch (e) { d = null; }
      if (!d || typeof d !== 'object' || Array.isArray(d)) d = {};
      if (d.ok === undefined) d.ok = res.ok;
      if (!res.ok) {
        d.ok = false;
        if (res.status === 401 && d.login) { d.error = t('needCode'); needLogin(); }
        if (!d.error && !d.offline) d.error = httpText(res.status);
      }
      d.status = res.status;
      return d;
    });
  }), late]).catch(function () {
    return { ok: false, noAnswer: true, error: t('noAnswer') };
  }).then(function (d) { clearTimeout(timer); return d; });
}

// ---------- settings model ----------

function saved(k) { return S.state.settings[k]; }
function cur(k) { return has(S.draft, k) ? S.draft[k] : saved(k); }
function setVal(k, v) {
  if (same(v, saved(k))) delete S.draft[k]; else S.draft[k] = v;
  updateBar();
}
function secretInfo(key) {
  var m = /^ics\.(\d)$/.exec(key);
  var v = m ? (saved('ics') || [])[+m[1]] : saved(key);
  return v && typeof v === 'object' ? v : { set: false };
}
function setSecret(key, v) {
  if (v === undefined) delete S.secrets[key]; else S.secrets[key] = v;
  updateBar();
}
function settingsBody() {
  var b = clone(S.draft), ics = [], any = false;
  Object.keys(S.secrets).forEach(function (k) {
    var m = /^ics\.(\d)$/.exec(k);
    if (m) { ics[+m[1]] = S.secrets[k]; any = true; } else b[k] = S.secrets[k];
  });
  if (any) {
    // A JSON array can't leave a slot out: an entry that stays as it is goes as {} (neither a
    // string nor null); trailing ones are left off.
    var arr = [];
    for (var i = 0; i < 3; i++) arr.push(ics[i] === undefined ? {} : ics[i]);
    while (arr.length && !(typeof arr[arr.length - 1] === 'string' || arr[arr.length - 1] === null)) arr.pop();
    b.ics = arr;
  }
  return b;
}
function dirtyCount() { return Object.keys(S.draft).length + Object.keys(S.secrets).length; }
function normKey(k) { var m = /^(ics|pins)\W*([a-z0-9_]+)\W*$/i.exec(k); return m ? m[1] + '.' + m[2] : k; }
function fieldTab(key) { return FIELDS[key.split('.')[0]] || 'device'; }
function pinLabel(k) {
  if (k === 'display_power') return t('pinPower');
  if (k === 'battery') return t('pinBattery');
  if (k === 'demo_button') return t('pinButton');
  return 'Display ' + k.slice(4).toUpperCase();
}
function fieldLabel(key) {
  var m = /^ics\.(\d)$/.exec(key);
  if (m) return t('calLinkN', { n: +m[1] + 1 });
  if (key.indexOf('pins.') === 0) return pinLabel(key.slice(5));
  return has(FIELDS, key) ? t('f_' + key) : key;
}

// ---------- layout model ----------

function widgetList() { return S.state.widgets || []; }
function widgetsByKey() { return Layout.table(widgetList()); }
function grid() {
  var g = S.state.grid || {};
  return { cols: g.cols || 4, rows: g.rows || 3, cellW: g.cellW || 200, cellH: g.cellH || 150, statusH: g.statusH || 28 };
}
function structures() { return Layout.structures(S.state.structures); }
function orderShow(list) {
  return widgetList().map(function (w) { return w.key; }).filter(function (k) { return list.indexOf(k) >= 0; });
}
function initLayout() {
  var L = S.state.layout || {};
  var keys = structures().map(function (s) { return s.key; });
  S.lay = {
    classic: L.template === 'classic',
    key: keys.indexOf(L.template) >= 0 ? L.template : 'custom',
    statusBar: L.statusBar !== false,
    boxes: Layout.copyBoxes(L.boxes),
    show: orderShow(L.show || [])
  };
  if (!S.lay.classic && !S.lay.boxes.length) S.lay.boxes = Layout.fromStructure(Layout.GENERIC[0], widgetList());
  S.laySaved = JSON.stringify(layoutBody());
  S.sel = 0;
}
function layoutBody() {
  if (S.lay.classic) return { template: 'classic', show: S.lay.show.slice() };
  return { template: 'custom', statusBar: S.lay.statusBar, boxes: Layout.copyBoxes(S.lay.boxes) };
}
function layoutDirty() { return JSON.stringify(layoutBody()) !== S.laySaved; }
function layoutCheck() {
  return S.lay.classic ? { ok: true, errors: [] } : Layout.validate(S.lay, widgetList(), grid());
}
function vText(e) { return e.code === 'gap' ? tn('v_gap', e.p.free) : t('v_' + e.code, e.p); }
function layChanged() {
  if (S.preview && S.preview.url) URL.revokeObjectURL(S.preview.url);
  S.preview = null;
  if (S.sel >= S.lay.boxes.length) S.sel = Math.max(0, S.lay.boxes.length - 1);
  render();
}
function isGeneric(s) { return !s.classic && Layout.GENERIC.some(function (g) { return g.key === s.key; }); }
function structName(s) {
  if (isGeneric(s)) return t('st_' + s.key);
  if (s.classic && s.name === 'Classic') return t('classic');
  return s.name;
}
function chooseStructure(s) {
  if (s.classic) { S.lay.classic = true; S.lay.key = 'classic'; }
  else {
    S.lay.classic = false;
    S.lay.key = s.key;
    S.lay.boxes = isGeneric(s) ? Layout.copyBoxes(s.boxes).map(function (b) { b.widget = null; return b; }) : Layout.fromStructure(s, widgetList());
    S.sel = 0;
  }
  layChanged();
}

// ---------- e-paper sketch (SVG, 800 x 480) ----------

var BOLD = ' font-weight="700"', END = ' text-anchor="end"', MID = ' text-anchor="middle"';
function tx(x, y, size, text, extra) {
  return '<text x="' + Math.round(x) + '" y="' + Math.round(y) + '" font-size="' + Math.round(size) + '"' + (extra || '') + '>' + esc(text) + '</text>';
}
function ln(x1, y1, x2, y2, w) {
  return '<line x1="' + x1 + '" y1="' + y1 + '" x2="' + x2 + '" y2="' + y2 + '" stroke="#000" stroke-width="' + (w || 2) + '"/>';
}
function rc(x, y, w, hh, fill, sw, extra) {
  return '<rect x="' + Math.round(x) + '" y="' + Math.round(y) + '" width="' + Math.round(w) + '" height="' + Math.round(hh) +
    '" fill="' + (fill || 'none') + '" stroke="#000" stroke-width="' + (sw === undefined ? 2 : sw) + '"' + (extra || '') + '/>';
}
function head(title, w) { return tx(12, 24, 17, title, BOLD) + ln(8, 32, w - 8, 32); }
function rowCount(hh) { return Math.max(1, Math.floor((hh - 44) / 27)); }
function rowsSvg(list, w, hh, fn) {
  var s = '', n = Math.min(list.length, rowCount(hh));
  for (var i = 0; i < n; i++) s += fn(list[i], 56 + i * 27);
  return s;
}
function wrapText(text, maxChars, maxLines) {
  var out = [];
  String(text).split('\n').forEach(function (para) {
    var line = '';
    para.split(/\s+/).forEach(function (word) {
      if (!word) return;
      while (word.length > maxChars) {
        if (line) { out.push(line); line = ''; }
        out.push(word.slice(0, maxChars));
        word = word.slice(maxChars);
      }
      if (!line) line = word;
      else if ((line + ' ' + word).length <= maxChars) line += ' ' + word;
      else { out.push(line); line = word; }
    });
    out.push(line);
  });
  if (out.length > maxLines) { out = out.slice(0, maxLines); out[maxLines - 1] += '…'; }
  return out;
}
function nowParts() {
  var d = new Date(), hr = d.getHours(), m = pad(d.getMinutes());
  var t24 = cur('clock_24h') !== false;
  return {
    time: t24 ? hr + ':' + m : ((hr % 12) || 12) + ':' + m + (hr < 12 ? ' AM' : ' PM'),
    date: t('fmtDate', { wd: dayShort(d.getDay()), d: d.getDate(), mon: monthName(d.getMonth()) }),
    short: t('fmtShort', { wd: dayShort(d.getDay()), d: d.getDate(), mon: monShort(d.getMonth()) })
  };
}
function weatherIcon(kind, cx, cy) {
  var g = '<g transform="translate(' + Math.round(cx) + ' ' + Math.round(cy) + ')" fill="none" stroke="#000" stroke-width="3">';
  var cloud = '<path d="M-20 12h38a11 11 0 0 0 0-22 15 15 0 0 0-28-3 10 10 0 0 0-10 25z"/>';
  if (kind === 0) g += '<circle r="11"/><path d="M0-22v6M0 16v6M-22 0h6M16 0h6M-15-15l4 4M11 11l4 4M-15 15l4-4M11-11l4-4"/>';
  else if (kind === 1) g += cloud;
  else g += '<g transform="translate(0 -6)">' + cloud + '</g><path d="M-10 14l-4 10M0 14l-4 10M10 14l-4 10"/>';
  return g + '</g>';
}
function sampleTasks() {
  var list = S.p && S.p.tasks;
  if (!list || !list.length) return t('sTaskRows');
  var today = new Date(); today.setHours(0, 0, 0, 0);
  return list.filter(function (k) { return !k.done && k.title; }).slice().sort(function (a, b) {
    var ad = a.due || '9999', bd = b.due || '9999';
    return ad < bd ? -1 : ad > bd ? 1 : (a.prio || 2) - (b.prio || 2);
  }).map(function (k) {
    var label = '';
    if (k.due) {
      var p = k.due.split('-'), d = new Date(+p[0], +p[1] - 1, +p[2]), diff = Math.round((d - today) / 864e5);
      label = diff < 0 ? t('sOverdue') : diff === 0 ? t('sTodayLow') : diff < 7 ? dayShort(d.getDay()) : (+p[2]) + '.' + (+p[1]) + '.';
    }
    return [k.title, label];
  });
}
function bdWhen(days) { return days === 0 ? t('bdToday') : days === 1 ? t('bdTomorrow') : t('bdInD', { n: days }); }
function sampleBirthdays() {
  var up = S.p && S.p.bd && S.p.bd.upcoming;
  if (!up || !up.length) return t('sBdRows');
  return up.map(function (b) { return [bdWhen(b.days), b.name + (b.age ? ' (' + b.age + ')' : '')]; });
}

function widgetSvg(key, w, hh) {
  var s = '', i, n;
  switch (key) {
    case 'clock': {
      var np = nowParts(), size = Math.min(hh * 0.62, w / (np.time.length > 5 ? 3.6 : 2.6)), withDate = hh >= 120;
      var y = hh / 2 + size * 0.36 - (withDate ? 12 : 0);
      s = tx(w / 2, y, size, np.time, BOLD + MID + ' letter-spacing="-2"');
      if (withDate) s += tx(w / 2, y + 30, 17, np.date, MID);
      return s;
    }
    case 'weather': {
      n = Math.max(1, Math.min(7, Math.floor((w - 80) / 100) + 1));
      var dw = w / n, day = new Date().getDay();
      for (i = 0; i < n; i++) {
        var cx = dw * i + dw / 2;
        s += tx(cx, 26, 16, dayShort((day + i) % 7), MID + BOLD) + weatherIcon([0, 1, 2, 1, 0, 2, 0][i], cx, hh / 2 + 2) +
          tx(cx, hh - 14, 16, (9 + i) + '° / ' + (2 + i) + '°', MID);
      }
      return s;
    }
    case 'agenda':
      return head(t('sToday'), w) + rowsSvg(t('sAgenda'), w, hh, function (e, y) { return tx(12, y, 17, e[0], BOLD) + tx(104, y, 17, e[1]); });
    case 'month': {
      // As the device (src/widgets/MonthModel.h): sheet (1 x 1), two weeks (low boxes), narrow (no
      // week numbers), otherwise a grid with week numbers; today a small filled square, weekend bold.
      var now = new Date(), yr = now.getFullYear(), mo = now.getMonth(), today = now.getDate();
      if (w < 300 && hh < 200) {
        return tx(w / 2, 24, 19, dayShort(now.getDay()), MID + BOLD) + tx(w / 2, 92, 62, today, MID + BOLD) +
          tx(w / 2, 120, 15, monthName(mo) + ' · ' + t('sWeek') + ' ' + isoWeek(now), MID);
      }
      var two = hh < 200, kw = w >= 300, cols = kw ? 8 : 7, cw = w / cols, top = 46;
      var first = new Date(yr, mo, 1), start = (first.getDay() + 6) % 7, days = new Date(yr, mo + 1, 0).getDate();
      var monday = new Date(yr, mo, today - (now.getDay() + 6) % 7);
      var rows = two ? 2 : Math.ceil((start + days) / 7), rh = Math.min(two ? 46 : 60, (hh - top - (two ? 20 : 8)) / rows);
      s = tx(w / 2, 22, 19, monthName(mo) + ' ' + yr, MID + BOLD);
      if (kw) s += tx(cw / 2, 40, 13, t('sWeek'), MID);
      t('wdHead').forEach(function (d, k) { s += tx((kw ? cw : 0) + cw * k + cw / 2, 40, 13, kw ? d : d.charAt(0), MID); });
      for (var r = 0; r < rows; r++) {
        var y0 = top + rh * r, rowStart = two ? new Date(monday.getFullYear(), monday.getMonth(), monday.getDate() + 7 * r)
                                                 : new Date(yr, mo, 1 + 7 * r - start);
        if (kw) s += tx(cw / 2, y0 + 17, 14, isoWeek(rowStart), MID);
        for (var c = 0; c < 7; c++) {
          var dt = new Date(rowStart.getFullYear(), rowStart.getMonth(), rowStart.getDate() + c);
          if (!two && dt.getMonth() !== mo) continue;
          var x = (kw ? cw : 0) + cw * c + cw / 2, isToday = dt.toDateString() === now.toDateString(), wk = c >= 5 ? BOLD : '';
          if (isToday) s += rc(x - 14, y0 + 2, 28, 21, '#000', 0, ' rx="4"') + tx(x, y0 + 18, 16, dt.getDate(), MID + BOLD + ' fill="#fff"');
          else s += tx(x, y0 + 18, 16, dt.getDate(), MID + wk);
        }
      }
      return s;
    }
    case 'stocks': {
      // "SYMBOL" or "SYMBOL:Name" (own name, build 78)
      var syms = String(cur('stocks') || '').split(',').map(function (x) { return x.trim(); }).filter(Boolean).slice(0, 5);
      if (!syms.length) syms = ['AAPL', 'NVDA', '^IXIC'];
      return head(t('sStocks'), w) + rowsSvg(syms, w, hh, function (sym, y) {
        var parts = sym.split(':'), label = (parts[1] || '').trim() || parts[0].replace('^IXIC', 'NASDAQ');
        var k = parts[0].length * 37 % 90, up = k % 3 !== 0;
        return tx(12, y, 17, label, BOLD) + tx(Math.min(170, w / 2), y, 17, num(100 + k * 2.7, 2)) +
          tx(w - 12, y, 17, (up ? '+' : '−') + num(k / 30, 1) + ' %', END);
      });
    }
    case 'tracking':
      return head(t('sParcels'), w) + rowsSvg(t('sParcelRows'), w, hh, function (e, y) { return tx(12, y, 17, e[0], BOLD) + tx(92, y, 17, e[1]); });
    case 'printer':
      return tx(14, 28, 17, t('sPrinting', { name: cur('printer_name') || t('sPrinter') }), BOLD) + tx(14, 74, 40, '62 %', BOLD) +
        rc(14, 86, w - 28, 16) + rc(14, 86, (w - 28) * 0.62, 16, '#000', 0) + tx(14, 128, 16, t('sPrintLeft'));
    case 'tasks':
      return head(t('sTasks'), w) + rowsSvg(sampleTasks(), w, hh, function (k, y) {
        return rc(12, y - 14, 15, 15) + tx(36, y, 17, k[0]) + (k[1] ? tx(w - 12, y, 15, k[1], END + (k[1] === t('sOverdue') ? BOLD : '')) : '');
      });
    case 'notes': {
      var note = S.p && S.p.note ? S.p.note : t('sNote');
      var top0 = hh >= 100 ? 56 : 30;  // the device draws the heading in boxes of 100 px and more
      var lines = wrapText(note, Math.max(6, Math.floor((w - 24) / 10)), Math.max(1, Math.floor((hh - top0 + 18) / 26)));
      lines.forEach(function (l, k) { s += tx(12, top0 + k * 26, 19, l); });
      return (hh >= 100 ? head(t('sNoteHead'), w) : '') + s;
    }
    case 'birthdays':
      return head(t('sBirthdays'), w) + rowsSvg(sampleBirthdays(), w, hh, function (b, y) { return tx(12, y, 17, b[0], BOLD) + tx(110, y, 17, b[1]); });
  }
  return tx(w / 2, hh / 2, 17, key, MID);
}

// ISO 8601 week number (as holidays::isoWeek on the device).
function isoWeek(d) {
  var t0 = new Date(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate())), wd = (t0.getUTCDay() + 6) % 7;
  t0.setUTCDate(t0.getUTCDate() - wd + 3);  // the Thursday of this week decides the year
  var jan4 = new Date(Date.UTC(t0.getUTCFullYear(), 0, 4));
  return 1 + Math.round(((t0 - jan4) / 86400000 - 3 + (jan4.getUTCDay() + 6) % 7) / 7);
}

function boxRect(b, statusBar) {
  var g = grid(), top = statusBar ? g.statusH : 0, ch = statusBar ? g.cellH : Math.floor(480 / g.rows);
  var y = top + b.r * ch;
  return { x: b.c * g.cellW, y: y, w: b.w * g.cellW, h: b.r + b.h >= g.rows ? 480 - y : b.h * ch };
}

function boxSvg(key, R, label, picked) {
  var W = widgetsByKey()[key], inner;
  if (!key) {
    inner = rc(8, 8, R.w - 16, R.h - 16, 'none', 2, ' stroke-dasharray="8 6"') + tx(R.w / 2, R.h / 2 + 6, 18, t('sEmpty', { box: label }), MID);
  } else if (W && W.configured === false) {
    inner = tx(R.w / 2, R.h / 2 - 6, 18, W.name || key, MID + BOLD);
    wrapText(W.hint || t('sNotSet'), Math.max(8, Math.floor((R.w - 20) / 8)), 2).forEach(function (l, k) {
      inner += tx(R.w / 2, R.h / 2 + 18 + k * 20, 15, l, MID);
    });
  } else {
    inner = widgetSvg(key, R.w, R.h);
  }
  var s = '<svg x="' + R.x + '" y="' + R.y + '" width="' + R.w + '" height="' + R.h + '">' + inner + '</svg>';
  if (picked) s += rc(R.x + 4, R.y + 4, R.w - 8, R.h - 8, 'none', 6);
  return s;
}

function statusBarSvg(x, w, hh) {
  var np = nowParts(), d = S.state.device || {};
  return '<g transform="translate(' + x + ' 0)">' + tx(10, hh - 8, 15, np.short, BOLD) +
    tx(w - 10, hh - 8, 15, t('wifi') + ' ▮▮▮ · ' + (d.battery >= 0 ? d.battery + ' %' : 'USB'), END + BOLD) + ln(0, hh - 1, w, hh - 1) + '</g>';
}

function sketch(sel) {
  var lay = S.lay, W = widgetsByKey(), names = [];
  var s = '';
  if (lay.classic) {
    s += statusBarSvg(440, 360, 28);
    CLASSIC.forEach(function (c) {
      var on = c[0] === 'clock' || lay.show.indexOf(c[0]) >= 0;
      if (!on) return;
      names.push((W[c[0]] && W[c[0]].name) || c[0]);
      s += boxSvg(c[0], { x: c[1], y: c[2], w: c[3], h: c[4] }, '', false);
    });
  } else {
    if (lay.statusBar) s += statusBarSvg(0, 800, grid().statusH);
    lay.boxes.forEach(function (b, i) {
      names.push(b.widget ? ((W[b.widget] && W[b.widget].name) || b.widget) : t('sEmptyBox'));
      s += boxSvg(b.widget, boxRect(b, lay.statusBar), t('boxN', { n: i + 1 }), i === sel);
    });
  }
  var label = t('sketchLabel', { list: names.length ? names.join(', ') : t('sNothing') });
  return '<svg viewBox="0 0 800 480" role="img" aria-label="' + esc(label) + '" xmlns="http://www.w3.org/2000/svg">' + s + '</svg>';
}
function sketchEl(sel) { return h('div', { class: 'epd', html: sketch(sel) }); }

function structIcon(s) {
  var g = '';
  if (s.classic) {
    CLASSIC.forEach(function (c) {
      g += '<rect x="' + (c[1] * 60 / 800 + 0.8).toFixed(1) + '" y="' + (c[2] * 42 / 480 + 0.8).toFixed(1) + '" width="' + (c[3] * 60 / 800 - 1.6).toFixed(1) +
        '" height="' + (c[4] * 42 / 480 - 1.6).toFixed(1) + '" rx="1" fill="none" stroke="currentColor" stroke-width="1.4"/>';
    });
  } else {
    g = '<rect x="1" y="1" width="58" height="2.5" fill="currentColor"/>';
    s.boxes.forEach(function (b) {
      g += '<rect x="' + (b.c * 15 + 1) + '" y="' + (b.r * 12 + 5) + '" width="' + (b.w * 15 - 2) + '" height="' + (b.h * 12 - 2) +
        '" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.6"/>';
    });
  }
  return '<svg viewBox="0 0 60 42" aria-hidden="true">' + g + '</svg>';
}

// Thumbnail of a ready-made layout: its boxes with short widget names (120 x 84).
function layoutThumb(s) {
  var short = t('short'), g = '', rects = s.classic ?
    CLASSIC.map(function (c) { return [c[1] * 120 / 800, c[2] * 84 / 480, c[3] * 120 / 800, c[4] * 84 / 480, c[0]]; }) :
    s.boxes.map(function (b) { return [b.c * 30, b.r * 26 + 5, b.w * 30, b.h * 26, b.widget]; });
  if (!s.classic) g = '<rect x="1" y="1" width="118" height="2.5" fill="currentColor"/>';
  rects.forEach(function (r) {
    var w = r[2] - 2, hh = r[3] - 2, name = r[4] ? short[r[4]] || r[4] : '', max = Math.floor((w - 2) / 4.9);
    if (name.length > max) name = name.slice(0, Math.max(1, max - 1)) + '.';
    g += '<rect x="' + (r[0] + 1).toFixed(1) + '" y="' + (r[1] + 1).toFixed(1) + '" width="' + w.toFixed(1) + '" height="' + hh.toFixed(1) +
      '" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.4"/>';
    if (name) g += '<text x="' + (r[0] + r[2] / 2).toFixed(1) + '" y="' + (r[1] + r[3] / 2 + 3).toFixed(1) + '" font-size="8.5" text-anchor="middle" fill="currentColor">' + esc(name) + '</text>';
  });
  return '<svg viewBox="0 0 120 84" aria-hidden="true">' + g + '</svg>';
}
function stripOf(list, big, label) {
  return h('div', { class: 'strip' + (big ? ' big' : ''), role: 'group', 'aria-label': label }, list.map(function (s) {
    var on = S.lay.classic ? !!s.classic : (!s.classic && S.lay.key === s.key);
    var b = h('button', { type: 'button', 'aria-pressed': String(on), 'data-f': 'st:' + s.key, onclick: function () { chooseStructure(s); } });
    b.innerHTML = big ? layoutThumb(s) : structIcon(s);
    add(b, structName(s));
    return b;
  }));
}
// Two rows: the device's ready-made layouts (with widgets) + Classic, then the empty structures.
function structStrip() {
  var all = structures(), ready = all.filter(function (s) { return !isGeneric(s); }), empty = all.filter(isGeneric);
  return [lbl(t('readyMade')), stripOf(ready, true, t('readyMade')),
    lbl(t('emptyStructs')), stripOf(empty, false, t('emptyStructs')),
    h('p', { class: 'note' }, t('emptyNote'))];
}

// ---------- fields ----------

function fieldBox(key, label, control, note, after) {
  var id = control.id || (control.id = uid());
  var e = S.errors[key] || '';
  var errEl = h('p', { class: 'err', id: id + 'e', hidden: !e }, e);
  if (e) control.setAttribute('aria-invalid', 'true');
  control.setAttribute('aria-describedby', id + 'e' + (note ? ' ' + id + 'n' : ''));
  var wrap = h('div', { class: 'f' + (e ? ' bad' : ''), 'data-k': key }, h('label', { for: id }, label), control, after,
    note ? h('p', { class: 'note', id: id + 'n' }, note) : null, errEl);
  control.addEventListener('input', function () {
    if (!S.errors[key]) return;
    delete S.errors[key];
    wrap.className = 'f';
    errEl.hidden = true;
    control.removeAttribute('aria-invalid');
    updateBar();
  });
  return wrap;
}

function numOr(v) { v = String(v).trim(); return /^-?\d+$/.test(v) ? parseInt(v, 10) : v; }

function textField(key, label, o) {
  o = o || {};
  var v = cur(key);
  var inp = h('input', { type: o.type || 'text', value: v === null || v === undefined ? '' : String(v), placeholder: o.ph, maxlength: o.max,
    inputmode: o.im, autocomplete: 'off', autocapitalize: o.cap || 'off', autocorrect: 'off', spellcheck: 'false', list: o.list, 'data-f': 'in:' + key });
  inp.addEventListener('input', function () {
    setVal(key, o.num ? (inp.value.trim() === '' ? 0 : numOr(inp.value)) : inp.value);
    if (o.onin) o.onin(inp);
  });
  return fieldBox(key, label, inp, o.note, o.after);
}

// Secret: "Saved ✓" placeholder; a string goes out only when typed, null after "Remove".
function secretField(key, label, o) {
  o = o || {};
  var info = secretInfo(key), v = S.secrets[key], removing = v === null;
  var ph = removing ? t('willRemove') : info.set ? t('savedTypeReplace', { hint: info.hint ? ' (' + info.hint + ')' : '' }) : (o.ph || '');
  var inp = h('input', { type: o.pw ? 'password' : (o.type || 'text'), value: typeof v === 'string' ? v : '', placeholder: ph,
    autocomplete: o.pw ? 'new-password' : 'off', autocapitalize: 'off', autocorrect: 'off', spellcheck: 'false', inputmode: o.im, 'data-f': 'in:' + key });
  inp.addEventListener('input', function () {
    setSecret(key, inp.value === '' ? (removing ? null : undefined) : inp.value);
    if (o.onin) o.onin(inp);
  });
  var tools = [];
  if (o.pw) {
    var show = h('button', { type: 'button', class: 'btn sm', 'aria-pressed': 'false', onclick: function () {
      var on = inp.type === 'password';
      inp.type = on ? 'text' : 'password';
      show.setAttribute('aria-pressed', String(on));
      show.textContent = on ? t('hide') : t('show');
    } }, t('show'));
    tools.push(show);
  }
  if (o.extra) tools = tools.concat(o.extra);
  if (info.set && !removing) tools.push(btn(t('remove'), function () { setSecret(key, null); render(); focusF('rm:' + key); }, 'sm', { 'data-f': 'rm:' + key }));
  if (removing) tools.push(btn(t('keepSaved'), function () { setSecret(key, undefined); render(); focusF('in:' + key); }, 'sm', { 'data-f': 'rm:' + key }));
  var after = [];
  if (tools.length) after.push(h('div', { class: 'btns', style: 'margin-top:8px' }, tools));
  if (o.after) after.push(o.after);
  return fieldBox(key, label, inp, o.note, after);
}

function switchRow(title, sub, on, fn, f) {
  return h('button', { type: 'button', class: 'sw', role: 'switch', 'aria-checked': String(!!on), 'data-f': f, onclick: fn },
    h('span', { class: 't' }, title, sub ? h('small', null, sub) : null), h('span', { class: 'k', 'aria-hidden': 'true' }));
}
function settingSwitch(key, title, sub) {
  return switchRow(title, sub, !!cur(key), function () { setVal(key, !cur(key)); render(); }, 'sw:' + key);
}
function seg(label, options, value, fn, f) {
  return h('div', { class: 'seg', role: 'group', 'aria-label': label }, options.map(function (o, i) {
    return h('button', { type: 'button', 'aria-pressed': String(same(o[0], value)), 'data-f': f + i, onclick: function () { fn(o[0]); } }, o[1]);
  }));
}
function resultEl(res) {
  var p = h('p', { class: 'msg', role: 'status' });
  showResult(p, res);
  return p;
}
function showResult(p, res) {
  p.innerHTML = '';
  p.className = 'msg';
  if (!res) return;
  if (res.busy) { add(p, [spin(), res.text]); return; }
  p.className = res.ok ? 'msg okm' : 'err';
  add(p, res.text);
}

// ---------- calendars, places, Wi-Fi, time zone (shared by tabs and the guide) ----------

function calendarField(i) {
  var key = 'ics.' + i;
  var res = resultEl(S.res[key]);
  var check = btn(t('check'), function () { checkCalendar(key, i, res); }, 'sm', { 'data-f': 'chk:' + key });
  return secretField(key, t('calLinkN', { n: i + 1 }), { type: 'url', im: 'url', ph: 'https://calendar.google.com/…/basic.ics', extra: [check], after: res });
}
// Stage 8: per link a name (shown instead of "Kalender 1", also on the screen) and how often it's read.
function calNameField(i) {
  var names = cur('ics_names') || ['', '', ''];
  var inp = h('input', { type: 'text', value: names[i] || '', placeholder: t('calLinkN', { n: i + 1 }), maxlength: 20,
    autocomplete: 'off', autocapitalize: 'words', 'data-f': 'in:ics_names.' + i });
  inp.addEventListener('input', function () {
    var a = (cur('ics_names') || ['', '', '']).slice();
    a[i] = inp.value;
    setVal('ics_names', a);
  });
  return fieldBox('ics_names.' + i, t('calName'), inp, i === 0 ? t('calNameNote') : null);
}
function calEverySeg(i) {
  var e = cur('ics_every') || [0, 0, 0];
  return [h('p', { class: 'note' }, t('calEvery')), seg(t('calEvery'), [[0, t('calEveryCal')], [1, t('calEvery3h')], [2, t('calEveryMorning')]],
    e[i] || 0, function (v) { var a = (cur('ics_every') || [0, 0, 0]).slice(); a[i] = v; setVal('ics_every', a); render(); }, 'ev' + i + '-'),
    errLine('ics_every.' + i)];
}
// Stage 7: does calendar link i mark days in the month view (setting ics_month, off by default)?
function monthSwitch(i) {
  var m = (cur('ics_month') || [false, false, false]).slice();
  return switchRow(t('monthMark'), t('monthMarkSub'), !!m[i], function () { m[i] = !m[i]; setVal('ics_month', m); render(); }, 'sw:ics_month.' + i);
}
// Stage 7: public holidays and school holidays of a Bundesland (the list comes from the device).
function holidaysCard() {
  var st = cur('holiday_state') || '', list = S.state.holidayStates || [];
  var sel = h('select', { id: uid(), 'data-f': 'in:holiday_state', onchange: function () {
    setVal('holiday_state', sel.value);
    if (sel.value && !cur('holidays_on') && !cur('school_on')) { setVal('holidays_on', true); setVal('school_on', true); }
    render();
  } }, [h('option', { value: '', selected: !st }, t('stateOff'))].concat(list.map(function (s) {
    return h('option', { value: s.key, selected: s.key === st }, s.name);
  })));
  var parts = [fieldBox('holiday_state', t('f_holiday_state'), sel, t('stateNote'))];
  if (st) parts.push(settingSwitch('holidays_on', t('holidaysTitle'), t('holidaysSub')), settingSwitch('school_on', t('schoolTitle'), t('schoolSub')));
  if (st && cur('school_on')) {
    parts.push(textField('school_url', t('f_school_url'), { type: 'url', im: 'url', note: t('schoolUrlNote'),
      ph: (S.state.device && S.state.device.schoolDefault) || 'https://…/ferien.ics' }));
  }
  return card(parts);
}
function checkCalendar(key, index, el) {
  var typed = S.secrets[key], body = null;
  if (typeof typed === 'string' && typed.trim()) body = { url: typed.trim() };
  else if (typed !== null && secretInfo(key).set) body = key === 'bday_url' ? { bday: true } : index >= 0 ? { index: index } : null;
  if (!body) { S.res[key] = { ok: false, text: t('pasteFirst') }; showResult(el, S.res[key]); return; }
  S.res[key] = { busy: true, text: t('checking') };
  showResult(el, S.res[key]);
  api('POST', '/api/check-calendar', body, { timeout: 40000 }).then(function (r) {
    S.res[key] = r.ok ? { ok: true, text: tn('calWorks', r.events || 0) } :
      { ok: false, text: r.offline ? t('offlineCal') : (r.error || t('calBad')) };
    showResult(el, S.res[key]);
  });
}

function placeField() {
  var list = h('div', { class: 'list sugg', hidden: true });
  var info = h('p', { class: 'note', 'aria-live': 'polite' });
  var timer, seq = 0;
  return textField('weather_loc', t('f_weather_loc'), {
    ph: 'Berlin, DE', cap: 'words', after: [list, info], note: t('placeNote'),
    onin: function (inp) {
      clearTimeout(timer);
      var q = inp.value.trim();
      seq++;
      if (q.length < 2 || /^-?\d/.test(q)) { list.hidden = true; info.textContent = ''; return; }
      timer = setTimeout(function () {
        var my = seq;
        info.textContent = t('placeSearching');
        api('GET', '/api/places?q=' + encodeURIComponent(q), undefined, { timeout: 15000 }).then(function (r) {
          if (my !== seq) return;
          list.innerHTML = '';
          list.hidden = true;
          if (r.offline) { info.textContent = t('placeOffline'); return; }
          if (!r.ok) { info.textContent = (r.noAnswer ? t('placeNoAnswer') : t('placeNone')) + t('placeStillType'); return; }
          var places = r.places || [];
          info.textContent = places.length ? t('placeCount', { n: places.length }) : t('placeNotFound');
          places.forEach(function (p) {
            list.appendChild(h('button', { type: 'button', class: 'it', onclick: function () {
              inp.value = p.name;
              setVal('weather_loc', p.name);
              list.hidden = true;
              info.textContent = t('placePicked', { p: p.detail || p.name });
              inp.focus();
            } }, h('span', { class: 't' }, p.name, p.detail ? h('small', null, p.detail) : null)));
          });
          list.hidden = !places.length;
        });
      }, 500);
    }
  });
}

function signalWord(rssi) { return rssi >= -60 ? t('sigStrong') : rssi >= -72 ? t('sigGood') : t('sigWeak'); }
function bars(rssi) {
  var n = rssi >= -60 ? 3 : rssi >= -72 ? 2 : 1;
  return h('span', { class: 'bars', 'aria-hidden': 'true' }, [6, 11, 16].map(function (hh, i) {
    return h('i', { class: i < n ? 'on' : '', style: 'height:' + hh + 'px' });
  }));
}
function scanNetworks() {
  S.scan = { busy: true };
  render();
  api('GET', '/api/networks', undefined, { timeout: 20000 }).then(function (r) {
    S.scan = r.ok ? null : { error: r.error };
    if (r.ok) S.nets = r.networks || [];
    render();
  });
}
function wifiPart() {
  var ssid = cur('wifi_ssid') || '', out = [];
  if (saved('wifi_ssid')) out.push(h('p', { class: 'note' }, t('savedNet', { s: saved('wifi_ssid') })));
  var busy = S.scan && S.scan.busy;
  out.push(btn(busy ? t('netSearching') : S.nets ? t('netAgain') : t('netFind'), scanNetworks, 'wide', { disabled: busy, 'data-f': 'scan' }));
  if (busy) out.push(h('p', { class: 'msg', role: 'status' }, spin(), t('netWait')));
  if (S.scan && S.scan.error) out.push(h('p', { class: 'err', role: 'alert' }, S.scan.error));
  if (S.nets) {
    if (!S.nets.length) out.push(h('p', { class: 'note' }, t('netNone')));
    else out.push(h('div', { class: 'list' }, S.nets.map(function (n, i) {
      return h('button', { type: 'button', class: 'it', 'aria-pressed': String(n.ssid === ssid), 'data-f': 'net' + i, onclick: function () {
        setVal('wifi_ssid', n.ssid);
        S.netOpen = !n.secure;
        S.res.wifi = null;
        render();
        if (n.secure) focusF('in:wifi_pass');
      } }, bars(n.rssi), h('span', { class: 't' }, n.ssid, h('small', null, (n.secure ? t('locked') : t('open')) + ', ' + signalWord(n.rssi))),
      n.ssid === ssid ? h('span', { 'aria-hidden': 'true' }, '✓︎') : null);
    })));
  }
  out.push(textField('wifi_ssid', t('netName'), { note: t('netNameNote'), onin: function () { S.res.wifi = null; } }));
  out.push(secretField('wifi_pass', t('password'), { pw: true, onin: function () { S.res.wifi = null; } }));
  var res = resultEl(S.res.wifi);
  out.push(btn(t('wifiTestBtn'), function () { testWifi(res); }, '', { 'data-f': 'wtest' }), res);
  return out;
}
// A hint for the device's reason (it may come in German or English).
function wifiFix(err) {
  var e = String(err || '').toLowerCase();
  if (e.indexOf('passw') >= 0) return t('fixPw');
  if (e.indexOf('found') >= 0 || e.indexOf('gefunden') >= 0) return t('fixFound');
  if (e.indexOf('answer') >= 0 || e.indexOf('antwort') >= 0) return t('fixAnswer');
  return '';
}
function testWifi(el, then) {
  var ssid = String(cur('wifi_ssid') || '').trim(), typed = S.secrets.wifi_pass;
  var body = { ssid: ssid };
  var fail = function (msg) { S.res.wifi = { ok: false, text: msg }; if (el) showResult(el, S.res.wifi); if (then) then(false); };
  if (!ssid) return fail(t('pickNet'));
  if (typeof typed === 'string') body.pass = typed;
  else if (S.netOpen) body.pass = '';
  else if (ssid !== saved('wifi_ssid') || !secretInfo('wifi_pass').set) return fail(t('typePw'));
  S.res.wifi = { busy: true, text: t('connecting', { s: ssid }) };
  if (el) showResult(el, S.res.wifi);
  api('POST', '/api/wifi-test', body, { timeout: 35000 }).then(function (r) {
    S.res.wifi = r.ok ? { ok: true, text: t('connected') } :
      { ok: false, text: t('notConnected', { e: r.error || t('unknownReason') }) + wifiFix(r.error) };
    if (el) showResult(el, S.res.wifi);
    if (then) then(r.ok);
  });
}

function phoneTz() {
  var name = '';
  try { name = Intl.DateTimeFormat().resolvedOptions().timeZone || ''; } catch (e) { name = ''; }
  if (!name) return null;
  for (var i = 0; i < TZ_NAMES.length; i++) if (TZ_NAMES[i].toLowerCase() === name.toLowerCase()) return { value: TZ_NAMES[i], name: name };
  var y = new Date().getFullYear(), a = new Date(y, 0, 1).getTimezoneOffset(), b = new Date(y, 6, 1).getTimezoneOffset();
  if (a !== b) return { value: '', name: name };
  // No summer time: a fixed POSIX rule (POSIX counts west of UTC as positive).
  var east = -a, abs = Math.abs(east), hh = Math.floor(abs / 60), mm = abs % 60;
  var tag = '<' + (east >= 0 ? '+' : '-') + pad(hh) + (mm ? pad(mm) : '') + '>';
  return { value: tag + (east > 0 ? '-' : '') + hh + (mm ? ':' + pad(mm) : ''), name: name };
}
function tzField() {
  var dl = uid(), tz = S.tz, names = t('tzNames');
  var note = t('tzNote');
  if (tz && tz.value && S.tzFilled && cur('timezone') === tz.value) note += t('tzFilled', { n: tz.name });
  if (tz && !tz.value && !cur('timezone')) note += t('tzUnknown', { n: tz.name });
  var opts = TZ_LIST.map(function (z, i) { return h('option', { value: z }, names[i]); });
  return textField('timezone', t('f_timezone'), { list: dl, ph: 'Europe/Berlin', note: note, after: h('datalist', { id: dl }, opts) });
}

// ---------- screens: loading, error, login, restart ----------

function fullCard() { var c = card.apply(null, arguments); return h('div', { class: 'full' }, c); }

function errorScreen(text) {
  return function () {
    return fullCard(h('h1', null, t('cantReach')), h('p', null, text),
      btn(t('tryAgain'), load, 'pri wide', { 'data-f': 'retry' }),
      h('p', { class: 'note' }, t('basicHint')));
  };
}

function needLogin() {
  if (S.screen === loginScreen) return;
  closeSheet();
  S.screen = loginScreen;
  S.loginErr = '';
  render();
  focusF('code');
}
function loginScreen() {
  var inp = h('input', { type: 'text', id: 'code', class: 'code', inputmode: 'numeric', pattern: '[0-9]*', autocomplete: 'one-time-code', maxlength: 6, 'data-f': 'code' });
  var busy = false;
  function submit(e) {
    if (e) e.preventDefault();
    if (busy) return;
    var code = inp.value.replace(/\D/g, '');
    if (code.length !== 6) { S.loginErr = t('all6'); err.textContent = S.loginErr; err.hidden = false; return; }
    busy = true;
    go.disabled = true;
    api('POST', '/api/login', { code: code }).then(function (r) {
      busy = false;
      go.disabled = false;
      if (r.ok) { S.screen = null; load(); return; }
      S.loginErr = t('codeRetry', { e: r.error || t('wrongCode') });
      err.textContent = S.loginErr;
      err.hidden = false;
      inp.value = '';
      inp.focus();
    });
  }
  inp.addEventListener('input', function () {
    inp.value = inp.value.replace(/\D/g, '').slice(0, 6);
    if (inp.value.length === 6) submit();
  });
  var go = h('button', { type: 'submit', class: 'btn pri wide' }, t('openSetup'));
  var err = h('p', { class: 'err', role: 'alert', hidden: !S.loginErr }, S.loginErr);
  return fullCard(h('h1', null, t('codeTitle')), h('p', { class: 'note' }, t('codeWhy')),
    h('form', { onsubmit: submit }, h('div', { class: 'f' }, h('label', { for: 'code' }, t('code')), inp), err, h('div', { style: 'margin-top:12px' }, go)));
}

// A full-screen message. `lines` is a function so the texts follow a language change.
function infoScreen(title, lines, poll) {
  S.poll = { tries: 0, gaveUp: false };
  S.screen = function () {
    var p = S.poll || {};
    return fullCard(h('h1', null, title()), lines().map(function (l) { return h('p', null, l); }),
      poll ? h('p', { class: 'msg', role: 'status' }, p.gaveUp ? t('stillNoAnswer') : [spin(), t('waiting')]) : null,
      poll && p.gaveUp ? btn(t('reload'), function () { location.reload(); }, 'wide') : null);
  };
  closeSheet();
  render();
  window.scrollTo(0, 0);
  if (poll) setTimeout(pollBack, 6000);
}
function pollBack() {
  if (!S.poll || S.poll.gaveUp) return;
  S.poll.tries++;
  api('GET', '/api/state', undefined, { timeout: 4000 }).then(function (r) {
    if (!S.poll) return;
    if (r.ok && r.device) { S.poll = null; start(r); toast(t('deviceBack')); return; }
    if (r.login) return;
    if (S.poll.tries >= 50) { S.poll.gaveUp = true; render(); return; }
    setTimeout(pollBack, 3000);
  });
}
// After POST /api/settings: `stay` true = the device comes back in the same mode (poll and
// reload, same tab); false = setup ended, it shows the dashboard. Missing (older firmware): poll.
function restarting(r) {
  var home = S.state.device.mode === 'home';
  if (r && r.stay === false) {
    infoScreen(function () { return t('finishedTitle'); },
      function () { return [t('finishedText'), home ? t('finishedHome') : t('finishedAp')]; }, false);
    return;
  }
  infoScreen(function () { return t('restartTitle'); }, function () { return [home ? t('restartHome') : t('restartAp')]; }, true);
}

// ---------- sheet / dialog ----------

function openSheet(title, sub, content, back) {
  closeSheet();
  var id = uid();
  var scrim = h('div', { class: 'scrim', onclick: function () { closeSheet(); focusF(back); } });
  var box = h('div', { class: 'sheet', role: 'dialog', 'aria-modal': 'true', 'aria-labelledby': id },
    h('div', { class: 'in' }, h('div', { class: 'grab' }),
      h('div', { class: 'row' }, h('h2', { id: id, class: 'grow' }, title), h('button', { type: 'button', class: 'lnk', onclick: function () { closeSheet(); focusF(back); } }, t('close'))),
      sub ? h('p', { class: 'note' }, sub) : null, content));
  document.body.appendChild(scrim);
  document.body.appendChild(box);
  S.sheet = { scrim: scrim, box: box, back: back };
  var first = box.querySelector('.in > :not(.row) button:not([disabled])') || box.querySelector('button');
  if (first) first.focus();
}
function closeSheet() {
  if (!S.sheet) return;
  S.sheet.scrim.remove();
  S.sheet.box.remove();
  S.sheet = null;
}
function ask(title, text, okLabel, fn, back) {
  openSheet(title, text, h('div', { class: 'btns' },
    btn(t('cancel'), function () { closeSheet(); focusF(back); }),
    btn(okLabel, function () { closeSheet(); fn(); }, 'pri')), back);
}
document.addEventListener('keydown', function (e) {
  if (!S.sheet) return;
  if (e.key === 'Escape') { var b = S.sheet.back; closeSheet(); focusF(b); return; }
  if (e.key !== 'Tab') return;
  var f = S.sheet.box.querySelectorAll('button:not([disabled]), input, select, textarea');
  if (!f.length) return;
  var first = f[0], last = f[f.length - 1];
  if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last.focus(); }
  else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); }
  else if (!S.sheet.box.contains(document.activeElement)) { e.preventDefault(); first.focus(); }
});

// ---------- bottom bar ----------

function updateBar() { if (S.barEl) fillBar(); }
function setBusy(text) { S.busy = text; updateBar(); }
function fillBar() {
  var el = S.barEl, inner = h('div', { class: 'in' }), guide = isGuide();
  el.innerHTML = '';
  var errs = Object.keys(S.errors);
  if (errs.length && !guide) {
    inner.appendChild(h('div', { role: 'alert' }, h('p', { class: 'err' }, tn('fixFields', errs.length)),
      h('div', { class: 'btns' }, errs.map(function (k) {
        if (!has(FIELDS, k.split('.')[0])) return h('p', { class: 'msg' }, k + ': ' + S.errors[k]);
        return h('button', { type: 'button', class: 'lnk', onclick: function () { goField(k); } }, fieldLabel(k));
      }))));
  }
  if (S.barMsg) inner.appendChild(h('p', { class: 'err', role: 'alert' }, S.barMsg));
  if (S.busy) inner.appendChild(h('p', { class: 'msg', role: 'status' }, spin(), S.busy));
  if (guide) guideButtons(inner);
  else {
    var n = dirtyCount();
    if (n) inner.appendChild(btn(tn('saveN', n), saveSettings, 'pri wide', { disabled: !!S.busy, 'data-f': 'b-save' }));
    if (S.tab === 'screen') {
      var ok = layoutCheck().ok;
      inner.appendChild(h('div', { class: 'btns' },
        btn(t('exactPreview'), exactPreview, '', { disabled: !ok || !!S.busy, 'data-f': 'b-prev' }),
        btn(t('showOnDisplay'), sendLayout, n ? '' : 'pri', { disabled: !ok || !!S.busy, 'data-f': 'b-lay' })));
    }
  }
  el.hidden = !inner.childNodes.length;
  el.appendChild(inner);
}
function goField(k) {
  var tab = fieldTab(k);
  if (S.tab !== tab) { S.tab = tab; render(); }
  if (k.indexOf('pins') === 0) { S.adv = true; render(); }
  var w = document.querySelector('[data-k="' + k + '"]');
  if (w) {
    w.scrollIntoView({ block: 'center' });
    var c = w.querySelector('input, select, textarea');
    if (c) c.focus();
  }
}
function normErrors(errors) {
  var out = {};
  Object.keys(errors || {}).forEach(function (k) { out[normKey(k)] = String(errors[k]); });
  if (out.ics) { out['ics.0'] = out['ics.0'] || out.ics; delete out.ics; }
  if (out.pins) { out['pins.epd_cs'] = out['pins.epd_cs'] || out.pins; delete out.pins; }
  return out;
}

// ---------- saving ----------

function postLayoutFirst() {
  if (!layoutDirty() || !layoutCheck().ok) return Promise.resolve({ ok: true });
  return api('POST', '/api/layout', layoutBody(), { timeout: 20000 }).then(function (r) {
    if (r.ok) S.laySaved = JSON.stringify(layoutBody());
    else if (!r.login) r.error = t('layoutNotTaken', { e: r.error || t('unknownReason') });
    return r;
  });
}
function saveSettings() {
  var body = settingsBody();
  if (!Object.keys(body).length) return;
  S.barMsg = '';
  setBusy(t('saving'));
  postLayoutFirst().then(function (r) {
    if (!r.ok) return r;
    return api('POST', '/api/settings', body, { timeout: 15000 });
  }).then(function (r) {
    S.busy = '';
    if (r.ok) { restarting(r); return; }
    if (r.login) return;
    if (r.errors && Object.keys(r.errors).length) {
      S.errors = normErrors(r.errors);
      S.barMsg = '';
      render();
      toast(t('nothingSaved'));
    } else {
      S.barMsg = r.error || t('saveFailed');
      render();
    }
  });
}
function sendLayout() {
  S.barMsg = '';
  setBusy(t('sendingLayout'));
  api('POST', '/api/layout', layoutBody(), { timeout: 20000 }).then(function (r) {
    S.busy = '';
    if (r.ok) { S.laySaved = JSON.stringify(layoutBody()); toast(t('layoutSent')); }
    else if (!r.login) S.barMsg = t('layoutRefused', { e: r.error || t('unknownReason') });
    render();
  });
}
function exactPreview() {
  if (S.preview && S.preview.url) URL.revokeObjectURL(S.preview.url);
  S.preview = { busy: true };
  S.scrollPv = true;
  render();
  var body = layoutBody();
  body.clock_24h = cur('clock_24h') !== false;   // the unsaved choice counts for the preview
  api('POST', '/api/preview.bmp', body, { blob: true, timeout: 30000 }).then(function (r) {
    if (r.ok && r.blob) S.preview = { url: URL.createObjectURL(r.blob) };
    else if (r.login) S.preview = null;
    else S.preview = { err: r.status === 503 ? (r.error || t('noMemory')) + t('tryMoment') : r.error };
    S.scrollPv = true;
    render();
  });
}
// title / lines: functions (texts follow a language change).
function postAction(path, title, lines, back, body) {
  setBusy(t('asking'));
  api('POST', path, body || {}, { timeout: 15000 }).then(function (r) {
    S.busy = '';
    if (r.ok) { infoScreen(title, lines, false); return; }
    if (!r.login) { toast(r.error || t('didntWork')); updateBar(); focusF(back); }
  });
}
function changeWifi() {
  confirmUnsaved(t('chgWifiTitle'), t('chgWifiText'), t('chgWifiOk'), function () {
    postAction('/api/finish', function () { return t('chgWifiScreen'); }, function () { return [t('chgWifiText'), t('chgWifiThen')]; }, 'chgwifi', { ap: true });
  }, 'chgwifi');
}
function unsavedList() {
  var u = [];
  if (dirtyCount()) u.push(tn('uSettings', dirtyCount()));
  if (S.lay && layoutDirty()) u.push(t('uLayout'));
  if (S.p.tasks && JSON.stringify(S.p.tasks) !== S.p.tasksSaved) u.push(t('uTasks'));
  if (S.p.note !== null && S.p.note !== S.p.noteSaved) u.push(t('uNote'));
  if (S.p.bd && JSON.stringify(S.p.bd.manual) !== S.p.bdSaved) u.push(t('uBd'));
  return u;
}
function confirmUnsaved(title, text, ok, fn, back) {
  var u = unsavedList();
  ask(title, (u.length ? t('notSavedYet', { list: u.join(', ') }) : '') + text, ok, fn, back);
}
function installUpdate(back) {
  confirmUnsaved(t('updTitle'), t('updText'), t('install'), function () {
    postAction('/api/update', function () { return t('updScreen'); }, function () { return [t('updLine1'), t('updLine2')]; });
  }, back);
}
function finish() {
  confirmUnsaved(t('leaveTitle'), t('leaveText'), t('leaveOk'), function () {
    postAction('/api/finish', function () { return t('allSet'); }, function () { return [t('allSet1'), t('allSet2')]; }, 'finish');
  }, 'finish');
}

// ---------- Home tab ----------

// A Unix time as the local clock time, with the date when it isn't the device's today.
function clockAt(sec, now) {
  var d = new Date(sec * 1000), n = new Date((now || sec) * 1000);
  return d.toDateString() === n.toDateString() ? hmText(d) : t('fmtDayTime', { d: d.getDate(), mon: monShort(d.getMonth()), hm: hmText(d) });
}
// device.battery is the last reading with Wi-Fi off; batteryAt its time (null = measured now).
function batteryText(d, short) {
  if (d.usb) return t('onUsb');
  if (!(d.battery >= 0)) return t('noBatReading');
  var s = t('batteryPct', { p: d.battery });
  if (d.batteryAt) s += short ? ' (' + clockAt(d.batteryAt, d.time) + ')' : t('measured', { t: clockAt(d.batteryAt, d.time) });
  return s;
}
function homeTab() {
  var d = S.state.device, out = [], w = d.wifi || {};
  out.push(lbl(t('screen')), sketchEl(-1), h('p', { class: 'cap' }, layoutDirty() ? t('editedNotShown') : t('sketchCap')));
  var probs = (d.problems || []).slice();
  if (d.layoutFallback) {
    var why = typeof d.layoutFallback === 'string' ? d.layoutFallback : (d.layoutFallback.text || d.layoutFallback.reason || '');
    probs.unshift({ text: t('fallbackText', { why: why ? ': ' + why : '.' }), fix: t('fallbackFix') });
  }
  out.push(lbl(t('problems')));
  if (!probs.length) out.push(card(h('p', { class: 'msg okm' }, t('noProblems'))));
  probs.forEach(function (p) { out.push(h('div', { class: 'card warn' }, h('div', null, h('b', null, p.text), p.fix ? h('span', { class: 'note' }, p.fix) : null))); });
  if (d.updateAvailable) {
    out.push(lbl(t('update')), card(h('p', { class: 'big' }, t('updAvail', { v: d.updateAvailable })),
      h('p', { class: 'note' }, t('updAvailNote')),
      btn(t('installNow'), function () { installUpdate('upd'); }, 'pri wide', { 'data-f': 'upd' })));
  }
  out.push(lbl(t('device')), card(
    kv(t('battery'), batteryText(d)),
    S.state.power && S.state.power.note ? kv(t('power'), S.state.power.note) : null,
    S.state.power && S.state.power.saver ? h('p', { class: 'note' }, t('saverNote')) : null,
    kv(t('wifi'), w.ssid ? (w.connected ? w.ssid + ', ' + signalWord(w.rssi) : t('notConnNow', { s: w.ssid })) : t('notSetUp')),
    kv(t('version'), d.version + (d.build ? t('buildN', { n: d.build }) : '')),
    d.lastUpdate ? kv(t('lastUpdate'), d.lastUpdate) : null,
    kv(t('clock'), d.time ? dateTimeText(d.time) : t('clockNotSet'))));
  out.push(lbl(t('finishLbl')), card(h('p', null, t('finishText')), btn(t('done'), finish, 'pri wide', { 'data-f': 'finish' })));
  return out;
}

// ---------- Screen tab ----------

function boxList() {
  var W = widgetsByKey();
  return h('div', { class: 'list' }, S.lay.boxes.map(function (b, i) {
    var w = b.widget ? W[b.widget] : null, hint = '';
    if (b.widget && !w) hint = t('unknownWidget');
    else if (w && !Layout.fits(w, b)) hint = t('tooSmall', { name: w.name, w: w.minW, h: w.minH });
    else if (w && w.configured === false) hint = w.hint || t('sNotSet');
    return h('button', { type: 'button', class: 'it' + (i === S.sel ? ' sel' : ''), 'aria-current': i === S.sel ? 'true' : null, 'data-f': 'box' + i,
      onclick: function () { S.sel = i; render(); openPicker(i); } },
      icon(b.widget), h('span', { class: 't' }, w ? w.name : b.widget || t('emptyBoxT'),
        h('small', null, tn('boxInfo', b.w * b.h, { n: i + 1, w: b.w, h: b.h })), hint ? h('small', null, hint) : null),
      h('span', { class: 'ch', 'aria-hidden': 'true' }, '›'));
  }));
}
function splitInfo() {
  var b = S.lay.boxes[S.sel];
  if (!b) return t('selectBox');
  if (!Layout.canSplit(b)) return t('cantSplit1');
  if (S.lay.boxes.length >= Layout.MAX_BOXES) return t('maxBoxes', { n: Layout.MAX_BOXES });
  return '';
}
function doSplit() {
  var r = Layout.split(S.lay.boxes, S.sel, widgetList());
  if (!r || splitInfo()) return;
  S.lay.boxes = r;
  S.lay.key = 'custom';
  toast(t('splitDone', { a: S.sel + 1, b: S.sel + 2 }));
  layChanged();
  focusF('box' + S.sel);
}
function doMerge(j) {
  var i = S.sel, r = Layout.merge(S.lay.boxes, i, j);
  if (!r) return;
  S.lay.boxes = r;
  S.lay.key = 'custom';
  S.sel = Math.min(i, j);
  toast(t('mergeDone', { a: i + 1, b: j + 1, m: S.sel + 1 }));
  layChanged();
  focusF('box' + S.sel);
}
function openMerge() {
  var p = Layout.mergePartners(S.lay.boxes, S.sel), W = widgetsByKey(), a = S.lay.boxes[S.sel];
  if (!p.length) return;
  if (p.length === 1) { doMerge(p[0]); return; }
  var words = { right: t('dRight'), below: t('dBelow'), left: t('dLeft'), above: t('dAbove') };
  openSheet(t('mergeWith', { n: S.sel + 1 }), t('mergeKeeps', { n: S.sel + 1 }),
    h('div', { class: 'list' }, p.map(function (j) {
      var o = S.lay.boxes[j], w = o.widget && W[o.widget];
      return h('button', { type: 'button', class: 'it', onclick: function () { closeSheet(); doMerge(j); } }, icon(o.widget),
        h('span', { class: 't' }, t('boxDir', { n: j + 1, d: words[Layout.direction(a, o)] }), h('small', null, w ? w.name : t('emptyBoxT'))));
    })), 'merge');
}
function toolsRow(inSheet) {
  var why = splitInfo(), partners = Layout.mergePartners(S.lay.boxes, S.sel);
  var n = S.sel + 1;
  return [h('div', { class: 'tools' },
    btn(t('splitBox', { n: n }), function () { if (inSheet) closeSheet(); doSplit(); }, '', { disabled: !!why, 'data-f': inSheet ? null : 'split' }),
    btn(t('mergeBox', { n: n }) + (partners.length > 1 ? '…' : ''), function () { if (inSheet) closeSheet(); openMerge(); }, '', { disabled: !partners.length, 'data-f': inSheet ? null : 'merge' })),
  why || !partners.length ? h('p', { class: 'note', style: 'margin-top:6px' }, [why, partners.length ? '' : t('noMerge', { n: n })].join(' ').trim()) : null];
}
function openPicker(i) {
  var b = S.lay.boxes[i];
  var items = widgetList().map(function (w) {
    var fit = Layout.fits(w, b), used = S.lay.boxes.some(function (o, k) { return k !== i && o.widget === w.key; });
    var why = !fit ? t('needs', { w: w.minW, h: w.minH }) : used ? t('onScreen') : w.configured === false ? (w.hint || t('notSetUpLow')) : '';
    return h('button', { type: 'button', disabled: !fit || used, 'aria-pressed': String(b.widget === w.key), onclick: function () { pick(w.key); } },
      icon(w.key), h('span', null, w.name, why ? h('small', null, why) : null));
  });
  items.push(h('button', { type: 'button', 'aria-pressed': String(!b.widget), onclick: function () { pick(null); } }, icon(null), h('span', null, t('leaveEmpty'))));
  function pick(k) {
    S.lay.boxes[i].widget = k;
    closeSheet();
    layChanged();
    focusF('box' + i);
  }
  openSheet(t('pickTitle', { n: i + 1 }), tn('pickSub', b.w * b.h, { w: b.w, h: b.h }), [h('div', { class: 'wgrid' }, items), toolsRow(true)], 'box' + i);
}
function showList() {
  var W = widgetsByKey();
  var keys = CLASSIC.map(function (c) { return c[0]; }).filter(function (k) { return k !== 'clock' && W[k]; });
  return card(h('p', { class: 'note' }, t('classicNote')),
    h('div', null, keys.map(function (k) {
      var w = W[k], on = S.lay.show.indexOf(k) >= 0;
      return switchRow(w.name, w.configured === false ? w.hint : '', on, function () {
        var list = S.lay.show.filter(function (x) { return x !== k; });
        if (!on) list.push(k);
        S.lay.show = orderShow(list);
        layChanged();
      }, 'show:' + k);
    })));
}
function previewCard() {
  var p = S.preview, close = btn(t('closePreview'), function () { if (p.url) URL.revokeObjectURL(p.url); S.preview = null; render(); }, 'wide');
  if (p.busy) return h('div', { class: 'card', id: 'pv' }, h('p', { class: 'msg', role: 'status' }, spin(), t('drawing')));
  if (p.err) return h('div', { class: 'card', id: 'pv' }, h('p', { class: 'err', role: 'alert' }, t('noPreview', { e: p.err })), close);
  return h('div', { class: 'card', id: 'pv' }, h('p', { class: 'big' }, t('exactPreview')),
    h('div', { class: 'epd' }, h('img', { src: p.url, alt: t('previewAlt'), width: 800, height: 480 })),
    h('p', { class: 'note' }, t('previewNote')), close);
}
function screenTab() {
  var out = [lbl(t('preview')), sketchEl(S.lay.classic ? -1 : S.sel)];
  if (S.preview) out.push(previewCard());
  out.push(structStrip());
  if (S.lay.classic) out.push(lbl(t('showOnClassic')), showList());
  else {
    out.push(lbl(t('boxesLbl')), boxList(), toolsRow(false));
    var v = layoutCheck();
    if (!v.ok) out.push(h('div', { class: 'card', role: 'alert', style: 'margin-top:10px' }, h('p', { class: 'big' }, t('cantShow')),
      v.errors.map(function (e) { return h('p', { class: 'err' }, vText(e)); })));
    out.push(lbl(t('statusBar')), card(switchRow(t('sbOnTop'), t('sbSub'), S.lay.statusBar,
      function () { S.lay.statusBar = !S.lay.statusBar; layChanged(); }, 'sb')));
  }
  out.push(lbl(t('clock')), card(seg(t('clockFormat'), [[true, t('h24')], [false, t('h12')]], cur('clock_24h') !== false,
    function (v) { setVal('clock_24h', v); render(); }, 'c24-'), h('p', { class: 'note' }, t('clockSettingNote'))));
  if (saved('clock_interval') !== undefined) out.push(lbl(t('clockRefresh')), clockRefreshCard());
  if (saved('quiet_enabled') !== undefined) out.push(lbl(t('night')), nightCard());
  return out;
}

// ---------- Screen tab: power (build 75) ----------

function power() { return S.state.power || {}; }
function errLine(key) {
  var e = S.errors[key];
  return e ? h('p', { class: 'err', role: 'alert' }, e) : null;
}
function estimatesStale() { return has(S.draft, 'quiet_enabled') || has(S.draft, 'quiet_from') || has(S.draft, 'quiet_to') || has(S.draft, 'battery_mah'); }
function clockRefreshCard() {
  var P = power(), est = P.estimates || [], now = cur('clock_interval') || 1;
  var list = est.length ? est : [1, 2, 5, 10, 15, 30].map(function (n) { return { interval: n }; });
  var rows = list.map(function (e) {
    var n = e.interval, sub = e.days ? t('aboutDays', { d: e.days }) + (e.gainPercent > 0 ? ' (+' + e.gainPercent + ' %)' : '') : '';
    return h('button', { type: 'button', class: 'it', 'aria-pressed': String(n === now), 'data-f': 'ci' + n,
      onclick: function () { setVal('clock_interval', n); if (S.errors.clock_interval) delete S.errors.clock_interval; render(); } },
      h('span', { class: 't' }, n === 1 ? t('everyMin') : t('everyN', { n: n }), sub ? h('small', null, sub) : null),
      n === now ? h('span', { 'aria-hidden': 'true' }, '✓︎') : null);
  });
  return h('div', { class: 'card', 'data-k': 'clock_interval' },
    h('p', null, t('refreshExpl')),
    h('div', { class: 'list', role: 'group', 'aria-label': t('clockRefresh') }, rows), errLine('clock_interval'),
    estimatesStale() ? h('p', { class: 'note' }, t('estStale')) : null,
    P.estimateNote ? h('p', { class: 'note' }, P.estimateNote) : null);
}
function nightCard() {
  var P = power(), on = !!cur('quiet_enabled'), out = [];
  out.push(settingSwitch('quiet_enabled', t('quietTitle'), on ? t('quietRange', { a: cur('quiet_from') || '23:00', b: cur('quiet_to') || '06:00' }) : t('quietOff')));
  if (on) {
    out.push(h('div', { class: 'grid3', style: 'grid-template-columns:1fr 1fr' },
      textField('quiet_from', t('from'), { type: 'time' }), textField('quiet_to', t('until'), { type: 'time' })));
  }
  out.push(h('p', { class: 'note' }, t('quietExpl', { t: cur('quiet_to') || '06:00' })));
  if (P.daysWithQuietHours && P.daysWithoutQuietHours) {
    out.push(h('p', null, t('quietEffect', { a: P.daysWithQuietHours, b: P.daysWithoutQuietHours, s: has(S.draft, 'clock_interval') ? t('forSaved') : '' })));
  }
  out.push(errLine('quiet_enabled'));
  return card(out);
}

// ---------- Data tab: personal data ----------

function loadPersonal(only) {
  [['tasks', '/api/tasks'], ['note', '/api/notes'], ['bd', '/api/birthdays']].forEach(function (x) {
    if (typeof only === 'string' && only !== x[0]) return;
    S.p.err[x[0]] = '';
    api('GET', x[1]).then(function (r) {
      if (!r.ok) { if (!r.login) S.p.err[x[0]] = r.error; render(); return; }
      if (x[0] === 'tasks') { S.p.tasks = clone(r.tasks || []); S.p.tasksSaved = JSON.stringify(S.p.tasks); }
      if (x[0] === 'note') { S.p.note = r.note || ''; S.p.noteSaved = S.p.note; }
      if (x[0] === 'bd') { S.p.bd = { manual: clone(r.manual || []), upcoming: r.upcoming || [] }; S.p.bdSaved = JSON.stringify(S.p.bd.manual); }
      render();
    });
  });
}
function notLoaded(kind) {
  if (S.p.err[kind]) return card(h('p', { class: 'err' }, S.p.err[kind]), btn(t('tryAgain'), function () { loadPersonal(kind); }, '', { 'data-f': 'reload-' + kind }));
  return card(h('p', { class: 'msg' }, spin(), t('loading')));
}
function removedText(r) { return r.removed ? t('removedChars', { r: r.removed }) : ''; }
function putPersonal(kind, path, body, savedKey, value) {
  S.res[kind] = { busy: true, text: t('saving') };
  render();
  api('PUT', path, body).then(function (r) {
    if (r.ok) {
      S.p[savedKey] = value;
      S.res[kind] = { ok: true, text: t('savedDot') + removedText(r) };
      if (r.removed || kind !== 'note') loadPersonal(kind);   // new doneAt dates, upcoming birthdays
    }
    else S.res[kind] = r.login ? null : { ok: false, text: r.error || t('saveFailed') };
    render();
  });
}

function dmText(ymd) { var p = String(ymd).split('-'); return p.length === 3 ? (+p[2]) + '.' + (+p[1]) + '.' : ymd; }
function tasksCard() {
  if (!S.p.tasks) return notLoaded('tasks');
  var list = S.p.tasks, prios = t('prios');
  var rows = list.map(function (task, i) {
    var n = i + 1;
    var title = h('input', { type: 'text', value: task.title || '', maxlength: 60, placeholder: t('taskPh'), 'aria-label': t('taskN', { n: n }), 'data-f': 'tt' + i,
      oninput: function () { task.title = title.value; } });
    var due = h('input', { type: 'date', value: task.due || '', 'aria-label': t('taskDue', { n: n }), 'data-f': 'tdue' + i,
      onchange: function () { task.due = due.value || ''; } });
    var prio = h('select', { 'aria-label': t('taskPrio', { n: n }), 'data-f': 'tp' + i, onchange: function () { task.prio = +prio.value; } },
      [1, 2, 3].map(function (p) { return h('option', { value: String(p), selected: (task.prio || 2) === p }, prios[p - 1]); }));
    return h('div', { class: 'task' + (task.done ? ' done' : '') },
      h('div', { class: 'row' }, h('button', { type: 'button', class: 'chk', role: 'checkbox', 'aria-checked': String(!!task.done), 'aria-label': t('taskDone', { n: n }), 'data-f': 'td' + i,
        onclick: function () { task.done = !task.done; if (!task.done) delete task.doneAt; render(); } }, h('span', { 'aria-hidden': 'true' }, task.done ? '✓︎' : '')), title),
      task.done ? h('p', { class: 'note', style: 'margin:4px 0 0 52px' }, t('doneAt', { d: task.doneAt ? ' ' + dmText(task.doneAt) : '' })) : null,
      h('div', { class: 'row' }, due, prio, h('button', { type: 'button', class: 'x', 'aria-label': t('deleteTask', { n: n }),
        onclick: function () { list.splice(i, 1); render(); focusF('addtask'); } }, '✕︎')));
  });
  return card(rows.length ? h('div', null, rows) : h('p', { class: 'note' }, t('noTasks')),
    h('p', { class: 'note' }, t('tasksCount', { n: list.length })),
    h('div', { class: 'btns' },
      btn(t('addTask'), function () {
        list.push({ title: '', due: '', prio: 2, done: false });
        render();
        focusF('tt' + (list.length - 1));
      }, '', { disabled: list.length >= 30, 'data-f': 'addtask' }),
      btn(t('saveTasks'), saveTasks, 'pri', { 'data-f': 'savetasks' })),
    resultEl(S.res.tasks));
}
function saveTasks() {
  var list = S.p.tasks, bad = -1;
  list.forEach(function (task, i) { task.title = String(task.title || '').trim(); if (!task.title && bad < 0) bad = i; });
  if (bad >= 0) { S.res.tasks = { ok: false, text: t('taskNoTitle', { n: bad + 1 }) }; render(); focusF('tt' + bad); return; }
  putPersonal('tasks', '/api/tasks', { tasks: list }, 'tasksSaved', JSON.stringify(list));
}

function noteCard() {
  if (S.p.note === null) return notLoaded('note');
  var count = h('p', { class: 'note', id: 'notecount' }, t('noteCount', { n: S.p.note.length }));
  var ta = h('textarea', { id: 'note', maxlength: 400, rows: 6, value: S.p.note, 'data-f': 'note', 'aria-describedby': 'notecount',
    oninput: function () { S.p.note = ta.value; count.textContent = t('noteCount', { n: ta.value.length }); } });
  return card(h('div', { class: 'f' }, h('label', { for: 'note' }, t('noteLabel')), ta, count),
    btn(t('saveNote'), function () { putPersonal('note', '/api/notes', { note: S.p.note }, 'noteSaved', S.p.note); }, 'pri wide', { 'data-f': 'savenote' }),
    resultEl(S.res.note));
}

function daysIn(m) { return new Date(2000, m, 0).getDate(); }
function bdText(b) { return t('fmtBday', { d: b.day, mon: monthName(b.month - 1) }) + (b.year ? ' ' + b.year : ''); }
function birthdaysCard() {
  var parts = [];
  var chkRes = resultEl(S.res.bday_url);
  parts.push(secretField('bday_url', t('f_bday_url'), { type: 'url', im: 'url', ph: 'https://…/basic.ics', after: chkRes,
    extra: [btn(t('check'), function () { checkCalendar('bday_url', -1, chkRes); }, 'sm', { 'data-f': 'chk:bday_url' })], note: t('bdLinkNote') }));
  parts.push(settingSwitch('cal_birthdays', t('calBdTitle'), t('calBdSub')));
  if (!S.p.bd) { parts.push(notLoaded('bd')); return card(parts); }
  var man = S.p.bd.manual, nb = S.p.newBd;
  parts.push(h('p', { class: 'fl', style: 'font-weight:650' }, t('bdByHand', { n: man.length })));
  if (man.length) {
    parts.push(h('div', null, man.map(function (b, i) {
      return h('div', { class: 'kv' }, h('b', null, b.name), h('span', { class: 'row' }, bdText(b),
        h('button', { type: 'button', class: 'x', 'aria-label': t('deleteX', { n: b.name }), 'data-f': 'bdx' + i, onclick: function () { man.splice(i, 1); render(); focusF('bdname'); } }, '✕︎')));
    })));
  } else parts.push(h('p', { class: 'note' }, t('noneYet')));
  var name = h('input', { type: 'text', id: uid(), maxlength: 32, value: nb.name, placeholder: t('name'), autocapitalize: 'words', 'data-f': 'bdname', oninput: function () { nb.name = name.value; } });
  var day = h('input', { type: 'text', id: uid(), inputmode: 'numeric', maxlength: 2, value: nb.day, placeholder: t('day'), oninput: function () { nb.day = day.value; } });
  var month = h('select', { id: uid(), onchange: function () { nb.month = month.value; } }, t('months').map(function (m, i) { return h('option', { value: String(i + 1), selected: String(i + 1) === String(nb.month) }, m); }));
  var year = h('input', { type: 'text', id: uid(), inputmode: 'numeric', maxlength: 4, value: nb.year, placeholder: t('optional'), oninput: function () { nb.year = year.value; } });
  parts.push(h('div', { class: 'sub' }, h('p', { style: 'font-weight:650' }, t('addBd')),
    h('div', { class: 'f' }, h('label', { for: name.id }, t('name')), name),
    h('div', { class: 'grid3' }, h('div', { class: 'f' }, h('label', { for: day.id }, t('day')), day), h('div', { class: 'f' }, h('label', { for: month.id }, t('month')), month),
      h('div', { class: 'f' }, h('label', { for: year.id }, t('year')), year)),
    h('p', { class: 'note' }, t('bdAgeNote')),
    btn(t('add'), addBirthday, 'wide', { disabled: man.length >= 50, 'data-f': 'bdadd' })));
  parts.push(btn(t('saveBd'), function () {
    putPersonal('bd', '/api/birthdays', { manual: man }, 'bdSaved', JSON.stringify(man));
  }, 'pri wide', { 'data-f': 'savebd' }), resultEl(S.res.bd));
  var up = S.p.bd.upcoming || [];
  parts.push(h('p', { style: 'font-weight:650' }, t('next60')));
  parts.push(up.length ? h('div', null, up.map(function (b) {
    var when = b.days === 0 ? t('bdToday') : b.days === 1 ? t('bdTomorrow') : t('inDays', { n: b.days });
    return kv(b.name + (b.age ? ' (' + b.age + ')' : ''), when + (b.fromFeed ? t('fromCal') : ''));
  })) : h('p', { class: 'note' }, t('noBd60')));
  return card(parts);
}
function addBirthday() {
  var nb = S.p.newBd, name = String(nb.name || '').trim(), d = parseInt(nb.day, 10), m = parseInt(nb.month, 10) || 1, y = String(nb.year || '').trim();
  var yr = y ? parseInt(y, 10) : 0, now = new Date().getFullYear(), err = '';
  if (!name) err = t('typeName');
  else if (!(d >= 1 && d <= daysIn(m))) err = t('badDay', { n: daysIn(m), m: monthName(m - 1) });
  else if (y && !(/^\d{4}$/.test(y) && yr >= 1900 && yr <= now)) err = t('badYear', { n: now });
  if (err) { S.res.bd = { ok: false, text: err }; render(); focusF('bdadd'); return; }
  S.p.bd.manual.push({ name: name, day: d, month: m, year: yr });
  S.p.newBd = { name: '', day: '', month: nb.month, year: '' };
  S.res.bd = { ok: true, text: t('bdAdded', { n: name }) };
  render();
  focusF('bdname');
}

// Stage 8 (B5): which box takes the printer's place while it is switched off ("" = automatic).
function printerMergeField() {
  var short = t('short'), v = cur('printer_merge') || '';
  var keys = ['weather', 'agenda', 'month', 'stocks', 'tracking', 'clock', 'tasks', 'notes', 'birthdays'];
  var sel = h('select', { id: uid(), 'data-f': 'in:printer_merge', onchange: function () { setVal('printer_merge', sel.value); } },
    [h('option', { value: '', selected: !v }, t('mergeAuto'))].concat(keys.map(function (k) {
      return h('option', { value: k, selected: k === v }, short[k] || k);
    })));
  return fieldBox('printer_merge', t('f_printer_merge'), sel, t('mergeNote'));
}

// Stage 8: how often each data type loads; the clock interval caps it (shown when it does).
var INTERVALS = [15, 30, 60, 120, 180, 360];
function intervalsCard() {
  var clk = cur('clock_interval') || 1;
  var rows = [['interval_calendar', 'calendars'], ['interval_weather', 'weather'], ['interval_stocks', 'stocks'],
    ['interval_tracking', 'parcelsFlights']].map(function (r) {
    var v = cur(r[0]) || 60;
    return [h('p', { class: 'big' }, t(r[1])),
      seg(t(r[1]), INTERVALS.map(function (n) { return [n, n < 60 ? n + ' min' : n / 60 + ' h']; }), v,
        function (n) { setVal(r[0], n); if (S.errors[r[0]]) delete S.errors[r[0]]; render(); }, 'iv-' + r[0] + '-'),
      v < clk ? h('p', { class: 'note' }, t('ivCapped', { n: clk })) : null, errLine(r[0])];
  });
  return card(h('p', null, t('ivExpl')), rows);
}

function dataTab() {
  return [
    lbl(t('calendars')), card([0, 1, 2].map(function (i) {
      return [calNameField(i), calendarField(i), monthSwitch(i), saved('ics_every') !== undefined ? calEverySeg(i) : null];
    }), h('p', { class: 'note' }, t('calNote'))),
    lbl(t('holidaysLbl')), holidaysCard(),
    lbl(t('weather')), card(placeField()),
    lbl(t('stocks')), card(textField('stocks', t('f_stocks'), { ph: 'AAPL,NVDA,EUNL.DE:MSCI World', note: t('stocksNote') })),
    lbl(t('parcelsFlights')), card(
      secretField('pkge_key', t('pkgeLabel'), { note: t('pkgeNote') }),
      secretField('adb_key', t('adbLabel'), { note: t('adbNote') })),
    lbl(t('printer3d')), card(
      textField('printer_name', t('f_printer_name'), { ph: 'X1C', cap: 'words' }),
      textField('mqtt_ip', t('f_mqtt_ip'), { ph: '192.168.1.50', im: 'decimal', note: t('printerAddrNote') }),
      textField('mqtt_sn', t('serial'), { cap: 'characters' }),
      secretField('mqtt_pass', t('accessCode'), { pw: true, note: t('accessNote') }),
      textField('mqtt_port', t('port'), { num: true, im: 'numeric', ph: '8883' }),
      textField('makerworld_uid', t('f_makerworld_uid'), { im: 'numeric', note: t('mwNote') }),
      saved('printer_idle') !== undefined && cur('makerworld_uid') ? [h('p', { class: 'note' }, t('idleView')),
        seg(t('idleView'), [[0, t('idleMw')], [1, t('idlePrinter')]], cur('printer_idle') || 0,
          function (v) { setVal('printer_idle', v); render(); }, 'pi-')] : null,
      saved('printer_merge') !== undefined ? printerMergeField() : null),
    saved('interval_weather') !== undefined ? [lbl(t('ivTitle')), intervalsCard()] : null,
    lbl(t('tasks')), tasksCard(),
    lbl(t('note')), noteCard(),
    lbl(t('birthdays')), birthdaysCard()
  ];
}

// ---------- Device tab ----------

function slug(n) { return String(n || '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'dashboard'; }
function nameField() {
  var wrap = textField('device_name', t('f_device_name'), { ph: t('namePh'), max: 32, cap: 'sentences',
    note: t('nameNote', { a: slug(cur('device_name')) }),
    onin: function (inp) {
      var n = wrap.querySelector('.note');
      if (n) n.textContent = t('nameNote', { a: slug(inp.value) });
    } });
  return wrap;
}
function langPart() {
  var lng = langNow();
  var sel = h('select', { id: uid(), 'data-f': 'in:lang', onchange: function () { chooseLang(sel.value); } },
    h('option', { value: 'de', selected: lng === 'de' }, 'Deutsch'), h('option', { value: 'en', selected: lng === 'en' }, 'English'));
  return fieldBox('lang', t('langLbl'), sel, saved('lang') === undefined ? null : t('langNote'));
}
function curPins() { return clone(cur('pins') || {}); }
function pinsPart() {
  var presets = S.state.pinPresets || [], pre = cur('pin_preset');
  var sel = h('select', { id: uid(), 'data-f': 'in:pin_preset', onchange: function () { setVal('pin_preset', sel.value); render(); } },
    presets.map(function (p) { return h('option', { value: p.key, selected: p.key === pre }, p.name); }));
  var out = [h('p', { class: 'note' }, t('pinsNote')), fieldBox('pin_preset', t('f_pin_preset'), sel)];
  if (pre === 'custom') {
    var pins = curPins();
    out.push(h('div', { class: 'grid3', style: 'grid-template-columns:1fr 1fr' }, PINS.map(function (k) {
      var v = pins[k];
      var inp = h('input', { type: 'text', inputmode: 'numeric', value: v === undefined || v === null || v < 0 ? '' : String(v), 'data-f': 'pin:' + k,
        oninput: function () { var q = curPins(); q[k] = inp.value.trim() === '' ? -1 : numOr(inp.value); setVal('pins', q); } });
      return fieldBox('pins.' + k, pinLabel(k), inp);
    })), h('p', { class: 'note' }, t('gpioNote')));
  }
  return out;
}
function deviceTab() {
  var d = S.state.device;
  var adv = h('details', { open: !!S.adv, ontoggle: function () { S.adv = adv.open; } }, h('summary', null, t('advPins')), h('div', { class: 'card' }, pinsPart()));
  var reset = S.resetAsk ?
    card(h('p', { class: 'big' }, t('eraseQ')), h('p', null, t('eraseText')),
      h('div', { class: 'btns' }, btn(t('cancel'), function () { S.resetAsk = false; render(); focusF('reset'); }, '', { 'data-f': 'reset-no' }),
        btn(t('eraseAll'), function () {
          S.resetAsk = false;
          postAction('/api/factory-reset', function () { return t('erased'); }, function () { return [t('erased1'), t('erased2')]; }, 'reset');
        }, 'pri', { 'data-f': 'reset-yes' }))) :
    card(h('p', null, t('resetText')),
      btn(t('resetBtn'), function () { S.resetAsk = true; render(); focusF('reset-no'); }, 'wide', { 'data-f': 'reset' }));
  return [
    lbl(t('language')), card(langPart()),
    lbl(t('name')), card(nameField()),
    lbl(t('wifi')), card(wifiPart(), d.mode === 'home' ? [
      h('p', { class: 'note' }, t('chgWifiNote')),
      btn(t('chgWifiBtn'), changeWifi, 'wide', { 'data-f': 'chgwifi' })] : null),
    lbl(t('f_timezone')), card(tzField()),
    saved('battery_mah') !== undefined ? [lbl(t('battery')), card(textField('battery_mah', t('batCapLabel'), { num: true, im: 'numeric', ph: '3000', max: 5,
      note: t('batCapNote') }))] : null,
    lbl(t('updates')), card(
      settingSwitch('ota_auto', t('otaTitle'), t('otaSub')),
      h('div', null, h('p', { class: 'fl', style: 'font-weight:650;margin-bottom:6px' }, t('f_ota_beta')),
        seg(t('f_ota_beta'), [[false, t('stable')], [true, t('beta')]], !!cur('ota_beta'), function (v) { setVal('ota_beta', v); render(); }, 'beta-'),
        h('p', { class: 'note', style: 'margin-top:6px' }, t('betaNote'))),
      kv(t('installed'), d.version + (d.lastUpdate ? ' · ' + d.lastUpdate : '')),
      btn(t('checkUpd'), function () { installUpdate('chkupd'); }, 'wide', { 'data-f': 'chkupd' })),
    lbl(t('about')), card(kv(t('f_pin_preset'), d.variant || '?'), kv(t('version'), d.version + t('buildN', { n: d.build })),
      kv(t('mode'), d.mode === 'home' ? t('modeHome') : t('modeAp')), kv(t('battery'), batteryText(d))),
    h('div', { style: 'margin-top:16px' }, diagSection()),
    h('div', { style: 'margin-top:8px' }, adv),
    lbl(t('factoryReset')), reset
  ];
}

// ---------- Device tab: diagnostics (GET /api/diagnostics, build 72) ----------

function loadDiag() {
  S.diag = { busy: true, data: S.diag && S.diag.data };
  render();
  api('GET', '/api/diagnostics', undefined, { timeout: 15000 }).then(function (r) {
    if (r.ok && r.battery) S.diag = { data: r };
    else S.diag = { err: r.login ? '' : r.error || t('noDiag'), data: S.diag && S.diag.data };
    render();
  });
}
// Relative times, against the device's clock.
function agoText(sec, now) {
  if (!sec) return t('never');
  if (!now) return t('clockNotSetLow');
  var s = now - sec;
  if (s < 60) return t('justNow');
  if (s < 3600) return t('minAgo', { n: Math.floor(s / 60) });
  if (s < 86400) return t('hAgo', { n: Math.floor(s / 3600) });
  return t('dAgo', { n: Math.floor(s / 86400) });
}
function inText(sec, now) {
  if (!sec) return t('none');
  if (!now) return t('clockNotSetLow');
  var s = sec - now;
  if (s <= 0) return t('now');
  if (s < 3600) return t('inMin', { n: Math.max(1, Math.ceil(s / 60)) });
  return t('inH', { n: Math.round(s / 3600) });
}
function volts(mv) { return num(mv / 1000, 2) + ' V'; }
function kb(b) { return Math.round((b || 0) / 1024) + ' KB'; }

function batteryNow(b) {
  if (!b.sensor) return t('noSensor');
  var s = volts(b.mv);
  if (b.plausible === false) s += t('noBattery');
  else if (b.percent >= 0) s += ', ' + b.percent + ' %';
  return s;
}
// The reading with Wi-Fi off (quietMv), when the device has one: shown first, the live one second.
function batteryLines(b, now) {
  if (!b.sensor || !(b.quietMv > 0)) return [batteryNow(b)];
  var q = volts(b.quietMv);
  if (b.quietMv < 3000 || b.quietMv >= 4165) q += t('noBattery');  // as BatteryCurve.h
  else if (b.quietPercent >= 0) q += ', ' + b.quietPercent + ' %';
  q += t('wifiOff', { t: b.quietAt ? ', ' + clockAt(b.quietAt, now) : '' });
  return [q, t('nowWifi', { v: volts(b.mv) }) + (b.plausible === false ? t('noBattery') : t('sag'))];
}

// Line chart of the battery history: x = last 7 days, y = 0-4.5 V, 3.0-4.16 V band hatched.
function batteryChart(hist, now) {
  var W = 340, H = 196, x0 = 34, x1 = 332, y0 = 8, y1 = 168, VMAX = 4500, DAY = 86400;
  var end = now || hist[hist.length - 1][0], start = end - 7 * DAY;
  function X(sec) { return (x0 + (sec - start) / (end - start) * (x1 - x0)).toFixed(1); }
  function Y(mv) { return (y1 - Math.max(0, Math.min(VMAX, mv)) / VMAX * (y1 - y0)).toFixed(1); }
  var s = '<defs><pattern id="hatch" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">' +
    '<line x1="0" y1="0" x2="0" y2="6" stroke="#000" stroke-width="1" opacity=".35"/></pattern></defs>';
  s += '<rect x="' + x0 + '" y="' + Y(4160) + '" width="' + (x1 - x0) + '" height="' + (Y(3000) - Y(4160)).toFixed(1) + '" fill="url(#hatch)"/>';
  s += '<text x="' + (x0 + 4) + '" y="' + (+Y(4160) + 12) + '" font-size="10">' + esc(t('workingBat')) + '</text>';
  for (var v = 0; v <= 4; v++) {
    s += '<line x1="' + x0 + '" y1="' + Y(v * 1000) + '" x2="' + x1 + '" y2="' + Y(v * 1000) + '" stroke="#000" stroke-width="' + (v ? .5 : 1.5) + '"' + (v ? ' stroke-dasharray="2 3"' : '') + '/>' +
      '<text x="' + (x0 - 4) + '" y="' + (+Y(v * 1000) + 4) + '" font-size="11" text-anchor="end">' + v + ' V</text>';
  }
  var d = new Date(start * 1000);
  d.setHours(24, 0, 0, 0);
  for (var sec = d.getTime() / 1000; sec <= end; sec += DAY) {
    var day = new Date(sec * 1000);
    s += '<line x1="' + X(sec) + '" y1="' + y1 + '" x2="' + X(sec) + '" y2="' + (y1 + 5) + '" stroke="#000"/>' +
      '<text x="' + X(sec + DAY / 2) + '" y="' + (y1 + 18) + '" font-size="11" text-anchor="middle">' + (sec + DAY / 2 < end ? esc(dayShort(day.getDay())) : '') + '</text>';
  }
  var path = '', prev = 0;
  hist.forEach(function (p) {
    if (p[0] < start) return;
    path += (prev && p[0] - prev <= 3 * 3600 ? 'L' : 'M') + X(p[0]) + ' ' + Y(p[1]);
    prev = p[0];
  });
  s += '<path d="' + path + '" fill="none" stroke="#000" stroke-width="2" stroke-linejoin="round"/>';
  hist.forEach(function (p) {
    if (p[0] >= start && p[2]) s += '<circle cx="' + X(p[0]) + '" cy="' + Y(p[1]) + '" r="2.6" fill="#000"/>';
  });
  var last = hist[hist.length - 1];
  var label = t('chartLabel', { n: hist.length, a: volts(hist[0][1]), b: volts(last[1]) });
  return '<svg viewBox="0 0 ' + W + ' ' + H + '" role="img" aria-label="' + esc(label) + '" xmlns="http://www.w3.org/2000/svg">' + s + '</svg>';
}

// Log lines (oldest first) -> days, newest first, each day's lines newest first.
function logGroups(lines, filter) {
  var groups = [], g = { date: '', lines: [] }, f = (filter || '').toLowerCase();
  groups.push(g);
  (lines || []).forEach(function (l) {
    var m = /^--- (\d{4}-\d{2}-\d{2}) ---$/.exec(l);
    if (m) { g = { date: m[1], lines: [] }; groups.push(g); return; }
    if (!f || l.toLowerCase().indexOf(f) >= 0) g.lines.push(l);
  });
  return groups.filter(function (x) { return x.lines.length; }).reverse().map(function (x) { x.lines.reverse(); return x; });
}
function fillLog(el, count, lines, filter) {
  var groups = logGroups(lines, filter), n = 0, total = (lines || []).filter(function (l) { return !/^--- /.test(l); }).length;
  el.innerHTML = '';
  groups.forEach(function (g) {
    n += g.lines.length;
    if (g.date) el.appendChild(h('h3', { class: 'logd' }, g.date));
    el.appendChild(h('pre', null, g.lines.join('\n')));
  });
  if (!groups.length) el.appendChild(h('p', { class: 'note' }, total ? t('noLine', { f: filter }) : t('logEmpty')));
  count.textContent = filter ? t('nOfLines', { n: n, m: total }) : t('linesNewest', { m: total });
}
function legacyCopy(text) {
  var ta = h('textarea', { readonly: true, style: 'position:fixed;top:0;left:0;opacity:0' });
  ta.value = text;
  document.body.appendChild(ta);
  ta.select();
  var ok = false;
  try { ok = document.execCommand('copy'); } catch (e) { ok = false; }
  ta.remove();
  return ok;
}
function copyText(text) {
  if (navigator.clipboard && window.isSecureContext) {
    return navigator.clipboard.writeText(text).then(function () { return true; }, function () { return legacyCopy(text); });
  }
  return Promise.resolve(legacyCopy(text));
}
function canCopy() {
  return !!(navigator.clipboard && window.isSecureContext) || !!(document.queryCommandSupported && document.queryCommandSupported('copy'));
}

function diagPart() {
  var st = S.diag || {}, D = st.data, out = [];
  out.push(btn(st.busy ? t('loading') : t('refresh'), loadDiag, 'wide', { disabled: !!st.busy, 'data-f': 'diag-refresh' }));
  if (st.err) out.push(h('p', { class: 'err', role: 'alert' }, st.err));
  if (!D) {
    if (st.busy) out.push(h('p', { class: 'msg', role: 'status' }, spin(), t('asking')));
    return out;
  }
  var now = D.time || 0, b = D.battery || {}, hist = b.history || [];
  // battery
  var bl = batteryLines(b, now);
  out.push(h('h3', { class: 'dh' }, t('battery')), h('p', null, bl[0]), bl[1] ? h('p', { class: 'note' }, bl[1]) : null);
  if (b.usb) out.push(h('p', { class: 'note' }, t('usbComputer')));
  if (hist.length) out.push(h('div', { class: 'chart', html: batteryChart(hist, now) }), h('p', { class: 'note' }, t('hatchNote')));
  else out.push(h('p', { class: 'note' }, t('noHistory')));
  // services
  out.push(h('h3', { class: 'dh' }, t('services')));
  var rows = (D.services || []).map(function (s) {
    return h('tr', null, h('td', null, s.name || s.key, s.stale ? h('span', { class: 'tag' }, t('old')) : null),
      h('td', null, agoText(s.lastOk, now)), h('td', null, String(s.failures || 0)), h('td', null, inText(s.retryAt, now)));
  });
  out.push(h('div', { class: 'tblw' }, h('table', { class: 'tbl' },
    h('thead', null, h('tr', null, t('thService').map(function (x) { return h('th', { scope: 'col' }, x); }))),
    h('tbody', null, rows))));
  // the rest
  var known = {};
  (S.state.device.problems || []).forEach(function (p) { known[p.key] = p.text; });
  var probs = (D.problems || []).map(function (k) { return known[k] || k; });
  var w = D.wifi || {}, mem = D.heap || {};
  out.push(h('div', null,
    kv(t('wifiFails'), (w.failures || 0) + (w.retryAfter ? t('nextTry', { t: inText(w.retryAfter, now) }) : '')),
    w.rssi ? kv(t('wifiSignal'), w.rssi + ' dBm, ' + signalWord(w.rssi)) : null,
    kv(t('lastSync'), agoText(D.lastTimeSync, now)),
    kv(t('problems'), probs.length ? probs.join('; ') : t('none')),
    kv(t('freeMem'), t('memText', { f: kb(mem.free), m: kb(mem.minFree), l: kb(mem.largest) })),
    D.bootCount ? kv(t('starts'), String(D.bootCount)) : null,
    kv(t('devClock'), now ? dateTimeText(now) : t('notSet'))));
  // log (its lines are technical and stay as the device writes them)
  out.push(h('h3', { class: 'dh' }, t('log')));
  var logEl = h('div', { class: 'log' }), count = h('p', { class: 'note', 'aria-live': 'polite' });
  var fid = uid();
  var filter = h('input', { type: 'search', id: fid, value: S.logFilter || '', placeholder: t('logFilterPh'), autocomplete: 'off', autocapitalize: 'off',
    spellcheck: 'false', 'data-f': 'logfilter', oninput: function () { S.logFilter = filter.value; fillLog(logEl, count, D.log, filter.value.trim()); } });
  fillLog(logEl, count, D.log, (S.logFilter || '').trim());
  out.push(h('div', { class: 'f' }, h('label', { for: fid }, t('logFilter')), filter), count, logEl,
    h('div', { class: 'btns' },
      h('a', { class: 'btn', href: '/api/log.txt', download: 'dashboard-log.txt' }, t('downloadLog')),
      canCopy() ? btn(t('copy'), function () {
        var shown = logGroups(D.log, (S.logFilter || '').trim()).map(function (g) { return (g.date ? '--- ' + g.date + ' ---\n' : '') + g.lines.join('\n'); }).join('\n');
        copyText(shown).then(function (ok) { toast(ok ? t('copied') : t('copyFail')); });
      }, '', { 'data-f': 'logcopy' }) : null,
      btn(t('clearLog'), function () {
        ask(t('clearQ'), t('clearText'), t('clearLog'), function () {
          api('POST', '/api/log/clear', {}).then(function (r) {
            if (r.ok) { toast(t('cleared')); loadDiag(); }
            else if (!r.login) toast(r.error || t('clearFail'));
          });
        }, 'logclear');
      }, '', { 'data-f': 'logclear' })),
    h('p', { class: 'note' }, t('logNote')));
  return out;
}
function diagSection() {
  var el = h('details', { open: !!S.diagOpen, ontoggle: function () {
    S.diagOpen = el.open;
    if (el.open && !S.diag) loadDiag();
  } }, h('summary', null, t('diagnostics')), h('div', { class: 'card' }, S.diagOpen ? diagPart() : null));
  return el;
}

// ---------- first-start guide ----------

// Steps: 0 language (always starts in German), 1 Wi-Fi, 2 name and place, 3 calendars, 4 screen, 5 done.
var GUIDE_N = 6;
function isGuide() { return !!(S.state && S.state.device.firstRun && !S.screen); }
function guideView() {
  var g = S.guide, step = g.step, body = [], steps = t('guideSteps');
  document.title = t('guideTitle');
  if (step === 0) {
    body.push(card(h('p', null, t('langIntro')),
      seg(t('langLbl'), [['de', 'Deutsch'], ['en', 'English']], langNow(), chooseLang, 'lang-')));
  } else if (step === 1) {
    body.push(card(h('p', null, t('wifiIntro')), wifiPart()));
    if (S.nets === null && !S.scan) setTimeout(scanNetworks, 0);
    if (g.wifiFailed) body.push(card(h('p', null, t('testFailed')),
      btn(t('goOn'), function () { g.wifiFailed = false; g.step = 2; render(); window.scrollTo(0, 0); }, 'wide')));
  } else if (step === 2) {
    body.push(card(nameField(), tzField(), placeField()));
  } else if (step === 3) {
    body.push(card(h('p', null, t('calIntro')), calendarField(0), calendarField(1), calendarField(2), h('p', { class: 'note' }, t('calNoteShort'))));
  } else if (step === 4) {
    var W = widgetsByKey();
    body.push(structStrip(), lbl(t('preview')), sketchEl(-1),
      card(S.lay.classic ? h('p', null, t('classicOrig')) : S.lay.boxes.map(function (b, i) {
        return kv(t('boxN', { n: i + 1 }), b.widget ? ((W[b.widget] && W[b.widget].name) || b.widget) : t('emptyShort'));
      }), h('p', { class: 'note' }, t('changeLater'))));
  } else {
    var n = [0, 1, 2].filter(function (i) { var v = S.secrets['ics.' + i]; return typeof v === 'string' ? !!v.trim() : secretInfo('ics.' + i).set; }).length;
    var st = structures().filter(function (s) { return S.lay.classic ? s.classic : s.key === S.lay.key; })[0];
    body.push(card(h('p', null, t('savedAtOnce')),
      h('div', null, kv(t('language'), langNow() === 'en' ? 'English' : 'Deutsch'),
        kv(t('wifi'), cur('wifi_ssid') || t('notChosen')), kv(t('name'), cur('device_name') || t('display')),
        kv(t('f_timezone'), cur('timezone') || t('notSetCap')), kv(t('weather'), cur('weather_loc') || t('notSetCap')),
        kv(t('calendars'), n ? tn('links', n) : t('noneCap')), kv(t('screen'), st ? structName(st) : t('ownLayout')))));
  }
  var top = h('header', { class: 'top' }, h('div', { class: 'in' },
    h('div', { class: 'head' }, h('h1', null, t('guideTitle')), h('p', { class: 'st' }, t('stepOf', { n: step + 1, m: GUIDE_N }))),
    h('div', { class: 'steps', 'aria-hidden': 'true' }, steps.map(function (x, i) { return h('i', { class: i <= step ? 'on' : '' }); })),
    h('p', { style: 'font-weight:700;padding-bottom:10px' }, steps[step])));
  S.barEl = h('div', { class: 'bar' });
  fillBar();
  return h('div', null, top, h('main', { class: 'wrap' }, body), S.barEl);
}
var GUIDE_STEP = { lang: 0, wifi_ssid: 1, wifi_pass: 1, device_name: 2, timezone: 2, weather_loc: 2, ics: 3 };
function guideButtons(inner) {
  var g = S.guide, last = g.step === GUIDE_N - 1;
  var errs = Object.keys(S.errors);
  if (errs.length) inner.appendChild(h('p', { class: 'err', role: 'alert' }, t('fixColon', { list: errs.map(fieldLabel).join(', ') })));
  inner.appendChild(h('div', { class: 'btns' },
    g.step ? btn(t('back'), function () { g.step--; S.barMsg = ''; render(); window.scrollTo(0, 0); }, '', { disabled: !!S.busy, 'data-f': 'g-back' }) : null,
    btn(last ? t('saveRestart') : t('next'), last ? guideSave : guideNext, 'pri', { disabled: !!S.busy, 'data-f': 'g-next' })));
}
function guideNext() {
  var g = S.guide;
  S.barMsg = '';
  if (g.step === 1) {
    if (!String(cur('wifi_ssid') || '').trim()) { S.barMsg = t('pickWifiFirst'); updateBar(); return; }
    if (!(S.res.wifi && S.res.wifi.ok)) {
      setBusy(t('testingWifi'));
      testWifi(null, function (ok) {
        S.busy = '';
        if (ok) { g.step = 2; render(); window.scrollTo(0, 0); return; }
        g.wifiFailed = true;
        render();
      });
      return;
    }
  }
  g.wifiFailed = false;
  g.step++;
  render();
  window.scrollTo(0, 0);
}
function guideSave() {
  S.barMsg = '';
  setBusy(t('saving'));
  var lay = layoutCheck().ok ? api('POST', '/api/layout', layoutBody(), { timeout: 20000 }) : Promise.resolve({ ok: true });
  lay.then(function (r) {
    if (!r.ok) { if (!r.login) { S.guide.step = 4; S.barMsg = t('layoutNotTakenShort', { e: r.error || t('unknownReason') }); } return null; }
    var body = settingsBody();
    body.lang = langNow();
    body.finish = true;
    return api('POST', '/api/settings', body, { timeout: 15000 });
  }).then(function (r) {
    S.busy = '';
    if (!r) { render(); return; }
    if (r.ok) { restarting(r); return; }
    if (r.login) return;
    if (r.errors && Object.keys(r.errors).length) {
      S.errors = normErrors(r.errors);
      var steps = Object.keys(S.errors).map(function (k) { var s = GUIDE_STEP[k.split('.')[0]]; return s === undefined ? GUIDE_N - 1 : s; });
      S.guide.step = Math.min.apply(null, steps);
      if (S.guide.step === GUIDE_N - 1) S.barMsg = r.error || t('notAccepted');
    } else S.barMsg = r.error || t('saveFailed');
    render();
    window.scrollTo(0, 0);
  });
}

// ---------- main view ----------

function mainView() {
  var d = S.state.device, name = cur('device_name') || d.name || t('display');
  document.title = t('setupTitle', { n: name });
  var tabs = [['home', t('tabHome')], ['screen', t('tabScreen')], ['data', t('tabData')], ['device', t('tabDevice')]];
  var top = h('header', { class: 'top' }, h('div', { class: 'in' },
    h('div', { class: 'head' }, h('h1', null, name), h('p', { class: 'st' }, d.mode === 'home' ? t('homeNet') : t('modeAp'), h('br'), batteryText(d, true))),
    h('div', { class: 'tabs', role: 'tablist', 'aria-label': t('sections') }, tabs.map(function (tab) {
      return h('button', { type: 'button', role: 'tab', id: 'tab-' + tab[0], 'aria-selected': String(S.tab === tab[0]), 'aria-controls': 'panel', 'data-f': 'tab-' + tab[0],
        onclick: function () { if (S.tab !== tab[0]) { S.tab = tab[0]; S.barMsg = ''; window.scrollTo(0, 0); render(); } } }, tab[1]);
    }))));
  var body = { home: homeTab, screen: screenTab, data: dataTab, device: deviceTab }[S.tab]();
  S.barEl = h('div', { class: 'bar' });
  fillBar();
  return h('div', null, top, h('main', { class: 'wrap', id: 'panel', role: 'tabpanel', 'aria-labelledby': 'tab-' + S.tab }, body), S.barEl);
}

function render() {
  I18N.setLang(langNow());
  document.documentElement.lang = langNow();
  var ae = document.activeElement, f = ae && ae.getAttribute ? ae.getAttribute('data-f') : null, y = window.pageYOffset, sel = null;
  try { if (f && typeof ae.selectionStart === 'number') sel = [ae.selectionStart, ae.selectionEnd]; } catch (e) { sel = null; }
  S.barEl = null;
  var view = S.screen ? S.screen() : !S.state ? null : S.state.device.firstRun ? guideView() : mainView();
  root.innerHTML = '';
  if (view) root.appendChild(view);
  window.scrollTo(0, y);
  if (f && (!S.sheet)) {
    var el = root.querySelector('[data-f="' + f + '"]');
    if (el) {
      try { el.focus({ preventScroll: true }); } catch (e) { el.focus(); }
      if (sel) { try { el.setSelectionRange(sel[0], sel[1]); } catch (e) { /* not a text field */ } }
    }
  }
  if (S.scrollPv) {
    S.scrollPv = false;
    var pv = document.getElementById('pv');
    if (pv) pv.scrollIntoView({ block: 'center' });
  }
}

// ---------- start ----------

function start(state) {
  closeSheet();
  S = {
    state: state, tab: S.tab || 'home', uiLang: S.uiLang, draft: {}, secrets: {}, errors: {}, res: {}, busy: '', barMsg: '',
    nets: null, scan: null, netOpen: false, guide: { step: 0 }, preview: null, adv: false, resetAsk: false, screen: null,
    p: { tasks: null, tasksSaved: '[]', note: null, noteSaved: '', bd: null, bdSaved: '[]', err: {}, newBd: { name: '', day: '', month: '1', year: '' } }
  };
  // The first start always begins in German (owner's decision); the language is its first question.
  if (state.device.firstRun) {
    if (saved('lang') !== undefined && saved('lang') !== 'de') S.draft.lang = 'de';
    S.uiLang = 'de';
  }
  initLayout();
  S.tz = phoneTz();
  if (!saved('timezone') && S.tz && S.tz.value) { S.draft.timezone = S.tz.value; S.tzFilled = true; }
  render();
  if (!state.device.firstRun) loadPersonal();
}

function load() {
  S.screen = function () { return h('div', { class: 'full' }, h('p', { role: 'status' }, spin(), t('connectingDev'))); };
  render();
  api('GET', '/api/state', undefined, { timeout: 12000 }).then(function (r) {
    if (r.login) return;
    if (!r.ok || !r.device || !r.settings) { var msg = r.ok ? t('badAnswer') : r.error; S.screen = errorScreen(msg); render(); return; }
    start(r);
  });
}

window.addEventListener('beforeunload', function (e) {
  if (S.state && !S.screen && unsavedList().length) { e.preventDefault(); e.returnValue = ''; }
});

load();
})();
