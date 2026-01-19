#pragma once

#include <httplib.h>

namespace detector_service {

void setupWebSocketRoutes(httplib::Server& svr);

} // namespace detector_service

