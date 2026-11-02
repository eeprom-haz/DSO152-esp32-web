// Background CSV logging (LittleFS -> FTP) and optional MQTT publishing.
#pragma once
#include <WebServer.h>

#include "frame.h"

namespace logger {

void begin(WebServer &http);
void loop();
void onFrame(const Frame &f);
bool wantsScope();

}  // namespace logger
