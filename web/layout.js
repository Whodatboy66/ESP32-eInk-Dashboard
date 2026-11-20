// Layout editor logic (docs/plans/STAGE5_SETUP_APP.md, decision 6): structures, split, merge,
// fits and validate on the 4 x 3 grid. No DOM: used by app.js in the browser (global `Layout`)
// and by tests/web/layout.test.cjs in Node. Widget min sizes always come from the caller
// (/api/state `widgets`), never from a copy in here.
var Layout = (function () {
  'use strict';

  var MAX_BOXES = 12;

  // Starting points of the editor: [col, row, cols, rows, suggested widget].
  var GENERIC = [
    ['full', 'Full', [[0, 0, 4, 3, 'agenda']]],
    ['halves', 'Halves', [[0, 0, 2, 3, 'agenda'], [2, 0, 2, 3, 'month']]],
    ['banner', 'Top + main', [[0, 0, 4, 1, 'clock'], [0, 1, 4, 2, 'agenda']]],
    ['top2', 'Top + 2', [[0, 0, 4, 1, 'agenda'], [0, 1, 2, 2, 'clock'], [2, 1, 2, 2, 'month']]],
    ['four', 'Four', [[0, 0, 2, 1, 'clock'], [2, 0, 2, 1, 'weather'], [0, 1, 2, 2, 'agenda'], [2, 1, 2, 2, 'month']]],
    ['side', 'Main + side', [[0, 0, 2, 3, 'month'], [2, 0, 2, 1, 'clock'], [2, 1, 2, 1, 'weather'], [2, 2, 2, 1, 'agenda']]],
    ['six', 'Six', [[0, 0, 2, 1, 'clock'], [2, 0, 2, 1, 'weather'], [0, 1, 2, 1, 'agenda'],
      [2, 1, 2, 1, 'stocks'], [0, 2, 2, 1, 'tracking'], [2, 2, 2, 1, 'printer']]],
    ['wide3', 'Big + 2 small', [[0, 0, 3, 3, 'agenda'], [3, 0, 1, 2, 'weather'], [3, 2, 1, 1, 'notes']]]
  ].map(function (s) {
    return {
      key: s[0], name: s[1],
      boxes: s[2].map(function (b) { return { c: b[0], r: b[1], w: b[2], h: b[3], widget: b[4] }; })
    };
  });

  function gridOf(grid) {
    return { cols: (grid && grid.cols) || 4, rows: (grid && grid.rows) || 3 };
  }

  // Widget table (array of {key, minW, minH, ...} or a map) -> map by key.
  function table(widgets) {
    if (!widgets) return {};
    if (!Array.isArray(widgets)) return widgets;
    var m = {};
    widgets.forEach(function (w) { m[w.key] = w; });
    return m;
  }

  function copyBox(b) { return { c: b.c, r: b.r, w: b.w, h: b.h, widget: b.widget || null }; }
  function copyBoxes(boxes) { return (boxes || []).map(copyBox); }

  // Does this widget ({minW, minH}) fit into this box ({w, h})?
  function fits(widget, box) {
    if (!widget || !box) return false;
    return box.w >= (widget.minW || 1) && box.h >= (widget.minH || 1);
  }

  // Boxes of a structure with its suggested widgets; a widget that is unknown, doesn't fit or is
  // used twice leaves its box empty.
  function fromStructure(structure, widgets) {
    var t = table(widgets), used = {};
    return copyBoxes(structure.boxes).map(function (b) {
      var w = b.widget && t[b.widget];
      if (!w || used[b.widget] || !fits(w, b)) b.widget = null;
      else used[b.widget] = true;
      return b;
    });
  }

  // The editor's structure list: the generic ones, replaced or extended by the device's own
  // (same key = the device's version wins), Classic last. Classic has no boxes (`classic: true`).
  function structures(fromDevice) {
    var list = GENERIC.slice(), classic = { key: 'classic', name: 'Classic', boxes: [], classic: true };
    (fromDevice || []).forEach(function (s) {
      if (!s || !s.key) return;
      if (s.key === 'classic') { classic.name = s.name || classic.name; return; }
      var item = { key: s.key, name: s.name || s.key, boxes: copyBoxes(s.boxes) };
      for (var i = 0; i < list.length; i++) {
        if (list[i].key === s.key) { list[i] = item; return; }
      }
      list.push(item);
    });
    list.push(classic);
    return list;
  }

  function canSplit(box) { return !!box && (box.w > 1 || box.h > 1); }

  // Split boxes[index] in two (plan rule): cut across the side with more cells, equal sides cut
  // horizontally, the larger part first. The widget stays in the first part if it fits there,
  // moves to the second if it fits there, otherwise the box becomes empty. The new part follows
  // the old one in the list. Returns a new array, or null when the box can't be split (1 x 1).
  function split(boxes, index, widgets) {
    var b = boxes[index];
    if (!canSplit(b)) return null;
    var a = copyBox(b), n = copyBox(b), k;
    n.widget = null;
    if (b.w > b.h) {
      k = Math.ceil(b.w / 2);
      a.w = k; n.c = b.c + k; n.w = b.w - k;
    } else {
      k = Math.ceil(b.h / 2);
      a.h = k; n.r = b.r + k; n.h = b.h - k;
    }
    if (b.widget) {
      var w = table(widgets)[b.widget];
      if (!fits(w, a)) {
        a.widget = null;
        if (fits(w, n)) n.widget = b.widget;
      }
    }
    var out = copyBoxes(boxes);
    out.splice(index, 1, a, n);
    return out;
  }

  // Do boxes a and b share a full edge?
  function sharesEdge(a, b) {
    return (a.r === b.r && a.h === b.h && (b.c === a.c + a.w || b.c + b.w === a.c)) ||
           (a.c === b.c && a.w === b.w && (b.r === a.r + a.h || b.r + b.h === a.r));
  }

  // Indexes of the boxes boxes[index] can merge with, in the order right, below, left, above.
  function mergePartners(boxes, index) {
    var a = boxes[index], out = [];
    if (!a) return out;
    var dirs = [
      function (o) { return o.r === a.r && o.h === a.h && o.c === a.c + a.w; },
      function (o) { return o.c === a.c && o.w === a.w && o.r === a.r + a.h; },
      function (o) { return o.r === a.r && o.h === a.h && o.c + o.w === a.c; },
      function (o) { return o.c === a.c && o.w === a.w && o.r + o.h === a.r; }
    ];
    dirs.forEach(function (test) {
      for (var i = 0; i < boxes.length; i++) if (i !== index && test(boxes[i])) out.push(i);
    });
    return out;
  }

  // Where box j lies seen from box i: 'right', 'below', 'left' or 'above' (null if not touching).
  function direction(a, b) {
    if (!sharesEdge(a, b)) return null;
    if (b.c === a.c + a.w) return 'right';
    if (b.r === a.r + a.h) return 'below';
    if (b.c + b.w === a.c) return 'left';
    return 'above';
  }

  // The first box boxes[index] can merge with, -1 if none.
  function mergePartner(boxes, index) {
    var p = mergePartners(boxes, index);
    return p.length ? p[0] : -1;
  }

  // Merge boxes[index] (the selected one) with boxes[other]; the selected box's widget wins,
  // the other's when the selected one is empty. The merged box takes the lower index.
  // Returns a new array, or null when the two don't share a full edge.
  function merge(boxes, index, other) {
    var a = boxes[index], b = boxes[other];
    if (!a || !b || index === other || !sharesEdge(a, b)) return null;
    var c = Math.min(a.c, b.c), r = Math.min(a.r, b.r);
    var m = {
      c: c, r: r,
      w: Math.max(a.c + a.w, b.c + b.w) - c,
      h: Math.max(a.r + a.h, b.r + b.h) - r,
      widget: a.widget || b.widget || null
    };
    var out = copyBoxes(boxes);
    out.splice(Math.max(index, other), 1);
    out.splice(Math.min(index, other), 1, m);
    return out;
  }

  function isInt(v) { return typeof v === 'number' && isFinite(v) && Math.floor(v) === v; }

  // Check a box layout ({boxes: [...]}) with the firmware's rules. Returns {ok, errors}; each
  // error is {code, text, box, p} (p: values for a translated text) with code one of: count, outside, overlap, gap, unknown,
  // duplicate, size. `box` is the 0-based box index (-1 for the whole layout).
  function validate(layout, widgets, grid) {
    var g = gridOf(grid), t = table(widgets), errors = [];
    var boxes = (layout && layout.boxes) || [];
    var cells = [], used = {}, i, x, y;
    function err(code, text, box, p) { errors.push({ code: code, text: text, box: box, p: p || {} }); }

    if (boxes.length > MAX_BOXES) err('count', 'At most ' + MAX_BOXES + ' boxes fit on the screen; merge some.', -1, { max: MAX_BOXES });
    for (i = 0; i < g.cols * g.rows; i++) cells.push(-1);

    boxes.forEach(function (b, n) {
      var name = 'Box ' + (n + 1);
      if (!b || !isInt(b.c) || !isInt(b.r) || !isInt(b.w) || !isInt(b.h) ||
          b.w < 1 || b.h < 1 || b.c < 0 || b.r < 0 || b.c + b.w > g.cols || b.r + b.h > g.rows) {
        err('outside', name + ' is outside the ' + g.cols + ' × ' + g.rows + ' grid.', n, { n: n + 1, cols: g.cols, rows: g.rows });
        return;
      }
      var overlap = -1;
      for (y = b.r; y < b.r + b.h; y++) {
        for (x = b.c; x < b.c + b.w; x++) {
          var k = y * g.cols + x;
          if (cells[k] >= 0 && overlap < 0) overlap = cells[k];
          else if (cells[k] < 0) cells[k] = n;
        }
      }
      if (overlap >= 0) err('overlap', name + ' overlaps box ' + (overlap + 1) + '.', n, { n: n + 1, other: overlap + 1 });

      var key = b.widget;
      if (key === null || key === undefined || key === '') return;
      var w = t[key];
      if (!w) { err('unknown', name + ' shows an unknown widget ("' + key + '").', n, { n: n + 1, key: key }); return; }
      if (used[key] !== undefined) {
        err('duplicate', (w.name || key) + ' is in box ' + (used[key] + 1) + ' and box ' + (n + 1) + '; a widget can be shown once.', n,
          { name: w.name || key, other: used[key] + 1, n: n + 1 });
      } else {
        used[key] = n;
      }
      if (!fits(w, b)) {
        err('size', (w.name || key) + ' needs ' + (w.minW || 1) + ' × ' + (w.minH || 1) + ' cells; box ' + (n + 1) + ' is ' + b.w + ' × ' + b.h + '.', n,
          { name: w.name || key, minW: w.minW || 1, minH: w.minH || 1, n: n + 1, w: b.w, h: b.h });
      }
    });

    var free = 0;
    for (i = 0; i < cells.length; i++) if (cells[i] < 0) free++;
    if (free) err('gap', free + (free === 1 ? ' cell isn\'t' : ' cells aren\'t') + ' covered by a box.', -1, { free: free });
    return { ok: errors.length === 0, errors: errors };
  }

  // Box layouts are equal (same boxes in the same order, same widgets).
  function sameBoxes(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) {
      var x = a[i], y = b[i];
      if (x.c !== y.c || x.r !== y.r || x.w !== y.w || x.h !== y.h || (x.widget || null) !== (y.widget || null)) return false;
    }
    return true;
  }

  return {
    MAX_BOXES: MAX_BOXES,
    GENERIC: GENERIC,
    table: table,
    copyBoxes: copyBoxes,
    fits: fits,
    fromStructure: fromStructure,
    structures: structures,
    canSplit: canSplit,
    split: split,
    sharesEdge: sharesEdge,
    direction: direction,
    mergePartners: mergePartners,
    mergePartner: mergePartner,
    merge: merge,
    validate: validate,
    sameBoxes: sameBoxes
  };
})();

if (typeof module !== 'undefined') module.exports = Layout;
