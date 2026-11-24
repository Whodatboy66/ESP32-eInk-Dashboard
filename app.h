#pragma once
// The device's settings (stage 7b, own code): what the setup app edits, kept in NVS
// (src/core/ConfigManager.cpp). Fixed-size buffers: no heap use across the many short wakes.
// Also the screen language's text lookup tr().
#include "configure.h"
#include "src/hardware/EpaperDriver.h"
#include "src/i18n/Strings.h"
#include <Arduino.h>
#include <cstring>

// ---- Buffer sizes (with the terminating zero) ----
constexpr unsigned CFG_WIFI_SSID_MAX = 33;      // 802.11: up to 32 bytes
constexpr unsigned CFG_WIFI_PASS_MAX = 65;      // WPA2: up to 64 characters
constexpr unsigned CFG_MQTT_IP_MAX = 64;        // the printer: an address or a local name
constexpr unsigned CFG_MQTT_SN_MAX = 65;        // its serial number
constexpr unsigned CFG_MQTT_PASS_MAX = 65;      // its LAN access code
constexpr unsigned CFG_PRINTER_NAME_MAX = 24;   // the printer widget's title
constexpr unsigned CFG_TIMEZONE_MAX = 65;       // a POSIX rule ("CET-1CEST,M3.5.0,M10.5.0/3")
constexpr unsigned CFG_WEATHER_LOC_MAX = 65;    // "City", "City, CC" or "lat, lon"
constexpr unsigned CFG_STOCKS_MAX = 201;        // up to 5 "SYMBOL" or "SYMBOL:Label", comma-separated
constexpr unsigned CFG_ICS_URL_MAX = 257;       // a calendar link
constexpr unsigned CFG_CAL_TITLE_MAX = 128;     // a calendar entry's title
constexpr unsigned CFG_MW_UID_MAX = 24;         // MakerWorld user id
constexpr unsigned CFG_PKGE_KEY_MAX = 96;       // PKGE API key
constexpr unsigned CFG_ADB_KEY_MAX = 96;        // RapidAPI key (AeroDataBox)
constexpr unsigned CFG_LAYOUT_MAX = 16;         // template key
constexpr unsigned CFG_LAYOUT_BOXES_MAX = 200;  // own layout (src/layout/BoxLayout.h)
constexpr unsigned CFG_DEVICE_NAME_MAX = 33;    // "Küche": app header and <name>.local
constexpr int ICS_MAX_FEEDS = 3;                // calendar links

// ---- Network and printer ----
extern char wifi_ssid[CFG_WIFI_SSID_MAX], wifi_pass[CFG_WIFI_PASS_MAX];
extern char mqtt_ip[CFG_MQTT_IP_MAX], mqtt_sn[CFG_MQTT_SN_MAX], mqtt_pass[CFG_MQTT_PASS_MAX];
extern uint16_t mqtt_port;
extern char printer_name[CFG_PRINTER_NAME_MAX];

// ---- Data sources ----
extern char device_timezone[CFG_TIMEZONE_MAX];
extern char weather_location[CFG_WEATHER_LOC_MAX];
extern char stock_symbols[CFG_STOCKS_MAX];
extern char ics_urls[ICS_MAX_FEEDS][CFG_ICS_URL_MAX];
extern char bday_url[CFG_ICS_URL_MAX];  // the birthday calendar link (stage 4)
extern char makerworld_uid[CFG_MW_UID_MAX];
extern char pkge_api_key[CFG_PKGE_KEY_MAX];
extern char aerodatabox_key[CFG_ADB_KEY_MAX];
// Stage 7: the Bundesland for holidays ("" = off), school holidays and their link ("" = our feed
// for the state), which calendar links mark the month (bit i = link i), calendar birthdays.
extern char holiday_state[4];
extern bool holidays_on, school_on;
extern char school_url[CFG_ICS_URL_MAX];
extern uint8_t ics_month_mask;
extern bool cal_birthdays;

inline bool hasIcsFeeds() {
  for (const auto& url : ics_urls)
    if (url[0]) return true;
  return false;
}

// ---- Screen ----
extern char layout_template[CFG_LAYOUT_MAX];     // "classic", "planner", ... or "custom" (layout_boxes)
extern char layout_boxes[CFG_LAYOUT_BOXES_MAX];  // the own layout's boxes (stage 5)
extern uint16_t widget_show_mask;                // the widgets switched on (layouts::widgetBit)
extern bool clock_24h;
extern char device_name[CFG_DEVICE_NAME_MAX];
// Which personal widgets have content (NVS, so a wake needn't open the file system to know).
extern uint8_t user_content;
constexpr uint8_t USER_HAS_TASKS = 0x01, USER_HAS_NOTE = 0x02, USER_HAS_BIRTHDAYS = 0x04;

// ---- Power (stage 6, src/core/PowerPlan.h) ----
extern uint8_t clock_interval;           // minutes between clock wakes
extern bool quiet_on;                    // night quiet hours
extern uint16_t quiet_from, quiet_to;    // minutes of the day (equal: none)
extern uint16_t battery_mah;             // for the battery estimate

// ---- Language (NVS "lang": "de", the default, or "en") ----
extern i18n::Lang ui_lang;
inline const char* tr(i18n::S key) { return i18n::text(key, ui_lang); }
