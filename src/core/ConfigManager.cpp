// Settings: the buffers the services read, loaded from and saved to NVS (namespace "config").
#include "ConfigManager.h"

#include "../personal/UserStore.h"

#include <string.h>
#include <time.h>

#include "../hardware/PinMapping.h"
#include "../hardware/StatusLed.h"
#include "../update/OtaFlow.h"
#include "AppState.h"
#include "ConfigValidation.h"
#include "Timezone.h"
#include "Logger.h"
#include "../layout/Templates.h"
#include "PowerPlan.h"

char wifi_ssid[CFG_WIFI_SSID_MAX];
char wifi_pass[CFG_WIFI_PASS_MAX];
char mqtt_pass[CFG_MQTT_PASS_MAX];
char mqtt_sn[CFG_MQTT_SN_MAX];
char mqtt_ip[CFG_MQTT_IP_MAX];
uint16_t mqtt_port;
char device_timezone[CFG_TIMEZONE_MAX];
char weather_location[CFG_WEATHER_LOC_MAX];
char stock_symbols[CFG_STOCKS_MAX];
char ics_urls[ICS_MAX_FEEDS][CFG_ICS_URL_MAX];
char makerworld_uid[CFG_MW_UID_MAX];
char printer_name[CFG_PRINTER_NAME_MAX];
char pkge_api_key[CFG_PKGE_KEY_MAX];
char aerodatabox_key[CFG_ADB_KEY_MAX];
char bday_url[CFG_ICS_URL_MAX];
uint8_t user_content = 0;

// Preference keys for the calendar feeds (the first keeps the original single-feed key).
char layout_template[CFG_LAYOUT_MAX] = "classic";
uint16_t widget_show_mask = 0xFFFF;
bool clock_24h = true;
char layout_boxes[CFG_LAYOUT_BOXES_MAX] = "";
char device_name[CFG_DEVICE_NAME_MAX] = "";
uint8_t clock_interval = 1;
bool quiet_on = true;
uint16_t quiet_from = power::DEFAULT_QUIET_FROM, quiet_to = power::DEFAULT_QUIET_TO;
uint16_t battery_mah = power::DEFAULT_BATTERY_MAH;

const char* const ICS_PREF_KEYS[ICS_MAX_FEEDS] = { "ics_url", "ics_url2", "ics_url3" };


void configLoad() {
  bool tzFixed = false;
  preferences.begin("config", true);
  memset(wifi_ssid, 0, sizeof(wifi_ssid));
  memset(wifi_pass, 0, sizeof(wifi_pass));
  memset(mqtt_pass, 0, sizeof(mqtt_pass));
  memset(mqtt_sn, 0, sizeof(mqtt_sn));
  memset(mqtt_ip, 0, sizeof(mqtt_ip));
  memset(device_timezone, 0, sizeof(device_timezone));
  preferences.getString("wifi_ssid", wifi_ssid, sizeof(wifi_ssid));
  preferences.getString("wifi_pass", wifi_pass, sizeof(wifi_pass));
  preferences.getString("mqtt_pass", mqtt_pass, sizeof(mqtt_pass));
  preferences.getString("mqtt_sn", mqtt_sn, sizeof(mqtt_sn));
  preferences.getString("mqtt_ip", mqtt_ip, sizeof(mqtt_ip));
  mqtt_port = preferences.getUInt("mqtt_port", 8883);
  preferences.getString("device_timezone", device_timezone, sizeof(device_timezone));
  // Right away, so the log lines that follow already carry the local time.
  setenv("TZ", device_timezone[0] ? device_timezone : "UTC0", 1);
  tzset();
  memset(weather_location, 0, sizeof(weather_location));
  memset(stock_symbols, 0, sizeof(stock_symbols));
  memset(ics_urls, 0, sizeof(ics_urls));
  preferences.getString("weather_loc", weather_location, sizeof(weather_location));
  preferences.getString("stock_symbols", stock_symbols, sizeof(stock_symbols));
  for (int i = 0; i < ICS_MAX_FEEDS; i++)
    preferences.getString(ICS_PREF_KEYS[i], ics_urls[i], sizeof(ics_urls[i]));
  memset(makerworld_uid, 0, sizeof(makerworld_uid));
  preferences.getString("makerworld_uid", makerworld_uid, sizeof(makerworld_uid));
  memset(printer_name, 0, sizeof(printer_name));
  preferences.getString("printer_name", printer_name, sizeof(printer_name));
  memset(pkge_api_key, 0, sizeof(pkge_api_key));
  preferences.getString("pkge_key", pkge_api_key, sizeof(pkge_api_key));
  memset(aerodatabox_key, 0, sizeof(aerodatabox_key));
  preferences.getString("adb_key", aerodatabox_key, sizeof(aerodatabox_key));
  memset(bday_url, 0, sizeof(bday_url));
  preferences.getString("bday_url", bday_url, sizeof(bday_url));
  user_content = preferences.getUChar("user_flags", 0);
  // Older setups may have saved a bare name like "CET" (which the C library treats as UTC).
  char tzRule[CFG_TIMEZONE_MAX];
  if (!normalizeTimezone(device_timezone, tzRule, sizeof(tzRule))) {
    logPrintf("Timezone \"%s\" not usable, using UTC: fix it on the setup page\n", device_timezone);
    snprintf(tzRule, sizeof(tzRule), "UTC0");
  }
  if (strcmp(tzRule, device_timezone) != 0) {
    logPrintf("Timezone \"%s\" -> \"%s\"\n", device_timezone, tzRule);
    snprintf(device_timezone, sizeof(device_timezone), "%s", tzRule);
    tzFixed = true;
  }
  activePinPreset = parsePinPreset(preferences.getString("pin_preset", pinPresetValue(DEFAULT_PIN_PRESET)));
  customPins = loadCustomPinConfig(preferences, makePinPreset(DEFAULT_PIN_PRESET));
  applyStoredPinSelection();
  logPinConfiguration();
  if (activePinPreset == PinPreset::Esp32C6SuperMini) {
    forceSuperMiniLedsOff();
  }
  otaAutoInstall = preferences.getBool("ota_auto", true);
  otaBeta = preferences.getBool("ota_beta", false);
  // Layout (schema 2; missing on older setups = today's Classic layout, everything shown, 24 h).
  preferences.getString("lay_tpl", layout_template, sizeof(layout_template));
  preferences.getString("lay_box", layout_boxes, sizeof(layout_boxes));
  preferences.getString("dev_name", device_name, sizeof(device_name));
  clock_interval = preferences.getUChar("clk_int", 1);
  if (!power::validClockInterval(clock_interval)) clock_interval = 1;
  quiet_on = preferences.getBool("quiet_on", true);
  quiet_from = preferences.getUShort("quiet_from", power::DEFAULT_QUIET_FROM) % 1440;
  quiet_to = preferences.getUShort("quiet_to", power::DEFAULT_QUIET_TO) % 1440;
  battery_mah = preferences.getUShort("bat_mah", power::DEFAULT_BATTERY_MAH);
  // "custom" = the boxes of the setup app's editor (an unusable string falls back to Classic later).
  if (layouts::templateIndex(layout_template) < 0 && strcmp(layout_template, "custom") != 0)
    snprintf(layout_template, sizeof(layout_template), "classic");
  widget_show_mask = preferences.getUShort("show_mask", 0xFFFF);
  clock_24h = preferences.getBool("clk_24h", true);
  otaAvailBuild = preferences.getUInt("ota_avail", 0);
  preferences.getString("ota_availv", otaAvailVersion, sizeof(otaAvailVersion));
  if (otaAvailBuild <= FW_BUILD) otaAvailBuild = 0;  // already installed
  otaLastCheckDay = preferences.getInt("ota_day", 0);
  otaBadBuild = preferences.getUInt("ota_bad", 0);

  preferences.end();
  if (tzFixed) {  // store the corrected rule once
    preferences.begin("config", false);
    preferences.putString("device_timezone", device_timezone);
    preferences.end();
  }

  // Secrets never appear in the log (masked by logPrintf): pointers to the buffers themselves.
  static const char* const secrets[] = {wifi_pass, mqtt_pass, pkge_api_key, aerodatabox_key,
                                        ics_urls[0], ics_urls[1], ics_urls[2], bday_url};
  static_assert(ICS_MAX_FEEDS == 3, "list every calendar feed in secrets[]");
  logRegisterSecrets(secrets, sizeof(secrets) / sizeof(secrets[0]));

  // Settings schema: devices without the key have schema 0, the same layout as schema 1.
  // Opened for writing only when a migration is due (once), not on every wake.
  preferences.begin("config", true);
  int schema = preferences.getInt("cfg_schema", 0);
  preferences.end();
  if (configNeedsMigration(schema, FW_CONFIG_SCHEMA)) {
    // 0 -> 1: nothing to convert. 2 -> 3: new keys (custom layout, device name, hotspot password)
    // whose defaults change nothing. 1 -> 2: the layout keys are new; their defaults (Classic,
    // everything shown, 24 h) are today's screen. Only the key is stored.
    preferences.begin("config", false);
    if (schema < 2) {  // the defaults, so they are found from now on
      preferences.putString("lay_tpl", layout_template);
      preferences.putUShort("show_mask", widget_show_mask);
      preferences.putBool("clk_24h", clock_24h);
    }
    preferences.putInt("cfg_schema", FW_CONFIG_SCHEMA);
    preferences.end();
    DBGF("Settings schema %d -> %d", schema, FW_CONFIG_SCHEMA);
  }
}

void configSave() {
  preferences.begin("config", false);

  preferences.putString("wifi_ssid", wifi_ssid);
  preferences.putString("wifi_pass", wifi_pass);
  preferences.putString("device_timezone", device_timezone);
  preferences.putString("weather_loc", weather_location);
  preferences.putString("stock_symbols", stock_symbols);
  for (int i = 0; i < ICS_MAX_FEEDS; i++)
    preferences.putString(ICS_PREF_KEYS[i], ics_urls[i]);
  preferences.putString("makerworld_uid", makerworld_uid);
  preferences.putString("printer_name", printer_name);
  preferences.putString("pkge_key", pkge_api_key);
  preferences.putString("adb_key", aerodatabox_key);
  preferences.putString("bday_url", bday_url);

  preferences.putString("mqtt_pass", mqtt_pass);
  preferences.putString("mqtt_sn", mqtt_sn);
  preferences.putString("mqtt_ip", mqtt_ip);
  preferences.putUInt("mqtt_port", mqtt_port);
  preferences.putString("pin_preset", pinPresetValue(activePinPreset));
  saveCustomPinConfig(preferences, customPins);
  preferences.putBool("ota_auto", otaAutoInstall);
  preferences.putBool("ota_beta", otaBeta);
  preferences.putString("lay_tpl", layout_template);
  preferences.putUShort("show_mask", widget_show_mask);
  preferences.putBool("clk_24h", clock_24h);
  preferences.putString("lay_box", layout_boxes);
  preferences.putString("dev_name", device_name);
  preferences.putUChar("clk_int", clock_interval);
  preferences.putBool("quiet_on", quiet_on);
  preferences.putUShort("quiet_from", quiet_from);
  preferences.putUShort("quiet_to", quiet_to);
  preferences.putUShort("bat_mah", battery_mah);

  preferences.putInt("cfg_schema", FW_CONFIG_SCHEMA);
  preferences.end();
}

void configFactoryReset() {
  logPrintln("Factory reset: erasing all settings, restarting into setup mode");
  // A user action: a new firmware that waits for confirmation must not roll back on this restart.
  otaConfirmByUser("factory reset");
  preferences.begin("config", false);
  preferences.clear();
  // Keep the build that rolled back, so the night-time check doesn't auto-install it again.
  if (otaBadBuild) preferences.putUInt("ota_bad", otaBadBuild);
  preferences.end();
  eraseUserData();  // tasks, note, birthdays: the device may be given away
  delay(200);
  ESP.restart();
}

void configCheckSerialFactoryReset() {
  // Typed into the serial monitor while the board waited for the BOOT button after a reset.
  static const char COMMAND[] = "factory-reset";
  char line[24];
  size_t n = 0;
  while (Serial.available() && n < sizeof(line) - 1) {
    int c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (n) break;
      continue;
    }
    line[n++] = static_cast<char>(c);
  }
  line[n] = '\0';
  if (n && strcmp(line, COMMAND) == 0) configFactoryReset();
  if (n) logPrintf("Serial command \"%s\" unknown (only \"%s\")\n", line, COMMAND);
}
