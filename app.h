#pragma once
#include "configure.h"
#include "src/hardware/EpaperDriver.h"
#include <Arduino.h>
#include <cstring>

// Fixed buffers avoid Arduino String heap churn on long-running / deep-sleep cycles.
#define CFG_WIFI_SSID_MAX     33u   // 32 + NUL (802.11 SSID)
#define CFG_WIFI_PASS_MAX      65u   // WPA2-PSK max + NUL
#define CFG_MQTT_PASS_MAX      65u
#define CFG_MQTT_SN_MAX        65u
#define CFG_MQTT_IP_MAX        64u   // IPv4 literal or short hostname
#define CFG_CAL_TITLE_MAX     128u
#define CFG_TIMEZONE_MAX       65u
#define CFG_WEATHER_LOC_MAX    65u   // "City, CC" or "lat,lon"
#define CFG_STOCKS_MAX         81u   // comma separated symbols
#define CFG_ICS_URL_MAX       257u   // iCal feed URL
#define CFG_MW_UID_MAX         24u   // MakerWorld user id (digits)
#define CFG_PRINTER_NAME_MAX   24u   // printer widget title
#define CFG_PKGE_KEY_MAX       96u   // PKGE Business API key
#define CFG_ADB_KEY_MAX        96u   // RapidAPI key for AeroDataBox

extern char wifi_ssid[CFG_WIFI_SSID_MAX];
extern char wifi_pass[CFG_WIFI_PASS_MAX];
extern char mqtt_pass[CFG_MQTT_PASS_MAX];
extern char mqtt_sn[CFG_MQTT_SN_MAX];
extern char mqtt_ip[CFG_MQTT_IP_MAX];
extern uint16_t mqtt_port;
extern char device_timezone[CFG_TIMEZONE_MAX];
extern char weather_location[CFG_WEATHER_LOC_MAX];
extern char stock_symbols[CFG_STOCKS_MAX];
extern char makerworld_uid[CFG_MW_UID_MAX];
extern char printer_name[CFG_PRINTER_NAME_MAX];
extern char pkge_api_key[CFG_PKGE_KEY_MAX];
extern char aerodatabox_key[CFG_ADB_KEY_MAX];

// Layout (stage 3): template key, which widgets are shown, clock format.
#define CFG_LAYOUT_MAX         16u
extern char layout_template[CFG_LAYOUT_MAX];   // "classic", "planner", "minimal", "technical"
extern uint16_t widget_show_mask;              // widgetBit(layouts::WidgetId): switched on on the setup page
extern bool clock_24h;

#define ICS_MAX_FEEDS 3
extern char ics_urls[ICS_MAX_FEEDS][CFG_ICS_URL_MAX];
// Personal widgets (stage 4): birthday calendar link (iCal), and which ones have content
// (USER_HAS_*, kept in NVS so a wake needn't open the file system to find out).
extern char bday_url[CFG_ICS_URL_MAX];
extern uint8_t user_content;
#define USER_HAS_TASKS 0x01
#define USER_HAS_NOTE 0x02
#define USER_HAS_BIRTHDAYS 0x04

inline bool hasIcsFeeds() {
    for (int i = 0; i < ICS_MAX_FEEDS; i++)
        if (ics_urls[i][0] != '\0') return true;
    return false;
}

