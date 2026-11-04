// Wi-Fi: remembers several known networks, joins the strongest one in range,
// and falls back to its own hotspot (with the scope page) if none work.
#pragma once
#include <WebServer.h>

namespace wifi_net {

void begin(WebServer &server);
void loop();
bool apMode();

}  // namespace wifi_net
