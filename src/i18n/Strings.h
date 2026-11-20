#pragma once
// Every text the user sees, in German and English (docs/plans/LOOK_AND_LANGUAGE.md, part 1): the
// screen (widgets, status bar, update / setup / charge screens) and what the setup app API sends
// (problems, field errors, check results). Not here: the diagnostics log (stays English) and user
// content (task titles, calendar entries, names). Plain C++, tested on the PC
// (tests/host/test_i18n.cpp). The firmware reads them with tr(key) (app.h) in the chosen language.
//
// One line per text: X(key, German, English). A text with printf conversions has the same ones in
// both languages, in the same order (the test checks it). The screen font has letters, digits,
// ä ö ü Ä Ö Ü ß € and # % * + , - . / : = ? _ ~ ° ! " & ' ( ) ; @ [ ] – ‘ ’ “ ” „ … (no ✓, < >, emoji).
// Tables and texts are const: they stay in flash.
#include <cstdint>
#include <cstring>

namespace i18n {

enum class Lang : uint8_t { De, En };

// clang-format off
#define I18N_TEXTS(X) \
  /* ---- widgets ---- */ \
  X(Unavailable1,      "Zurzeit nicht",                    "Not available") \
  X(Unavailable2,      "verfügbar",                        "at the moment") \
  X(Stale,             "alt",                              "old") \
  X(AllDay,            "Ganztägig",                        "All day") \
  X(CountdownMin,      "in %ldm",                          "in %ldm") \
  X(CountdownHours,    "in %ldh%02ldm",                    "in %ldh%02ldm") \
  X(CountdownDays,     "in %ldT %ldh",                     "in %ldd %ldh") \
  X(FlightCanceled,    "ANNULLIERT",                       "CANCELED") \
  X(FlightMaybeCanceled, "Evtl. annulliert",               "Possibly canceled") \
  X(FlightDiverted,    "Umgeleitet",                       "Diverted") \
  X(FlightLanded,      "Gelandet %s",                      "Landed %s") \
  X(FlightInAir,       "In der Luft, landet %s",           "In the air, lands %s") \
  X(FlightScheduled,   "Geplant",                          "Scheduled") \
  X(FlightOnTime,      "Pünktlich",                        "On time") \
  X(FlightCheckIn,     "Check-in",                         "Check-in") \
  X(FlightBoarding,    "Boarding",                         "Boarding") \
  X(FlightGateClosed,  "Gate geschlossen",                 "Gate closed") \
  X(FlightDelayed,     "Verspätet",                        "Delayed") \
  X(FlightGate,        ", Gate %s",                        ", gate %s") \
  X(FlightTerminal,    ", Terminal %s",                    ", Terminal %s") \
  X(TrackChecking,     "In Prüfung",                       "Checking") \
  X(TrackNoInfo,       "Keine Info",                       "No info yet") \
  X(TrackInTransit,    "Unterwegs",                        "In transit") \
  X(TrackPickup,       "Abholbereit",                      "At pickup point") \
  X(TrackDelivered,    "Zugestellt",                       "Delivered") \
  X(TrackNotDelivered, "Unzustellbar",                     "Not delivered") \
  X(TrackError,        "Zustellfehler",                    "Delivery error") \
  X(TrackPreparing,    "Angekündigt",                      "Preparing") \
  X(TrackEnded,        "Beendet",                          "Route ended") \
  X(TrackUnknown,      "Unbekannt",                        "Unknown") \
  X(TrackNone,         "Keine Pakete in Verfolgung",       "No packages being tracked") \
  X(TrackEta,          "Ankunft",                          "ETA") \
  X(MwLikes,           "Likes:",                           "Likes :") \
  X(MwDownloads,       "Downloads:",                       "Downloads :") \
  X(MwPrints,          "Drucke:",                          "Prints :") \
  X(MwCollections,     "Sammlungen:",                      "Collections :") \
  X(MwBoosts,          "Boosts:",                          "Boosts :") \
  X(PrinterDone,       "F: %d%%",                          "C: %d%%") \
  X(PrinterEmpty,      "LEER",                             "EMPTY") \
  X(PrinterRunning,    "Druckt",                           "RUNNING") \
  X(PrinterPrepare,    "Vorbereitung",                     "PREPARE") \
  X(PrinterPause,      "Pausiert",                         "PAUSE") \
  X(PrinterFinish,     "Fertig",                           "FINISH") \
  X(PrinterFailed,     "Fehler",                           "FAILED") \
  X(PrinterIdle,       "Bereit",                           "IDLE") \
  X(TasksAllDone,      "Alles erledigt",                   "All tasks done") \
  X(TasksNone,         "Keine Aufgaben",                   "No tasks") \
  X(TasksMore,         "+%d weitere",                      "+%d more") \
  X(DueToday,          "heute",                            "today") \
  X(DueTomorrow,       "morgen",                           "tomorrow") \
  X(NoteNone,          "Keine Notiz",                      "No note") \
  X(BdayNone,          "Keine Geburtstage in 60 Tagen",    "No birthdays in the next 60 days") \
  X(BdayToday,         "Heute",                            "Today") \
  X(BdayTomorrow,      "Morgen",                           "Tomorrow") \
  X(BdayInDays,        "in %d T.",                         "in %d d") \
  /* ---- status bar ---- */ \
  X(UpdateAvailable,   "Update verfügbar  v%s",            "Update available  v%s") \
  X(NoteNight,         "Nacht bis %s",                     "Night until %s") \
  X(NoteSaver,         "Akku-Sparen: alle %d Min.",        "Battery saver: every %d min") \
  X(NoteClock,         "Uhr alle %d Min.",                 "Clock every %d min") \
  X(ProbWifi,          "WLAN",                             "WiFi") \
  X(ProbTime,          "Zeit",                             "Time") \
  X(ProbWeather,       "Wetter",                           "Weather") \
  X(ProbStocks,        "Aktien",                           "Stocks") \
  X(ProbCalendar,      "Kalender",                         "Calendar") \
  X(ProbPrinter,       "Drucker",                          "Printer") \
  X(ProbPrinterCode,   "Zugangscode",                      "Access code") \
  X(ProbMakerWorld,    "MakerWorld",                       "MakerWorld") \
  X(ProbTracking,      "Pakete",                           "Tracking") \
  X(ProbFlight,        "Flug",                             "Flight") \
  X(ProbBirthdays,     "Geburtstage",                      "Birthdays") \
  /* ---- widget names and what to set up (box layouts, the app) ---- */ \
  X(WStatusBar,        "Statusleiste",                     "Status bar") \
  X(WClock,            "Uhr",                              "Clock") \
  X(WWeather,          "Wetter",                           "Weather") \
  X(WAgenda,           "Termine",                          "Upcoming events") \
  X(WMonth,            "Monatskalender",                   "Month calendar") \
  X(WStocks,           "Aktien",                           "Stocks") \
  X(WTracking,         "Paketverfolgung",                  "Package tracking") \
  X(WPrinter,          "Drucker / MakerWorld",             "Printer / MakerWorld") \
  X(WTasks,            "Aufgaben",                         "Tasks") \
  X(WNote,             "Notiz",                            "Note") \
  X(WBirthdays,        "Geburtstage",                      "Birthdays") \
  X(HintWeather,       "Wetter: Ort in der App eintragen", "Weather: set a place in the app") \
  X(HintAgenda,        "Termine: Kalender-Link in der App eintragen", "Agenda: add a calendar link in the app") \
  X(HintStocks,        "Aktien: Symbole in der App eintragen", "Stocks: add symbols in the app") \
  X(HintTracking,      "Pakete: Tracking-Schlüssel in der App eintragen", "Parcels: add the tracking key in the app") \
  X(HintPrinter,       "Drucker: Adresse oder MakerWorld-ID in der App eintragen", "Printer: set its address or a MakerWorld ID in the app") \
  X(HintTasks,         "Aufgaben: in der App anlegen",     "Tasks: add tasks in the app") \
  X(HintNote,          "Notiz: in der App schreiben",      "Note: write a note in the app") \
  X(HintBirthdays,     "Geburtstage: in der App eintragen", "Birthdays: add birthdays in the app") \
  /* ---- charge screen (src/widgets/Dashboard.cpp) ---- */ \
  X(ChargeTitle,       "Akku leer: bitte laden",           "Battery empty: please charge") \
  X(ChargeVolts,       "Akku: %s V",                       "Battery: %s V") \
  X(ChargeBack,        "Das Dashboard kommt von selbst zurück, sobald der Akku lädt.", "The dashboard comes back by itself once it charges.") \
  /* ---- update screens (src/update/UpdateScreens.cpp) ---- */ \
  X(UpdFirmware,       "Firmware %s   -   %s",             "Firmware %s   -   %s") \
  X(UpdChannel,        "Kanal: %s   -   Automatisch installieren: %s", "Channel: %s   -   Auto install: %s") \
  X(UpdOn,             "an",                               "on") \
  X(UpdOff,            "aus",                              "off") \
  X(UpdStable,         "stabil",                           "stable") \
  X(UpdBeta,           "Beta",                             "beta") \
  X(UpdLast,           "Letztes Update: %s",               "Last update: %s") \
  X(UpdNever,          "nie",                              "never") \
  X(UpdPowerUsb,       "Strom: USB",                       "Power: USB") \
  X(UpdBattery,        "Akku: %d %%%s",                    "Battery: %d %%%s") \
  X(UpdUsbConnected,   " - USB verbunden",                 " - USB connected") \
  X(UpdWifi,           "WLAN: %s",                         "Wi-Fi: %s") \
  X(UpdWifiNone,       "nicht eingerichtet",               "not set up") \
  X(UpdDeviceInfo,     "Geräteinfo",                       "Device info") \
  X(UpdChecking,       "Suche nach Updates...",            "Checking for updates...") \
  X(UpdNewVersion,     "Neue Version %s",                  "New version %s") \
  X(UpdInstalled,      "Installiert: %s",                  "Installed: %s") \
  X(UpdTapToInstall,   "Tippen zum Installieren - Abbruch in %u s", "Tap to install - cancels in %u s") \
  X(UpdInstalling,     "Wird installiert...",              "Installing...") \
  X(UpdUpdatingTo,     "Update auf %s",                    "Updating to %s") \
  X(UpdStepDownload,   "1/4  Herunterladen  %d %%",        "1/4  Downloading  %d %%") \
  X(UpdStepVerify,     "2/4  Prüfen",                      "2/4  Verifying") \
  X(UpdStepInstall,    "3/4  Installieren",                "3/4  Installing") \
  X(UpdStepRestart,    "4/4  Neustart",                    "4/4  Restarting") \
  X(UpdKeepPowered,    "Bitte das Gerät nicht vom Strom trennen.", "Please keep the device powered.") \
  X(UpdTitle,          "Firmware-Update",                  "Firmware update") \
  X(UpdBackSoon,       "In wenigen Sekunden zurück zum Dashboard.", "Back to the dashboard in a few seconds.") \
  X(UpdUpdatedTo,      "Aktualisiert auf %s",              "Updated to %s") \
  X(UpdDashboardFollows, "Das Dashboard folgt gleich.",    "The dashboard follows in a moment.") \
  X(UpdNewInVersion,   "Neu in dieser Version:",           "New in this version:") \
  /* ---- update results (src/update/OtaFlow.cpp: screen and the app's "last update") ---- */ \
  X(OtaPostponed,      "Update verschoben",                "Update postponed") \
  X(OtaStillStarting,  "Diese Version startet noch: in einer Minute erneut versuchen", "This version is still starting up: try again in a minute") \
  X(OtaInstalledRestarting, "%s installiert, Neustart",    "Installed %s, restarting") \
  X(OtaFailedTo,       "Update auf %s fehlgeschlagen: %s", "Update to %s failed: %s") \
  X(OtaFailedKept,     "Update fehlgeschlagen: alte Version bleibt", "Update failed: previous version kept") \
  X(OtaNoRelease,      "Noch kein Release (%s) auf GitHub", "No %s release on GitHub yet") \
  X(OtaBadSignature,   "Signatur des Release ungültig: nicht installiert", "Release signature not valid: not installed") \
  X(OtaBadManifest,    "Release-Infos nicht lesbar, Fehler %d", "Release information not readable, error %d") \
  X(OtaNoMemory,       "Zu wenig Speicher: später erneut versuchen", "Not enough memory: try again later") \
  X(OtaNoGithub,       "Keine Verbindung zu GitHub",       "No connection to GitHub") \
  X(OtaGithubAnswered, "GitHub antwortete %d",             "GitHub answered %d") \
  X(OtaNoWifi,         "Kein WLAN: später erneut versuchen", "No Wi-Fi: try again later") \
  X(OtaSetUpWifi,      "Zuerst das WLAN in der Einrichtung eintragen", "Set up Wi-Fi on the setup page first") \
  X(OtaCheckFailed,    "Update-Prüfung fehlgeschlagen: später erneut versuchen", "Update check failed: try again later") \
  X(OtaUpToDate,       "Aktuell: %s",                      "Up to date: %s") \
  X(OtaNoUpdate,       "Kein Update für dieses Gerät",     "No update for this device") \
  X(OtaNoFile,         "Release %s hat keine Datei für %s", "Release %s has no file for %s") \
  X(OtaWrongDisplay,   "Release %s ist für Display %s, dieses hat %s", "Release %s is built for display %s, this one is %s") \
  X(OtaBadSettings,    "Release %s kann die Einstellungen dieses Geräts nicht lesen", "Release %s can't read this device's settings") \
  X(OtaNeedsNewer,     "Release %s braucht Build %u oder neuer: über USB installieren", "Release %s needs build %u or newer: install it over USB") \
  X(OtaNeedsUsb,       "Update nur über USB möglich",      "Update needs a USB install") \
  X(OtaBatteryDetail,  "Akku %d %% - ein Update braucht %d %% oder USB-Strom", "Battery %d %% - an update needs %d %% or USB power") \
  X(OtaBatteryLow,     "Akku zu schwach für ein Update: bitte laden", "Battery too low for an update: please charge") \
  X(OtaNotInstalled,   "%s verfügbar, nicht installiert",  "%s available, not installed") \
  X(OtaNightFailed,    "Nachtprüfung fehlgeschlagen: %s",  "Night check failed: %s") \
  X(OtaNightUpToDate,  "Nachtprüfung: aktuell",            "Night check: up to date") \
  X(OtaNightNothing,   "Nachtprüfung: kein installierbares Update", "Night check: no installable update") \
  X(OtaNightFailedBefore, "Nachtprüfung: %s verfügbar, schon einmal fehlgeschlagen - mit Doppeltipp installieren", "Night check: %s available, failed before - install it with a double tap") \
  X(OtaNightAvailable, "Nachtprüfung: %s verfügbar",       "Night check: %s available") \
  X(OtaNightWaits,     "Nachtprüfung: %s wartet, Akku zu schwach", "Night check: %s waits, battery too low") \
  X(OtaRolledBack,     "Update auf Build %u nach dem Neustart fehlgeschlagen: %s bleibt", "Update to build %u failed after the restart: %s kept") \
  X(OtaDidNotStart,    "Die neue Version ist nicht richtig gestartet.", "The new version did not start correctly.") \
  X(InstOk,            "installiert",                      "installed") \
  X(InstBadUrl,        "ungültiger Dateiname im Release",  "bad file name in the release") \
  X(InstDownload,      "Download fehlgeschlagen",          "download failed") \
  X(InstWrongSize,     "Dateigröße stimmt nicht",          "file size does not match") \
  X(InstNoSpace,       "passt nicht - oder kein Update-Platz: einmal über USB flashen", "does not fit - or no update slot: flash once over USB") \
  X(InstHash,          "Datei beschädigt - Prüfsumme falsch", "file damaged - checksum wrong") \
  X(InstWrite,         "Schreiben fehlgeschlagen",         "writing failed") \
  X(InstUnknown,       "unbekannter Fehler",               "unknown error") \
  /* ---- setup screens (src/web/SetupScreen.cpp) ---- */ \
  X(SetupTitle,        "Einrichtung",                      "Setup mode") \
  X(SetupWifi,         "WLAN:  %s",                        "Wi-Fi:  %s") \
  X(SetupPassword,     "Passwort:  %s",                    "Password:  %s") \
  X(SetupStep1,        "1. Den Code mit der Handykamera scannen", "1. Scan the code with your phone's camera,") \
  X(SetupStep1b,       "   oder dieses WLAN von Hand wählen:", "   or join this Wi-Fi by hand:") \
  X(SetupStep2,        "2. Die Einrichtung öffnet sich von selbst.", "2. The setup page opens by itself.") \
  X(SetupStep2b,       "   Sonst http://192.168.4.1 öffnen", "   If not, open http://192.168.4.1") \
  X(SetupTimeout,      "Endet nach 5 Minuten ohne Benutzung.", "Leaves setup mode after 5 minutes without use.") \
  X(EditTitle,         "Bearbeiten",                       "Edit mode") \
  X(EditLine1,         "Auf einem Handy oder Computer in diesem", "On a phone or computer in this Wi-Fi,") \
  X(EditLine2,         "WLAN den Code scannen oder öffnen:", "scan the code or open:") \
  X(EditOr,            "oder http://%s",                   "or http://%s") \
  X(EditCode,          "Code zum Eingeben:",               "Code to type in:") \
  X(EditTimeout,       "Endet nach 10 Min. Pause.",        "Closes after 10 min unused.") \
  /* ---- setup app API (src/web/Api.cpp): from here on not drawn on the screen (the app has its own font) ---- */ \
  X(ApiOffline,        "Das Gerät hat keine Internetverbindung: speichere oder teste zuerst dein WLAN.", "The device has no internet connection: save or test your Wi-Fi first.") \
  X(ApiHotspotOnly,    "Nur über den Einrichtungs-Hotspot", "Only from the setup hotspot") \
  X(ApiWrongAddress,   "Falsche Adresse",                  "Wrong address") \
  X(ApiJsonExpected,   "JSON erwartet",                    "JSON expected") \
  X(ApiEmptyRequest,   "Leere Anfrage",                    "Empty request") \
  X(ApiTooMuchData,    "Zu viele Daten",                   "Too much data") \
  X(ApiUnreadable,     "Die Anfrage konnte nicht gelesen werden.", "The request could not be read.") \
  X(PtWifi,            "Das WLAN ist nicht erreichbar",    "The Wi-Fi network can't be reached") \
  X(PfWifi,            "Prüfe Netz und Passwort im Tab Gerät.", "Check the network and password on the Device tab.") \
  X(PtTime,            "Die Uhr konnte nicht über das Internet gestellt werden", "The clock could not be set from the internet") \
  X(PfTime,            "Das behebt sich meist von selbst; bleibt es, prüfe das WLAN.", "It usually fixes itself; check the Wi-Fi if it stays.") \
  X(PtWeather,         "Das Wetter kann nicht geladen werden", "The weather can't be loaded") \
  X(PfWeather,         "Prüfe den Ort im Tab Daten.",    "Check the place on the Data tab.") \
  X(PtStocks,          "Einige Aktienkurse können nicht geladen werden", "Some stock prices can't be loaded") \
  X(PfStocks,          "Prüfe die Aktiensymbole im Tab Daten.", "Check the stock symbols on the Data tab.") \
  X(PtCalendar,        "Ein Kalender kann nicht gelesen werden", "A calendar can't be read") \
  X(PfCalendar,        "Prüfe die Kalender-Links im Tab Daten.", "Check the calendar links on the Data tab.") \
  X(PtPrinter,         "Der Drucker antwortet nicht",      "The printer doesn't answer") \
  X(PfPrinter,         "Ist er eingeschaltet? Prüfe seine Adresse im Tab Daten.", "Is it switched on? Check its address on the Data tab.") \
  X(PtPrinterCode,     "Der Drucker hat den Zugangscode abgelehnt", "The printer refused the access code") \
  X(PfPrinterCode,     "Übernimm den Zugangscode vom Display des Druckers.", "Copy the access code from the printer's screen.") \
  X(PtMakerWorld,      "MakerWorld kann nicht geladen werden", "MakerWorld can't be loaded") \
  X(PfMakerWorld,      "Prüfe die MakerWorld-Benutzer-ID im Tab Daten.", "Check the MakerWorld user ID on the Data tab.") \
  X(PtTracking,        "Die Paketverfolgung kann nicht geladen werden", "Parcel tracking can't be loaded") \
  X(PfTracking,        "Prüfe den Tracking-Schlüssel im Tab Daten.", "Check the tracking key on the Data tab.") \
  X(PtFlight,          "Der Flugstatus kann nicht geladen werden", "The flight status can't be loaded") \
  X(PfFlight,          "Prüfe den Flug-Schlüssel im Tab Daten.", "Check the flight key on the Data tab.") \
  X(PtBirthdays,       "Der Geburtstagskalender kann nicht gelesen werden", "The birthday calendar can't be read") \
  X(PfBirthdays,       "Prüfe den Link zum Geburtstagskalender im Tab Daten.", "Check the birthday calendar link on the Data tab.") \
  X(ApiLayoutFallback, "Das gespeicherte Layout war nicht nutzbar: Klassisch wird gezeigt.", "The saved layout could not be used: Classic is shown.") \
  X(ApiEstimateNote,   "Schätzung aus den ersten Messungen (20.11.2026); echte Strommessungen folgen.", "Estimate from the first measurements (2026-11-20); real current measurements follow.") \
  X(TplPlanner,        "Planer",                           "Planner") \
  X(TplMinimal,        "Minimal",                          "Minimal") \
  X(TplTechnical,      "Technik",                          "Technical") \
  X(TplPersonal,       "Persönlich",                       "Personal") \
  X(PinsCustom,        "Eigene Pins",                      "Custom") \
  X(SvcParcels,        "Pakete",                           "Parcels") \
  X(FieldMaxChars,     "%s: höchstens %u Zeichen.",        "%s: at most %u characters.") \
  X(LabelName,         "Name",                             "Name") \
  X(LabelWifiName,     "WLAN-Name",                        "Wi-Fi name") \
  X(LabelPlace,        "Ort",                              "Place") \
  X(LabelSerial,       "Seriennummer",                     "Serial number") \
  X(LabelPrinterName,  "Druckername",                      "Printer name") \
  X(LabelMakerWorld,   "MakerWorld-Benutzer-ID",           "MakerWorld user ID") \
  X(ErrClockInterval,  "Wähle 1, 2, 5, 10, 15 oder 30 Minuten.", "Choose 1, 2, 5, 10, 15 or 30 minutes.") \
  X(ErrQuietFrom,      "Eine Uhrzeit wie 23:00.",          "A time like 23:00.") \
  X(ErrQuietTo,        "Eine Uhrzeit wie 06:00.",          "A time like 06:00.") \
  X(ErrBatteryMah,     "Zwischen 100 und 20000 mAh.",      "Between 100 and 20000 mAh.") \
  X(ErrTimezone,       "Zeitzone nicht erkannt: gib einen Namen wie Europe/Berlin oder CET ein, oder eine Regel wie CET-1CEST,M3.5.0,M10.5.0/3.", "Time zone not recognized: use a name like Europe/Berlin or CET, or a rule like CET-1CEST,M3.5.0,M10.5.0/3.") \
  X(ErrStocks,         "Yahoo-Finance-Symbole (Buchstaben, Ziffern, ^ . = -) durch Kommas getrennt, z. B. AAPL,NVDA,^IXIC.", "Yahoo Finance symbols (letters, digits, ^ . = -) separated by commas, e.g. AAPL,NVDA,^IXIC.") \
  X(ErrLink,           "Keine Adresse mit https://, http:// oder webcal://.", "Not an https://, http:// or webcal:// address.") \
  X(ErrPrinterAddress, "Eine Adresse in deinem Heimnetz (z. B. 192.168.1.100) oder dort der Name des Druckers.", "An address in your home network (e.g. 192.168.1.100) or the printer's name there.") \
  X(ErrPort,           "Eine Zahl von 1 bis 65535 (meist 8883).", "A number from 1 to 65535 (usually 8883).") \
  X(ErrSecret,         "Zu lang oder kein Text.",          "Too long or not text.") \
  X(ErrPinPreset,      "Unbekannte Board-Vorgabe.",        "Unknown board preset.") \
  X(ErrPins,           "Eigene Pins: CS, DC, RST, BUSY, SCK und MOSI sind nötig.", "Custom pins: CS, DC, RST, BUSY, SCK and MOSI are required.") \
  X(ErrLang,           "Sprache: de oder en.",             "Language: de or en.") \
  X(ErrNothingSaved,   "Nichts gespeichert: korrigiere die markierten Felder.", "Nothing was saved: please correct the marked fields.") \
  X(BoxEmpty,          "Das Layout hat keine Felder.",     "The layout has no boxes.") \
  X(BoxTooMany,        "Höchstens 12 Felder.",             "At most 12 boxes.") \
  X(BoxOutside,        "Ein Feld ragt über den Bildschirm hinaus.", "A box reaches outside the screen.") \
  X(BoxOverlap,        "Zwei Felder überlappen sich.",     "Two boxes overlap.") \
  X(BoxGap,            "Ein Teil des Bildschirms ist von keinem Feld bedeckt.", "Part of the screen is not covered by a box.") \
  X(BoxDuplicate,      "Ein Widget kann nur einmal gezeigt werden.", "A widget can be shown only once.") \
  X(BoxTooSmall,       "Ein Widget ist zu groß für sein Feld.", "A widget is too large for its box.") \
  X(BoxUnknownWidget,  "Unbekanntes Widget.",              "Unknown widget.") \
  X(BoxInvalid,        "Ungültiges Layout.",               "Invalid layout.") \
  X(ApiLayoutTooLong,  "Das Layout ist zu lang.",          "The layout is too long.") \
  X(ApiUnknownLayout,  "Unbekanntes Layout.",              "Unknown layout.") \
  X(ApiPreviewMemory,  "Gerade zu wenig Speicher für die Vorschau: versuche es noch einmal.", "Not enough memory for the preview right now: try again.") \
  X(ApiChooseNetwork,  "Wähle zuerst ein Netz.",      "Choose a network first.") \
  X(ApiOtherNetwork,   "Ein anderes Netz lässt sich nur im Einrichtungsmodus testen: nimm „WLAN wechseln“.", "Another network can only be tested in setup mode: use \"Change Wi-Fi\".") \
  X(ApiNotSavedPass,   "Nicht das gespeicherte Passwort: ein neues lässt sich im Einrichtungsmodus testen („WLAN wechseln“).", "Not the saved password: a new one can be tested in setup mode (\"Change Wi-Fi\").") \
  X(WifiNotFound,      "Netz nicht gefunden: ist es in Reichweite?", "Network not found: is it in range?") \
  X(WifiWrongPass,     "Falsches Passwort",                "Wrong password") \
  X(WifiNoConnection,  "Keine Verbindung: prüfe das Passwort und ob das Netz in Reichweite ist.", "No connection: check the password and that the network is in range.") \
  X(ApiNoSavedLink,    "Dort ist kein Link gespeichert.",  "No saved link there.") \
  X(CalBadAddress,     "Die Adresse ist nicht nutzbar.",   "The address can't be used.") \
  X(CalRefused,        "Zugriff verweigert (HTTP %d): nimm die geheime oder öffentliche iCal-Adresse.", "Access refused (HTTP %d): use the secret or public iCal address.") \
  X(CalNotFound,       "Nicht gefunden (HTTP 404): der Link ist falsch oder wurde zurückgesetzt.", "Not found (HTTP 404): the link is wrong or was reset.") \
  X(CalNoConnection,   "Keine Verbindung zum Server (Zertifikat- oder Netzwerkproblem).", "No connection to the server (certificate or network problem).") \
  X(CalHttp,           "Der Server antwortete mit HTTP %d.", "The server answered HTTP %d.") \
  X(CalNotIcal,        "Das ist kein iCal-Kalender (kein BEGIN:VCALENDAR): kopiere die iCal-Adresse.", "This is not an iCal calendar (no BEGIN:VCALENDAR): copy the iCal address.") \
  X(ApiPlacesNoAnswer, "Die Ortssuche antwortet nicht: versuche es noch einmal oder tippe den Ort ein.", "The place search didn't answer: try again or type the place.") \
  X(ApiNoMemory,       "Gerade zu wenig Speicher: versuche es noch einmal.", "Not enough memory right now: try again.") \
  X(ApiTasksNotSaved,  "Die Aufgaben konnten nicht gespeichert werden.", "The tasks could not be saved.") \
  X(ApiNoteNotSaved,   "Die Notiz konnte nicht gespeichert werden.", "The note could not be saved.") \
  X(ApiBdaysNotSaved,  "Die Geburtstage konnten nicht gespeichert werden.", "The birthdays could not be saved.") \
  X(ApiWrongCode,      "Falscher Code: gib die 6 Ziffern von der Anzeige ein.", "Wrong code: type the 6 digits shown on the display.") \
  X(ApiSaveWifiFirst,  "Speichere zuerst deine WLAN-Einstellungen.", "Save your Wi-Fi settings first.") \
  /* ---- personal data checks (src/personal/: Tasks.h, Birthdays.h, Notes.h) ---- */ \
  X(TasksUnreadable,   "Die Aufgabenliste konnte nicht gelesen werden.", "The task list could not be read.") \
  X(TasksTooMany,      "Höchstens %d Aufgaben.",           "At most %d tasks.") \
  X(TaskBadDate,       "Aufgabe „%.40s“: das Fälligkeitsdatum ist kein Datum.", "Task \"%.40s\": the due date is not a date.") \
  X(BdaysUnreadable,   "Die Geburtstagsliste konnte nicht gelesen werden.", "The birthday list could not be read.") \
  X(BdaysTooMany,      "Höchstens %d Geburtstage.",        "At most %d birthdays.") \
  X(BdayBadDate,       "Geburtstag von „%.32s“: das Datum ist nicht möglich.", "Birthday of \"%.32s\": the date is not possible.") \
  X(NoteUnreadable,    "Die Notiz konnte nicht gelesen werden.", "The note could not be read.") \
  X(NoteTooLong,       "Die Notiz ist zu lang: höchstens %u Zeichen.", "The note is too long: at most %u characters.")
// clang-format on

enum class S : uint16_t {
#define I18N_KEY(key, de, en) key,
  I18N_TEXTS(I18N_KEY)
#undef I18N_KEY
  Count
};

constexpr int TEXT_COUNT = static_cast<int>(S::Count);
constexpr S FIRST_APP_TEXT = S::ApiOffline;  // the texts before it are drawn on the screen

inline const char* text(S key, Lang lang) {
  static const char* const TABLE[TEXT_COUNT][2] = {
#define I18N_ROW(key, de, en) {de, en},
      I18N_TEXTS(I18N_ROW)
#undef I18N_ROW
  };
  int i = static_cast<int>(key);
  if (i < 0 || i >= TEXT_COUNT) return "";
  return TABLE[i][lang == Lang::En ? 1 : 0];
}

// The setting: "en" = English, anything else German (the default, also for older settings).
inline Lang parseLang(const char* code) { return code && std::strcmp(code, "en") == 0 ? Lang::En : Lang::De; }
inline const char* langCode(Lang lang) { return lang == Lang::En ? "en" : "de"; }

// German numbers use a decimal comma ("98,12"): replaces the points in a formatted number.
inline void localizeNumber(char* s, Lang lang) {
  if (lang != Lang::De) return;
  for (; *s; s++)
    if (*s == '.') *s = ',';
}

}  // namespace i18n
