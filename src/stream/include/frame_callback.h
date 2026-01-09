#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include "image_utils.h"

namespace detector_service {

// 处理帧的回调函数
void processFrameCallback(int channel_id, const cv::Mat& frame, 
                         const std::vector<Detection>& detections);

} // namespace detector_service

