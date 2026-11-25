// Settings: the buffers the services read, loaded from and saved to NVS (namespace "config").
#include "ConfigManager.h"

#include "../personal/UserStore.h"

#include <string.h>
#include <time.h>
#include <esp_wifi.h>

#include "../hardware/PinMapping.h"
#include "../hardware/StatusLed.h"
#include "../update/OtaFlow.h"
#include "AppState.h"
#include "ConfigValidation.h"
#include "DataStore.h"
#include "../services/CalendarLinks.h"
#include "ServiceState.h"
#include "Timezone.h"
#include "Logger.h"
#include "../layout/Templates.h"
#include "PowerPlan.h"

// ---- The settings (app.h), filled by configLoad() ----
// Network and printer
char wifi_ssid[CFG_WIFI_SSID_MAX], wifi_pass[CFG_WIFI_PASS_MAX];
char mqtt_ip[CFG_MQTT_IP_MAX], mqtt_sn[CFG_MQTT_SN_MAX], mqtt_pass[CFG_MQTT_PASS_MAX];
uint16_t mqtt_port;
char printer_name[CFG_PRINTER_NAME_MAX];
// Data sources
char device_timezone[CFG_TIMEZONE_MAX];
char weather_location[CFG_WEATHER_LOC_MAX];
char stock_symbols[CFG_STOCKS_MAX];
char ics_urls[ICS_MAX_FEEDS][CFG_ICS_URL_MAX];
char bday_url[CFG_ICS_URL_MAX];
char ics_names[ICS_MAX_FEEDS][CFG_ICS_NAME_MAX];
uint8_t ics_every[ICS_MAX_FEEDS] = {0, 0, 0};
char makerworld_uid[CFG_MW_UID_MAX];
char pkge_api_key[CFG_PKGE_KEY_MAX];
char aerodatabox_key[CFG_ADB_KEY_MAX];
// Holidays, month marks, calendar birthdays (stage 7)
char holiday_state[4] = "";
bool holidays_on = false, school_on = false;
char school_url[CFG_ICS_URL_MAX] = "";
uint8_t ics_month_mask = 0;
uint16_t data_interval[4] = {datastore::DEFAULT_WEATHER_MIN, datastore::DEFAULT_STOCKS_MIN,
                             datastore::DEFAULT_CALENDAR_MIN, datastore::DEFAULT_TRACKING_MIN};
static_assert(SERVICE_COUNT == 4, "data_interval: one per Service");
bool cal_birthdays = true;
uint8_t user_content = 0;
// Screen and power
char layout_template[CFG_LAYOUT_MAX] = "classic";
uint16_t widget_show_mask = 0xFFFF;
bool clock_24h = true;
char layout_boxes[CFG_LAYOUT_BOXES_MAX] = "";
char device_name[CFG_DEVICE_NAME_MAX] = "";
uint8_t clock_interval = 1;
bool quiet_on = true;
uint16_t quiet_from = power::DEFAULT_QUIET_FROM, quiet_to = power::DEFAULT_QUIET_TO;
uint16_t battery_mah = power::DEFAULT_BATTERY_MAH;
i18n::Lang ui_lang = i18n::Lang::De;

namespace {

// The text settings and their NVS keys ("config"). The keys are stored on the devices: they must
// not change ("ics_url" is the first calendar link's key from the time there was only one).
struct TextSetting {
  const char* key;
  char* value;
  size_t size;
};
const TextSetting TEXT_SETTINGS[] = {
    {"wifi_ssid", wifi_ssid, sizeof(wifi_ssid)},
    {"wifi_pass", wifi_pass, sizeof(wifi_pass)},
    {"mqtt_ip", mqtt_ip, sizeof(mqtt_ip)},
    {"mqtt_sn", mqtt_sn, sizeof(mqtt_sn)},
    {"mqtt_pass", mqtt_pass, sizeof(mqtt_pass)},
    {"printer_name", printer_name, sizeof(printer_name)},
    {"device_timezone", device_timezone, sizeof(device_timezone)},
    {"weather_loc", weather_location, sizeof(weather_location)},
    {"stock_symbols", stock_symbols, sizeof(stock_symbols)},
    {"ics_url", ics_urls[0], sizeof(ics_urls[0])},
    {"ics_url2", ics_urls[1], sizeof(ics_urls[1])},
    {"ics_url3", ics_urls[2], sizeof(ics_urls[2])},
    {"bday_url", bday_url, sizeof(bday_url)},
    {"makerworld_uid", makerworld_uid, sizeof(makerworld_uid)},
    {"pkge_key", pkge_api_key, sizeof(pkge_api_key)},
    {"adb_key", aerodatabox_key, sizeof(aerodatabox_key)},
    {"hol_state", holiday_state, sizeof(holiday_state)},
    {"school_url", school_url, sizeof(school_url)},
    {"ics_name0", ics_names[0], sizeof(ics_names[0])},  // stage 8
    {"ics_name1", ics_names[1], sizeof(ics_names[1])},
    {"ics_name2", ics_names[2], sizeof(ics_names[2])},
};
static_assert(ICS_MAX_FEEDS == 3, "one TEXT_SETTINGS entry per calendar link");

}  // namespace


void configLoad() {
  bool tzFixed = false;
  preferences.begin("config", true);
  for (const TextSetting& t : TEXT_SETTINGS) {
    memset(t.value, 0, t.size);
    if (preferences.isKey(t.key)) preferences.getString(t.key, t.value, t.size);  // newer keys: absent on old devices
  }
  mqtt_port = preferences.getUInt("mqtt_port", 8883);
  // Right away, so the log lines that follow already carry the local time.
  setenv("TZ", device_timezone[0] ? device_timezone : "UTC0", 1);
  tzset();
  if (!validStateCode(holiday_state)) holiday_state[0] = '\0';
  holidays_on = preferences.getBool("hol_on", false);
  school_on = preferences.getBool("school_on", false);
  // Stage 8: each data type's interval (absent on older devices: the defaults).
  static const char* const INTERVAL_KEYS[4] = {"int_wx", "int_stk", "int_cal", "int_trk"};
  static const uint16_t INTERVAL_DEFAULTS[4] = {datastore::DEFAULT_WEATHER_MIN, datastore::DEFAULT_STOCKS_MIN,
                                                datastore::DEFAULT_CALENDAR_MIN, datastore::DEFAULT_TRACKING_MIN};
  const uint8_t every = preferences.getUChar("ics_every", 0);  // 2 bits per link
  for (int i = 0; i < ICS_MAX_FEEDS; i++) {
    ics_every[i] = (every >> (2 * i)) & 3;
    if (ics_every[i] > icslinks::EVERY_MORNING) ics_every[i] = icslinks::EVERY_CALENDAR;
  }
  for (int i = 0; i < 4; i++)
    data_interval[i] = (uint16_t)datastore::intervalOr(preferences.getUShort(INTERVAL_KEYS[i], INTERVAL_DEFAULTS[i]),
                                                       INTERVAL_DEFAULTS[i]);
  ics_month_mask = preferences.getUChar("ics_mon", 0) & ((1 << ICS_MAX_FEEDS) - 1);
  cal_birthdays = preferences.getBool("cal_bday", true);
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
  activePinPreset = parsePinPreset(preferences.getString("pin_preset", pinPresetValue(DEFAULT_PIN_PRESET)).c_str(), DEFAULT_PIN_PRESET);
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
  // German unless "en" was chosen: also for devices from before build 78 (no key).
  char lang[4] = "";
  if (preferences.isKey("lang")) preferences.getString("lang", lang, sizeof(lang));
  ui_lang = i18n::parseLang(lang);
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

  for (const TextSetting& t : TEXT_SETTINGS) preferences.putString(t.key, t.value);
  preferences.putUInt("mqtt_port", mqtt_port);
  preferences.putBool("hol_on", holidays_on);
  preferences.putBool("school_on", school_on);
  uint8_t every = 0;
  for (int i = 0; i < ICS_MAX_FEEDS; i++) every |= (ics_every[i] & 3) << (2 * i);
  preferences.putUChar("ics_every", every);
  preferences.putUShort("int_wx", data_interval[0]);
  preferences.putUShort("int_stk", data_interval[1]);
  preferences.putUShort("int_cal", data_interval[2]);
  preferences.putUShort("int_trk", data_interval[3]);
  preferences.putUChar("ics_mon", ics_month_mask);
  preferences.putBool("cal_bday", cal_birthdays);
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
  preferences.putString("lang", i18n::langCode(ui_lang));

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
  esp_wifi_restore();  // the Wi-Fi driver's own copy of networks and passwords (the setup hotspot writes it)
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
