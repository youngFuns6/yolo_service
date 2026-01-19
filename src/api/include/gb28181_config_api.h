#pragma once

#include <httplib.h>

namespace detector_service {

void setupGB28181ConfigRoutes(httplib::Server& svr);

} // namespace detector_service

