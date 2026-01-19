#include "gb28181_config_api.h"
#include "gb28181_config.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace detector_service {

void setupGB28181ConfigRoutes(httplib::Server& svr) {
    // 配置GB28181 - PUT
    svr.Put("/api/config/gb28181", [](const httplib::Request& req, httplib::Response& res) {
        try {
            nlohmann::json json_body;
            try {
                json_body = nlohmann::json::parse(req.body);
            } catch (const nlohmann::json::exception&) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
                return;
            }
            
            auto& gb28181_config_mgr = GB28181ConfigManager::getInstance();
            
            // 先加载现有配置
            GB28181Config config = gb28181_config_mgr.getGB28181Config();
            
            // 更新配置
            if (json_body.contains("enabled")) {
                config.enabled = json_body["enabled"].get<bool>();
            }
            if (json_body.contains("sip_server_ip")) {
                config.sip_server_ip = json_body["sip_server_ip"].get<std::string>();
            }
            if (json_body.contains("sip_server_port")) {
                config.sip_server_port = json_body["sip_server_port"].get<int>();
            }
            if (json_body.contains("sip_server_id")) {
                config.sip_server_id = json_body["sip_server_id"].get<std::string>();
            }
            if (json_body.contains("sip_server_domain")) {
                config.sip_server_domain = json_body["sip_server_domain"].get<std::string>();
            }
            if (json_body.contains("device_id")) {
                config.device_id = json_body["device_id"].get<std::string>();
            }
            if (json_body.contains("device_password")) {
                config.device_password = json_body["device_password"].get<std::string>();
            }
            if (json_body.contains("device_name")) {
                config.device_name = json_body["device_name"].get<std::string>();
            }
            if (json_body.contains("manufacturer")) {
                config.manufacturer = json_body["manufacturer"].get<std::string>();
            }
            if (json_body.contains("model")) {
                config.model = json_body["model"].get<std::string>();
            }
            if (json_body.contains("local_sip_port")) {
                config.local_sip_port = json_body["local_sip_port"].get<int>();
            }
            if (json_body.contains("rtp_port_start")) {
                config.rtp_port_start = json_body["rtp_port_start"].get<int>();
            }
            if (json_body.contains("rtp_port_end")) {
                config.rtp_port_end = json_body["rtp_port_end"].get<int>();
            }
            if (json_body.contains("heartbeat_interval")) {
                config.heartbeat_interval = json_body["heartbeat_interval"].get<int>();
            }
            if (json_body.contains("heartbeat_count")) {
                config.heartbeat_count = json_body["heartbeat_count"].get<int>();
            }
            if (json_body.contains("register_expires")) {
                config.register_expires = json_body["register_expires"].get<int>();
            }
            if (json_body.contains("stream_mode")) {
                config.stream_mode = json_body["stream_mode"].get<std::string>();
            }
            if (json_body.contains("max_channels")) {
                config.max_channels = json_body["max_channels"].get<int>();
            }
            if (json_body.contains("sip_transport")) {
                config.sip_transport = json_body["sip_transport"].get<std::string>();
            }
            
            // 保存到数据库
            bool success = gb28181_config_mgr.saveGB28181Config(config);
            
            nlohmann::json response;
            response["success"] = success;
            if (!success) {
                response["error"] = "Failed to save GB28181 config to database";
            }
            res.status = success ? 200 : 500;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(std::string("Error: ") + e.what(), "text/plain");
        }
    });
    
    // 获取GB28181配置 - GET
    svr.Get("/api/config/gb28181", [](const httplib::Request& req, httplib::Response& res) {
        auto& gb28181_config_mgr = GB28181ConfigManager::getInstance();
        GB28181Config config = gb28181_config_mgr.getGB28181Config();
        
        nlohmann::json response;
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
        
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
}

} // namespace detector_service
