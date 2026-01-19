#pragma once

#include <httplib.h>

namespace detector_service {

void setupAlertRoutes(httplib::Server& svr);

} // namespace detector_service

