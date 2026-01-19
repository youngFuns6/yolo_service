#include "report_config_api.h"
#include "report_config.h"
#include "report_service.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace detector_service {

void setupReportConfigRoutes(LwsServer& svr) {
    // 获取上报配置
    svr.Get("/api/report-config", [](const HttpRequest& req, HttpResponse& res) {
        auto& config_manager = ReportConfigManager::getInstance();
        const auto& config = config_manager.getReportConfig();
        
        nlohmann::json response;
        response["success"] = true;
        response["config"]["type"] = (config.type == ReportType::HTTP) ? "HTTP" : "MQTT";
        response["config"]["http_url"] = config.http_url;
        response["config"]["mqtt_broker"] = config.mqtt_broker;
        response["config"]["mqtt_port"] = config.mqtt_port;
        response["config"]["mqtt_topic"] = config.mqtt_topic;
        response["config"]["mqtt_username"] = config.mqtt_username;
        response["config"]["mqtt_password"] = config.mqtt_password;
        response["config"]["mqtt_client_id"] = config.mqtt_client_id;
        response["config"]["enabled"] = config.enabled.load();
        
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
    // 更新上报配置
    svr.Put("/api/report-config", [](const HttpRequest& req, HttpResponse& res) {
        try {
            nlohmann::json json_body;
            try {
                json_body = nlohmann::json::parse(req.body);
            } catch (const nlohmann::json::exception&) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
                return;
            }
            
            ReportConfig config;
            
            if (json_body.contains("type")) {
                std::string type_str = json_body["type"].get<std::string>();
                config.type = (type_str == "MQTT") ? ReportType::MQTT : ReportType::HTTP;
            }
            
            if (json_body.contains("http_url")) {
                config.http_url = json_body["http_url"].get<std::string>();
            }
            
            if (json_body.contains("mqtt_broker")) {
                config.mqtt_broker = json_body["mqtt_broker"].get<std::string>();
            }
            
            if (json_body.contains("mqtt_port")) {
                config.mqtt_port = json_body["mqtt_port"].get<int>();
            }
            
            if (json_body.contains("mqtt_topic")) {
                config.mqtt_topic = json_body["mqtt_topic"].get<std::string>();
            }
            
            if (json_body.contains("mqtt_username")) {
                config.mqtt_username = json_body["mqtt_username"].get<std::string>();
            }
            
            if (json_body.contains("mqtt_password")) {
                config.mqtt_password = json_body["mqtt_password"].get<std::string>();
            }
            
            if (json_body.contains("mqtt_client_id")) {
                config.mqtt_client_id = json_body["mqtt_client_id"].get<std::string>();
            }
            
            bool enabled_changed = false;
            bool new_enabled = true;
            if (json_body.contains("enabled")) {
                new_enabled = json_body["enabled"].get<bool>();
                config.enabled = new_enabled;
                enabled_changed = true;
            }
            
            auto& config_manager = ReportConfigManager::getInstance();
            
            // 获取当前配置（用于合并更新）
            const auto& current_config = config_manager.getReportConfig();
            
            // 合并配置：如果某个字段没有在请求中提供，使用当前配置的值
            if (!json_body.contains("type")) {
                config.type = current_config.type;
            }
            if (!json_body.contains("http_url")) {
                config.http_url = current_config.http_url;
            }
            if (!json_body.contains("mqtt_broker")) {
                config.mqtt_broker = current_config.mqtt_broker;
            }
            if (!json_body.contains("mqtt_port")) {
                config.mqtt_port = current_config.mqtt_port;
            }
            if (!json_body.contains("mqtt_topic")) {
                config.mqtt_topic = current_config.mqtt_topic;
            }
            if (!json_body.contains("mqtt_username")) {
                config.mqtt_username = current_config.mqtt_username;
            }
            if (!json_body.contains("mqtt_password")) {
                config.mqtt_password = current_config.mqtt_password;
            }
            if (!json_body.contains("mqtt_client_id")) {
                config.mqtt_client_id = current_config.mqtt_client_id;
            }
            if (!json_body.contains("enabled")) {
                config.enabled = current_config.enabled.load();
            }
            
            auto& report_service = ReportService::getInstance();
            
            // 如果配置被禁用，停止 MQTT 连接
            if (enabled_changed && !new_enabled) {
                report_service.stopMqttConnection();
            }
            
            bool success = config_manager.updateReportConfig(config);
            
            // 如果配置被启用且是 MQTT 类型，MQTT 连接将在上报时自动创建
            if (enabled_changed && new_enabled && config.type == ReportType::MQTT) {
                if (config.mqtt_broker.empty() || config.mqtt_topic.empty()) {
                    std::cerr << "MQTT 配置不完整: broker=" << config.mqtt_broker 
                              << ", topic=" << config.mqtt_topic << std::endl;
                }
            }
            
            nlohmann::json response;
            response["success"] = success;
            if (!success) {
                response["error"] = "Failed to update report config";
            }
            res.status = 200;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(std::string("Error: ") + e.what(), "text/plain");
        }
    });
}

} // namespace detector_service
