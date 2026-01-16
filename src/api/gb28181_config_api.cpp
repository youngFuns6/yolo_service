#include "gb28181_config_api.h"
#include "gb28181_config.h"
#include <iostream>

namespace detector_service {

void setupGB28181ConfigRoutes(crow::SimpleApp& app) {
    // 配置GB28181
    CROW_ROUTE(app, "/api/config/gb28181").methods("PUT"_method)
    ([](const crow::request& req) {
        try {
            auto json_body = crow::json::load(req.body);
            if (!json_body) {
                return crow::response(400, "Invalid JSON");
            }
            
            auto& gb28181_config_mgr = GB28181ConfigManager::getInstance();
            
            // 先加载现有配置
            GB28181Config config = gb28181_config_mgr.getGB28181Config();
            
            // 更新配置
            if (json_body.has("enabled")) {
                config.enabled = json_body["enabled"].b();
            }
            if (json_body.has("sip_server_ip")) {
                config.sip_server_ip = json_body["sip_server_ip"].s();
            }
            if (json_body.has("sip_server_port")) {
                config.sip_server_port = json_body["sip_server_port"].i();
            }
            if (json_body.has("sip_server_id")) {
                config.sip_server_id = json_body["sip_server_id"].s();
            }
            if (json_body.has("sip_server_domain")) {
                config.sip_server_domain = json_body["sip_server_domain"].s();
            }
            if (json_body.has("device_id")) {
                config.device_id = json_body["device_id"].s();
            }
            if (json_body.has("device_password")) {
                config.device_password = json_body["device_password"].s();
            }
            if (json_body.has("device_name")) {
                config.device_name = json_body["device_name"].s();
            }
            if (json_body.has("manufacturer")) {
                config.manufacturer = json_body["manufacturer"].s();
            }
            if (json_body.has("model")) {
                config.model = json_body["model"].s();
            }
            if (json_body.has("local_sip_port")) {
                config.local_sip_port = json_body["local_sip_port"].i();
            }
            if (json_body.has("rtp_port_start")) {
                config.rtp_port_start = json_body["rtp_port_start"].i();
            }
            if (json_body.has("rtp_port_end")) {
                config.rtp_port_end = json_body["rtp_port_end"].i();
            }
            if (json_body.has("heartbeat_interval")) {
                config.heartbeat_interval = json_body["heartbeat_interval"].i();
            }
            if (json_body.has("heartbeat_count")) {
                config.heartbeat_count = json_body["heartbeat_count"].i();
            }
            if (json_body.has("register_expires")) {
                config.register_expires = json_body["register_expires"].i();
            }
            if (json_body.has("stream_mode")) {
                config.stream_mode = json_body["stream_mode"].s();
            }
            if (json_body.has("max_channels")) {
                config.max_channels = json_body["max_channels"].i();
            }
            if (json_body.has("sip_transport")) {
                config.sip_transport = json_body["sip_transport"].s();
            }
            
            // 保存到数据库
            bool success = gb28181_config_mgr.saveGB28181Config(config);
            
            crow::json::wvalue response;
            response["success"] = success;
            if (!success) {
                response["error"] = "Failed to save GB28181 config to database";
            }
            return crow::response(response);
        } catch (const std::exception& e) {
            return crow::response(500, std::string("Error: ") + e.what());
        }
    });
    
    // 获取GB28181配置
    CROW_ROUTE(app, "/api/config/gb28181").methods("GET"_method)
    ([]() {
        auto& gb28181_config_mgr = GB28181ConfigManager::getInstance();
        GB28181Config config = gb28181_config_mgr.getGB28181Config();
        
        crow::json::wvalue response;
        response["success"] = true;
        response["enabled"] = config.enabled.load();
        response["sip_server_ip"] = config.sip_server_ip;
        response["sip_server_port"] = config.sip_server_port;
        response["sip_server_id"] = config.sip_server_id;
        response["sip_server_domain"] = config.sip_server_domain;
        response["device_id"] = config.device_id;
        response["device_password"] = config.device_password;
        response["device_name"] = config.device_name;
        response["manufacturer"] = config.manufacturer;
        response["model"] = config.model;
        response["local_sip_port"] = config.local_sip_port;
        response["rtp_port_start"] = config.rtp_port_start;
        response["rtp_port_end"] = config.rtp_port_end;
        response["heartbeat_interval"] = config.heartbeat_interval;
        response["heartbeat_count"] = config.heartbeat_count;
        response["register_expires"] = config.register_expires;
        response["stream_mode"] = config.stream_mode;
        response["max_channels"] = config.max_channels;
        response["sip_transport"] = config.sip_transport;
        
        return crow::response(response);
    });
}

} // namespace detector_service

