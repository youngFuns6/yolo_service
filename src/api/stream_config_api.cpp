#include "stream_config_api.h"
#include "stream_config.h"
#include <iostream>

namespace detector_service {

void setupStreamConfigRoutes(crow::SimpleApp& app) {
    // 配置全局推流地址
    CROW_ROUTE(app, "/api/config/stream").methods("PUT"_method)
    ([](const crow::request& req) {
        try {
            auto json_body = crow::json::load(req.body);
            if (!json_body) {
                return crow::response(400, "Invalid JSON");
            }
            
            auto& stream_config_mgr = StreamConfigManager::getInstance();
            
            // 先加载现有配置
            StreamConfig stream_config = stream_config_mgr.getStreamConfig();
            
            // 更新配置
            if (json_body.has("rtmp_url")) {
                stream_config.rtmp_url = json_body["rtmp_url"].s();
            }
            if (json_body.has("width")) {
                stream_config.width = json_body["width"].i();
            }
            if (json_body.has("height")) {
                stream_config.height = json_body["height"].i();
            }
            if (json_body.has("fps")) {
                stream_config.fps = json_body["fps"].i();
            }
            if (json_body.has("bitrate")) {
                stream_config.bitrate = json_body["bitrate"].i();
            }
            
            // 保存到数据库
            bool success = stream_config_mgr.saveStreamConfig(stream_config);
            
            crow::json::wvalue response;
            response["success"] = success;
            if (!success) {
                response["error"] = "Failed to save stream config to database";
            }
            return crow::response(response);
        } catch (const std::exception& e) {
            return crow::response(500, std::string("Error: ") + e.what());
        }
    });
    
    // 获取全局推流配置
    CROW_ROUTE(app, "/api/config/stream").methods("GET"_method)
    ([]() {
        auto& stream_config_mgr = StreamConfigManager::getInstance();
        StreamConfig stream_config = stream_config_mgr.getStreamConfig();
        
        crow::json::wvalue response;
        response["success"] = true;
        response["rtmp_url"] = stream_config.rtmp_url;
        response["width"] = stream_config.width;
        response["height"] = stream_config.height;
        response["fps"] = stream_config.fps;
        response["bitrate"] = stream_config.bitrate;
        
        return crow::response(response);
    });
}

} // namespace detector_service

