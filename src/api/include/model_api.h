#pragma once

#include <httplib.h>

namespace detector_service {

void setupModelRoutes(httplib::Server& svr);

} // namespace detector_service

