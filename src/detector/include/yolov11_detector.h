#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <memory>
#include "image_utils.h"

namespace detector_service {

class YOLOv11Detector {
public:
    YOLOv11Detector(const std::string& model_path, 
                    float conf_threshold = 0.5f,
                    float nms_threshold = 0.4f,
                    int input_width = 640,
                    int input_height = 640);
    
    ~YOLOv11Detector();
    
    bool initialize();
    std::vector<Detection> detect(const cv::Mat& image);
    cv::Mat processFrame(const cv::Mat& frame);

private:
    std::string model_path_;
    float conf_threshold_;
    float nms_threshold_;
    int input_width_;
    int input_height_;
    
    Ort::Env env_;
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;
    
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<std::vector<int64_t>> input_shapes_;
    std::vector<std::vector<int64_t>> output_shapes_;
    
    std::vector<std::string> class_names_;
    
    bool loadModel();
    cv::Mat preprocess(const cv::Mat& image, float& scale, int& pad_x, int& pad_y);
    std::vector<Detection> postprocess(const std::vector<float>& output, 
                                      const cv::Size& original_size,
                                      const std::vector<int64_t>& output_shape,
                                      float scale, int pad_x, int pad_y);
    std::vector<Detection> applyNMS(const std::vector<Detection>& detections);
    void loadClassNames();
};

} // namespace detector_service

