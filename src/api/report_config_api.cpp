#include "report_config_api.h"
#include "report_config.h"
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
            
            if (json_body.has("enabled")) {
                config.enabled = json_body["enabled"].b();
            }
            
            auto& config_manager = ReportConfigManager::getInstance();
            bool success = config_manager.updateReportConfig(config);
            
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

