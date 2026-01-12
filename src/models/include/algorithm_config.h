#pragma once

#include <string>
#include <vector>
#include <map>
#include <opencv2/opencv.hpp>
#include "image_utils.h"

namespace detector_service {

// ROI区域类型
enum class ROIType {
    RECTANGLE,  // 矩形
    POLYGON     // 多边形
};

// ROI区域结构
struct ROI {
    int id;
    ROIType type;
    std::string name;
    bool enabled;
    std::vector<cv::Point2f> points;  // 对于矩形，使用前两个点作为左上和右下
    
    ROI() : id(0), type(ROIType::RECTANGLE), enabled(true) {}
};

// 告警规则结构
struct AlertRule {
    int id;
    std::string name;
    bool enabled;
    std::vector<int> target_classes;  // 目标类别ID列表，空表示所有类别
    float min_confidence;              // 最小置信度阈值
    int min_count;                     // 最小检测数量（满足条件的检测框数量）
    int max_count;                     // 最大检测数量（超过此数量也告警，0表示不限制）
    int suppression_window_seconds;    // 告警抑制时间窗口（秒），相同告警在此时间内只触发一次
    std::vector<int> roi_ids;         // 关联的ROI ID列表，空表示全图
    
    AlertRule() : id(0), enabled(true), min_confidence(0.5f), 
                  min_count(1), max_count(0), suppression_window_seconds(60) {}
};

// 算法配置结构（每个通道一个）
struct AlgorithmConfig {
    int channel_id;
    std::string model_path;              // 模型文件路径
    float conf_threshold;                // 置信度阈值
    float nms_threshold;                 // NMS阈值
    int input_width;                     // 输入宽度
    int input_height;                    // 输入高度
    int detection_interval;              // 检测间隔（每N帧检测一次）
    std::vector<int> enabled_classes;    // 启用的类别ID列表，空表示所有类别
    std::vector<ROI> rois;               // ROI区域列表
    std::vector<AlertRule> alert_rules;  // 告警规则列表
    std::string created_at;
    std::string updated_at;
    
    AlgorithmConfig() : channel_id(0), 
                       model_path("yolov11n.onnx"),
                       conf_threshold(0.65f),
                       nms_threshold(0.45f),
                       input_width(640),
                       input_height(640),
                       detection_interval(3) {}
};

// 算法配置管理器
class AlgorithmConfigManager {
public:
    static AlgorithmConfigManager& getInstance() {
        static AlgorithmConfigManager instance;
        return instance;
    }

    // 获取通道的算法配置
    bool getAlgorithmConfig(int channel_id, AlgorithmConfig& config);
    
    // 保存通道的算法配置
    bool saveAlgorithmConfig(const AlgorithmConfig& config);
    
    // 删除通道的算法配置
    bool deleteAlgorithmConfig(int channel_id);
    
    // 获取默认算法配置
    AlgorithmConfig getDefaultConfig(int channel_id);
    
    // 验证配置有效性
    bool validateConfig(const AlgorithmConfig& config, std::string& error_msg);
    
    // 检查点是否在ROI内
    // frame_width和frame_height用于将归一化的ROI坐标转换为像素坐标
    static bool isPointInROI(const cv::Point2f& point, const ROI& roi, int frame_width, int frame_height);
    
    // 检查检测框是否与ROI相交
    // frame_width和frame_height用于将归一化的ROI坐标转换为像素坐标
    static bool isDetectionInROI(const cv::Rect& bbox, const ROI& roi, int frame_width, int frame_height);
    
    // 评估告警规则：检查检测结果是否满足告警规则条件
    // 返回满足条件的检测结果列表
    // frame_width和frame_height用于将归一化的ROI坐标转换为像素坐标
    static std::vector<Detection> evaluateAlertRule(
        const AlertRule& rule,
        const std::vector<Detection>& detections,
        const std::vector<ROI>& rois,
        int frame_width,
        int frame_height);
    
    // 检查告警规则是否应该触发（考虑所有条件）
    // frame_width和frame_height用于将归一化的ROI坐标转换为像素坐标
    static bool shouldTriggerAlert(
        const AlertRule& rule,
        const std::vector<Detection>& detections,
        const std::vector<ROI>& rois,
        int frame_width,
        int frame_height);

private:
    AlgorithmConfigManager() = default;
    ~AlgorithmConfigManager() = default;
    AlgorithmConfigManager(const AlgorithmConfigManager&) = delete;
    AlgorithmConfigManager& operator=(const AlgorithmConfigManager&) = delete;
};

} // namespace detector_service

