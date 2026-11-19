#pragma once
// Setup mode state shared by the setup server (src/web/SetupServer.cpp) and the app's API
// (src/web/Api.cpp): which mode runs, the session on the home network, work to do after an answer.
#include <Arduino.h>

enum class SetupKind : uint8_t {
  Off,
  Ap,    // setup hotspot "Dashboard-Setup" (own password, captive portal)
  Home,  // edit mode on the home network (code shown on the screen)
};
extern SetupKind setupKind;

// A request came in: setup mode stays open (timeout counts from here).
void setupNoteActivity();
// After the answer was sent: restart in `delayMs` (settings saved, finish, update).
void setupScheduleRestart(uint32_t delayMs);
// Settings saved: restart back into the same mode (hotspot, or edit mode with the same code and
// session), so the app reconnects and editing goes on.
void setupScheduleRestartSameMode(uint32_t delayMs);
// After the answer was sent: draw the dashboard with the new layout (no restart).
void setupScheduleDashboard();

// Home network: the request comes with a valid session cookie / for our own address.
bool setupSessionOk();
bool setupHostOk();
// Hotspot: the request came in over the hotspot (not the home network of a Wi-Fi test) and for its
// address (no DNS rebinding).
bool setupFromHotspot();
// Checks the code shown on the screen; on success fills `cookie` (Set-Cookie value). A wrong code
// costs a second; after 5 a new code is shown, after 3 new codes edit mode ends. While the
// dashboard covers the code (after a save), an attempt brings the code screen back.
bool setupLogin(const char* code, String& cookie);
// The address on the home network ("kitchen-display.local").
const char* setupHostName();

// Internet for the checks (calendar link, place search, Wi-Fi test): joins a network as a station
// next to the hotspot. pass == nullptr: the saved password (same network). err gets the reason.
bool setupStaConnect(const char* ssid, const char* pass, char* err, size_t cap);
// Connected as a station (home mode, or after setupStaConnect).
bool setupOnline();
// Connects with the saved Wi-Fi if not online yet; false if that's not possible.
bool setupEnsureOnline();
