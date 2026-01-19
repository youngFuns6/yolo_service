#pragma once

#include <httplib.h>

namespace detector_service {

void setupAlgorithmConfigRoutes(httplib::Server& svr);

} // namespace detector_service

