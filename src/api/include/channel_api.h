#pragma once

#include <crow.h>
#include <memory>
#include "channel.h"

namespace detector_service {

class YOLOv11Detector;
class StreamManager;

void setupChannelRoutes(crow::SimpleApp& app, 
                       std::shared_ptr<YOLOv11Detector> detector,
                       StreamManager* stream_manager);

} // namespace detector_service

