#pragma once

#include <httplib.h>
#include <memory>
#include "channel.h"

namespace detector_service {

class YOLOv11Detector;
class StreamManager;

void setupChannelRoutes(httplib::Server& svr, 
                       std::shared_ptr<YOLOv11Detector> detector,
                       StreamManager* stream_manager);

} // namespace detector_service

