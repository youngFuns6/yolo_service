#include "algorithm_config_api.h"
#include "algorithm_config.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>

namespace detector_service {

void setupAlgorithmConfigRoutes(crow::SimpleApp& app) {
    auto& config_manager = AlgorithmConfigManager::getInstance();
    
    // 获取通道的算法配置
    CROW_ROUTE(app, "/api/algorithm-configs/<int>").methods("GET"_method)
    ([](int channel_id) {
        AlgorithmConfig config;
        auto& config_manager = AlgorithmConfigManager::getInstance();
        
        if (!config_manager.getAlgorithmConfig(channel_id, config)) {
            crow::json::wvalue response;
            response["success"] = false;
            response["error"] = "获取配置失败";
            return crow::response(500, response);
        }
        
        crow::json::wvalue response;
        response["success"] = true;
        response["data"]["channel_id"] = config.channel_id;
        response["data"]["model_path"] = config.model_path;
        response["data"]["conf_threshold"] = config.conf_threshold;
        response["data"]["nms_threshold"] = config.nms_threshold;
        response["data"]["input_width"] = config.input_width;
        response["data"]["input_height"] = config.input_height;
        response["data"]["detection_interval"] = config.detection_interval;
        
        // 序列化 enabled_classes
        crow::json::wvalue classes_array;
        for (size_t i = 0; i < config.enabled_classes.size(); i++) {
            classes_array[i] = config.enabled_classes[i];
        }
        response["data"]["enabled_classes"] = std::move(classes_array);
        
        // 序列化 ROIs
        crow::json::wvalue rois_array;
        for (size_t i = 0; i < config.rois.size(); i++) {
            const auto& roi = config.rois[i];
            rois_array[i]["id"] = roi.id;
            rois_array[i]["type"] = (roi.type == ROIType::RECTANGLE) ? "RECTANGLE" : "POLYGON";
            rois_array[i]["name"] = roi.name;
            rois_array[i]["enabled"] = roi.enabled;
            crow::json::wvalue points_array;
            for (size_t j = 0; j < roi.points.size(); j++) {
                points_array[j]["x"] = roi.points[j].x;
                points_array[j]["y"] = roi.points[j].y;
            }
            rois_array[i]["points"] = std::move(points_array);
        }
        response["data"]["rois"] = std::move(rois_array);
        
        // 序列化 AlertRules
        crow::json::wvalue rules_array;
        for (size_t i = 0; i < config.alert_rules.size(); i++) {
            const auto& rule = config.alert_rules[i];
            rules_array[i]["id"] = rule.id;
            rules_array[i]["name"] = rule.name;
            rules_array[i]["enabled"] = rule.enabled;
            crow::json::wvalue target_classes_array;
            for (size_t j = 0; j < rule.target_classes.size(); j++) {
                target_classes_array[j] = rule.target_classes[j];
            }
            rules_array[i]["target_classes"] = std::move(target_classes_array);
            rules_array[i]["min_confidence"] = rule.min_confidence;
            rules_array[i]["min_count"] = rule.min_count;
            rules_array[i]["max_count"] = rule.max_count;
            rules_array[i]["suppression_window_seconds"] = rule.suppression_window_seconds;
            crow::json::wvalue roi_ids_array;
            for (size_t j = 0; j < rule.roi_ids.size(); j++) {
                roi_ids_array[j] = rule.roi_ids[j];
            }
            rules_array[i]["roi_ids"] = std::move(roi_ids_array);
        }
        response["data"]["alert_rules"] = std::move(rules_array);
        
        response["data"]["created_at"] = config.created_at;
        response["data"]["updated_at"] = config.updated_at;
        
        return crow::response(200, response);
    });
    
    // 保存通道的算法配置
    CROW_ROUTE(app, "/api/algorithm-configs/<int>").methods("PUT"_method)
    ([](const crow::request& req, int channel_id) {
        try {
            auto json_body = crow::json::load(req.body);
            if (!json_body) {
                crow::json::wvalue response;
                response["success"] = false;
                response["error"] = "Invalid JSON";
                return crow::response(400, response);
            }
            
            AlgorithmConfig config;
            config.channel_id = channel_id;
            
            if (json_body.has("model_path")) {
                config.model_path = json_body["model_path"].s();
            }
            if (json_body.has("conf_threshold")) {
                config.conf_threshold = static_cast<float>(json_body["conf_threshold"].d());
            }
            if (json_body.has("nms_threshold")) {
                config.nms_threshold = static_cast<float>(json_body["nms_threshold"].d());
            }
            if (json_body.has("input_width")) {
                config.input_width = json_body["input_width"].i();
            }
            if (json_body.has("input_height")) {
                config.input_height = json_body["input_height"].i();
            }
            if (json_body.has("detection_interval")) {
                config.detection_interval = json_body["detection_interval"].i();
            }
            
            // 解析 enabled_classes
            if (json_body.has("enabled_classes")) {
                auto classes_array = json_body["enabled_classes"];
                for (size_t i = 0; i < classes_array.size(); i++) {
                    config.enabled_classes.push_back(classes_array[i].i());
                }
            }
            
            // 解析 ROIs
            // 注意：如果前端传入的坐标大于1，说明是像素坐标，需要归一化
            // 否则，假设已经是归一化坐标（0-1之间）
            if (json_body.has("rois")) {
                auto rois_array = json_body["rois"];
                float ref_width = static_cast<float>(config.input_width);
                float ref_height = static_cast<float>(config.input_height);
                
                for (size_t i = 0; i < rois_array.size(); i++) {
                    ROI roi;
                    roi.id = rois_array[i].has("id") ? rois_array[i]["id"].i() : static_cast<int>(i);
                    roi.type = (rois_array[i].has("type") && rois_array[i]["type"].s() == "POLYGON") 
                               ? ROIType::POLYGON : ROIType::RECTANGLE;
                    roi.name = rois_array[i].has("name") ? rois_array[i]["name"].s() : std::string("");
                    roi.enabled = rois_array[i].has("enabled") ? rois_array[i]["enabled"].b() : true;
                    if (rois_array[i].has("points")) {
                        auto points_array = rois_array[i]["points"];
                        for (size_t j = 0; j < points_array.size(); j++) {
                            cv::Point2f point;
                            float x = points_array[j].has("x") ? static_cast<float>(points_array[j]["x"].d()) : 0.0f;
                            float y = points_array[j].has("y") ? static_cast<float>(points_array[j]["y"].d()) : 0.0f;
                            
                            // 如果坐标大于1，假设是像素坐标，进行归一化
                            // 否则，假设已经是归一化坐标，直接使用
                            if (x > 1.0f || y > 1.0f) {
                                point.x = x / ref_width;
                                point.y = y / ref_height;
                            } else {
                                point.x = x;
                                point.y = y;
                            }
                            
                            // 确保归一化坐标在0-1范围内
                            point.x = std::max(0.0f, std::min(1.0f, point.x));
                            point.y = std::max(0.0f, std::min(1.0f, point.y));
                            
                            roi.points.push_back(point);
                        }
                    }
                    config.rois.push_back(roi);
                }
            }
            
            // 解析 AlertRules
            if (json_body.has("alert_rules")) {
                auto rules_array = json_body["alert_rules"];
                for (size_t i = 0; i < rules_array.size(); i++) {
                    AlertRule rule;
                    rule.id = rules_array[i].has("id") ? rules_array[i]["id"].i() : static_cast<int>(i);
                    rule.name = rules_array[i].has("name") ? rules_array[i]["name"].s() : std::string("");
                    rule.enabled = rules_array[i].has("enabled") ? rules_array[i]["enabled"].b() : true;
                    if (rules_array[i].has("target_classes")) {
                        auto target_classes_array = rules_array[i]["target_classes"];
                        for (size_t j = 0; j < target_classes_array.size(); j++) {
                            rule.target_classes.push_back(target_classes_array[j].i());
                        }
                    }
                    rule.min_confidence = rules_array[i].has("min_confidence") 
                                         ? static_cast<float>(rules_array[i]["min_confidence"].d()) : 0.5f;
                    rule.min_count = rules_array[i].has("min_count") 
                                    ? rules_array[i]["min_count"].i() : 1;
                    rule.max_count = rules_array[i].has("max_count") 
                                    ? rules_array[i]["max_count"].i() : 0;
                    rule.suppression_window_seconds = rules_array[i].has("suppression_window_seconds") 
                                                     ? rules_array[i]["suppression_window_seconds"].i() : 60;
                    if (rules_array[i].has("roi_ids")) {
                        auto roi_ids_array = rules_array[i]["roi_ids"];
                        for (size_t j = 0; j < roi_ids_array.size(); j++) {
                            rule.roi_ids.push_back(roi_ids_array[j].i());
                        }
                    }
                    config.alert_rules.push_back(rule);
                }
            }
            
            auto& config_manager = AlgorithmConfigManager::getInstance();
            if (!config_manager.saveAlgorithmConfig(config)) {
                crow::json::wvalue response;
                response["success"] = false;
                response["error"] = "保存配置失败";
                return crow::response(500, response);
            }
            
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "配置保存成功";
            return crow::response(200, response);
            
        } catch (const std::exception& e) {
            crow::json::wvalue response;
            response["success"] = false;
            response["error"] = std::string("处理请求时发生错误: ") + e.what();
            return crow::response(500, response);
        }
    });
    
    // 删除通道的算法配置（恢复默认配置）
    CROW_ROUTE(app, "/api/algorithm-configs/<int>").methods("DELETE"_method)
    ([](int channel_id) {
        auto& config_manager = AlgorithmConfigManager::getInstance();
        
        if (!config_manager.deleteAlgorithmConfig(channel_id)) {
            crow::json::wvalue response;
            response["success"] = false;
            response["error"] = "删除配置失败";
            return crow::response(500, response);
        }
        
        crow::json::wvalue response;
        response["success"] = true;
        response["message"] = "配置已删除，将使用默认配置";
        return crow::response(200, response);
    });
    
    // 获取默认算法配置
    CROW_ROUTE(app, "/api/algorithm-configs/default").methods("GET"_method)
    ([]() {
        AlgorithmConfig default_config = AlgorithmConfigManager::getInstance().getDefaultConfig(0);
        
        crow::json::wvalue response;
        response["success"] = true;
        response["data"]["model_path"] = default_config.model_path;
        response["data"]["conf_threshold"] = default_config.conf_threshold;
        response["data"]["nms_threshold"] = default_config.nms_threshold;
        response["data"]["input_width"] = default_config.input_width;
        response["data"]["input_height"] = default_config.input_height;
        response["data"]["detection_interval"] = default_config.detection_interval;
        
        return crow::response(200, response);
    });
}

} // namespace detector_service

