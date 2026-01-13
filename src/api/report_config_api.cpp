#include "report_config_api.h"
#include "report_config.h"
#include "report_service.h"
#include <iostream>

namespace detector_service {

void setupReportConfigRoutes(crow::SimpleApp& app) {
    // 获取上报配置
    CROW_ROUTE(app, "/api/report-config").methods("GET"_method)
    ([]() {
        auto& config_manager = ReportConfigManager::getInstance();
        const auto& config = config_manager.getReportConfig();
        
        crow::json::wvalue response;
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
        
        return crow::response(response);
    });
    
    // 更新上报配置
    CROW_ROUTE(app, "/api/report-config").methods("PUT"_method)
    ([](const crow::request& req) {
        try {
            auto json_body = crow::json::load(req.body);
            if (!json_body) {
                return crow::response(400, "Invalid JSON");
            }
            
            ReportConfig config;
            
            if (json_body.has("type")) {
                std::string type_str = json_body["type"].s();
                config.type = (type_str == "MQTT") ? ReportType::MQTT : ReportType::HTTP;
            }
            
            if (json_body.has("http_url")) {
                config.http_url = json_body["http_url"].s();
            }
            
            if (json_body.has("mqtt_broker")) {
                config.mqtt_broker = json_body["mqtt_broker"].s();
            }
            
            if (json_body.has("mqtt_port")) {
                config.mqtt_port = json_body["mqtt_port"].i();
            }
            
            if (json_body.has("mqtt_topic")) {
                config.mqtt_topic = json_body["mqtt_topic"].s();
            }
            
            if (json_body.has("mqtt_username")) {
                config.mqtt_username = json_body["mqtt_username"].s();
            }
            
            if (json_body.has("mqtt_password")) {
                config.mqtt_password = json_body["mqtt_password"].s();
            }
            
            if (json_body.has("mqtt_client_id")) {
                config.mqtt_client_id = json_body["mqtt_client_id"].s();
            }
            
            bool enabled_changed = false;
            bool new_enabled = true;
            if (json_body.has("enabled")) {
                new_enabled = json_body["enabled"].b();
                config.enabled = new_enabled;
                enabled_changed = true;
            }
            
            auto& config_manager = ReportConfigManager::getInstance();
            
            // 获取当前配置（用于合并更新）
            const auto& current_config = config_manager.getReportConfig();
            
            // 合并配置：如果某个字段没有在请求中提供，使用当前配置的值
            if (!json_body.has("type")) {
                config.type = current_config.type;
            }
            if (!json_body.has("http_url")) {
                config.http_url = current_config.http_url;
            }
            if (!json_body.has("mqtt_broker")) {
                config.mqtt_broker = current_config.mqtt_broker;
            }
            if (!json_body.has("mqtt_port")) {
                config.mqtt_port = current_config.mqtt_port;
            }
            if (!json_body.has("mqtt_topic")) {
                config.mqtt_topic = current_config.mqtt_topic;
            }
            if (!json_body.has("mqtt_username")) {
                config.mqtt_username = current_config.mqtt_username;
            }
            if (!json_body.has("mqtt_password")) {
                config.mqtt_password = current_config.mqtt_password;
            }
            if (!json_body.has("mqtt_client_id")) {
                config.mqtt_client_id = current_config.mqtt_client_id;
            }
            if (!json_body.has("enabled")) {
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
            
            crow::json::wvalue response;
            response["success"] = success;
            if (!success) {
                response["error"] = "Failed to update report config";
            }
            return crow::response(response);
        } catch (const std::exception& e) {
            return crow::response(500, std::string("Error: ") + e.what());
        }
    });
}

} // namespace detector_service

