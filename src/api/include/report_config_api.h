#pragma once

#include <httplib.h>

namespace detector_service {

void setupReportConfigRoutes(httplib::Server& svr);

} // namespace detector_service

