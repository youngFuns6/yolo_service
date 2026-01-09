#include "image_utils.h"
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace detector_service {

std::string ImageUtils::matToBase64(const cv::Mat& image, const std::string& format, int quality) {
    std::vector<uchar> buffer;
    std::vector<int> params;
    
    if (format == ".jpg" || format == ".jpeg") {
        // 限制质量范围在 0-100
        int jpeg_quality = std::max(0, std::min(100, quality));
        params = {cv::IMWRITE_JPEG_QUALITY, jpeg_quality};
    } else if (format == ".png") {
        params = {cv::IMWRITE_PNG_COMPRESSION, 3};
    }
    
    cv::imencode(format, image, buffer, params);
    
    // Base64 编码
    const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    result.reserve((buffer.size() + 2) / 3 * 4);
    
    for (size_t i = 0; i < buffer.size(); i += 3) {
        uint32_t value = 0;
        int bits = 0;
        
        for (int j = 0; j < 3 && i + j < buffer.size(); j++) {
            value |= static_cast<uint32_t>(buffer[i + j]) << (16 - j * 8);
            bits += 8;
        }
        
        for (int j = 0; j < 4; j++) {
            if (j * 6 < bits) {
                int index = (value >> (18 - j * 6)) & 0x3F;
                result += base64_chars[index];
            } else {
                result += '=';
            }
        }
    }
    
    return result;
}

cv::Mat ImageUtils::base64ToMat(const std::string& base64_string) {
    // Base64 解码
    const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::vector<uchar> buffer;
    buffer.reserve(base64_string.size() * 3 / 4);
    
    uint32_t value = 0;
    int bits = 0;
    
    for (char c : base64_string) {
        if (c == '=') break;
        
        size_t index = base64_chars.find(c);
        if (index == std::string::npos) continue;
        
        value = (value << 6) | index;
        bits += 6;
        
        if (bits >= 8) {
            buffer.push_back(static_cast<uchar>((value >> (bits - 8)) & 0xFF));
            bits -= 8;
        }
    }
    
    if (buffer.empty()) {
        return cv::Mat();
    }
    
    cv::Mat image = cv::imdecode(buffer, cv::IMREAD_COLOR);
    return image;
}

cv::Mat ImageUtils::drawDetections(const cv::Mat& image, const std::vector<Detection>& detections) {
    cv::Mat result = image.clone();
    
    for (const auto& det : detections) {
        // 绘制边界框
        cv::rectangle(result, det.bbox, cv::Scalar(0, 255, 0), 2);
        
        // 准备标签文本
        std::ostringstream label;
        label << det.class_name << " " << std::fixed << std::setprecision(2) << det.confidence;
        
        // 计算文本大小
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label.str(), cv::FONT_HERSHEY_SIMPLEX, 
                                              0.5, 1, &baseline);
        
        // 绘制标签背景
        cv::Rect label_bg(det.bbox.x, det.bbox.y - text_size.height - 10,
                         text_size.width + 10, text_size.height + 10);
        cv::rectangle(result, label_bg, cv::Scalar(0, 255, 0), -1);
        
        // 绘制标签文本
        cv::putText(result, label.str(), 
                   cv::Point(det.bbox.x + 5, det.bbox.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }
    
    return result;
}

cv::Mat ImageUtils::resizeImage(const cv::Mat& image, int width, int height) {
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(width, height));
    return resized;
}

bool ImageUtils::saveImage(const cv::Mat& image, const std::string& filepath) {
    return cv::imwrite(filepath, image);
}

} // namespace detector_service

