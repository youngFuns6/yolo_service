#include "algorithm_config_api.h"
#include "algorithm_config.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>

namespace detector_service {

void setupAlgorithmConfigRoutes(httplib::Server& svr) {
    auto& config_manager = AlgorithmConfigManager::getInstance();
    
    // 获取通道的算法配置 - GET
    svr.Get(R"(/api/algorithm-configs/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int channel_id = std::stoi(req.matches[1]);
        AlgorithmConfig config;
        auto& config_manager = AlgorithmConfigManager::getInstance();
        
        if (!config_manager.getAlgorithmConfig(channel_id, config)) {
            nlohmann::json response;
            response["success"] = false;
            response["error"] = "获取配置失败";
            res.status = 500;
            res.set_content(response.dump(), "application/json");
            return;
        }
        
        nlohmann::json response;
        response["success"] = true;
        response["data"]["channel_id"] = config.channel_id;
        response["data"]["model_path"] = config.model_path;
        response["data"]["conf_threshold"] = config.conf_threshold;
        response["data"]["nms_threshold"] = config.nms_threshold;
        response["data"]["input_width"] = config.input_width;
        response["data"]["input_height"] = config.input_height;
        response["data"]["detection_interval"] = config.detection_interval;
        
        // 序列化 enabled_classes
        response["data"]["enabled_classes"] = nlohmann::json::array();
        for (size_t i = 0; i < config.enabled_classes.size(); i++) {
            response["data"]["enabled_classes"].push_back(config.enabled_classes[i]);
        }
        
        // 序列化 ROIs
        response["data"]["rois"] = nlohmann::json::array();
        for (size_t i = 0; i < config.rois.size(); i++) {
            const auto& roi = config.rois[i];
            nlohmann::json roi_json;
            roi_json["id"] = roi.id;
            roi_json["type"] = (roi.type == ROIType::RECTANGLE) ? "RECTANGLE" : "POLYGON";
            roi_json["name"] = roi.name;
            roi_json["enabled"] = roi.enabled;
            roi_json["points"] = nlohmann::json::array();
            for (size_t j = 0; j < roi.points.size(); j++) {
                nlohmann::json point_json;
                point_json["x"] = roi.points[j].x;
                point_json["y"] = roi.points[j].y;
                roi_json["points"].push_back(point_json);
            }
            response["data"]["rois"].push_back(roi_json);
        }
        
        // 序列化 AlertRules
        response["data"]["alert_rules"] = nlohmann::json::array();
        for (size_t i = 0; i < config.alert_rules.size(); i++) {
            const auto& rule = config.alert_rules[i];
            nlohmann::json rule_json;
            rule_json["id"] = rule.id;
            rule_json["name"] = rule.name;
            rule_json["enabled"] = rule.enabled;
            rule_json["target_classes"] = nlohmann::json::array();
            for (size_t j = 0; j < rule.target_classes.size(); j++) {
                rule_json["target_classes"].push_back(rule.target_classes[j]);
            }
            rule_json["min_confidence"] = rule.min_confidence;
            rule_json["min_count"] = rule.min_count;
            rule_json["max_count"] = rule.max_count;
            rule_json["suppression_window_seconds"] = rule.suppression_window_seconds;
            rule_json["roi_ids"] = nlohmann::json::array();
            for (size_t j = 0; j < rule.roi_ids.size(); j++) {
                rule_json["roi_ids"].push_back(rule.roi_ids[j]);
            }
            response["data"]["alert_rules"].push_back(rule_json);
        }
        
        response["data"]["created_at"] = config.created_at;
        response["data"]["updated_at"] = config.updated_at;
        
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
    // 保存通道的算法配置 - PUT
    svr.Put(R"(/api/algorithm-configs/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int channel_id = std::stoi(req.matches[1]);
        try {
            nlohmann::json json_body;
            try {
                json_body = nlohmann::json::parse(req.body);
            } catch (const nlohmann::json::exception&) {
                nlohmann::json response;
                response["success"] = false;
                response["error"] = "Invalid JSON";
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            AlgorithmConfig config;
            config.channel_id = channel_id;
            
            if (json_body.contains("model_path")) {
                config.model_path = json_body["model_path"].get<std::string>();
            }
            if (json_body.contains("conf_threshold")) {
                config.conf_threshold = json_body["conf_threshold"].get<float>();
            }
            if (json_body.contains("nms_threshold")) {
                config.nms_threshold = json_body["nms_threshold"].get<float>();
            }
            if (json_body.contains("input_width")) {
                config.input_width = json_body["input_width"].get<int>();
            }
            if (json_body.contains("input_height")) {
                config.input_height = json_body["input_height"].get<int>();
            }
            if (json_body.contains("detection_interval")) {
                config.detection_interval = json_body["detection_interval"].get<int>();
            }
            
            // 解析 enabled_classes
            if (json_body.contains("enabled_classes")) {
                for (const auto& class_id : json_body["enabled_classes"]) {
                    config.enabled_classes.push_back(class_id.get<int>());
                }
            }
            
            // 解析 ROIs
            // 注意：如果前端传入的坐标大于1，说明是像素坐标，需要归一化
            // 否则，假设已经是归一化坐标（0-1之间）
            if (json_body.contains("rois")) {
                float ref_width = static_cast<float>(config.input_width);
                float ref_height = static_cast<float>(config.input_height);
                
                for (const auto& roi_json : json_body["rois"]) {
                    ROI roi;
                    roi.id = roi_json.contains("id") ? roi_json["id"].get<int>() : static_cast<int>(config.rois.size());
                    roi.type = (roi_json.contains("type") && roi_json["type"].get<std::string>() == "POLYGON") 
                               ? ROIType::POLYGON : ROIType::RECTANGLE;
                    roi.name = roi_json.contains("name") ? roi_json["name"].get<std::string>() : std::string("");
                    roi.enabled = roi_json.contains("enabled") ? roi_json["enabled"].get<bool>() : true;
                    if (roi_json.contains("points")) {
                        for (const auto& point_json : roi_json["points"]) {
                            cv::Point2f point;
                            float x = point_json.contains("x") ? point_json["x"].get<float>() : 0.0f;
                            float y = point_json.contains("y") ? point_json["y"].get<float>() : 0.0f;
                            
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
            if (json_body.contains("alert_rules")) {
                for (const auto& rule_json : json_body["alert_rules"]) {
                    AlertRule rule;
                    rule.id = rule_json.contains("id") ? rule_json["id"].get<int>() : static_cast<int>(config.alert_rules.size());
                    rule.name = rule_json.contains("name") ? rule_json["name"].get<std::string>() : std::string("");
                    rule.enabled = rule_json.contains("enabled") ? rule_json["enabled"].get<bool>() : true;
                    if (rule_json.contains("target_classes")) {
                        for (const auto& class_id : rule_json["target_classes"]) {
                            rule.target_classes.push_back(class_id.get<int>());
                        }
                    }
                    rule.min_confidence = rule_json.contains("min_confidence") 
                                         ? rule_json["min_confidence"].get<float>() : 0.5f;
                    rule.min_count = rule_json.contains("min_count") 
                                    ? rule_json["min_count"].get<int>() : 1;
                    rule.max_count = rule_json.contains("max_count") 
                                    ? rule_json["max_count"].get<int>() : 0;
                    rule.suppression_window_seconds = rule_json.contains("suppression_window_seconds") 
                                                     ? rule_json["suppression_window_seconds"].get<int>() : 60;
                    if (rule_json.contains("roi_ids")) {
                        for (const auto& roi_id : rule_json["roi_ids"]) {
                            rule.roi_ids.push_back(roi_id.get<int>());
                        }
                    }
                    config.alert_rules.push_back(rule);
                }
            }
            
            auto& config_manager = AlgorithmConfigManager::getInstance();
            if (!config_manager.saveAlgorithmConfig(config)) {
                nlohmann::json response;
                response["success"] = false;
                response["error"] = "保存配置失败";
                res.status = 500;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            nlohmann::json response;
            response["success"] = true;
            response["message"] = "配置保存成功";
            res.status = 200;
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            nlohmann::json response;
            response["success"] = false;
            response["error"] = std::string("处理请求时发生错误: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });
    
    // 删除通道的算法配置（恢复默认配置） - DELETE
    svr.Delete(R"(/api/algorithm-configs/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int channel_id = std::stoi(req.matches[1]);
        auto& config_manager = AlgorithmConfigManager::getInstance();
        
        if (!config_manager.deleteAlgorithmConfig(channel_id)) {
            nlohmann::json response;
            response["success"] = false;
            response["error"] = "删除配置失败";
            res.status = 500;
            res.set_content(response.dump(), "application/json");
            return;
        }
        
        nlohmann::json response;
        response["success"] = true;
        response["message"] = "配置已删除，将使用默认配置";
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
    // 获取默认算法配置 - GET
    svr.Get("/api/algorithm-configs/default", [](const httplib::Request& req, httplib::Response& res) {
        AlgorithmConfig default_config = AlgorithmConfigManager::getInstance().getDefaultConfig(0);
        
        nlohmann::json response;
        response["success"] = true;
        response["data"]["model_path"] = default_config.model_path;
        response["data"]["conf_threshold"] = default_config.conf_threshold;
        response["data"]["nms_threshold"] = default_config.nms_threshold;
        response["data"]["input_width"] = default_config.input_width;
        response["data"]["input_height"] = default_config.input_height;
        response["data"]["detection_interval"] = default_config.detection_interval;
        
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
}

} // namespace detector_service
