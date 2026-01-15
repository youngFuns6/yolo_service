#include "algorithm_config.h"
#include "database.h"
#include "utils/include/image_utils.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace detector_service {

bool AlgorithmConfigManager::getAlgorithmConfig(int channel_id, AlgorithmConfig& config) {
    auto& db = Database::getInstance();
    sqlite3* db_handle = db.getDb();
    
    if (!db_handle) {
        std::cerr << "数据库未初始化" << std::endl;
        return false;
    }
    
    // 从数据库加载配置
    std::string sql = R"(
        SELECT model_path, conf_threshold, nms_threshold,
               input_width, input_height, detection_interval, enabled_classes,
               rois_json, alert_rules_json,
               created_at, updated_at
        FROM algorithm_configs
        WHERE channel_id = ?
    )";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_handle) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, channel_id);
    
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        config.channel_id = channel_id;
        config.model_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        config.conf_threshold = sqlite3_column_double(stmt, 1);
        config.nms_threshold = sqlite3_column_double(stmt, 2);
        config.input_width = sqlite3_column_int(stmt, 3);
        config.input_height = sqlite3_column_int(stmt, 4);
        config.detection_interval = sqlite3_column_int(stmt, 5);
        
        // 解析 enabled_classes (JSON数组字符串)
        const char* classes_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (classes_json && strlen(classes_json) > 0) {
            // 简单解析JSON数组，格式: [1,2,3]
            std::string classes_str(classes_json);
            classes_str.erase(0, 1); // 移除 '['
            classes_str.erase(classes_str.length() - 1); // 移除 ']'
            std::istringstream iss(classes_str);
            std::string item;
            while (std::getline(iss, item, ',')) {
                int class_id = std::stoi(item);
                config.enabled_classes.push_back(class_id);
            }
        }
        
        // 解析 ROIs JSON
        // 注意：ROI坐标在数据库中存储为归一化坐标（0-1之间）
        const char* rois_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        if (rois_json && strlen(rois_json) > 0) {
            try {
                nlohmann::json rois_data = nlohmann::json::parse(rois_json);
                for (const auto& roi_data : rois_data) {
                    ROI roi;
                    roi.id = roi_data.value("id", 0);
                    roi.type = (roi_data.value("type", "RECTANGLE") == "POLYGON") 
                               ? ROIType::POLYGON : ROIType::RECTANGLE;
                    roi.name = roi_data.value("name", "");
                    roi.enabled = roi_data.value("enabled", true);
                    if (roi_data.contains("points")) {
                        for (const auto& point_data : roi_data["points"]) {
                            cv::Point2f point;
                            // 从数据库读取的是归一化坐标（0-1之间）
                            point.x = point_data.value("x", 0.0f);
                            point.y = point_data.value("y", 0.0f);
                            roi.points.push_back(point);
                        }
                    }
                    config.rois.push_back(roi);
                }
            } catch (const std::exception& e) {
                std::cerr << "解析ROIs JSON失败: " << e.what() << std::endl;
            }
        }
        
        // 解析 AlertRules JSON
        const char* alert_rules_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        if (alert_rules_json && strlen(alert_rules_json) > 0) {
            try {
                nlohmann::json rules_data = nlohmann::json::parse(alert_rules_json);
                for (const auto& rule_data : rules_data) {
                    AlertRule rule;
                    rule.id = rule_data.value("id", 0);
                    rule.name = rule_data.value("name", "");
                    rule.enabled = rule_data.value("enabled", true);
                    if (rule_data.contains("target_classes")) {
                        for (const auto& class_id : rule_data["target_classes"]) {
                            rule.target_classes.push_back(class_id);
                        }
                    }
                    rule.min_confidence = rule_data.value("min_confidence", 0.5f);
                    rule.min_count = rule_data.value("min_count", 1);
                    rule.max_count = rule_data.value("max_count", 0);
                    rule.suppression_window_seconds = rule_data.value("suppression_window_seconds", 60);
                    if (rule_data.contains("roi_ids")) {
                        for (const auto& roi_id : rule_data["roi_ids"]) {
                            rule.roi_ids.push_back(roi_id);
                        }
                    }
                    config.alert_rules.push_back(rule);
                }
            } catch (const std::exception& e) {
                std::cerr << "解析AlertRules JSON失败: " << e.what() << std::endl;
            }
        }
        
        config.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        config.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        
        found = true;
    }
    
    sqlite3_finalize(stmt);
    
    if (!found) {
        // 如果不存在，返回默认配置
        config = getDefaultConfig(channel_id);
    }
    
    return true;
}

bool AlgorithmConfigManager::saveAlgorithmConfig(const AlgorithmConfig& config) {
    std::string error_msg;
    if (!validateConfig(config, error_msg)) {
        std::cerr << "配置验证失败: " << error_msg << std::endl;
        return false;
    }
    
    auto& db = Database::getInstance();
    sqlite3* db_handle = db.getDb();
    
    if (!db_handle) {
        std::cerr << "数据库未初始化" << std::endl;
        return false;
    }
    
    // 序列化 enabled_classes 为JSON数组字符串
    std::ostringstream classes_oss;
    classes_oss << "[";
    for (size_t i = 0; i < config.enabled_classes.size(); i++) {
        if (i > 0) classes_oss << ",";
        classes_oss << config.enabled_classes[i];
    }
    classes_oss << "]";
    std::string classes_json = classes_oss.str();
    
    // 序列化 ROIs 为JSON
    // 注意：ROI坐标需要归一化后存储（0-1之间），使用input_width和input_height作为参考尺寸
    nlohmann::json rois_data = nlohmann::json::array();
    float ref_width = static_cast<float>(config.input_width);
    float ref_height = static_cast<float>(config.input_height);
    
    for (const auto& roi : config.rois) {
        nlohmann::json roi_data;
        roi_data["id"] = roi.id;
        roi_data["type"] = (roi.type == ROIType::RECTANGLE) ? "RECTANGLE" : "POLYGON";
        roi_data["name"] = roi.name;
        roi_data["enabled"] = roi.enabled;
        nlohmann::json points_array = nlohmann::json::array();
        for (const auto& point : roi.points) {
            nlohmann::json point_data;
            // 将像素坐标归一化（除以参考尺寸）
            // 如果坐标已经在0-1之间，说明已经是归一化的，直接存储
            // 否则，假设是像素坐标，进行归一化
            float norm_x = point.x;
            float norm_y = point.y;
            if (point.x > 1.0f || point.y > 1.0f) {
                // 如果坐标大于1，假设是像素坐标，进行归一化
                norm_x = point.x / ref_width;
                norm_y = point.y / ref_height;
            }
            point_data["x"] = norm_x;
            point_data["y"] = norm_y;
            points_array.push_back(point_data);
        }
        roi_data["points"] = points_array;
        rois_data.push_back(roi_data);
    }
    std::string rois_json = rois_data.dump();
    
    // 序列化 AlertRules 为JSON
    nlohmann::json alert_rules_data = nlohmann::json::array();
    for (const auto& rule : config.alert_rules) {
        nlohmann::json rule_data;
        rule_data["id"] = rule.id;
        rule_data["name"] = rule.name;
        rule_data["enabled"] = rule.enabled;
        rule_data["target_classes"] = rule.target_classes;
        rule_data["min_confidence"] = rule.min_confidence;
        rule_data["min_count"] = rule.min_count;
        rule_data["max_count"] = rule.max_count;
        rule_data["suppression_window_seconds"] = rule.suppression_window_seconds;
        rule_data["roi_ids"] = rule.roi_ids;
        alert_rules_data.push_back(rule_data);
    }
    std::string alert_rules_json = alert_rules_data.dump();
    
    std::string sql = R"(
        INSERT OR REPLACE INTO algorithm_configs 
        (channel_id, model_path, conf_threshold, nms_threshold,
         input_width, input_height, detection_interval, enabled_classes,
         rois_json, alert_rules_json, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 
                COALESCE((SELECT created_at FROM algorithm_configs WHERE channel_id = ?), datetime('now')),
                datetime('now'))
    )";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_handle) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, config.channel_id);
    sqlite3_bind_text(stmt, 2, config.model_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, config.conf_threshold);
    sqlite3_bind_double(stmt, 4, config.nms_threshold);
    sqlite3_bind_int(stmt, 5, config.input_width);
    sqlite3_bind_int(stmt, 6, config.input_height);
    sqlite3_bind_int(stmt, 7, config.detection_interval);
    sqlite3_bind_text(stmt, 8, classes_json.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, rois_json.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, alert_rules_json.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 11, config.channel_id);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return success;
}

bool AlgorithmConfigManager::deleteAlgorithmConfig(int channel_id) {
    auto& db = Database::getInstance();
    sqlite3* db_handle = db.getDb();
    
    if (!db_handle) {
        std::cerr << "数据库未初始化" << std::endl;
        return false;
    }
    
    std::string sql = "DELETE FROM algorithm_configs WHERE channel_id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_handle) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, channel_id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return success;
}

AlgorithmConfig AlgorithmConfigManager::getDefaultConfig(int channel_id) {
    AlgorithmConfig config;
    config.channel_id = channel_id;
    config.model_path = "yolov11n.onnx";
    config.conf_threshold = 0.65f;
    config.nms_threshold = 0.45f;
    config.input_width = 640;
    config.input_height = 640;
    config.detection_interval = 3;
    // enabled_classes 为空表示所有类别
    return config;
}

bool AlgorithmConfigManager::validateConfig(const AlgorithmConfig& config, std::string& error_msg) {
    if (config.channel_id <= 0) {
        error_msg = "通道ID必须大于0";
        return false;
    }
    
    if (config.model_path.empty()) {
        error_msg = "模型路径不能为空";
        return false;
    }
    
    if (config.conf_threshold < 0.0f || config.conf_threshold > 1.0f) {
        error_msg = "置信度阈值必须在0-1之间";
        return false;
    }
    
    if (config.nms_threshold < 0.0f || config.nms_threshold > 1.0f) {
        error_msg = "NMS阈值必须在0-1之间";
        return false;
    }
    
    if (config.input_width <= 0 || config.input_height <= 0) {
        error_msg = "输入尺寸必须大于0";
        return false;
    }
    
    if (config.detection_interval < 1) {
        error_msg = "检测间隔必须大于等于1";
        return false;
    }
    
    return true;
}

bool AlgorithmConfigManager::isPointInROI(const cv::Point2f& point, const ROI& roi, int frame_width, int frame_height) {
    if (!roi.enabled) {
        return false;
    }
    
    if (roi.points.empty()) {
        return false;
    }
    
    // 将归一化的ROI坐标转换为当前帧的像素坐标
    float scale_x = static_cast<float>(frame_width);
    float scale_y = static_cast<float>(frame_height);
    
    if (roi.type == ROIType::RECTANGLE) {
        if (roi.points.size() < 2) return false;
        // 将归一化坐标转换为像素坐标
        cv::Point2f top_left(roi.points[0].x * scale_x, roi.points[0].y * scale_y);
        cv::Point2f bottom_right(roi.points[1].x * scale_x, roi.points[1].y * scale_y);
        return point.x >= top_left.x && point.x <= bottom_right.x &&
               point.y >= top_left.y && point.y <= bottom_right.y;
    } else if (roi.type == ROIType::POLYGON) {
        if (roi.points.size() < 3) return false;
        // 使用射线法判断点是否在多边形内
        // 先将归一化的多边形坐标转换为像素坐标
        std::vector<cv::Point2f> pixel_points;
        for (const auto& norm_point : roi.points) {
            pixel_points.push_back(cv::Point2f(norm_point.x * scale_x, norm_point.y * scale_y));
        }
        
        int intersections = 0;
        for (size_t i = 0, j = pixel_points.size() - 1; i < pixel_points.size(); j = i++) {
            const cv::Point2f& p1 = pixel_points[i];
            const cv::Point2f& p2 = pixel_points[j];
            
            if (((p1.y > point.y) != (p2.y > point.y)) &&
                (point.x < (p2.x - p1.x) * (point.y - p1.y) / (p2.y - p1.y) + p1.x)) {
                intersections++;
            }
        }
        return (intersections % 2) == 1;
    }
    
    return false;
}

bool AlgorithmConfigManager::isDetectionInROI(const cv::Rect& bbox, const ROI& roi, int frame_width, int frame_height) {
    if (!roi.enabled) {
        return false;
    }
    
    // 检查检测框的中心点或任意角点是否在ROI内
    cv::Point2f center(bbox.x + bbox.width / 2.0f, bbox.y + bbox.height / 2.0f);
    return isPointInROI(center, roi, frame_width, frame_height);
}

std::vector<Detection> AlgorithmConfigManager::evaluateAlertRule(
    const AlertRule& rule,
    const std::vector<Detection>& detections,
    const std::vector<ROI>& rois,
    int frame_width,
    int frame_height) {
    std::vector<Detection> matched_detections;
    
    if (!rule.enabled) {
        return matched_detections;
    }
    
    for (const auto& detection : detections) {
        // 检查类别过滤
        if (!rule.target_classes.empty()) {
            bool class_matched = false;
            for (int target_class : rule.target_classes) {
                if (detection.class_id == target_class) {
                    class_matched = true;
                    break;
                }
            }
            if (!class_matched) {
                continue;
            }
        }
        
        // 检查置信度阈值
        if (detection.confidence < rule.min_confidence) {
            continue;
        }
        
        // 检查ROI过滤
        if (!rule.roi_ids.empty()) {
            bool in_roi = false;
            for (int roi_id : rule.roi_ids) {
                // 查找对应的ROI
                for (const auto& roi : rois) {
                    if (roi.id == roi_id && isDetectionInROI(detection.bbox, roi, frame_width, frame_height)) {
                        in_roi = true;
                        break;
                    }
                }
                if (in_roi) break;
            }
            if (!in_roi) {
                continue;
            }
        } else {
            // 如果roi_ids为空，表示全图检测，不需要ROI过滤
        }
        
        matched_detections.push_back(detection);
    }
    
    return matched_detections;
}

bool AlgorithmConfigManager::shouldTriggerAlert(
    const AlertRule& rule,
    const std::vector<Detection>& detections,
    const std::vector<ROI>& rois,
    int frame_width,
    int frame_height) {
    if (!rule.enabled) {
        return false;
    }
    
    // 评估规则，获取满足条件的检测结果
    std::vector<Detection> matched = evaluateAlertRule(rule, detections, rois, frame_width, frame_height);
    
    if (matched.empty()) {
        return false;
    }
    
    // 检查数量条件
    int count = static_cast<int>(matched.size());
    
    // 检查最小数量
    if (count < rule.min_count) {
        return false;
    }
    
    // 检查最大数量（如果设置了）
    if (rule.max_count > 0 && count > rule.max_count) {
        return true;  // 超过最大数量也触发告警
    }
    
    // 满足最小数量条件
    return true;
}

} // namespace detector_service

