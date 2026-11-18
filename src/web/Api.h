#pragma once
// The setup app's JSON API (docs/SETUP_API.md), served next to the setup pages.
class WebServer;

void registerApiRoutes(WebServer& server);
