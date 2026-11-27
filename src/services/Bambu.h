#pragma once
// A Bambu printer in the local network (MQTT over TLS), stage 7b own code: a short connection per
// wake (connect, ask for the full status, read it, disconnect). The report is read by PrinterReport.h.
#include "../../app.h"
#include "../widgets/Widgets.h"
#include "PrinterReport.h"

extern bambu::PrinterStatus printer;  // the last full report
extern bool isPrinting;               // RTC memory: printing (also preparing, paused)
extern bool previousIsPrinting;       // RTC memory: the state before this wake's report
extern bool printViewOnScreen;        // RTC memory: the printer view (not MakerWorld) is drawn
extern bool bambuAuthRejected;        // the last connection was refused: wrong LAN access code
// RTC memory (stage 8): the printer didn't answer twice in a row (usually: switched off at its switch).
// No warning then; its box goes to a neighbour (Registry.cpp) until it answers again.
extern bool printerOff;

// The printer view stays this long after a print ends, then MakerWorld's counters come back.
constexpr long PRINT_FINISHED_HOLD_S = 5 * 60;

// Printing, or within PRINT_FINISHED_HOLD_S after the end of a print.
bool showPrintView();

// Reads the printer's status (mqtt_ip, mqtt_sn, mqtt_pass, mqtt_port). False without a full report.
bool fetchBambu(LayoutItem* item);
// The printer widget (layout id 64) while printing and shortly after.
void bambuWidget(LayoutItem* item);
