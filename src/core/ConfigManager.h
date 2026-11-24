#pragma once
// Settings (setup page), stored in NVS namespace "config". The buffers are declared in app.h.
#include "../../app.h"

// Loads all settings from NVS into the buffers, pin map and update settings; corrects an old
// timezone name; migrates the settings schema (FW_CONFIG_SCHEMA).
void configLoad();
// Saves all settings (the buffers, pin map, update settings) to NVS.
void configSave();
// Erases all settings (NVS namespace "config", incl. update state) and restarts into setup mode.
void configFactoryReset();
// After a reset: if "factory-reset" was typed on the USB serial console (during the BOOT button
// window), do it.
void configCheckSerialFactoryReset();
