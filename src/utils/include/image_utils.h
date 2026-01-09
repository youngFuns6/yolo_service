#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace detector_service {

struct Detection {
    int class_id;
    std::string class_name;
    float confidence;
    cv::Rect bbox;
};

class ImageUtils {
public:
    // 将 OpenCV Mat 转换为 base64 字符串
    // quality: JPEG 质量 (0-100)，默认 90
    static std::string matToBase64(const cv::Mat& image, const std::string& format = ".jpg", int quality = 90);
    
    // 将 base64 字符串转换为 OpenCV Mat
    static cv::Mat base64ToMat(const std::string& base64_string);
    
    // 在图像上绘制检测框和标签
    static cv::Mat drawDetections(const cv::Mat& image, const std::vector<Detection>& detections);
    
    // 调整图像大小
    static cv::Mat resizeImage(const cv::Mat& image, int width, int height);
    
    // 保存图像到文件
    static bool saveImage(const cv::Mat& image, const std::string& filepath);
};

} // namespace detector_service

