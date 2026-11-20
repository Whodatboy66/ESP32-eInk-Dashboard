// Fake device for developing the setup app without hardware (web/dev/index.html). Replaces
// window.fetch and answers every /api/ route of docs/SETUP_API.md with sample data. Not part of
// the bundle. Address options: ?first=1 ?home=1 ?offline=1 ?slow=1 ?down=1 ?classic=1 ?mem=1 ?lowbat=1
(function () {
  'use strict';
  function opt(k) { return new RegExp('[?&]' + k + '=1').test(location.search); }
  var FIRST = opt('first'), HOME = opt('home'), OFFLINE = opt('offline'), SLOW = opt('slow'), DOWN = opt('down');
  var CLASSIC = opt('classic'), MEM = opt('mem'), LOWBAT = opt('lowbat');
  var loggedIn = !HOME, wrongCodes = 0, code = '123456', downUntil = 0;

  var settings = {
    device_name: FIRST ? 'Dashboard' : 'Kitchen display',
    wifi_ssid: FIRST ? '' : 'Home', wifi_pass: FIRST ? null : 'secret',
    timezone: FIRST ? '' : 'CET-1CEST,M3.5.0,M10.5.0/3',
    weather_loc: FIRST ? '' : 'Berlin, DE',
    stocks: FIRST ? '' : 'AAPL,NVDA,^IXIC',
    ics: FIRST ? [null, null, null] : ['https://calendar.google.com/calendar/ical/x/basic.ics', null, null],
    bday_url: FIRST ? null : 'https://calendar.google.com/calendar/ical/birthdays/basic.ics', pkge_key: null, adb_key: null,
    mqtt_ip: FIRST ? '' : '192.168.1.50', mqtt_sn: FIRST ? '' : '01S00A123456789', mqtt_pass: FIRST ? null : '12345678',
    mqtt_port: 8883, printer_name: FIRST ? '' : 'X1C', makerworld_uid: '',
    ota_auto: true, ota_beta: false, clock_24h: true,
    pin_preset: 'esp32c6_default',
    pins: { epd_cs: 1, epd_dc: 8, epd_rst: 14, epd_busy: 7, epd_sck: 23, epd_mosi: 22, display_power: 4, battery: 0, demo_button: 2 }
  };
  var SECRETS = ['wifi_pass', 'bday_url', 'pkge_key', 'adb_key', 'mqtt_pass'];
  var layout = CLASSIC || FIRST ?
    { template: 'classic', statusBar: true, boxes: [], show: ['weather', 'agenda', 'month', 'stocks'] } :
    { template: 'planner', statusBar: true, show: ['weather', 'agenda', 'month'],
      boxes: [{ c: 0, r: 0, w: 2, h: 1, widget: 'clock' }, { c: 2, r: 0, w: 2, h: 1, widget: 'weather' },
        { c: 0, r: 1, w: 2, h: 2, widget: 'agenda' }, { c: 2, r: 1, w: 2, h: 2, widget: 'month' }] };
  var tasks = [{ title: 'Buy milk', due: '2026-11-20', prio: 2, done: false }, { title: 'Call the plumber', due: '', prio: 1, done: false },
    { title: 'Return library books', due: '2026-11-10', prio: 3, done: true }];
  var note = 'Wi-Fi for guests: Sunflower-42\nBins out on Thursday';
  var manual = [{ name: 'Anna', day: 17, month: 11, year: 1990 }, { name: 'Grandpa Joe', day: 22, month: 11, year: 0 }];

  function secretOut(v, url) {
    if (!v) return { set: false };
    var o = { set: true };
    if (url) { var m = /^\w+:\/\/([^/]+)/.exec(v); if (m) o.hint = m[1]; }
    return o;
  }
  function configured(key) {
    switch (key) {
      case 'weather': return !!settings.weather_loc;
      case 'agenda': case 'month': return settings.ics.some(Boolean);
      case 'stocks': return !!settings.stocks;
      case 'tracking': return !!settings.pkge_key;
      case 'printer': return !!settings.mqtt_ip;
      case 'tasks': return tasks.some(function (t) { return !t.done; });
      case 'notes': return !!note;
      case 'birthdays': return manual.length > 0 || !!settings.bday_url;
    }
    return true;
  }
  var HINTS = { weather: 'Set a place on the Data tab', agenda: 'Add a calendar link on the Data tab', month: 'Add a calendar link on the Data tab',
    stocks: 'Add stock symbols on the Data tab', tracking: 'Add the parcels key on the Data tab', printer: 'Add the printer on the Data tab',
    tasks: 'Add tasks on the Data tab', notes: 'Write a note on the Data tab', birthdays: 'Add birthdays on the Data tab' };
  var WIDGETS = [['clock', 'Clock', 2, 1], ['weather', 'Weather', 1, 1], ['agenda', 'Agenda', 2, 1], ['month', 'Month calendar', 2, 2],
    ['stocks', 'Stocks', 2, 1], ['tracking', 'Parcels', 2, 1], ['printer', '3D printer', 2, 1], ['tasks', 'Tasks', 2, 1],
    ['notes', 'Note', 1, 1], ['birthdays', 'Birthdays', 2, 1]];

  function upcoming() {
    var now = new Date(); now.setHours(0, 0, 0, 0);
    return manual.map(function (b) {
      var d = new Date(now.getFullYear(), b.month - 1, b.day);
      if (d < now) d.setFullYear(d.getFullYear() + 1);
      var days = Math.round((d - now) / 864e5);
      return { name: b.name, date: d.toISOString().slice(0, 10), days: days, age: b.year ? d.getFullYear() - b.year : 0, fromFeed: false };
    }).concat([{ name: 'Mia', date: '2026-11-29', days: 11, age: 8, fromFeed: true }]).filter(function (b) { return b.days <= 60; })
      .sort(function (a, b) { return a.days - b.days; });
  }

  function state() {
    var s = {};
    Object.keys(settings).forEach(function (k) {
      if (SECRETS.indexOf(k) >= 0) s[k] = secretOut(settings[k], k === 'bday_url');
      else if (k === 'ics') s.ics = settings.ics.map(function (v) { return secretOut(v, true); });
      else s[k] = JSON.parse(JSON.stringify(settings[k]));
    });
    return {
      device: {
        name: settings.device_name, version: '.71', build: 71, variant: 'esp32c6_firebeetle', mode: HOME ? 'home' : 'ap',
        firstRun: !settings.wifi_ssid, battery: LOWBAT ? -1 : 68, batteryAt: LOWBAT ? null : Math.floor(Date.now() / 1000) - 25 * 60, usb: false,
        wifi: { ssid: settings.wifi_ssid, connected: HOME, rssi: HOME ? -58 : 0 },
        time: HOME ? Math.floor(Date.now() / 1000) : 0,
        problems: settings.ics[1] && /bad/.test(settings.ics[1]) || !FIRST ?
          [{ key: 'calendar', text: 'A calendar can\'t be read', fix: 'Check the calendar links on the Data tab.' }] : [],
        lastUpdate: 'Updated to .70', updateAvailable: FIRST ? null : '.72', layoutFallback: null
      },
      settings: s,
      pinPresets: [{ key: 'esp32c6_default', name: 'ESP32-C6 FireBeetle' }, { key: 'custom', name: 'Custom pins' }],
      layout: JSON.parse(JSON.stringify(layout)),
      widgets: WIDGETS.map(function (w) {
        var c = configured(w[0]);
        return { key: w[0], name: w[1], minW: w[2], minH: w[3], configured: c, hint: c ? '' : HINTS[w[0]] };
      }),
      structures: [
        { key: 'planner', name: 'Planner', boxes: [{ c: 0, r: 0, w: 2, h: 1, widget: 'clock' }, { c: 2, r: 0, w: 2, h: 1, widget: 'weather' },
          { c: 0, r: 1, w: 2, h: 2, widget: 'agenda' }, { c: 2, r: 1, w: 2, h: 2, widget: 'month' }] },
        { key: 'minimal', name: 'Minimal', boxes: [{ c: 0, r: 0, w: 4, h: 2, widget: 'clock' }, { c: 0, r: 2, w: 2, h: 1, widget: 'weather' },
          { c: 2, r: 2, w: 2, h: 1, widget: 'agenda' }] },
        { key: 'technical', name: 'Technical', boxes: [{ c: 0, r: 0, w: 2, h: 1, widget: 'clock' }, { c: 2, r: 0, w: 2, h: 1, widget: 'printer' },
          { c: 0, r: 1, w: 2, h: 1, widget: 'stocks' }, { c: 2, r: 1, w: 2, h: 1, widget: 'tracking' }, { c: 0, r: 2, w: 2, h: 1, widget: 'weather' },
          { c: 2, r: 2, w: 2, h: 1, widget: 'agenda' }] },
        { key: 'personal', name: 'Personal', boxes: [{ c: 0, r: 0, w: 2, h: 1, widget: 'clock' }, { c: 2, r: 0, w: 2, h: 1, widget: 'notes' },
          { c: 0, r: 1, w: 2, h: 2, widget: 'tasks' }, { c: 2, r: 1, w: 2, h: 1, widget: 'birthdays' }, { c: 2, r: 2, w: 2, h: 1, widget: 'weather' }] }
      ],
      grid: { cols: 4, rows: 3, cellW: 200, cellH: 150, statusH: 28 }
    };
  }

  // 800 x 480, 1 bit per pixel BMP with the box outlines of a layout body.
  function bmp(body) {
    var W = 800, H = 480, row = 100, size = 62 + row * H;
    var buf = new ArrayBuffer(size), v = new DataView(buf), px = new Uint8Array(buf, 62);
    v.setUint8(0, 0x42); v.setUint8(1, 0x4d); v.setUint32(2, size, true); v.setUint32(10, 62, true);
    v.setUint32(14, 40, true); v.setInt32(18, W, true); v.setInt32(22, -H, true); v.setUint16(26, 1, true); v.setUint16(28, 1, true);
    v.setUint32(34, row * H, true); v.setUint32(46, 2, true);
    v.setUint32(54, 0x00000000, true); v.setUint32(58, 0x00ffffff, true);
    px.fill(0xff);
    function dot(x, y) { if (x >= 0 && x < W && y >= 0 && y < H) px[y * row + (x >> 3)] &= ~(0x80 >> (x & 7)); }
    function rect(x, y, w, h, t) {
      for (var i = 0; i < t; i++) {
        for (var a = x; a < x + w; a++) { dot(a, y + i); dot(a, y + h - 1 - i); }
        for (var b = y; b < y + h; b++) { dot(x + i, b); dot(x + w - 1 - i, b); }
      }
    }
    if (body.template === 'classic') {
      [[10, 0, 430, 155], [440, 25, 360, 230], [440, 255, 360, 145], [10, 155, 430, 130], [10, 285, 430, 105], [10, 390, 430, 90], [440, 400, 360, 80]]
        .forEach(function (r) { rect(r[0], r[1], r[2], r[3], 2); });
    } else {
      var top = body.statusBar ? 28 : 0, ch = body.statusBar ? 150 : 160;
      if (body.statusBar) rect(0, 26, 800, 2, 2);
      (body.boxes || []).forEach(function (b) {
        var y = top + b.r * ch, hh = b.r + b.h >= 3 ? 480 - y : b.h * ch;
        rect(b.c * 200 + 6, y + 6, b.w * 200 - 12, hh - 12, b.widget ? 4 : 1);
      });
    }
    return new Blob([buf], { type: 'image/bmp' });
  }

  function validTz(t) { return !t || /^(Europe|America)\/\w+$|^UTC$|^CET$/.test(t) || /^<?[A-Za-z+\-0-9]{3,}>?[+-]?\d/.test(t); }
  function applySettings(b) {
    var errors = {};
    if ('timezone' in b && !validTz(b.timezone)) errors.timezone = 'Unknown time zone "' + b.timezone + '". Use a name like Europe/Berlin or a rule like CET-1CEST,M3.5.0,M10.5.0/3.';
    if ('mqtt_port' in b && !(b.mqtt_port >= 1 && b.mqtt_port <= 65535)) errors.mqtt_port = 'The port is a number from 1 to 65535 (usually 8883).';
    if ('device_name' in b && !/^[\w \-äöüÄÖÜß]{1,32}$/.test(b.device_name)) errors.device_name = 'Use letters, digits, spaces and hyphens (up to 32).';
    if ('stocks' in b && String(b.stocks).split(',').filter(Boolean).length > 5) errors.stocks = 'Up to 5 symbols.';
    if (b.ics) b.ics.forEach(function (v, i) {
      if (typeof v === 'string' && v && !/^(https?|webcals?):\/\//i.test(v.trim())) errors['ics.' + i] = 'A calendar link starts with https:// or webcal://.';
    });
    if (Object.keys(errors).length) return { status: 400, body: { ok: false, error: 'Some fields need a fix.', errors: errors } };
    // stay: back in the same mode after the restart; finish (or the end of the first start): dashboard.
    var stay = typeof b.stay === 'boolean' ? b.stay : b.finish ? false : !!settings.wifi_ssid;
    delete b.stay;
    delete b.finish;
    Object.keys(b).forEach(function (k) {
      if (k === 'ics') b.ics.forEach(function (v, i) { if (typeof v === 'string') settings.ics[i] = v; else if (v === null) settings.ics[i] = null; });
      else settings[k] = b[k];
    });
    downUntil = Date.now() + (stay ? 4000 : 600000);
    return { status: 200, body: { ok: true, restart: true, stay: stay } };
  }


  // Diagnostics (build 72): a few wakes with a Wi-Fi retry and a failed calendar fetch; battery
  // 1.1-1.4 V for 2 days (no battery, board on a charger), then ~3.9 V rising to 4.15 V on a computer's USB.
  var NOW = Math.floor(Date.now() / 1000);
  function hm(t) { var d = new Date(t * 1000); return ('0' + d.getHours()).slice(-2) + ':' + ('0' + d.getMinutes()).slice(-2) + ':' + ('0' + d.getSeconds()).slice(-2); }
  function day(t) { var d = new Date(t * 1000); return d.getFullYear() + '-' + ('0' + (d.getMonth() + 1)).slice(-2) + '-' + ('0' + d.getDate()).slice(-2); }
  function makeLog() {
    var out = [], lastDay = '';
    function at(t, text) { var dd = day(t); if (dd !== lastDay) { out.push('--- ' + dd + ' ---'); lastDay = dd; } out.push(hm(t) + ' ' + text); }
    var wakes = [NOW - 26 * 3600, NOW - 14 * 3600, NOW - 3 * 3600, NOW - 40 * 60, NOW - 12 * 60];
    wakes.forEach(function (t, i) {
      at(t, 'Wake ' + (405 + i) + ': full refresh due');
      at(t + 1, 'WiFi connecting to Home...');
      if (i === 2) {
        at(t + 9, 'WiFi failed (no answer from the router), retry in 5 min');
        at(t + 10, 'Sleep 290 s');
        t += 300;
        at(t, 'Wake ' + (405 + i) + ': WiFi retry');
        at(t + 1, 'WiFi connecting to Home...');
      }
      at(t + 3, 'WiFi connected, RSSI -6' + (i % 4));
      at(t + 4, '[NTP] time set, drift 0.4 s');
      at(t + 5, '[Weather] Berlin, DE: 7 days, 9° / 2°');
      at(t + 6, '[Stocks] AAPL 231.50, NVDA 141.20, ^IXIC 18920.1');
      at(t + 8, '[ICS] feed 1: 12 upcoming, 9 days marked');
      if (i >= 3) { at(t + 9, '[ICS] feed 2: HTTP 404 from calendar.google.com, keeping the last list'); if (i === 3) at(t + 9, 'Problem on: calendar'); }
      at(t + 11, 'Display: full refresh (3.1 s)');
      at(t + 11, 'Battery ' + (i < 2 ? '1.21 V (not plausible)' : '4.1' + i + ' V') + ', USB ' + (i < 2 ? 'no' : 'computer'));
      at(t + 12, 'Sleep ' + (48 - i) + ' s');
    });
    out.push('+0.412 s Boot 412, reset reason: button (setup hold)');
    out.push('+0.950 s Setup mode: hotspot Dashboard-Setup');
    at(NOW - 60, '[Setup] app opened, WEB_APP 29 KB');
    return out;
  }
  var diagLog = makeLog();
  function history() {
    var out = [], start = NOW - 80 * 3600;
    for (var t = start, i = 0; t <= NOW; t += 3600, i++) {
      if (i === 30 || i === 31) continue;                               // two missing hours
      if (i < 48) out.push([t, Math.round(1250 + 140 * Math.sin(i / 3) + (i * 7 % 30)), 0]);
      else out.push([t, Math.round(3890 + Math.min(260, (i - 48) * 9) + (i * 5 % 12)), 1]);
    }
    return out;
  }
  function diagnostics() {
    var hist = history();
    return {
      time: NOW, version: '.72', bootCount: 412, log: diagLog.slice(-200),
      battery: LOWBAT ? { sensor: true, mv: 1180, plausible: false, percent: -1, usb: false, minMv: 3000, history: hist } :
        { sensor: true, mv: 3850, plausible: true, percent: 60, usb: false, minMv: 3000, history: hist,
          quietMv: 3910, quietPercent: 68, quietAt: NOW - 25 * 60 },
      services: [
        { key: 'weather', name: 'Weather', lastOk: NOW - 12 * 60, failures: 0, retryAt: 0, stale: false },
        { key: 'stocks', name: 'Stocks', lastOk: NOW - 12 * 60, failures: 0, retryAt: 0, stale: false },
        { key: 'calendar', name: 'Calendar', lastOk: NOW - 3 * 3600, failures: 2, retryAt: NOW + 4 * 60, stale: true },
        { key: 'tracking', name: 'Parcels', lastOk: 0, failures: 0, retryAt: 0, stale: false }
      ],
      problems: ['calendar'],
      wifi: { failures: 0, retryAfter: 0, rssi: -61 },
      lastTimeSync: NOW - 12 * 60,
      heap: { free: 182340, minFree: 141200, largest: 110592 }
    };
  }

  function json(status, body) {
    return new Response(JSON.stringify(body), { status: status, headers: { 'Content-Type': 'application/json' } });
  }

  function route(method, path, body) {
    var url = new URL(path, location.href), p = url.pathname.replace(/^.*\/api\//, '/api/');
    if (HOME && !loggedIn && p !== '/api/login') return json(401, { ok: false, login: true });
    switch (method + ' ' + p) {
      case 'POST /api/login':
        if (body.code === code) { loggedIn = true; return json(200, { ok: true }); }
        wrongCodes++;
        if (wrongCodes >= 5) { wrongCodes = 0; code = '654321'; console.log('[mock] new code on the display: 654321'); }
        return json(400, { ok: false, error: 'Wrong code' });
      case 'GET /api/state': return json(200, state());
      case 'GET /api/networks':
        return json(200, { networks: [{ ssid: 'Home', rssi: -52, secure: true }, { ssid: 'Home-Guest', rssi: -61, secure: false },
          { ssid: 'Neighbour 5G', rssi: -77, secure: true }, { ssid: 'FRITZ!Box 7590 XY', rssi: -84, secure: true }] });
      case 'POST /api/wifi-test':
        if (body.ssid === 'Nowhere') return json(200, { ok: false, error: 'Network not found' });
        if (body.pass === 'wrong' || (body.pass === undefined && body.ssid !== settings.wifi_ssid)) return json(200, { ok: false, error: 'Wrong password' });
        OFFLINE = false;
        return json(200, { ok: true });
      case 'POST /api/check-calendar':
        if (OFFLINE) return json(200, { ok: false, offline: true });
        if (body.url && /bad/.test(body.url)) return json(200, { ok: false, error: 'The link answered with an error (404). Copy it again from your calendar.' });
        if (body.bday) return settings.bday_url ? json(200, { ok: true, events: 23 }) : json(200, { ok: false, error: 'No birthday link is saved.' });
        return json(200, { ok: true, events: body.index !== undefined ? 12 : 7 });
      case 'GET /api/places': {
        if (OFFLINE) return json(200, { ok: false, offline: true });
        var q = (url.searchParams.get('q') || '').toLowerCase();
        var all = [['Berlin, DE', 'Berlin, Germany'], ['Berlingen, CH', 'Thurgau, Switzerland'], ['Bern, CH', 'Bern, Switzerland'],
          ['London, GB', 'England, United Kingdom'], ['Londonderry, GB', 'Northern Ireland, United Kingdom'], ['Munich, DE', 'Bavaria, Germany'],
          ['New York, US', 'New York, United States']];
        return json(200, { places: all.filter(function (x) { return x[0].toLowerCase().indexOf(q) === 0; }).slice(0, 5)
          .map(function (x) { return { name: x[0], detail: x[1] }; }) });
      }
      case 'POST /api/settings': { var r = applySettings(body); return json(r.status, r.body); }
      case 'POST /api/layout': {
        var v = Layout.validate(body.template === 'classic' ? { boxes: [{ c: 0, r: 0, w: 4, h: 3 }] } : body, state().widgets);
        if (!v.ok) return json(400, { ok: false, error: v.errors[0].text });
        layout = { template: body.template, statusBar: body.statusBar !== false, boxes: body.boxes || [], show: body.show || layout.show };
        return json(200, { ok: true });
      }
      case 'POST /api/preview.bmp':
        if (MEM) return json(503, { ok: false, error: 'Not enough memory for the preview right now.' });
        return new Response(bmp(body), { status: 200, headers: { 'Content-Type': 'image/bmp' } });
      case 'GET /api/tasks': return json(200, { tasks: tasks });
      case 'PUT /api/tasks':
        if ((body.tasks || []).length > 30) return json(400, { ok: false, error: 'At most 30 tasks.' });
        tasks = body.tasks;
        return json(200, tasks.some(function (t) { return /✓/.test(t.title); }) ? { ok: true, removed: '✓' } : { ok: true });
      case 'GET /api/notes': return json(200, { note: note });
      case 'PUT /api/notes':
        if (String(body.note).length > 400) return json(400, { ok: false, error: 'The note is longer than 400 characters.' });
        note = body.note;
        return json(200, { ok: true });
      case 'GET /api/birthdays': return json(200, { manual: manual, upcoming: upcoming() });
      case 'PUT /api/birthdays':
        if ((body.manual || []).length > 50) return json(400, { ok: false, error: 'At most 50 birthdays.' });
        manual = body.manual;
        return json(200, { ok: true });
      case 'GET /api/diagnostics': return json(200, diagnostics());
      case 'GET /api/log.txt':
        return new Response(diagLog.join('\n') + '\n', { status: 200, headers: { 'Content-Type': 'text/plain; charset=utf-8', 'Content-Disposition': 'attachment; filename="log.txt"' } });
      case 'POST /api/log/clear': diagLog = []; return json(200, { ok: true });
      case 'POST /api/update': case 'POST /api/finish': case 'POST /api/factory-reset':
        downUntil = Date.now() + 600000;
        return json(200, { ok: true });
    }
    return json(404, { ok: false, error: 'No such route: ' + method + ' ' + p });
  }

  var realFetch = window.fetch;
  window.fetch = function (input, init) {
    init = init || {};
    var path = typeof input === 'string' ? input : input.url;
    if (path.indexOf('/api/') < 0) return realFetch.apply(this, arguments);
    var method = (init.method || 'GET').toUpperCase(), body = {};
    if (init.body) {
      if (!/application\/json/.test((init.headers || {})['Content-Type'] || '')) console.warn('[mock] POST without JSON content type', path);
      body = JSON.parse(init.body);
    }
    var wait = /networks|wifi-test|check-calendar|preview/.test(path) ? 1500 : 250;
    if (SLOW) wait *= 8;
    console.log('[mock]', method, path, init.body ? body : '');
    return new Promise(function (resolve, reject) {
      if (init.signal) init.signal.addEventListener('abort', function () { reject(new DOMException('Aborted', 'AbortError')); });
      setTimeout(function () {
        if (DOWN || Date.now() < downUntil) { reject(new TypeError('Failed to fetch')); return; }
        try { resolve(route(method, path, body)); } catch (e) { console.error(e); resolve(json(500, { ok: false })); }
      }, wait);
    });
  };
})();
