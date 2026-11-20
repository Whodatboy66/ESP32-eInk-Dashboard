// Setup app (docs/plans/STAGE5_SETUP_APP.md, API: docs/SETUP_API.md). Plain ES2017, no
// framework, no external files: the phone has no internet in setup mode. Needs layout.js.
(function () {
'use strict';

var NO_ANSWER = 'The device didn\'t answer. Check that your phone is still connected to the device\'s Wi-Fi, then try again.';
var OFFLINE_CAL = 'The device has no internet connection yet, so the link can\'t be checked now. It\'s checked when the display first wakes up.';
var root = document.getElementById('app');
var S = { tab: 'home' };

var ICONS = { clock: '◷', weather: '☀', agenda: '≡', month: '▦', stocks: '↗', tracking: '▣',
  printer: '▤', tasks: '✓', notes: '✎', birthdays: '✱' };
var MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
var DAYS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
// Classic is a pixel layout (src/layout/Templates.h): only drawn as a sketch here.
var CLASSIC = [['clock', 10, 0, 430, 155], ['month', 440, 25, 360, 230], ['agenda', 440, 255, 360, 145],
  ['stocks', 10, 155, 430, 130], ['weather', 10, 285, 430, 105], ['printer', 10, 390, 430, 90], ['tracking', 440, 400, 360, 80]];
// Time zone names the firmware knows (src/core/Timezone.h); POSIX rules work too.
var TZ_LIST = [['Europe/Berlin', 'Germany, Austria, Switzerland, most of Central Europe'], ['Europe/London', 'United Kingdom'],
  ['Europe/Dublin', 'Ireland'], ['Europe/Lisbon', 'Portugal'], ['Europe/Helsinki', 'Finland, Greece, Romania'],
  ['America/New_York', 'US Eastern'], ['America/Chicago', 'US Central'], ['America/Denver', 'US Mountain'],
  ['America/Los_Angeles', 'US Pacific'], ['UTC', 'UTC']];
var TZ_NAMES = ('Europe/Berlin Europe/Vienna Europe/Zurich Europe/Paris Europe/Amsterdam Europe/Brussels Europe/Luxembourg ' +
  'Europe/Rome Europe/Madrid Europe/Copenhagen Europe/Oslo Europe/Stockholm Europe/Warsaw Europe/Prague Europe/Budapest ' +
  'Europe/Helsinki Europe/Athens Europe/Bucharest Europe/Lisbon Europe/London Europe/Dublin UTC America/New_York ' +
  'America/Chicago America/Denver America/Los_Angeles').split(' ');
var PINS = [['epd_cs', 'Display CS'], ['epd_dc', 'Display DC'], ['epd_rst', 'Display RST'], ['epd_busy', 'Display BUSY'],
  ['epd_sck', 'Display SCK'], ['epd_mosi', 'Display MOSI'], ['display_power', 'Display power (optional)'],
  ['battery', 'Battery (optional)'], ['demo_button', 'Button (optional)']];
// Setting -> [tab, label]: where a field error is shown.
var FIELDS = {
  device_name: ['device', 'Device name'], wifi_ssid: ['device', 'Wi-Fi network'], wifi_pass: ['device', 'Wi-Fi password'],
  timezone: ['device', 'Time zone'], weather_loc: ['data', 'Weather place'], stocks: ['data', 'Stock symbols'],
  ics: ['data', 'Calendar links'], bday_url: ['data', 'Birthday calendar link'], pkge_key: ['data', 'Parcels key'],
  adb_key: ['data', 'Flights key'], mqtt_ip: ['data', 'Printer address'], mqtt_sn: ['data', 'Printer serial number'],
  mqtt_pass: ['data', 'Printer access code'], mqtt_port: ['data', 'Printer port'], printer_name: ['data', 'Printer name'],
  makerworld_uid: ['data', 'MakerWorld user ID'], ota_auto: ['device', 'Automatic updates'], ota_beta: ['device', 'Update channel'],
  clock_24h: ['screen', 'Clock'], pin_preset: ['device', 'Board'], pins: ['device', 'Pins'],
  clock_interval: ['screen', 'Clock refresh'], quiet_enabled: ['screen', 'Quiet hours'], quiet_from: ['screen', 'Quiet hours from'],
  quiet_to: ['screen', 'Quiet hours until'], battery_mah: ['device', 'Battery capacity']
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
function plural(n, one, many) { return n + ' ' + (n === 1 ? one : many); }
function lbl(t) { return h('h2', { class: 'lbl' }, t); }
function card() { var c = h('div', { class: 'card' }); for (var i = 0; i < arguments.length; i++) add(c, arguments[i]); return c; }
function kv(k, v) { return h('div', { class: 'kv' }, h('b', null, k), h('span', null, v)); }
function btn(text, fn, cls, more) {
  var p = { type: 'button', class: 'btn' + (cls ? ' ' + cls : ''), onclick: fn };
  if (more) Object.keys(more).forEach(function (k) { p[k] = more[k]; });
  return h('button', p, text);
}
function icon(key) { return h('span', { class: 'ico' + (key ? '' : ' o'), 'aria-hidden': 'true' }, key ? (ICONS[key] || '◻') + '︎' : '+'); }
function focusF(f) {
  var el = f && document.querySelector('[data-f="' + f + '"]');
  if (el) { try { el.focus({ preventScroll: false }); } catch (e) { el.focus(); } }
}

var toastTimer;
function toast(msg) {
  var t = document.getElementById('toast');
  if (!t) { t = h('div', { id: 'toast', class: 'toast', role: 'status' }); document.body.appendChild(t); }
  t.textContent = msg;
  t.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(function () { t.hidden = true; }, 4000);
}

// ---------- device API ----------

function httpText(status) {
  if (status === 413) return 'That\'s too much for the device at once (over 8 KB). Make the text shorter and try again.';
  if (status === 404) return 'The device doesn\'t know this request. Its firmware may be older than this page: update it.';
  return 'The device had a problem (HTTP ' + status + '). Try again.';
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
    return res.text().then(function (t) {
      var d = null;
      try { d = t ? JSON.parse(t) : {}; } catch (e) { d = null; }
      if (!d || typeof d !== 'object' || Array.isArray(d)) d = {};
      if (d.ok === undefined) d.ok = res.ok;
      if (!res.ok) {
        d.ok = false;
        if (res.status === 401 && d.login) { d.error = 'Type the code shown on the display first.'; needLogin(); }
        if (!d.error && !d.offline) d.error = httpText(res.status);
      }
      d.status = res.status;
      return d;
    });
  }), late]).catch(function () {
    return { ok: false, noAnswer: true, error: NO_ANSWER };
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
function fieldMeta(key) { return FIELDS[key.split('.')[0]] || ['device', key]; }
function fieldLabel(key) {
  var m = /^ics\.(\d)$/.exec(key);
  if (m) return 'Calendar link ' + (+m[1] + 1);
  if (key.indexOf('pins.') === 0) {
    for (var i = 0; i < PINS.length; i++) if ('pins.' + PINS[i][0] === key) return PINS[i][1];
  }
  return fieldMeta(key)[1];
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
function layChanged() {
  if (S.preview && S.preview.url) URL.revokeObjectURL(S.preview.url);
  S.preview = null;
  if (S.sel >= S.lay.boxes.length) S.sel = Math.max(0, S.lay.boxes.length - 1);
  render();
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
function tx(x, y, size, t, extra) {
  return '<text x="' + Math.round(x) + '" y="' + Math.round(y) + '" font-size="' + Math.round(size) + '"' + (extra || '') + '>' + esc(t) + '</text>';
}
function ln(x1, y1, x2, y2, w) {
  return '<line x1="' + x1 + '" y1="' + y1 + '" x2="' + x2 + '" y2="' + y2 + '" stroke="#000" stroke-width="' + (w || 2) + '"/>';
}
function rc(x, y, w, hh, fill, sw, extra) {
  return '<rect x="' + Math.round(x) + '" y="' + Math.round(y) + '" width="' + Math.round(w) + '" height="' + Math.round(hh) +
    '" fill="' + (fill || 'none') + '" stroke="#000" stroke-width="' + (sw === undefined ? 2 : sw) + '"' + (extra || '') + '/>';
}
function head(t, w) { return tx(12, 24, 17, t, BOLD) + ln(8, 32, w - 8, 32); }
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
    date: DAYS[d.getDay()] + ', ' + d.getDate() + ' ' + MONTHS[d.getMonth()],
    short: DAYS[d.getDay()] + ' ' + d.getDate() + ' ' + MONTHS[d.getMonth()].slice(0, 3)
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
  if (!list || !list.length) return [['Buy milk', 'today'], ['Call the plumber', 'Fri'], ['Return library books', ''], ['Water the plants', ''], ['Pay invoice', '24.11.']];
  var today = new Date(); today.setHours(0, 0, 0, 0);
  return list.filter(function (t) { return !t.done && t.title; }).slice().sort(function (a, b) {
    var ad = a.due || '9999', bd = b.due || '9999';
    return ad < bd ? -1 : ad > bd ? 1 : (a.prio || 2) - (b.prio || 2);
  }).map(function (t) {
    var label = '';
    if (t.due) {
      var p = t.due.split('-'), d = new Date(+p[0], +p[1] - 1, +p[2]), diff = Math.round((d - today) / 864e5);
      label = diff < 0 ? 'overdue' : diff === 0 ? 'today' : diff < 7 ? DAYS[d.getDay()] : (+p[2]) + '.' + (+p[1]) + '.';
    }
    return [t.title, label];
  });
}
function sampleBirthdays() {
  var up = S.p && S.p.bd && S.p.bd.upcoming;
  if (!up || !up.length) return [['Today', 'Anna (36)'], ['Tomorrow', 'Grandpa Joe'], ['in 12 d', 'Mia (8)']];
  return up.map(function (b) {
    return [b.days === 0 ? 'Today' : b.days === 1 ? 'Tomorrow' : 'in ' + b.days + ' d', b.name + (b.age ? ' (' + b.age + ')' : '')];
  });
}

function widgetSvg(key, w, hh) {
  var s = '', i, n;
  switch (key) {
    case 'clock': {
      var t = nowParts(), size = Math.min(hh * 0.62, w / (t.time.length > 5 ? 3.6 : 2.6)), withDate = hh >= 120;
      var y = hh / 2 + size * 0.36 - (withDate ? 12 : 0);
      s = tx(w / 2, y, size, t.time, BOLD + MID + ' letter-spacing="-2"');
      if (withDate) s += tx(w / 2, y + 30, 17, t.date, MID);
      return s;
    }
    case 'weather': {
      n = Math.max(1, Math.min(7, Math.floor((w - 80) / 100) + 1));
      var dw = w / n, day = new Date().getDay();
      for (i = 0; i < n; i++) {
        var cx = dw * i + dw / 2;
        s += tx(cx, 26, 16, DAYS[(day + i) % 7], MID + BOLD) + weatherIcon([0, 1, 2, 1, 0, 2, 0][i], cx, hh / 2 + 2) +
          tx(cx, hh - 14, 16, (9 + i) + '° / ' + (2 + i) + '°', MID);
      }
      return s;
    }
    case 'agenda':
      return head('Today and tomorrow', w) + rowsSvg([['10:00', 'Dentist'], ['13:00', 'Team meeting'], ['18:30', 'Football training'],
        ['All day', 'Birthday Anna'], ['Wed 7:15', 'LH 400 FRA → JFK'], ['Wed 12:00', 'Lunch with Ben'], ['Thu 9:00', 'Car service'],
        ['Thu 19:00', 'Book club'], ['Fri 16:00', 'Pick up parcel'], ['Sat', 'Market'], ['Sun 11:00', 'Brunch'], ['Mon 8:00', 'Standup'],
        ['Mon 14:00', 'Review']], w, hh, function (e, y) { return tx(12, y, 17, e[0], BOLD) + tx(104, y, 17, e[1]); });
    case 'month': {
      var now = new Date(), yr = now.getFullYear(), mo = now.getMonth(), today = now.getDate();
      var start = (new Date(yr, mo, 1).getDay() + 6) % 7, days = new Date(yr, mo + 1, 0).getDate();
      var weeks = Math.ceil((start + days) / 7), cw = (w - 24) / 7, top = 40, rh = Math.max(16, (hh - top - 30) / weeks);
      var fs = Math.min(17, rh * 0.75);
      s = head(MONTHS[mo] + ' ' + yr, w);
      ['Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa', 'Su'].forEach(function (d, k) { s += tx(12 + cw * k + cw / 2, top + 14, 13, d, MID + BOLD); });
      s += ln(12, top + 20, w - 12, top + 20);
      for (var d = 1; d <= days; d++) {
        var idx = start + d - 1, x = 12 + cw * (idx % 7), y0 = top + 24 + rh * Math.floor(idx / 7);
        if (d === today) s += rc(x + 2, y0 + 1, cw - 4, rh - 2, '#000', 0) + tx(x + cw / 2, y0 + rh / 2 + fs * 0.36, fs, d, MID + BOLD + ' fill="#fff"');
        else s += tx(x + cw / 2, y0 + rh / 2 + fs * 0.36, fs, d, MID);
      }
      return s;
    }
    case 'stocks': {
      var syms = String(cur('stocks') || '').split(',').map(function (x) { return x.trim(); }).filter(Boolean).slice(0, 5);
      if (!syms.length) syms = ['AAPL', 'NVDA', '^IXIC'];
      return head('Stocks', w) + rowsSvg(syms, w, hh, function (sym, y) {
        var k = sym.length * 37 % 90, up = k % 3 !== 0;
        return tx(12, y, 17, sym.replace('^IXIC', 'NASDAQ'), BOLD) + tx(116, y, 17, (100 + k * 2.7).toFixed(2)) +
          tx(w - 12, y, 17, (up ? '+' : '−') + (k / 30).toFixed(1) + ' %', END);
      });
    }
    case 'tracking':
      return head('Parcels', w) + rowsSvg([['DHL', 'Out for delivery'], ['UPS', 'In transit, Fri'], ['Hermes', 'Delivered']], w, hh,
        function (e, y) { return tx(12, y, 17, e[0], BOLD) + tx(92, y, 17, e[1]); });
    case 'printer':
      return tx(14, 28, 17, (cur('printer_name') || 'Printer') + ' printing', BOLD) + tx(14, 74, 40, '62 %', BOLD) +
        rc(14, 86, w - 28, 16) + rc(14, 86, (w - 28) * 0.62, 16, '#000', 0) + tx(14, 128, 16, 'Benchy.3mf, 41 min left');
    case 'tasks':
      return head('Tasks', w) + rowsSvg(sampleTasks(), w, hh, function (t, y) {
        return rc(12, y - 14, 15, 15) + tx(36, y, 17, t[0]) + (t[1] ? tx(w - 12, y, 15, t[1], END + (t[1] === 'overdue' ? BOLD : '')) : '');
      });
    case 'notes': {
      var note = S.p && S.p.note ? S.p.note : 'Wi-Fi for guests: Sunflower-42\nBins out on Thursday';
      var lines = wrapText(note, Math.max(6, Math.floor((w - 24) / 10)), Math.max(1, Math.floor((hh - 12) / 26)));
      lines.forEach(function (l, k) { s += tx(12, 30 + k * 26, 19, l); });
      return s;
    }
    case 'birthdays':
      return head('Birthdays', w) + rowsSvg(sampleBirthdays(), w, hh, function (b, y) { return tx(12, y, 17, b[0], BOLD) + tx(110, y, 17, b[1]); });
  }
  return tx(w / 2, hh / 2, 17, key, MID);
}

function boxRect(b, statusBar) {
  var g = grid(), top = statusBar ? g.statusH : 0, ch = statusBar ? g.cellH : Math.floor(480 / g.rows);
  var y = top + b.r * ch;
  return { x: b.c * g.cellW, y: y, w: b.w * g.cellW, h: b.r + b.h >= g.rows ? 480 - y : b.h * ch };
}

function boxSvg(key, R, label, picked) {
  var W = widgetsByKey()[key], inner;
  if (!key) {
    inner = rc(8, 8, R.w - 16, R.h - 16, 'none', 2, ' stroke-dasharray="8 6"') + tx(R.w / 2, R.h / 2 + 6, 18, label + ': empty', MID);
  } else if (W && W.configured === false) {
    inner = tx(R.w / 2, R.h / 2 - 6, 18, W.name || key, MID + BOLD);
    wrapText(W.hint || 'Not set up yet', Math.max(8, Math.floor((R.w - 20) / 8)), 2).forEach(function (l, k) {
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
  var t = nowParts(), d = S.state.device || {};
  return '<g transform="translate(' + x + ' 0)">' + tx(10, hh - 8, 15, t.short, BOLD) +
    tx(w - 10, hh - 8, 15, 'Wi-Fi ▮▮▮ · ' + (d.battery >= 0 ? d.battery + ' %' : 'USB'), END + BOLD) + ln(0, hh - 1, w, hh - 1) + '</g>';
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
      names.push(b.widget ? ((W[b.widget] && W[b.widget].name) || b.widget) : 'empty box');
      s += boxSvg(b.widget, boxRect(b, lay.statusBar), 'Box ' + (i + 1), i === sel);
    });
  }
  var label = 'Sketch of the display: ' + (names.length ? names.join(', ') : 'nothing') + '.';
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

// Short widget names for the layout thumbnails.
var SHORT = { clock: 'Clock', weather: 'Weather', agenda: 'Agenda', month: 'Month', stocks: 'Stocks', tracking: 'Parcels',
  printer: 'Printer', tasks: 'Tasks', notes: 'Note', birthdays: 'Birthdays' };
function isGeneric(s) { return !s.classic && Layout.GENERIC.some(function (g) { return g.key === s.key; }); }
// Thumbnail of a ready-made layout: its boxes with the widget names (120 x 84).
function layoutThumb(s) {
  var g = '', rects = s.classic ?
    CLASSIC.map(function (c) { return [c[1] * 120 / 800, c[2] * 84 / 480, c[3] * 120 / 800, c[4] * 84 / 480, c[0]]; }) :
    s.boxes.map(function (b) { return [b.c * 30, b.r * 26 + 5, b.w * 30, b.h * 26, b.widget]; });
  if (!s.classic) g = '<rect x="1" y="1" width="118" height="2.5" fill="currentColor"/>';
  rects.forEach(function (r) {
    var w = r[2] - 2, hh = r[3] - 2, name = r[4] ? SHORT[r[4]] || r[4] : '', max = Math.floor((w - 2) / 4.9);
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
    add(b, s.name);
    return b;
  }));
}
// Two rows: the device's ready-made layouts (with widgets) + Classic, then the empty structures.
function structStrip() {
  var all = structures(), ready = all.filter(function (s) { return !isGeneric(s); }), empty = all.filter(isGeneric);
  return [lbl('Ready-made layouts'), stripOf(ready, true, 'Ready-made layouts'),
    lbl('Empty structures'), stripOf(empty, false, 'Empty structures'),
    h('p', { class: 'note' }, 'Empty structures have only boxes: choose what each one shows below.')];
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
  var ph = removing ? 'Will be removed when you save' :
    info.set ? 'Saved ✓' + (info.hint ? ' (' + info.hint + ')' : '') + ' — type to replace' : (o.ph || '');
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
      show.textContent = on ? 'Hide' : 'Show';
    } }, 'Show');
    tools.push(show);
  }
  if (o.extra) tools = tools.concat(o.extra);
  if (info.set && !removing) tools.push(btn('Remove', function () { setSecret(key, null); render(); focusF('rm:' + key); }, 'sm', { 'data-f': 'rm:' + key }));
  if (removing) tools.push(btn('Keep the saved one', function () { setSecret(key, undefined); render(); focusF('in:' + key); }, 'sm', { 'data-f': 'rm:' + key }));
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
  if (res.busy) { add(p, [h('span', { class: 'spin', 'aria-hidden': 'true' }), res.text]); return; }
  p.className = res.ok ? 'msg okm' : 'err';
  add(p, res.text);
}

// ---------- calendars, places, Wi-Fi, time zone (shared by tabs and the guide) ----------

function calendarField(i) {
  var key = 'ics.' + i;
  var res = resultEl(S.res[key]);
  var check = btn('Check', function () { checkCalendar(key, i, res); }, 'sm', { 'data-f': 'chk:' + key });
  return secretField(key, 'Calendar link ' + (i + 1), { type: 'url', im: 'url', ph: 'https://calendar.google.com/…/basic.ics', extra: [check], after: res });
}
function checkCalendar(key, index, el) {
  var typed = S.secrets[key], body = null;
  if (typeof typed === 'string' && typed.trim()) body = { url: typed.trim() };
  else if (typed !== null && secretInfo(key).set) body = key === 'bday_url' ? { bday: true } : index >= 0 ? { index: index } : null;
  if (!body) { S.res[key] = { ok: false, text: 'Paste a link first, then check it.' }; showResult(el, S.res[key]); return; }
  S.res[key] = { busy: true, text: 'Checking the link…' };
  showResult(el, S.res[key]);
  api('POST', '/api/check-calendar', body, { timeout: 40000 }).then(function (r) {
    S.res[key] = r.ok ? { ok: true, text: 'Works: ' + plural(r.events || 0, 'event', 'events') + ' found.' } :
      { ok: false, text: r.offline ? OFFLINE_CAL : (r.error || 'This link doesn\'t work. Copy it again from your calendar.') };
    showResult(el, S.res[key]);
  });
}

function placeField() {
  var list = h('div', { class: 'list sugg', hidden: true });
  var info = h('p', { class: 'note', 'aria-live': 'polite' });
  var timer, seq = 0;
  return textField('weather_loc', 'Weather place', {
    ph: 'Berlin, DE', cap: 'words', after: [list, info],
    note: 'Type a city and pick it from the list, or type "City, CC" or "latitude,longitude".',
    onin: function (inp) {
      clearTimeout(timer);
      var q = inp.value.trim();
      seq++;
      if (q.length < 2 || /^-?\d/.test(q)) { list.hidden = true; info.textContent = ''; return; }
      timer = setTimeout(function () {
        var my = seq;
        info.textContent = 'Looking for places…';
        api('GET', '/api/places?q=' + encodeURIComponent(q), undefined, { timeout: 15000 }).then(function (r) {
          if (my !== seq) return;
          list.innerHTML = '';
          list.hidden = true;
          if (r.offline) { info.textContent = 'The device has no internet yet, so there are no suggestions. Type the place as "City, CC" (for example Berlin, DE); it\'s checked on the first wake.'; return; }
          if (!r.ok) { info.textContent = (r.noAnswer ? 'No suggestions: the device didn\'t answer.' : 'No suggestions right now.') + ' You can still type the place.'; return; }
          var places = r.places || [];
          info.textContent = places.length ? places.length + ' suggestions below.' : 'No place found with that name. Check the spelling or type "latitude,longitude".';
          places.forEach(function (p) {
            list.appendChild(h('button', { type: 'button', class: 'it', onclick: function () {
              inp.value = p.name;
              setVal('weather_loc', p.name);
              list.hidden = true;
              info.textContent = 'Picked: ' + (p.detail || p.name) + '.';
              inp.focus();
            } }, h('span', { class: 't' }, p.name, p.detail ? h('small', null, p.detail) : null)));
          });
          list.hidden = !places.length;
        });
      }, 500);
    }
  });
}

function signalWord(rssi) { return rssi >= -60 ? 'strong signal' : rssi >= -72 ? 'good signal' : 'weak signal'; }
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
  if (saved('wifi_ssid')) out.push(h('p', { class: 'note' }, 'Saved network: ' + saved('wifi_ssid')));
  var busy = S.scan && S.scan.busy;
  out.push(btn(busy ? 'Looking for networks…' : S.nets ? 'Look again' : 'Find networks', scanNetworks, 'wide', { disabled: busy, 'data-f': 'scan' }));
  if (busy) out.push(h('p', { class: 'msg', role: 'status' }, h('span', { class: 'spin', 'aria-hidden': 'true' }), 'This takes a few seconds.'));
  if (S.scan && S.scan.error) out.push(h('p', { class: 'err', role: 'alert' }, S.scan.error));
  if (S.nets) {
    if (!S.nets.length) out.push(h('p', { class: 'note' }, 'No networks found. Move the device closer to the router and look again, or type the name below.'));
    else out.push(h('div', { class: 'list' }, S.nets.map(function (n, i) {
      return h('button', { type: 'button', class: 'it', 'aria-pressed': String(n.ssid === ssid), 'data-f': 'net' + i, onclick: function () {
        setVal('wifi_ssid', n.ssid);
        S.netOpen = !n.secure;
        S.res.wifi = null;
        render();
        if (n.secure) focusF('in:wifi_pass');
      } }, bars(n.rssi), h('span', { class: 't' }, n.ssid, h('small', null, (n.secure ? 'Locked' : 'Open') + ', ' + signalWord(n.rssi))),
      n.ssid === ssid ? h('span', { 'aria-hidden': 'true' }, '✓︎') : null);
    })));
  }
  out.push(textField('wifi_ssid', 'Network name', { note: 'Pick it above, or type it for a hidden network.', onin: function () { S.res.wifi = null; } }));
  out.push(secretField('wifi_pass', 'Password', { pw: true, onin: function () { S.res.wifi = null; } }));
  var res = resultEl(S.res.wifi);
  out.push(btn('Test the connection', function () { testWifi(res); }, '', { 'data-f': 'wtest' }), res);
  return out;
}
function wifiFix(err) {
  var e = String(err || '').toLowerCase();
  if (e.indexOf('password') >= 0) return ' Check the password: capitals count.';
  if (e.indexOf('found') >= 0) return ' Check the name, and that the network uses 2.4 GHz and is in range.';
  if (e.indexOf('answer') >= 0) return ' The router didn\'t answer: try again, or move the device closer.';
  return '';
}
function testWifi(el, then) {
  var ssid = String(cur('wifi_ssid') || '').trim(), typed = S.secrets.wifi_pass;
  var body = { ssid: ssid };
  var fail = function (t) { S.res.wifi = { ok: false, text: t }; if (el) showResult(el, S.res.wifi); if (then) then(false); };
  if (!ssid) return fail('Pick a network first.');
  if (typeof typed === 'string') body.pass = typed;
  else if (S.netOpen) body.pass = '';
  else if (ssid !== saved('wifi_ssid') || !secretInfo('wifi_pass').set) return fail('Type the password first.');
  S.res.wifi = { busy: true, text: 'Connecting to ' + ssid + '… this can take 20 seconds.' };
  if (el) showResult(el, S.res.wifi);
  api('POST', '/api/wifi-test', body, { timeout: 35000 }).then(function (r) {
    S.res.wifi = r.ok ? { ok: true, text: 'Connected: the network and password work.' } :
      { ok: false, text: 'Didn\'t connect: ' + (r.error || 'unknown reason') + '.' + wifiFix(r.error) };
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
  var dl = uid(), tz = S.tz;
  var note = 'A name like Europe/Berlin, or a rule like CET-1CEST,M3.5.0,M10.5.0/3. Pick one from the list or type it.';
  if (tz && tz.value && S.tzFilled && cur('timezone') === tz.value) note += ' Filled in from your phone (' + tz.name + ').';
  if (tz && !tz.value && !cur('timezone')) note += ' Your phone\'s time zone (' + tz.name + ') isn\'t in the list: type its rule.';
  var opts = TZ_LIST.map(function (t) { return h('option', { value: t[0] }, t[1]); });
  return textField('timezone', 'Time zone', { list: dl, ph: 'Europe/Berlin', note: note,
    after: h('datalist', { id: dl }, opts) });
}

// ---------- screens: loading, error, login, restart ----------

function fullCard() { var c = card.apply(null, arguments); return h('div', { class: 'full' }, c); }

function errorScreen(text) {
  return function () {
    return fullCard(h('h1', null, 'Can\'t reach the device'), h('p', null, text),
      btn('Try again', load, 'pri wide', { 'data-f': 'retry' }),
      h('p', { class: 'note' }, 'If it keeps failing, the old setup page is at /basic.'));
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
    if (code.length !== 6) { S.loginErr = 'Type all 6 digits.'; err.textContent = S.loginErr; err.hidden = false; return; }
    busy = true;
    go.disabled = true;
    api('POST', '/api/login', { code: code }).then(function (r) {
      busy = false;
      go.disabled = false;
      if (r.ok) { S.screen = null; load(); return; }
      S.loginErr = (r.error || 'Wrong code') + '. Check the code on the display and type it again; after 5 wrong codes the display shows a new one.';
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
  var go = h('button', { type: 'submit', class: 'btn pri wide' }, 'Open setup');
  var err = h('p', { class: 'err', role: 'alert', hidden: !S.loginErr }, S.loginErr);
  return fullCard(h('h1', null, 'Type the 6-digit code shown on the display'),
    h('p', { class: 'note' }, 'The code makes sure only someone at the display can change it.'),
    h('form', { onsubmit: submit }, h('div', { class: 'f' }, h('label', { for: 'code' }, 'Code'), inp), err, h('div', { style: 'margin-top:12px' }, go)));
}

function infoScreen(title, lines, poll) {
  S.poll = { tries: 0, gaveUp: false };
  S.screen = function () {
    var p = S.poll || {};
    return fullCard(h('h1', null, title), lines.map(function (l) { return h('p', null, l); }),
      poll ? h('p', { class: 'msg', role: 'status' }, p.gaveUp ? 'Still no answer. Reconnect your phone to the device\'s Wi-Fi and reload this page.' :
        [h('span', { class: 'spin', 'aria-hidden': 'true' }), 'Waiting for the device…']) : null,
      poll && p.gaveUp ? btn('Reload', function () { location.reload(); }, 'wide') : null);
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
    if (r.ok && r.device) { S.poll = null; start(r); toast('The device is back.'); return; }
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
    infoScreen('Setup finished', ['The display shows your dashboard in a moment. You can close this page.',
      home ? 'Open edit mode again with a 4-second hold.' : 'To change something later: hold the button for 4 seconds.'], false);
    return;
  }
  infoScreen('Saved — restarting…', [home ?
    'Saved. The device restarts and comes back in about 20 seconds (it reconnects to your Wi-Fi). This page reloads by itself.' :
    'Saved. The device restarts and comes back in about 10 seconds. Your phone reconnects to Dashboard-Setup by itself; if this page doesn\'t come back, join Dashboard-Setup again (password on the display) and open http://192.168.4.1.'], true);
}

// ---------- sheet / dialog ----------

function openSheet(title, sub, content, back) {
  closeSheet();
  var id = uid();
  var scrim = h('div', { class: 'scrim', onclick: function () { closeSheet(); focusF(back); } });
  var box = h('div', { class: 'sheet', role: 'dialog', 'aria-modal': 'true', 'aria-labelledby': id },
    h('div', { class: 'in' }, h('div', { class: 'grab' }),
      h('div', { class: 'row' }, h('h2', { id: id, class: 'grow' }, title), h('button', { type: 'button', class: 'lnk', onclick: function () { closeSheet(); focusF(back); } }, 'Close')),
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
    btn('Cancel', function () { closeSheet(); focusF(back); }),
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
function setBusy(t) { S.busy = t; updateBar(); }
function fillBar() {
  var el = S.barEl, inner = h('div', { class: 'in' }), guide = isGuide();
  el.innerHTML = '';
  var errs = Object.keys(S.errors);
  if (errs.length && !guide) {
    inner.appendChild(h('div', { role: 'alert' }, h('p', { class: 'err' }, 'Fix ' + (errs.length === 1 ? 'this field' : 'these fields') + ', then save again:'),
      h('div', { class: 'btns' }, errs.map(function (k) {
        if (!has(FIELDS, k.split('.')[0])) return h('p', { class: 'msg' }, k + ': ' + S.errors[k]);
        return h('button', { type: 'button', class: 'lnk', onclick: function () { goField(k); } }, fieldLabel(k));
      }))));
  }
  if (S.barMsg) inner.appendChild(h('p', { class: 'err', role: 'alert' }, S.barMsg));
  if (S.busy) inner.appendChild(h('p', { class: 'msg', role: 'status' }, h('span', { class: 'spin', 'aria-hidden': 'true' }), S.busy));
  if (guide) guideButtons(inner);
  else {
    var n = dirtyCount();
    if (n) inner.appendChild(btn('Save ' + plural(n, 'change', 'changes') + ' and restart', saveSettings, 'pri wide', { disabled: !!S.busy, 'data-f': 'b-save' }));
    if (S.tab === 'screen') {
      var ok = layoutCheck().ok;
      inner.appendChild(h('div', { class: 'btns' },
        btn('Exact preview', exactPreview, '', { disabled: !ok || !!S.busy, 'data-f': 'b-prev' }),
        btn('Show on the display', sendLayout, n ? '' : 'pri', { disabled: !ok || !!S.busy, 'data-f': 'b-lay' })));
    }
  }
  el.hidden = !inner.childNodes.length;
  el.appendChild(inner);
}
function goField(k) {
  var t = fieldMeta(k)[0];
  if (S.tab !== t) { S.tab = t; render(); }
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
    else if (!r.login) r.error = 'The screen layout wasn\'t taken: ' + (r.error || 'unknown reason') + ' Fix it on the Screen tab.';
    return r;
  });
}
function saveSettings() {
  var body = settingsBody();
  if (!Object.keys(body).length) return;
  S.barMsg = '';
  setBusy('Saving…');
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
      toast('Nothing was saved yet: fix the marked fields.');
    } else {
      S.barMsg = r.error || 'Saving didn\'t work. Try again.';
      render();
    }
  });
}
function sendLayout() {
  S.barMsg = '';
  setBusy('Sending the layout to the display…');
  api('POST', '/api/layout', layoutBody(), { timeout: 20000 }).then(function (r) {
    S.busy = '';
    if (r.ok) { S.laySaved = JSON.stringify(layoutBody()); toast('Sent: the display is drawing it now.'); }
    else if (!r.login) S.barMsg = 'The display didn\'t take this layout: ' + (r.error || 'unknown reason');
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
    else S.preview = { err: r.status === 503 ? (r.error || 'The device is short of memory.') + ' Try again in a moment.' : r.error };
    S.scrollPv = true;
    render();
  });
}
function postAction(path, title, lines, back, body) {
  setBusy('Asking the device…');
  api('POST', path, body || {}, { timeout: 15000 }).then(function (r) {
    S.busy = '';
    if (r.ok) { infoScreen(title, lines, false); return; }
    if (!r.login) { toast(r.error || 'That didn\'t work. Try again.'); updateBar(); focusF(back); }
  });
}
function changeWifi() {
  var text = 'The device restarts into setup mode. Join the Wi-Fi "Dashboard-Setup" with the password shown on the display.';
  confirmUnsaved('Change Wi-Fi?', text, 'Restart into setup mode', function () {
    postAction('/api/finish', 'Restarting into setup mode…', [text, 'Then open this page again: it opens by itself, or go to any web address.'], 'chgwifi', { ap: true });
  }, 'chgwifi');
}
function unsavedList() {
  var u = [];
  if (dirtyCount()) u.push(plural(dirtyCount(), 'setting', 'settings'));
  if (S.lay && layoutDirty()) u.push('the screen layout');
  if (S.p.tasks && JSON.stringify(S.p.tasks) !== S.p.tasksSaved) u.push('tasks');
  if (S.p.note !== null && S.p.note !== S.p.noteSaved) u.push('the note');
  if (S.p.bd && JSON.stringify(S.p.bd.manual) !== S.p.bdSaved) u.push('birthdays');
  return u;
}
function confirmUnsaved(title, text, ok, fn, back) {
  var u = unsavedList();
  if (u.length) ask(title, 'Not saved yet: ' + u.join(', ') + '. These changes are lost. ' + text, ok, fn, back);
  else ask(title, text, ok, fn, back);
}
function installUpdate(back) {
  confirmUnsaved('Install the update now?', 'The device restarts, looks for the newest version and installs it (it needs 30 % battery or USB power). This page stops working until then.',
    'Install', function () {
      postAction('/api/update', 'Installing the update', ['The device restarts and installs the update; the display shows the progress.',
        'When it\'s done the display shows your dashboard. Open setup again if you need it.']);
    }, back);
}
function finish() {
  confirmUnsaved('Leave setup?', 'The display restarts and shows your dashboard. This page stops working.', 'Leave setup', function () {
    postAction('/api/finish', 'All set', ['The display restarts and shows your dashboard.', 'You can close this page.'], 'finish');
  }, 'finish');
}

// ---------- Home tab ----------

// A Unix time as the local clock time, with the date when it isn't the device's today.
function clockAt(t, now) {
  var d = new Date(t * 1000), n = new Date((now || t) * 1000);
  var hm = pad(d.getHours()) + ':' + pad(d.getMinutes());
  return d.toDateString() === n.toDateString() ? hm : d.getDate() + ' ' + MONTHS[d.getMonth()].slice(0, 3) + ' ' + hm;
}
// device.battery is the last reading with Wi-Fi off; batteryAt its time (null = measured now).
function batteryText(d, short) {
  if (d.usb) return 'On USB power';
  if (!(d.battery >= 0)) return 'No battery reading';
  var t = 'Battery ' + d.battery + ' %';
  if (d.batteryAt) t += short ? ' (' + clockAt(d.batteryAt, d.time) + ')' : ', measured ' + clockAt(d.batteryAt, d.time);
  return t;
}
function homeTab() {
  var d = S.state.device, out = [], w = d.wifi || {};
  out.push(lbl('Screen'), sketchEl(-1), h('p', { class: 'cap' }, layoutDirty() ?
    'This is the edited layout; it isn\'t on the display yet. Open Screen and tap "Show on the display".' :
    'A sketch of the screen. For the exact picture use "Exact preview" on the Screen tab.'));
  var probs = (d.problems || []).slice();
  if (d.layoutFallback) {
    var why = typeof d.layoutFallback === 'string' ? d.layoutFallback : (d.layoutFallback.text || d.layoutFallback.reason || '');
    probs.unshift({ text: 'The saved screen layout can\'t be used, so Classic is shown' + (why ? ': ' + why : '.'), fix: 'Choose a layout on the Screen tab and tap "Show on the display".' });
  }
  out.push(lbl('Problems'));
  if (!probs.length) out.push(card(h('p', { class: 'msg okm' }, 'No problems.')));
  probs.forEach(function (p) { out.push(h('div', { class: 'card warn' }, h('div', null, h('b', null, p.text), p.fix ? h('span', { class: 'note' }, p.fix) : null))); });
  if (d.updateAvailable) {
    out.push(lbl('Update'), card(h('p', { class: 'big' }, 'Version ' + d.updateAvailable + ' is available'),
      h('p', { class: 'note' }, 'The device restarts, installs it and shows the progress on the display.'),
      btn('Install now', function () { installUpdate('upd'); }, 'pri wide', { 'data-f': 'upd' })));
  }
  out.push(lbl('Device'), card(
    kv('Battery', batteryText(d)),
    S.state.power && S.state.power.note ? kv('Power', S.state.power.note) : null,
    S.state.power && S.state.power.saver ? h('p', { class: 'note' }, 'Battery below 20 %: the display updates every 15 minutes until it\'s charged above 30 %.') : null,
    kv('Wi-Fi', w.ssid ? (w.connected ? w.ssid + ', ' + signalWord(w.rssi) : w.ssid + ' (not connected now)') : 'Not set up'),
    kv('Version', d.version + (d.build ? ' (build ' + d.build + ')' : '')),
    d.lastUpdate ? kv('Last update', d.lastUpdate) : null,
    kv('Clock', d.time ? new Date(d.time * 1000).toLocaleString() : 'Not set yet: set from the internet on the next wake')));
  out.push(lbl('Finish'), card(h('p', null, 'When everything is set, leave setup: the display restarts and shows your dashboard.'),
    btn('Done', finish, 'pri wide', { 'data-f': 'finish' })));
  return out;
}

// ---------- Screen tab ----------

function boxList() {
  var W = widgetsByKey();
  return h('div', { class: 'list' }, S.lay.boxes.map(function (b, i) {
    var w = b.widget ? W[b.widget] : null, hint = '';
    if (b.widget && !w) hint = 'Unknown widget: choose another one';
    else if (w && !Layout.fits(w, b)) hint = 'Too small: ' + w.name + ' needs ' + w.minW + ' × ' + w.minH;
    else if (w && w.configured === false) hint = w.hint || 'Not set up yet';
    return h('button', { type: 'button', class: 'it' + (i === S.sel ? ' sel' : ''), 'aria-current': i === S.sel ? 'true' : null, 'data-f': 'box' + i,
      onclick: function () { S.sel = i; render(); openPicker(i); } },
      icon(b.widget), h('span', { class: 't' }, w ? w.name : b.widget || 'Empty box',
        h('small', null, 'Box ' + (i + 1) + ', ' + b.w + ' × ' + b.h + (b.w * b.h === 1 ? ' cell' : ' cells')), hint ? h('small', null, hint) : null),
      h('span', { class: 'ch', 'aria-hidden': 'true' }, '›'));
  }));
}
function splitInfo() {
  var b = S.lay.boxes[S.sel];
  if (!b) return 'Select a box first.';
  if (!Layout.canSplit(b)) return 'A 1 × 1 box can\'t be split.';
  if (S.lay.boxes.length >= Layout.MAX_BOXES) return 'The screen has room for ' + Layout.MAX_BOXES + ' boxes at most.';
  return '';
}
function doSplit() {
  var r = Layout.split(S.lay.boxes, S.sel, widgetList());
  if (!r || splitInfo()) return;
  S.lay.boxes = r;
  S.lay.key = 'custom';
  toast('Box ' + (S.sel + 1) + ' is now box ' + (S.sel + 1) + ' and box ' + (S.sel + 2) + '.');
  layChanged();
  focusF('box' + S.sel);
}
function doMerge(j) {
  var i = S.sel, r = Layout.merge(S.lay.boxes, i, j);
  if (!r) return;
  S.lay.boxes = r;
  S.lay.key = 'custom';
  S.sel = Math.min(i, j);
  toast('Box ' + (i + 1) + ' and box ' + (j + 1) + ' are now box ' + (S.sel + 1) + '.');
  layChanged();
  focusF('box' + S.sel);
}
function openMerge() {
  var p = Layout.mergePartners(S.lay.boxes, S.sel), W = widgetsByKey(), a = S.lay.boxes[S.sel];
  if (!p.length) return;
  if (p.length === 1) { doMerge(p[0]); return; }
  var words = { right: 'on the right', below: 'below', left: 'on the left', above: 'above' };
  openSheet('Merge box ' + (S.sel + 1) + ' with', 'The merged box keeps what box ' + (S.sel + 1) + ' shows (or the other one\'s, if box ' + (S.sel + 1) + ' is empty).',
    h('div', { class: 'list' }, p.map(function (j) {
      var o = S.lay.boxes[j], w = o.widget && W[o.widget];
      return h('button', { type: 'button', class: 'it', onclick: function () { closeSheet(); doMerge(j); } }, icon(o.widget),
        h('span', { class: 't' }, 'Box ' + (j + 1) + ' ' + words[Layout.direction(a, o)], h('small', null, w ? w.name : 'Empty box')));
    })), 'merge');
}
function toolsRow(inSheet) {
  var why = splitInfo(), partners = Layout.mergePartners(S.lay.boxes, S.sel);
  var n = S.sel + 1;
  return [h('div', { class: 'tools' },
    btn('Split box ' + n, function () { if (inSheet) closeSheet(); doSplit(); }, '', { disabled: !!why, 'data-f': inSheet ? null : 'split' }),
    btn(partners.length > 1 ? 'Merge box ' + n + '…' : 'Merge box ' + n, function () { if (inSheet) closeSheet(); openMerge(); }, '', { disabled: !partners.length, 'data-f': inSheet ? null : 'merge' })),
  why || !partners.length ? h('p', { class: 'note', style: 'margin-top:6px' }, [why, partners.length ? '' : 'Box ' + n + ' has no neighbour sharing a full side, so it can\'t be merged.'].join(' ').trim()) : null];
}
function openPicker(i) {
  var b = S.lay.boxes[i];
  var items = widgetList().map(function (w) {
    var fit = Layout.fits(w, b), used = S.lay.boxes.some(function (o, k) { return k !== i && o.widget === w.key; });
    var why = !fit ? 'needs ' + w.minW + ' × ' + w.minH : used ? 'already on screen' : w.configured === false ? (w.hint || 'not set up yet') : '';
    return h('button', { type: 'button', disabled: !fit || used, 'aria-pressed': String(b.widget === w.key), onclick: function () { pick(w.key); } },
      icon(w.key), h('span', null, w.name, why ? h('small', null, why) : null));
  });
  items.push(h('button', { type: 'button', 'aria-pressed': String(!b.widget), onclick: function () { pick(null); } }, icon(null), h('span', null, 'Leave empty')));
  function pick(k) {
    S.lay.boxes[i].widget = k;
    closeSheet();
    layChanged();
    focusF('box' + i);
  }
  openSheet('Box ' + (i + 1) + ': what should it show?', b.w + ' × ' + b.h + (b.w * b.h === 1 ? ' cell' : ' cells') + '. Widgets that need more room are greyed out.',
    [h('div', { class: 'wgrid' }, items), toolsRow(true)], 'box' + i);
}
function showList() {
  var W = widgetsByKey();
  var keys = CLASSIC.map(function (c) { return c[0]; }).filter(function (k) { return k !== 'clock' && W[k]; });
  return card(h('p', { class: 'note' }, 'Classic is the original screen: its places are fixed. Choose what it shows; the clock is always on.'),
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
  var p = S.preview, close = btn('Close the exact preview', function () { if (p.url) URL.revokeObjectURL(p.url); S.preview = null; render(); }, 'wide');
  if (p.busy) return h('div', { class: 'card', id: 'pv' }, h('p', { class: 'msg', role: 'status' }, h('span', { class: 'spin', 'aria-hidden': 'true' }), 'The device is drawing the exact preview…'));
  if (p.err) return h('div', { class: 'card', id: 'pv' }, h('p', { class: 'err', role: 'alert' }, 'No exact preview: ' + p.err), close);
  return h('div', { class: 'card', id: 'pv' }, h('p', { class: 'big' }, 'Exact preview'),
    h('div', { class: 'epd' }, h('img', { src: p.url, alt: 'Exact picture of the layout, drawn by the device', width: 800, height: 480 })),
    h('p', { class: 'note' }, 'Drawn by the device with the data it has right now. After a restart some data (e.g. the weather) appears only after the next update. Not on the display yet.'), close);
}
function screenTab() {
  var out = [lbl('Preview'), sketchEl(S.lay.classic ? -1 : S.sel)];
  if (S.preview) out.push(previewCard());
  out.push(structStrip());
  if (S.lay.classic) out.push(lbl('Show on Classic'), showList());
  else {
    out.push(lbl('Boxes: tap one to choose what it shows'), boxList(), toolsRow(false));
    var v = layoutCheck();
    if (!v.ok) out.push(h('div', { class: 'card', role: 'alert', style: 'margin-top:10px' }, h('p', { class: 'big' }, 'This layout can\'t be shown yet'),
      v.errors.map(function (e) { return h('p', { class: 'err' }, e.text); })));
    out.push(lbl('Status bar'), card(switchRow('Status bar on top', 'Date, Wi-Fi, battery and problems. Off: the boxes use the full height.', S.lay.statusBar,
      function () { S.lay.statusBar = !S.lay.statusBar; layChanged(); }, 'sb')));
  }
  out.push(lbl('Clock'), card(seg('Clock format', [[true, '24 hours (14:30)'], [false, '12 hours (2:30 PM)']], cur('clock_24h') !== false,
    function (v) { setVal('clock_24h', v); render(); }, 'c24-'), h('p', { class: 'note' }, 'A setting: saved with "Save … and restart".')));
  if (saved('clock_interval') !== undefined) out.push(lbl('Clock refresh'), clockRefreshCard());
  if (saved('quiet_enabled') !== undefined) out.push(lbl('Night'), nightCard());
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
    var n = e.interval, sub = e.days ? 'about ' + e.days + ' days per charge' + (e.gainPercent > 0 ? ' (+' + e.gainPercent + ' %)' : '') : '';
    return h('button', { type: 'button', class: 'it', 'aria-pressed': String(n === now), 'data-f': 'ci' + n,
      onclick: function () { setVal('clock_interval', n); if (S.errors.clock_interval) delete S.errors.clock_interval; render(); } },
      h('span', { class: 't' }, n === 1 ? 'Every minute' : 'Every ' + n + ' minutes', sub ? h('small', null, sub) : null),
      n === now ? h('span', { 'aria-hidden': 'true' }, '✓︎') : null);
  });
  return h('div', { class: 'card', 'data-k': 'clock_interval' },
    h('p', null, 'The display wakes this often to update the clock. Less often = longer battery life; the clock then shows the time of the last update (e.g. 9:45 until 9:50).'),
    h('div', { class: 'list', role: 'group', 'aria-label': 'Clock refresh' }, rows), errLine('clock_interval'),
    estimatesStale() ? h('p', { class: 'note' }, 'The estimates are updated after saving the changed quiet hours or battery capacity.') : null,
    P.estimateNote ? h('p', { class: 'note' }, P.estimateNote) : null);
}
function nightCard() {
  var P = power(), on = !!cur('quiet_enabled'), out = [];
  out.push(settingSwitch('quiet_enabled', 'Quiet hours at night', on ? (cur('quiet_from') || '23:00') + ' to ' + (cur('quiet_to') || '06:00') : 'Off: updates day and night'));
  if (on) {
    out.push(h('div', { class: 'grid3', style: 'grid-template-columns:1fr 1fr' },
      textField('quiet_from', 'From', { type: 'time' }), textField('quiet_to', 'Until', { type: 'time' })));
  }
  out.push(h('p', { class: 'note' }, 'No updates during these hours; the display keeps its last picture and says "Night until ' + (cur('quiet_to') || '06:00') +
    '". A tap on the button shows the current dashboard.'));
  if (P.daysWithQuietHours && P.daysWithoutQuietHours) {
    out.push(h('p', null, 'With quiet hours: about ' + P.daysWithQuietHours + ' days, without: ' + P.daysWithoutQuietHours + ' days' +
      (has(S.draft, 'clock_interval') ? ' (for the saved clock refresh)' : '') + '.'));
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
  if (S.p.err[kind]) return card(h('p', { class: 'err' }, S.p.err[kind]), btn('Try again', function () { loadPersonal(kind); }, '', { 'data-f': 'reload-' + kind }));
  return card(h('p', { class: 'msg' }, h('span', { class: 'spin', 'aria-hidden': 'true' }), 'Loading…'));
}
function removedText(r) { return r.removed ? ' The display can\'t show these characters, so they were left out: ' + r.removed : ''; }
function putPersonal(kind, path, body, savedKey, value) {
  S.res[kind] = { busy: true, text: 'Saving…' };
  render();
  api('PUT', path, body).then(function (r) {
    if (r.ok) {
      S.p[savedKey] = value;
      S.res[kind] = { ok: true, text: 'Saved.' + removedText(r) };
      if (r.removed || kind !== 'note') loadPersonal(kind);   // new doneAt dates, upcoming birthdays
    }
    else S.res[kind] = r.login ? null : { ok: false, text: r.error || 'Saving didn\'t work. Try again.' };
    render();
  });
}

function dmText(ymd) { var p = String(ymd).split('-'); return p.length === 3 ? (+p[2]) + '.' + (+p[1]) + '.' : ymd; }
function tasksCard() {
  if (!S.p.tasks) return notLoaded('tasks');
  var list = S.p.tasks;
  var rows = list.map(function (t, i) {
    var n = i + 1;
    var title = h('input', { type: 'text', value: t.title || '', maxlength: 60, placeholder: 'What needs doing?', 'aria-label': 'Task ' + n, 'data-f': 'tt' + i,
      oninput: function () { t.title = title.value; } });
    var due = h('input', { type: 'date', value: t.due || '', 'aria-label': 'Task ' + n + ' due date (optional)', 'data-f': 'tdue' + i,
      onchange: function () { t.due = due.value || ''; } });
    var prio = h('select', { 'aria-label': 'Task ' + n + ' priority', 'data-f': 'tp' + i, onchange: function () { t.prio = +prio.value; } },
      [1, 2, 3].map(function (p) { return h('option', { value: String(p), selected: (t.prio || 2) === p }, ['High', 'Normal', 'Low'][p - 1]); }));
    return h('div', { class: 'task' + (t.done ? ' done' : '') },
      h('div', { class: 'row' }, h('button', { type: 'button', class: 'chk', role: 'checkbox', 'aria-checked': String(!!t.done), 'aria-label': 'Task ' + n + ' done', 'data-f': 'td' + i,
        onclick: function () { t.done = !t.done; if (!t.done) delete t.doneAt; render(); } }, h('span', { 'aria-hidden': 'true' }, t.done ? '✓︎' : '')), title),
      t.done ? h('p', { class: 'note', style: 'margin:4px 0 0 52px' }, 'done' + (t.doneAt ? ' ' + dmText(t.doneAt) : '') + ' · removed after 30 days') : null,
      h('div', { class: 'row' }, due, prio, h('button', { type: 'button', class: 'x', 'aria-label': 'Delete task ' + n,
        onclick: function () { list.splice(i, 1); render(); focusF('addtask'); } }, '✕︎')));
  });
  return card(rows.length ? h('div', null, rows) : h('p', { class: 'note' }, 'No tasks yet.'),
    h('p', { class: 'note' }, list.length + ' of 30. Done tasks aren\'t shown on the display; delete them to make room.'),
    h('div', { class: 'btns' },
      btn('Add a task', function () {
        list.push({ title: '', due: '', prio: 2, done: false });
        render();
        focusF('tt' + (list.length - 1));
      }, '', { disabled: list.length >= 30, 'data-f': 'addtask' }),
      btn('Save tasks', saveTasks, 'pri', { 'data-f': 'savetasks' })),
    resultEl(S.res.tasks));
}
function saveTasks() {
  var list = S.p.tasks, bad = -1;
  list.forEach(function (t, i) { t.title = String(t.title || '').trim(); if (!t.title && bad < 0) bad = i; });
  if (bad >= 0) { S.res.tasks = { ok: false, text: 'Task ' + (bad + 1) + ' has no title: type one or delete the task.' }; render(); focusF('tt' + bad); return; }
  putPersonal('tasks', '/api/tasks', { tasks: list }, 'tasksSaved', JSON.stringify(list));
}

function noteCard() {
  if (S.p.note === null) return notLoaded('note');
  var count = h('p', { class: 'note', id: 'notecount' }, S.p.note.length + ' / 400 characters');
  var ta = h('textarea', { id: 'note', maxlength: 400, rows: 6, value: S.p.note, 'data-f': 'note', 'aria-describedby': 'notecount',
    oninput: function () { S.p.note = ta.value; count.textContent = ta.value.length + ' / 400 characters'; } });
  return card(h('div', { class: 'f' }, h('label', { for: 'note' }, 'Note on the display'), ta, count),
    btn('Save note', function () { putPersonal('note', '/api/notes', { note: S.p.note }, 'noteSaved', S.p.note); }, 'pri wide', { 'data-f': 'savenote' }),
    resultEl(S.res.note));
}

function daysIn(m) { return new Date(2000, m, 0).getDate(); }
function bdText(b) { return b.day + ' ' + MONTHS[b.month - 1] + (b.year ? ' ' + b.year : ''); }
function birthdaysCard() {
  var parts = [];
  var chkRes = resultEl(S.res.bday_url);
  parts.push(secretField('bday_url', 'Birthday calendar link', { type: 'url', im: 'url', ph: 'https://…/basic.ics', after: chkRes,
    extra: [btn('Check', function () { checkCalendar('bday_url', -1, chkRes); }, 'sm', { 'data-f': 'chk:bday_url' })],
    note: 'Optional: an iCal link with birthdays (for example Google\'s birthday calendar). They don\'t show up in the agenda.' }));
  if (!S.p.bd) { parts.push(notLoaded('bd')); return card(parts); }
  var man = S.p.bd.manual, nb = S.p.newBd;
  parts.push(h('p', { class: 'fl', style: 'font-weight:650' }, 'Entered by hand (' + man.length + ' of 50)'));
  if (man.length) {
    parts.push(h('div', null, man.map(function (b, i) {
      return h('div', { class: 'kv' }, h('b', null, b.name), h('span', { class: 'row' }, bdText(b),
        h('button', { type: 'button', class: 'x', 'aria-label': 'Delete ' + b.name, 'data-f': 'bdx' + i, onclick: function () { man.splice(i, 1); render(); focusF('bdname'); } }, '✕︎')));
    })));
  } else parts.push(h('p', { class: 'note' }, 'None yet.'));
  var name = h('input', { type: 'text', id: uid(), maxlength: 32, value: nb.name, placeholder: 'Name', autocapitalize: 'words', 'data-f': 'bdname', oninput: function () { nb.name = name.value; } });
  var day = h('input', { type: 'text', id: uid(), inputmode: 'numeric', maxlength: 2, value: nb.day, placeholder: 'Day', oninput: function () { nb.day = day.value; } });
  var month = h('select', { id: uid(), onchange: function () { nb.month = month.value; } }, MONTHS.map(function (m, i) { return h('option', { value: String(i + 1), selected: String(i + 1) === String(nb.month) }, m); }));
  var year = h('input', { type: 'text', id: uid(), inputmode: 'numeric', maxlength: 4, value: nb.year, placeholder: 'optional', oninput: function () { nb.year = year.value; } });
  parts.push(h('div', { class: 'sub' }, h('p', { style: 'font-weight:650' }, 'Add a birthday'),
    h('div', { class: 'f' }, h('label', { for: name.id }, 'Name'), name),
    h('div', { class: 'grid3' }, h('div', { class: 'f' }, h('label', { for: day.id }, 'Day'), day), h('div', { class: 'f' }, h('label', { for: month.id }, 'Month'), month),
      h('div', { class: 'f' }, h('label', { for: year.id }, 'Year'), year)),
    h('p', { class: 'note' }, 'With the year, the display shows the age.'),
    btn('Add', addBirthday, 'wide', { disabled: man.length >= 50, 'data-f': 'bdadd' })));
  parts.push(btn('Save birthdays', function () {
    putPersonal('bd', '/api/birthdays', { manual: man }, 'bdSaved', JSON.stringify(man));
  }, 'pri wide', { 'data-f': 'savebd' }), resultEl(S.res.bd));
  var up = S.p.bd.upcoming || [];
  parts.push(h('p', { style: 'font-weight:650' }, 'Next 60 days'));
  parts.push(up.length ? h('div', null, up.map(function (b) {
    return kv(b.name + (b.age ? ' (' + b.age + ')' : ''), (b.days === 0 ? 'Today' : b.days === 1 ? 'Tomorrow' : 'in ' + b.days + ' days') + (b.fromFeed ? ', from the calendar' : ''));
  })) : h('p', { class: 'note' }, 'No birthdays in the next 60 days.'));
  return card(parts);
}
function addBirthday() {
  var nb = S.p.newBd, name = String(nb.name || '').trim(), d = parseInt(nb.day, 10), m = parseInt(nb.month, 10) || 1, y = String(nb.year || '').trim();
  var yr = y ? parseInt(y, 10) : 0, now = new Date().getFullYear(), err = '';
  if (!name) err = 'Type a name.';
  else if (!(d >= 1 && d <= daysIn(m))) err = 'Type a day between 1 and ' + daysIn(m) + ' for ' + MONTHS[m - 1] + '.';
  else if (y && !(/^\d{4}$/.test(y) && yr >= 1900 && yr <= now)) err = 'Type the year with 4 digits (1900 to ' + now + '), or leave it empty.';
  if (err) { S.res.bd = { ok: false, text: err }; render(); focusF('bdadd'); return; }
  S.p.bd.manual.push({ name: name, day: d, month: m, year: yr });
  S.p.newBd = { name: '', day: '', month: nb.month, year: '' };
  S.res.bd = { ok: true, text: name + ' added. Tap "Save birthdays" to keep it.' };
  render();
  focusF('bdname');
}

function dataTab() {
  return [
    lbl('Calendars'), card(calendarField(0), calendarField(1), calendarField(2),
      h('p', { class: 'note' }, 'Google Calendar: Settings → your calendar → Integrate calendar → secret address in iCal format. Up to 3, shown together.')),
    lbl('Weather'), card(placeField()),
    lbl('Stocks'), card(textField('stocks', 'Stock symbols', { ph: 'AAPL,NVDA,^IXIC', cap: 'characters', note: 'Up to 5 Yahoo Finance symbols, separated by commas.' })),
    lbl('Parcels and flights'), card(
      secretField('pkge_key', 'Parcels key (PKGE)', { note: 'Add your parcels on business.pkge.net; the display shows the ones on their way.' }),
      secretField('adb_key', 'Flights key (AeroDataBox)', { note: 'Optional: delays and gates for flights in your calendar. Free key at rapidapi.com → AeroDataBox.' })),
    lbl('3D printer'), card(
      textField('printer_name', 'Printer name', { ph: 'X1C', cap: 'words' }),
      textField('mqtt_ip', 'Printer address', { ph: '192.168.1.50', im: 'decimal', note: 'Leave empty to hide the printer.' }),
      textField('mqtt_sn', 'Serial number', { cap: 'characters' }),
      secretField('mqtt_pass', 'Access code', { pw: true, note: 'On the printer: Settings → WLAN → Access Code.' }),
      textField('mqtt_port', 'Port', { num: true, im: 'numeric', ph: '8883' }),
      textField('makerworld_uid', 'MakerWorld user ID', { im: 'numeric', note: 'Optional: shown when no print is running.' })),
    lbl('Tasks'), tasksCard(),
    lbl('Note'), noteCard(),
    lbl('Birthdays'), birthdaysCard()
  ];
}

// ---------- Device tab ----------

function slug(n) { return String(n || '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'dashboard'; }
function nameField() {
  var wrap = textField('device_name', 'Device name', { ph: 'Kitchen display', max: 32, cap: 'sentences',
    note: 'Shown here and used for the address on your home network: ' + slug(cur('device_name')) + '.local',
    onin: function (inp) {
      var n = wrap.querySelector('.note');
      if (n) n.textContent = 'Shown here and used for the address on your home network: ' + slug(inp.value) + '.local';
    } });
  return wrap;
}
function curPins() { return clone(cur('pins') || {}); }
function pinsPart() {
  var presets = S.state.pinPresets || [], pre = cur('pin_preset');
  var sel = h('select', { id: uid(), 'data-f': 'in:pin_preset', onchange: function () { setVal('pin_preset', sel.value); render(); } },
    presets.map(function (p) { return h('option', { value: p.key, selected: p.key === pre }, p.name); }));
  var out = [h('p', { class: 'note' }, 'Only for a device you built yourself with other wiring. Wrong pins leave the display blank.'),
    fieldBox('pin_preset', 'Board', sel)];
  if (pre === 'custom') {
    var pins = curPins();
    out.push(h('div', { class: 'grid3', style: 'grid-template-columns:1fr 1fr' }, PINS.map(function (p) {
      var v = pins[p[0]];
      var inp = h('input', { type: 'text', inputmode: 'numeric', value: v === undefined || v === null || v < 0 ? '' : String(v), 'data-f': 'pin:' + p[0],
        oninput: function () { var q = curPins(); q[p[0]] = inp.value.trim() === '' ? -1 : numOr(inp.value); setVal('pins', q); } });
      return fieldBox('pins.' + p[0], p[1], inp);
    })), h('p', { class: 'note' }, 'GPIO numbers. Leave an optional pin empty if it isn\'t connected.'));
  }
  return out;
}
function deviceTab() {
  var d = S.state.device;
  var adv = h('details', { open: !!S.adv, ontoggle: function () { S.adv = adv.open; } }, h('summary', null, 'Advanced: board pins'), h('div', { class: 'card' }, pinsPart()));
  var reset = S.resetAsk ?
    card(h('p', { class: 'big' }, 'Erase everything?'),
      h('p', null, 'All settings, tasks, notes and birthdays are erased. This can\'t be undone. The device restarts into setup mode.'),
      h('div', { class: 'btns' }, btn('Cancel', function () { S.resetAsk = false; render(); focusF('reset'); }, '', { 'data-f': 'reset-no' }),
        btn('Erase everything', function () {
          S.resetAsk = false;
          postAction('/api/factory-reset', 'Erased', ['The device restarts into setup mode.',
            'Connect your phone to its setup Wi-Fi again (the password is shown on the display) to set it up.'], 'reset');
        }, 'pri', { 'data-f': 'reset-yes' }))) :
    card(h('p', null, 'Erases all settings, tasks, notes and birthdays, then starts again with the first setup.'),
      btn('Factory reset…', function () { S.resetAsk = true; render(); focusF('reset-no'); }, 'wide', { 'data-f': 'reset' }));
  return [
    lbl('Name'), card(nameField()),
    lbl('Wi-Fi'), card(wifiPart(), d.mode === 'home' ? [
      h('p', { class: 'note' }, 'Moved, or a new router? Change Wi-Fi restarts the device into setup mode, with its own hotspot.'),
      btn('Change Wi-Fi', changeWifi, 'wide', { 'data-f': 'chgwifi' })] : null),
    lbl('Time zone'), card(tzField()),
    saved('battery_mah') !== undefined ? [lbl('Battery'), card(textField('battery_mah', 'Battery capacity (mAh)', { num: true, im: 'numeric', ph: '3000', max: 5,
      note: 'Only used for the estimates.' }))] : null,
    lbl('Updates'), card(
      settingSwitch('ota_auto', 'Install updates by themselves', 'At night, when there is a new version. Off: the status bar says "Update available".'),
      h('div', null, h('p', { class: 'fl', style: 'font-weight:650;margin-bottom:6px' }, 'Update channel'),
        seg('Update channel', [[false, 'Stable'], [true, 'Beta']], !!cur('ota_beta'), function (v) { setVal('ota_beta', v); render(); }, 'beta-'),
        h('p', { class: 'note', style: 'margin-top:6px' }, 'Beta gets test versions first. Use it only on a device you can reach.')),
      kv('Installed', d.version + (d.lastUpdate ? ' · ' + d.lastUpdate : '')),
      btn('Check for updates now', function () { installUpdate('chkupd'); }, 'wide', { 'data-f': 'chkupd' })),
    lbl('About'), card(kv('Board', d.variant || '?'), kv('Version', d.version + ' (build ' + d.build + ')'),
      kv('Mode', d.mode === 'home' ? 'Edit mode on the home network' : 'Setup hotspot'), kv('Battery', batteryText(d))),
    h('div', { style: 'margin-top:16px' }, diagSection()),
    h('div', { style: 'margin-top:8px' }, adv),
    lbl('Factory reset'), reset
  ];
}

// ---------- Device tab: diagnostics (GET /api/diagnostics, build 72) ----------

function loadDiag() {
  S.diag = { busy: true, data: S.diag && S.diag.data };
  render();
  api('GET', '/api/diagnostics', undefined, { timeout: 15000 }).then(function (r) {
    if (r.ok && r.battery) S.diag = { data: r };
    else S.diag = { err: r.login ? '' : r.error || 'The device sent no diagnostics.', data: S.diag && S.diag.data };
    render();
  });
}
// Relative times, against the device's clock.
function agoText(t, now) {
  if (!t) return 'never since power-on';
  if (!now) return 'clock not set';
  var s = now - t;
  if (s < 60) return 'just now';
  if (s < 3600) return Math.floor(s / 60) + ' min ago';
  if (s < 86400) return Math.floor(s / 3600) + ' h ago';
  return Math.floor(s / 86400) + ' d ago';
}
function inText(t, now) {
  if (!t) return 'none';
  if (!now) return 'clock not set';
  var s = t - now;
  if (s <= 0) return 'now';
  if (s < 3600) return 'in ' + Math.max(1, Math.ceil(s / 60)) + ' min';
  return 'in ' + Math.round(s / 3600) + ' h';
}
function volts(mv) { return (mv / 1000).toFixed(2) + ' V'; }
function kb(b) { return Math.round((b || 0) / 1024) + ' KB'; }

var NO_BATTERY = ': no battery measured (the board runs on USB power)';
function batteryNow(b) {
  if (!b.sensor) return 'No battery sensor on this board.';
  var t = volts(b.mv);
  if (b.plausible === false) t += NO_BATTERY;
  else if (b.percent >= 0) t += ', ' + b.percent + ' %';
  return t;
}
// The reading with Wi-Fi off (quietMv), when the device has one: shown first, the live one second.
function batteryLines(b, now) {
  if (!b.sensor || !(b.quietMv > 0)) return [batteryNow(b)];
  var q = volts(b.quietMv);
  if (b.quietMv < 3000 || b.quietMv > 4400) q += NO_BATTERY;
  else if (b.quietPercent >= 0) q += ', ' + b.quietPercent + ' %';
  q += ' (with Wi-Fi off' + (b.quietAt ? ', ' + clockAt(b.quietAt, now) : '') + ')';
  return [q, 'Now, with Wi-Fi on: ' + volts(b.mv) + (b.plausible === false ? NO_BATTERY : ' (the radio makes the voltage sag a little)')];
}

// Line chart of the battery history: x = last 7 days, y = 0-4.5 V, 3.0-4.2 V band hatched.
function batteryChart(hist, now) {
  var W = 340, H = 196, x0 = 34, x1 = 332, y0 = 8, y1 = 168, VMAX = 4500, DAY = 86400;
  var end = now || hist[hist.length - 1][0], start = end - 7 * DAY;
  function X(t) { return (x0 + (t - start) / (end - start) * (x1 - x0)).toFixed(1); }
  function Y(mv) { return (y1 - Math.max(0, Math.min(VMAX, mv)) / VMAX * (y1 - y0)).toFixed(1); }
  var s = '<defs><pattern id="hatch" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">' +
    '<line x1="0" y1="0" x2="0" y2="6" stroke="#000" stroke-width="1" opacity=".35"/></pattern></defs>';
  s += '<rect x="' + x0 + '" y="' + Y(4200) + '" width="' + (x1 - x0) + '" height="' + (Y(3000) - Y(4200)).toFixed(1) + '" fill="url(#hatch)"/>';
  s += '<text x="' + (x0 + 4) + '" y="' + (+Y(4200) + 12) + '" font-size="10">working battery</text>';
  for (var v = 0; v <= 4; v++) {
    s += '<line x1="' + x0 + '" y1="' + Y(v * 1000) + '" x2="' + x1 + '" y2="' + Y(v * 1000) + '" stroke="#000" stroke-width="' + (v ? .5 : 1.5) + '"' + (v ? ' stroke-dasharray="2 3"' : '') + '/>' +
      '<text x="' + (x0 - 4) + '" y="' + (+Y(v * 1000) + 4) + '" font-size="11" text-anchor="end">' + v + ' V</text>';
  }
  var d = new Date(start * 1000);
  d.setHours(24, 0, 0, 0);
  for (var t = d.getTime() / 1000; t <= end; t += DAY) {
    var day = new Date(t * 1000);
    s += '<line x1="' + X(t) + '" y1="' + y1 + '" x2="' + X(t) + '" y2="' + (y1 + 5) + '" stroke="#000"/>' +
      '<text x="' + X(t + DAY / 2) + '" y="' + (y1 + 18) + '" font-size="11" text-anchor="middle">' + (t + DAY / 2 < end ? DAYS[day.getDay()] : '') + '</text>';
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
  var label = 'Battery voltage, last 7 days: ' + hist.length + ' values, from ' + volts(hist[0][1]) + ' to ' + volts(last[1]) + ', the newest ' + volts(last[1]) + '.';
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
  if (!groups.length) el.appendChild(h('p', { class: 'note' }, total ? 'No line contains "' + filter + '".' : 'The log is empty.'));
  count.textContent = filter ? n + ' of ' + total + ' lines' : total + ' lines, newest first';
}
function legacyCopy(t) {
  var ta = h('textarea', { readonly: true, style: 'position:fixed;top:0;left:0;opacity:0' });
  ta.value = t;
  document.body.appendChild(ta);
  ta.select();
  var ok = false;
  try { ok = document.execCommand('copy'); } catch (e) { ok = false; }
  ta.remove();
  return ok;
}
function copyText(t) {
  if (navigator.clipboard && window.isSecureContext) {
    return navigator.clipboard.writeText(t).then(function () { return true; }, function () { return legacyCopy(t); });
  }
  return Promise.resolve(legacyCopy(t));
}
function canCopy() {
  return !!(navigator.clipboard && window.isSecureContext) || !!(document.queryCommandSupported && document.queryCommandSupported('copy'));
}

function diagPart() {
  var st = S.diag || {}, D = st.data, out = [];
  out.push(btn(st.busy ? 'Loading…' : 'Refresh', loadDiag, 'wide', { disabled: !!st.busy, 'data-f': 'diag-refresh' }));
  if (st.err) out.push(h('p', { class: 'err', role: 'alert' }, st.err));
  if (!D) {
    if (st.busy) out.push(h('p', { class: 'msg', role: 'status' }, h('span', { class: 'spin', 'aria-hidden': 'true' }), 'Asking the device…'));
    return out;
  }
  var now = D.time || 0, b = D.battery || {}, hist = b.history || [];
  // battery
  var bl = batteryLines(b, now);
  out.push(h('h3', { class: 'dh' }, 'Battery'), h('p', null, bl[0]), bl[1] ? h('p', { class: 'note' }, bl[1]) : null);
  if (b.usb) out.push(h('p', { class: 'note' }, 'USB from a computer.'));
  if (hist.length) {
    out.push(h('div', { class: 'chart', html: batteryChart(hist, now) }),
      h('p', { class: 'note' }, 'Hatched: 3.0-4.2 V, a working battery. Filled dots: USB from a computer. One value per hour.'));
  } else out.push(h('p', { class: 'note' }, 'No history yet: one value per hour.'));
  // services
  out.push(h('h3', { class: 'dh' }, 'Services'));
  var rows = (D.services || []).map(function (s) {
    return h('tr', null, h('td', null, s.name || s.key, s.stale ? h('span', { class: 'tag' }, 'old') : null),
      h('td', null, agoText(s.lastOk, now)), h('td', null, String(s.failures || 0)), h('td', null, inText(s.retryAt, now)));
  });
  out.push(h('div', { class: 'tblw' }, h('table', { class: 'tbl' },
    h('thead', null, h('tr', null, ['Service', 'Last success', 'Failures', 'Next retry'].map(function (t) { return h('th', { scope: 'col' }, t); }))),
    h('tbody', null, rows))));
  // the rest
  var known = {};
  (S.state.device.problems || []).forEach(function (p) { known[p.key] = p.text; });
  var probs = (D.problems || []).map(function (k) { return known[k] || k; });
  var w = D.wifi || {}, mem = D.heap || {};
  out.push(h('div', null,
    kv('Wi-Fi failures', (w.failures || 0) + (w.retryAfter ? ', next try ' + inText(w.retryAfter, now) : '')),
    w.rssi ? kv('Wi-Fi signal', w.rssi + ' dBm, ' + signalWord(w.rssi)) : null,
    kv('Last time sync', agoText(D.lastTimeSync, now)),
    kv('Problems', probs.length ? probs.join('; ') : 'none'),
    kv('Free memory', kb(mem.free) + ' (lowest ' + kb(mem.minFree) + ', largest block ' + kb(mem.largest) + ')'),
    D.bootCount ? kv('Starts since power-on', String(D.bootCount)) : null,
    kv('Device clock', now ? new Date(now * 1000).toLocaleString() : 'not set')));
  // log
  out.push(h('h3', { class: 'dh' }, 'Log'));
  var logEl = h('div', { class: 'log' }), count = h('p', { class: 'note', 'aria-live': 'polite' });
  var fid = uid();
  var filter = h('input', { type: 'search', id: fid, value: S.logFilter || '', placeholder: 'For example ICS or WiFi', autocomplete: 'off', autocapitalize: 'off',
    spellcheck: 'false', 'data-f': 'logfilter', oninput: function () { S.logFilter = filter.value; fillLog(logEl, count, D.log, filter.value.trim()); } });
  fillLog(logEl, count, D.log, (S.logFilter || '').trim());
  out.push(h('div', { class: 'f' }, h('label', { for: fid }, 'Show lines containing'), filter), count, logEl,
    h('div', { class: 'btns' },
      h('a', { class: 'btn', href: '/api/log.txt', download: 'dashboard-log.txt' }, 'Download log'),
      canCopy() ? btn('Copy', function () {
        var shown = logGroups(D.log, (S.logFilter || '').trim()).map(function (g) { return (g.date ? '--- ' + g.date + ' ---\n' : '') + g.lines.join('\n'); }).join('\n');
        copyText(shown).then(function (ok) { toast(ok ? 'Copied the shown lines.' : 'Copying didn\'t work here: use "Download log".'); });
      }, '', { 'data-f': 'logcopy' }) : null,
      btn('Clear log', function () {
        ask('Clear the log?', 'The stored log on the device is erased. The battery history and settings stay.', 'Clear log', function () {
          api('POST', '/api/log/clear', {}).then(function (r) {
            if (r.ok) { toast('The log is cleared.'); loadDiag(); }
            else if (!r.login) toast(r.error || 'Clearing didn\'t work. Try again.');
          });
        }, 'logclear');
      }, '', { 'data-f': 'logclear' })),
    h('p', { class: 'note' }, 'Download log saves the whole stored log as a text file; the list shows the last 200 lines.'));
  return out;
}
function diagSection() {
  var el = h('details', { open: !!S.diagOpen, ontoggle: function () {
    S.diagOpen = el.open;
    if (el.open && !S.diag) loadDiag();
  } }, h('summary', null, 'Diagnostics'), h('div', { class: 'card' }, S.diagOpen ? diagPart() : null));
  return el;
}

// ---------- first-start guide ----------

var GUIDE = ['Wi-Fi', 'Name and place', 'Calendars', 'Screen', 'Done'];
function isGuide() { return !!(S.state && S.state.device.firstRun && !S.screen); }
function guideView() {
  var g = S.guide, step = g.step, body = [];
  document.title = 'Set up your display';
  if (step === 0) {
    body.push(card(h('p', null, 'Pick your home Wi-Fi. The display uses it to load calendars, weather and updates.'), wifiPart()));
    if (S.nets === null && !S.scan) setTimeout(scanNetworks, 0);
    if (g.wifiFailed) body.push(card(h('p', null, 'The test didn\'t work. You can fix it above, or go on: the display tries again when it starts.'),
      btn('Go on without a working test', function () { g.wifiFailed = false; g.step = 1; render(); window.scrollTo(0, 0); }, 'wide')));
  } else if (step === 1) {
    body.push(card(nameField(), tzField(), placeField()));
  } else if (step === 2) {
    body.push(card(h('p', null, 'Paste up to three calendar links (iCal). You can skip this and add them later on the Data tab.'),
      calendarField(0), calendarField(1), calendarField(2),
      h('p', { class: 'note' }, 'Google Calendar: Settings → your calendar → Integrate calendar → secret address in iCal format.')));
  } else if (step === 3) {
    var W = widgetsByKey();
    body.push(structStrip(), lbl('Preview'), sketchEl(-1),
      card(S.lay.classic ? h('p', null, 'Classic: the original screen.') : S.lay.boxes.map(function (b, i) {
        return kv('Box ' + (i + 1), b.widget ? ((W[b.widget] && W[b.widget].name) || b.widget) : 'Empty');
      }), h('p', { class: 'note' }, 'You can change the boxes and what they show later, on the Screen tab.')));
  } else {
    var n = [0, 1, 2].filter(function (i) { var v = S.secrets['ics.' + i]; return typeof v === 'string' ? !!v.trim() : secretInfo('ics.' + i).set; }).length;
    var st = structures().filter(function (s) { return S.lay.classic ? s.classic : s.key === S.lay.key; })[0];
    body.push(card(h('p', null, 'Everything is saved at once, then the device restarts, connects to your Wi-Fi and shows your screen.'),
      h('div', null, kv('Wi-Fi', cur('wifi_ssid') || 'Not chosen'), kv('Name', cur('device_name') || 'Display'),
        kv('Time zone', cur('timezone') || 'Not set'), kv('Weather', cur('weather_loc') || 'Not set'),
        kv('Calendars', n ? plural(n, 'link', 'links') : 'None'), kv('Screen', st ? st.name : 'Your own layout'))));
  }
  var top = h('header', { class: 'top' }, h('div', { class: 'in' },
    h('div', { class: 'head' }, h('h1', null, 'Set up your display'), h('p', { class: 'st' }, 'Step ' + (step + 1) + ' of ' + GUIDE.length)),
    h('div', { class: 'steps', 'aria-hidden': 'true' }, GUIDE.map(function (x, i) { return h('i', { class: i <= step ? 'on' : '' }); })),
    h('p', { style: 'font-weight:700;padding-bottom:10px' }, GUIDE[step])));
  S.barEl = h('div', { class: 'bar' });
  fillBar();
  return h('div', null, top, h('main', { class: 'wrap' }, body), S.barEl);
}
var GUIDE_STEP = { wifi_ssid: 0, wifi_pass: 0, device_name: 1, timezone: 1, weather_loc: 1, ics: 2 };
function guideButtons(inner) {
  var g = S.guide, last = g.step === GUIDE.length - 1;
  var errs = Object.keys(S.errors);
  if (errs.length) inner.appendChild(h('p', { class: 'err', role: 'alert' }, 'Fix: ' + errs.map(fieldLabel).join(', ') + '.'));
  inner.appendChild(h('div', { class: 'btns' },
    g.step ? btn('Back', function () { g.step--; S.barMsg = ''; render(); window.scrollTo(0, 0); }, '', { disabled: !!S.busy, 'data-f': 'g-back' }) : null,
    btn(last ? 'Save and restart' : 'Next', last ? guideSave : guideNext, 'pri', { disabled: !!S.busy, 'data-f': 'g-next' })));
}
function guideNext() {
  var g = S.guide;
  S.barMsg = '';
  if (g.step === 0) {
    if (!String(cur('wifi_ssid') || '').trim()) { S.barMsg = 'Pick your Wi-Fi network first.'; updateBar(); return; }
    if (!(S.res.wifi && S.res.wifi.ok)) {
      setBusy('Testing the Wi-Fi…');
      testWifi(null, function (ok) {
        S.busy = '';
        if (ok) { g.step = 1; render(); window.scrollTo(0, 0); return; }
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
  setBusy('Saving…');
  var lay = layoutCheck().ok ? api('POST', '/api/layout', layoutBody(), { timeout: 20000 }) : Promise.resolve({ ok: true });
  lay.then(function (r) {
    if (!r.ok) { if (!r.login) { S.guide.step = 3; S.barMsg = 'The screen layout wasn\'t taken: ' + (r.error || 'unknown reason'); } return null; }
    var body = settingsBody();
    body.finish = true;
    return api('POST', '/api/settings', body, { timeout: 15000 });
  }).then(function (r) {
    S.busy = '';
    if (!r) { render(); return; }
    if (r.ok) { restarting(r); return; }
    if (r.login) return;
    if (r.errors && Object.keys(r.errors).length) {
      S.errors = normErrors(r.errors);
      var steps = Object.keys(S.errors).map(function (k) { var s = GUIDE_STEP[k.split('.')[0]]; return s === undefined ? 4 : s; });
      S.guide.step = Math.min.apply(null, steps);
      if (S.guide.step === 4) S.barMsg = r.error || 'The device didn\'t accept the settings.';
    } else S.barMsg = r.error || 'Saving didn\'t work. Try again.';
    render();
    window.scrollTo(0, 0);
  });
}

// ---------- main view ----------

function mainView() {
  var d = S.state.device, name = cur('device_name') || d.name || 'Display';
  document.title = name + ' setup';
  var tabs = [['home', 'Home'], ['screen', 'Screen'], ['data', 'Data'], ['device', 'Device']];
  var top = h('header', { class: 'top' }, h('div', { class: 'in' },
    h('div', { class: 'head' }, h('h1', null, name), h('p', { class: 'st' }, d.mode === 'home' ? 'Home network' : 'Setup hotspot', h('br'), batteryText(d, true))),
    h('div', { class: 'tabs', role: 'tablist', 'aria-label': 'Sections' }, tabs.map(function (t) {
      return h('button', { type: 'button', role: 'tab', id: 'tab-' + t[0], 'aria-selected': String(S.tab === t[0]), 'aria-controls': 'panel', 'data-f': 'tab-' + t[0],
        onclick: function () { if (S.tab !== t[0]) { S.tab = t[0]; S.barMsg = ''; window.scrollTo(0, 0); render(); } } }, t[1]);
    }))));
  var body = { home: homeTab, screen: screenTab, data: dataTab, device: deviceTab }[S.tab]();
  S.barEl = h('div', { class: 'bar' });
  fillBar();
  return h('div', null, top, h('main', { class: 'wrap', id: 'panel', role: 'tabpanel', 'aria-labelledby': 'tab-' + S.tab }, body), S.barEl);
}

function render() {
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
    state: state, tab: S.tab || 'home', draft: {}, secrets: {}, errors: {}, res: {}, busy: '', barMsg: '',
    nets: null, scan: null, netOpen: false, guide: { step: 0 }, preview: null, adv: false, resetAsk: false, screen: null,
    p: { tasks: null, tasksSaved: '[]', note: null, noteSaved: '', bd: null, bdSaved: '[]', err: {}, newBd: { name: '', day: '', month: '1', year: '' } }
  };
  initLayout();
  S.tz = phoneTz();
  if (!saved('timezone') && S.tz && S.tz.value) { S.draft.timezone = S.tz.value; S.tzFilled = true; }
  render();
  if (!state.device.firstRun) loadPersonal();
}

function load() {
  S.screen = function () { return h('div', { class: 'full' }, h('p', { role: 'status' }, h('span', { class: 'spin', 'aria-hidden': 'true' }), 'Connecting to the device…')); };
  render();
  api('GET', '/api/state', undefined, { timeout: 12000 }).then(function (r) {
    if (r.login) return;
    if (!r.ok || !r.device || !r.settings) { S.screen = errorScreen(r.ok ? 'The device sent an answer this page doesn\'t understand. Update the firmware, or use /basic.' : r.error); render(); return; }
    start(r);
  });
}

window.addEventListener('beforeunload', function (e) {
  if (S.state && !S.screen && unsavedList().length) { e.preventDefault(); e.returnValue = ''; }
});

load();
})();
