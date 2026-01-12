#include "push_stream_config_api.h"
#include "push_stream_config.h"
#include <iostream>
#include <optional>

namespace detector_service {

void setupPushStreamRoutes(crow::SimpleApp& app) {
    // 配置全局推流地址
    CROW_ROUTE(app, "/api/config/push_stream").methods("PUT"_method)
    ([](const crow::request& req) {
        try {
            auto json_body = crow::json::load(req.body);
            if (!json_body) {
                return crow::response(400, "Invalid JSON");
            }
            
            auto& push_stream_config_mgr = PushStreamConfigManager::getInstance();
            
            // 先加载现有配置
            PushStreamConfig push_stream_config = push_stream_config_mgr.getPushStreamConfig();
            
            // 更新配置
            if (json_body.has("rtmp_url")) {
                push_stream_config.rtmp_url = json_body["rtmp_url"].s();
            }
            if (json_body.has("width")) {
                if (json_body["width"].t() == crow::json::type::Number) {
                    push_stream_config.width = json_body["width"].i();
                } else if (json_body["width"].t() == crow::json::type::Null) {
                    push_stream_config.width = std::nullopt;
                }
            }
            if (json_body.has("height")) {
                if (json_body["height"].t() == crow::json::type::Number) {
                    push_stream_config.height = json_body["height"].i();
                } else if (json_body["height"].t() == crow::json::type::Null) {
                    push_stream_config.height = std::nullopt;
                }
            }
            if (json_body.has("fps")) {
                if (json_body["fps"].t() == crow::json::type::Number) {
                    push_stream_config.fps = json_body["fps"].i();
                } else if (json_body["fps"].t() == crow::json::type::Null) {
                    push_stream_config.fps = std::nullopt;
                }
            }
            if (json_body.has("bitrate")) {
                if (json_body["bitrate"].t() == crow::json::type::Number) {
                    push_stream_config.bitrate = json_body["bitrate"].i();
                } else if (json_body["bitrate"].t() == crow::json::type::Null) {
                    push_stream_config.bitrate = std::nullopt;
                }
            }
            
            // 保存到数据库
            bool success = push_stream_config_mgr.savePushStreamConfig(push_stream_config);
            
            crow::json::wvalue response;
            response["success"] = success;
            if (!success) {
                response["error"] = "Failed to save push stream config to database";
            }
            return crow::response(response);
        } catch (const std::exception& e) {
            return crow::response(500, std::string("Error: ") + e.what());
        }
    });
    
    // 获取全局推流配置
    CROW_ROUTE(app, "/api/config/push_stream").methods("GET"_method)
    ([]() {
        auto& push_stream_config_mgr = PushStreamConfigManager::getInstance();
        PushStreamConfig push_stream_config = push_stream_config_mgr.getPushStreamConfig();
        
        crow::json::wvalue response;
        response["success"] = true;
        response["rtmp_url"] = push_stream_config.rtmp_url;
        if (push_stream_config.width.has_value()) {
            response["width"] = push_stream_config.width.value();
        } else {
            response["width"] = nullptr;
        }
        if (push_stream_config.height.has_value()) {
            response["height"] = push_stream_config.height.value();
        } else {
            response["height"] = nullptr;
        }
        if (push_stream_config.fps.has_value()) {
            response["fps"] = push_stream_config.fps.value();
        } else {
            response["fps"] = nullptr;
        }
        if (push_stream_config.bitrate.has_value()) {
            response["bitrate"] = push_stream_config.bitrate.value();
        } else {
            response["bitrate"] = nullptr;
        }
        
        return crow::response(response);
    });
}

} // namespace detector_service

