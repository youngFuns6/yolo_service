#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <memory>
#include "image_utils.h"
#include "algorithm_config.h"
#include "onnx_env_singleton.h"

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
    
    // 动态配置更新
    void updateConfThreshold(float threshold) { conf_threshold_ = threshold; }
    void updateNmsThreshold(float threshold) { nms_threshold_ = threshold; }
    float getConfThreshold() const { return conf_threshold_; }
    float getNmsThreshold() const { return nms_threshold_; }
    
    // 应用过滤（类别、ROI等）
    // frame_width和frame_height用于将归一化的ROI坐标转换为像素坐标
    std::vector<Detection> applyFilters(const std::vector<Detection>& detections,
                                       const std::vector<int>& enabled_classes = {},
                                       const std::vector<ROI>& rois = {},
                                       int frame_width = 0,
                                       int frame_height = 0);
    
    // 获取类别名称列表
    const std::vector<std::string>& getClassNames() const { return class_names_; }

private:
    std::string model_path_;
    float conf_threshold_;
    float nms_threshold_;
    int input_width_;
    int input_height_;
    
    Ort::Env& env_;  // 使用单例引用，避免重复创建导致schema注册错误
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

