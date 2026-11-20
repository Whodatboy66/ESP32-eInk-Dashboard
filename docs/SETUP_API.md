# Setup app API

The JSON interface between the setup app (`web/`, served at `/`) and the firmware
(`src/web/`). Plans: [STAGE4_PERSONAL_WIDGETS.md](plans/STAGE4_PERSONAL_WIDGETS.md),
[STAGE5_SETUP_APP.md](plans/STAGE5_SETUP_APP.md).

## Basics

- All bodies are JSON (`Content-Type: application/json`, required on every POST / PUT, also the ones
  without a body (`{}`): a page from another site can't send that without a preflight the device
  never answers). UTF-8.
- In the hotspot, the API answers only requests that come in over the hotspot for `192.168.4.1`
  (403 otherwise: after a Wi-Fi test the device is also in the home network).
- Errors: HTTP 400 with `{"ok": false, "error": "text for people", "errors": {"field": "text"}}`
  (`errors` only for field problems). HTTP 401 `{"ok": false, "login": true}` when a session is
  needed (home network only). HTTP 413 when a body is larger than 8 KB.
- **Secrets are never sent back.** In `GET` answers a secret is `{"set": true}` or `{"set": false}`
  (calendar links also get `"hint": "calendar.google.com"`, the host only). In a `POST`, a secret
  field: a non-empty string = new value, `null` = remove, missing or `""` = keep (like the old page:
  an empty password field keeps the saved one). Calendar links are not secrets in this sense: `""`
  removes a link.
- Modes: `"ap"` (setup hotspot, the phone has no internet) and `"home"` (edit mode on the home
  network, needs the screen code). Everything works in both modes unless noted.

## Session (home network only)

`POST /api/login` `{"code": "123456"}` → `{"ok": true}` and a cookie `s=<token>` (HttpOnly,
SameSite=Strict), valid until edit mode ends. Wrong code: 400 `{"ok": false, "error": "Wrong code"}`;
after 5 wrong codes a new code is shown on the screen, after 3 new codes edit mode ends. A wrong code
answers after a second. While the dashboard covers the code (after saving a layout or personal data),
a login attempt shows the code screen again. In AP mode no login is needed (the hotspot has
its own password).

## State

`GET /api/state`

```json
{
  "device": {
    "name": "Kitchen display", "version": ".71", "build": 71, "variant": "esp32c6_firebeetle",
    "mode": "ap", "firstRun": false, "battery": 82, "usb": false,
    "wifi": {"ssid": "Home", "connected": false, "rssi": 0},
    "time": 1794904880,
    "problems": [{"key": "calendar", "text": "A calendar can't be read", "fix": "Check the calendar links on the Data tab."}],
    "lastUpdate": "Updated to .70", "updateAvailable": ".72",
    "layoutFallback": null
  },
  "settings": {
    "device_name": "Kitchen display",
    "wifi_ssid": "Home", "wifi_pass": {"set": true},
    "timezone": "CET-1CEST,M3.5.0,M10.5.0/3",
    "weather_loc": "Berlin, DE",
    "stocks": "EUNL.DE:MSCI World,PPFB.DE:Gold,^IXIC",
    "ics": [{"set": true, "hint": "calendar.google.com"}, {"set": false}, {"set": false}],
    "bday_url": {"set": false},
    "pkge_key": {"set": false}, "adb_key": {"set": false},
    "mqtt_ip": "192.168.1.50", "mqtt_sn": "01S00A123456789", "mqtt_pass": {"set": true},
    "mqtt_port": 8883, "printer_name": "X1C", "makerworld_uid": "123",
    "ota_auto": true, "ota_beta": false, "clock_24h": true, "lang": "de",
    "pin_preset": "esp32_c6",
    "pins": {"epd_cs": 1, "epd_dc": 8, "epd_rst": 14, "epd_busy": 7, "epd_sck": 23, "epd_mosi": 22,
             "display_power": 4, "battery": 0, "demo_button": 2}
  },
  "pinPresets": [{"key": "esp32_c6", "name": "ESP32 C6"}, {"key": "custom", "name": "Custom"}],
  "layout": {
    "template": "custom", "statusBar": true,
    "boxes": [{"c": 0, "r": 0, "w": 4, "h": 1, "widget": "agenda"}, {"c": 0, "r": 1, "w": 2, "h": 2, "widget": null}],
    "show": ["weather", "agenda", "month"]
  },
  "widgets": [
    {"key": "clock", "name": "Clock", "minW": 2, "minH": 1, "configured": true, "hint": ""},
    {"key": "weather", "name": "Weather", "minW": 1, "minH": 1, "configured": false, "hint": "Weather: set a place in the app"}
  ],
  "structures": [{"key": "planner", "name": "Planner", "boxes": [{"c": 0, "r": 0, "w": 2, "h": 1, "widget": "clock"}]}],
  "grid": {"cols": 4, "rows": 3, "cellW": 200, "cellH": 150, "statusH": 28}
}
```

- `device.mode`: `"ap"` or `"home"`. `firstRun`: no Wi-Fi saved yet (the app shows the guided setup).
- `battery`: percent, `-1` without a sensor (or no battery measured). From build 74 it's the last
  reading taken with Wi-Fi off (`batteryAt`: its Unix time; `null` = measured now): with Wi-Fi on in
  setup / edit mode the voltage sags by a few hundredths of a volt, up to ~10 % on the flat part of
  the LiPo curve. `usb`: always `false` for now (no USB detection in setup mode yet; planned with
  stage 6). `time`: Unix time, `0` when the clock isn't set.
- `layout.template`: `"custom"` (boxes) or a built-in key (`"classic"`, `"planner"`, `"minimal"`,
  `"technical"`, `"personal"`). For a built-in grid template, `boxes` is its current result (after gap
  filling), so the editor can start from it. `show` is the list of widget keys switched on (used by
  Classic). `widget: null` = empty box.
- `widgets`: every widget except the status bar, with its minimum size in cells and whether its
  settings are filled in (`hint` says what's missing).
- `structures`: starting points for the editor (built-in templates as boxes, plus Full, Halves, …).
- `pinPresets`: board presets for the Device tab (advanced).
- **Language (build 78):** setting `lang`: `"de"` (default, also for devices updated from older
  builds) or `"en"`. Every text the device sends to the app (problems with their fix, field errors,
  hints, check results, `power.note`, `estimateNote`) comes in that language; the app shows its own
  texts in the same language. Changing it is a normal settings save (the device restarts and draws
  the screen in the new language); another value than `"de"` / `"en"` is a field error under `lang`.
  The answer of that save is still in the old language. `lastUpdate` stays in the language it was
  written in. Place names from `/api/places` come in that language too ("Berlin, Deutschland").
- **Power (build 75, stage 6):** settings `clock_interval` (minutes: 1, 2, 5, 10, 15 or 30),
  `quiet_enabled` (bool), `quiet_from` / `quiet_to` ("HH:MM", default "23:00" / "06:00"; equal = no
  window), `battery_mah` (100-20000, default 3000, only for the estimate). `power` in the state:

  ```json
  "power": {"note": "Clock every 5 min", "saver": false, "night": false, "stepMin": 5,
            "estimateNote": "Estimate from the first measurements (2026-11-20); real current measurements follow.",
            "estimates": [{"interval": 1, "days": 28, "gainPercent": 0}, {"interval": 5, "days": 77, "gainPercent": 175}],
            "daysWithoutQuietHours": 56, "daysWithQuietHours": 77}
  ```

  `note`: what the status bar says now ("" = normal, "Night until 06:00", "Battery saver: every 15
  min", "Clock every 5 min"). `estimates`: days per charge for each clock choice with the current
  quiet hours and capacity, `gainPercent` against every minute. `daysWithoutQuietHours` /
  `daysWithQuietHours`: the chosen interval without / with the quiet window (to show what it saves).

## Settings

`POST /api/settings`: only the changed fields, names as in `settings` above (`stocks`: up to 5
entries, comma-separated, each `SYMBOL` or `SYMBOL:Name` with a name of up to 12 characters, build 78); `ics` is an array of up
to three entries: a string = new link, `null` = remove, anything else (`{}`) = keep, e.g.
`[{}, "https://…"]` changes only the second link. `pins` is sent whole when a pin changed (`-1` =
not used). Field errors use the keys of the request (`ics.1`, `pins`, `lang`). Validation as on the old page (timezone
names, calendar links, stock symbols, printer address, port, custom pins).
→ `{"ok": true, "restart": true, "stay": true}`; the device restarts about 3 s later (time for a
following `/api/layout` in the first-start guide). With `stay` it comes back in the **same mode**
(the hotspot with the same password, or edit mode with the same code and session), so the app
reconnects; without it (end of the first start) it shows the dashboard. The request may say
`"finish": true` or `"stay": true/false`; otherwise: first start (no Wi-Fi saved before) = finish,
else stay (from build 73; before, every save ended setup mode). (the app shows a "Saved,
restarting" screen). In AP mode the phone loses the hotspot while the device restarts.

`GET /api/networks` → `{"networks": [{"ssid": "Home", "rssi": -52, "secure": true}]}` (sorted by
signal, duplicates removed; takes 2-4 s).

`POST /api/wifi-test` `{"ssid": "Home", "pass": "..."}` (no `pass` = the saved password of that
network, `""` = an open network) → `{"ok": true}` or `{"ok": false, "error":
"Wrong password"}` (reasons: not found, wrong password, no answer). Doesn't save anything. Keeps the
connection for the checks below until setup mode ends (AP + station).

`POST /api/check-calendar` `{"url": "https://..."}` (or `{"index": 0}` for a saved agenda link,
`{"bday": true}` for the saved birthday link) →
`{"ok": true, "events": 12}` or `{"ok": false, "error": "..."}`; `{"ok": false, "offline": true}` when
the device has no internet connection (no saved or tested Wi-Fi).

`GET /api/places?q=Berlin` → `{"places": [{"name": "Berlin, DE", "detail": "Berlin, Germany"}]}`
(up to 5; `offline` as above). The app puts `name` into `weather_loc`.

## Layout

`POST /api/layout`

```json
{"template": "custom", "statusBar": true,
 "boxes": [{"c": 0, "r": 0, "w": 4, "h": 1, "widget": "agenda"}, {"c": 0, "r": 1, "w": 4, "h": 2, "widget": "clock"}]}
```

or `{"template": "classic", "show": ["weather", "agenda"]}`, or another built-in key. Checked by the
layout engine (inside the grid, no overlaps, the whole grid covered by boxes, every widget at most once
and within its minimum size, at most 12 boxes). → `{"ok": true}`, stored and drawn on the display at
once, **no restart**; or 400 with `error`.

`POST /api/preview.bmp`: same body as `/api/layout`, optionally with `"clock_24h": true/false` (the
app's current, maybe unsaved choice; build 76) → `image/bmp`, 800 × 480, 1 bit per pixel, drawn
by the device with the data it has (a widget without data shows its empty text). 503 `{"ok": false, "error": "..."}`
when there's not enough memory.

## Personal data

`GET /api/tasks` → `{"tasks": [{"title": "Buy milk", "due": "2026-11-20", "prio": 2, "done": false}]}`
(`due`: `""` or `YYYY-MM-DD`; `prio`: 1 high, 2 normal, 3 low; done tasks also have `doneAt`
`YYYY-MM-DD`, from build 76: set by the device when a task is ticked off (kept from the stored list if
the app doesn't send it back), and the task is deleted 30 days later). `PUT /api/tasks` with the same shape
replaces the list (at most 30, titles up to 60 characters).

`GET /api/notes` → `{"note": "text\nsecond line"}`; `PUT` the same (up to 400 characters).

`GET /api/birthdays` →
`{"manual": [{"name": "Anna", "day": 17, "month": 11, "year": 1990}], "upcoming": [{"name": "Anna", "date": "2026-11-17", "days": 0, "age": 36, "fromFeed": false}]}`
(`year` 0 = unknown; `upcoming` is read-only: the next 60 days incl. the calendar link's).
`PUT /api/birthdays` `{"manual": [...]}` replaces the hand-entered list (at most 50).

Characters the screen font can't show are removed when saving; the answer says so:
`{"ok": true, "removed": "✓"}`.

## Diagnostics (build 72)

`GET /api/diagnostics` →

```json
{
  "time": 1795000000, "version": ".72", "bootCount": 412,
  "log": ["--- 2026-11-19 ---", "07:41:20 WiFi connected.", "07:41:22 [ICS] 12 upcoming, 9 days marked"],
  "battery": {"sensor": true, "mv": 1180, "plausible": false, "percent": -1, "usb": false, "minMv": 3000,
              "history": [[1794990000, 1210, 0], [1794993600, 1180, 0]]},
  "services": [{"key": "weather", "name": "Weather", "lastOk": 1794999000, "failures": 0, "retryAt": 0, "stale": false}],
  "problems": ["calendar"],
  "wifi": {"failures": 0, "retryAfter": 0, "rssi": -61},
  "lastTimeSync": 1794998000,
  "heap": {"free": 182340, "minFree": 141200, "largest": 110592}
}
```

- `log`: the last 200 lines, oldest first (stored log plus the lines of the running setup session).
  Each line starts with the local time (`07:41:20`), or `+12.345 s` before the clock was set; date
  lines `--- YYYY-MM-DD ---` separate the days. Only wakes that did something are stored (Wi-Fi,
  a full refresh, a problem change, setup mode), not the quiet clock wakes.
- `battery.quietMv` / `quietPercent` / `quietAt` (build 74, when known): the last reading with Wi-Fi
  off; `battery.mv`: measured now, with Wi-Fi on (raw, also when not plausible); `plausible`: 3.0-4.4 V (a LiPo running
  the board); `percent` -1 when not plausible or without sensor; `usb`: a computer's USB seen (a
  charger isn't). `history`: `[unix time, millivolts, usb 0/1]` once an hour, last 7 days, oldest first.
- `services`: last success (0 = never this power-on), failures in a row, next retry (0 = none),
  `stale` = the widget shows "old".

`GET /api/log.txt` → the whole stored log as a text file (`Content-Disposition: attachment`).

`POST /api/log/clear` `{}` → `{"ok": true}`: erases the stored log.

## Device

`POST /api/update` → `{"ok": true}`, then the device restarts and runs the update check (as the
old "Check for updates now").

`POST /api/factory-reset` → `{"ok": true}`, then everything is erased (settings and personal data)
and the device restarts into setup mode.

`POST /api/finish` → `{"ok": true}`, leaves setup / edit mode: the device restarts into the
dashboard. With `{"ap": true}` ("Change Wi-Fi" on the home network) it restarts into the setup
hotspot instead.
