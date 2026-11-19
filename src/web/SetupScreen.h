#pragma once
// The e-paper screens of setup mode (stage 5): how to reach the setup app.

// Hotspot: network name, password, a Wi-Fi QR code (join by camera), the address.
void drawSetupScreenAp(const char* ssid, const char* password);
// Home network: the address (name.local and IP), a QR code of it, and the code to type in.
void drawSetupScreenHome(const char* hostName, const char* ip, const char* code);

// Draws a QR code of `text` (top-left x, y) no larger than maxSize pixels, quiet zone included.
// Returns the size drawn, 0 if the text doesn't fit.
int drawQrCode(const char* text, int x, int y, int maxSize);
