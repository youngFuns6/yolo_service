#include "channel_api.h"
#include "yolov11_detector.h"
#include "stream_manager.h"
#include "config.h"
#include "database.h"
#include <iostream>
#include <sstream>

namespace detector_service {

std::string channelStatusToString(ChannelStatus status) {
    switch (status) {
        case ChannelStatus::IDLE: return "idle";
        case ChannelStatus::RUNNING: return "running";
        case ChannelStatus::ERROR: return "error";
        case ChannelStatus::STOPPED: return "stopped";
        default: return "unknown";
    }
}

ChannelStatus stringToChannelStatus(const std::string& str) {
    if (str == "idle") return ChannelStatus::IDLE;
    if (str == "running") return ChannelStatus::RUNNING;
    if (str == "error") return ChannelStatus::ERROR;
    if (str == "stopped") return ChannelStatus::STOPPED;
    return ChannelStatus::IDLE;
}

void setupChannelRoutes(crow::SimpleApp& app,
                       std::shared_ptr<YOLOv11Detector> detector,
                       StreamManager* stream_manager) {
    // 创建通道
    CROW_ROUTE(app, "/api/channels").methods("POST"_method)
    ([detector, stream_manager](const crow::request& req) {
        try {
            auto json_body = crow::json::load(req.body);
            if (!json_body) {
                return crow::response(400, "Invalid JSON");
            }
            
            Channel channel;
            if (json_body.has("id")) {
                channel.id = json_body["id"].i();
            }
            if (json_body.has("name")) {
                channel.name = json_body["name"].s();
            }
            if (json_body.has("source_url")) {
                channel.source_url = json_body["source_url"].s();
            }
            if (json_body.has("enabled")) {
                channel.enabled = json_body["enabled"].b();
            }
            if (json_body.has("report_enabled")) {
                channel.report_enabled = json_body["report_enabled"].b();
            }
            
            auto& channel_manager = ChannelManager::getInstance();
            int channel_id = channel_manager.createChannel(channel);
            
            if (channel_id == -1) {
                crow::json::wvalue response;
                response["success"] = false;
                response["error"] = "Channel ID already exists";
                return crow::response(400, response);
            }
            
            // 如果通道启用，自动启动拉流分析
            if (channel.enabled.load() && stream_manager && detector) {
                auto created_channel = channel_manager.getChannel(channel_id);
                if (created_channel) {
                    // 启动拉流分析
                    stream_manager->startAnalysis(channel_id, created_channel, detector);
                } else {
                    std::cerr << "ChannelAPI: 无法获取通道 " << channel_id << " 的实例" << std::endl;
                }
            }
            
            crow::json::wvalue response;
            response["success"] = true;
            response["channel_id"] = channel_id;
            return crow::response(response);
        } catch (const std::exception& e) {
            return crow::response(500, std::string("Error: ") + e.what());
        }
    });
    
    // 获取所有通道
    CROW_ROUTE(app, "/api/channels").methods("GET"_method)
    ([]() {
        auto& channel_manager = ChannelManager::getInstance();
        auto channels = channel_manager.getAllChannels();
        
        crow::json::wvalue::list channel_list;
        for (const auto& channel : channels) {
            crow::json::wvalue ch;
            ch["id"] = channel->id;
            ch["name"] = channel->name;
            ch["source_url"] = channel->source_url;
            ch["status"] = channelStatusToString(channel->status);
            ch["enabled"] = channel->enabled.load();
            ch["report_enabled"] = channel->report_enabled.load();
            ch["width"] = channel->width;
            ch["height"] = channel->height;
            ch["fps"] = channel->fps;
            ch["created_at"] = channel->created_at;
            ch["updated_at"] = channel->updated_at;
            channel_list.push_back(ch);
        }
        
        crow::json::wvalue response;
        response["success"] = true;
        response["channels"] = std::move(channel_list);
        return crow::response(response);
    });
    
    // 获取单个通道
    CROW_ROUTE(app, "/api/channels/<int>").methods("GET"_method)
    ([](int channel_id) {
        auto& channel_manager = ChannelManager::getInstance();
        auto channel = channel_manager.getChannel(channel_id);
        
        if (!channel) {
            return crow::response(404, "Channel not found");
        }
        
        crow::json::wvalue response;
        response["success"] = true;
        response["channel"]["id"] = channel->id;
        response["channel"]["name"] = channel->name;
        response["channel"]["source_url"] = channel->source_url;
            response["channel"]["status"] = channelStatusToString(channel->status);
            response["channel"]["enabled"] = channel->enabled.load();
            response["channel"]["report_enabled"] = channel->report_enabled.load();
            response["channel"]["width"] = channel->width;
            response["channel"]["height"] = channel->height;
            response["channel"]["fps"] = channel->fps;
            response["channel"]["created_at"] = channel->created_at;
            response["channel"]["updated_at"] = channel->updated_at;
        
        return crow::response(response);
    });
    
    // 更新通道
    CROW_ROUTE(app, "/api/channels/<int>").methods("PUT"_method)
    ([detector, stream_manager](const crow::request& req, int channel_id) {
        try {
            auto json_body = crow::json::load(req.body);
            if (!json_body) {
                return crow::response(400, "Invalid JSON");
            }
            
            auto& channel_manager = ChannelManager::getInstance();
            auto existing = channel_manager.getChannel(channel_id);
            if (!existing) {
                return crow::response(404, "Channel not found");
            }
            
            Channel channel = *existing;
            bool old_enabled = channel.enabled.load();
            std::string old_source_url = channel.source_url;  // 保存旧的拉流地址
            bool source_url_changed = false;  // 标记拉流地址是否变化
            
            if (json_body.has("id")) {
                channel.id = json_body["id"].i();
            }
            if (json_body.has("name")) {
                channel.name = json_body["name"].s();
            }
            if (json_body.has("source_url")) {
                std::string new_source_url = json_body["source_url"].s();
                if (new_source_url != old_source_url) {
                    source_url_changed = true;
                }
                channel.source_url = new_source_url;
            }
            if (json_body.has("enabled")) {
                channel.enabled = json_body["enabled"].b();
            }
            if (json_body.has("report_enabled")) {
                channel.report_enabled = json_body["report_enabled"].b();
            }
            
            bool success = channel_manager.updateChannel(channel_id, channel);
            
            // 处理enabled状态变化和拉流地址变化
            if (success && stream_manager && detector) {
                bool new_enabled = channel.enabled.load();
                auto updated_channel = channel_manager.getChannel(channel_id);
                
                // 处理拉流地址变化：如果地址变化且通道启用，需要重新启动分析
                if (source_url_changed && new_enabled) {
                    // 如果通道正在运行，先停止当前分析
                    if (stream_manager->isAnalyzing(channel_id)) {
                        stream_manager->stopAnalysis(channel_id);
                    }
                    // 使用新的拉流地址重新启动分析
                    if (updated_channel) {
                        stream_manager->startAnalysis(channel_id, updated_channel, detector);
                    }
                }
                // 处理enabled状态变化（仅在拉流地址未变化时处理，避免重复操作）
                else if (old_enabled != new_enabled) {
                    if (new_enabled) {
                        // 启用：启动拉流分析
                        if (updated_channel) {
                            stream_manager->startAnalysis(channel_id, updated_channel, detector);
                        }
                    } else {
                        // 禁用：停止拉流分析
                        stream_manager->stopAnalysis(channel_id);
                    }
                }
            }
            
            crow::json::wvalue response;
            response["success"] = success;
            if (!success) {
                // 检查是否是因为id冲突
                if (json_body.has("id") && json_body["id"].i() != channel_id) {
                    response["error"] = "Channel ID already exists";
                    return crow::response(400, response);
                }
            }
            return crow::response(response);
        } catch (const std::exception& e) {
            return crow::response(500, std::string("Error: ") + e.what());
        }
    });
    
    // 删除通道
    CROW_ROUTE(app, "/api/channels/<int>").methods("DELETE"_method)
    ([](int channel_id) {
        auto& channel_manager = ChannelManager::getInstance();
        bool success = channel_manager.deleteChannel(channel_id);
        
        crow::json::wvalue response;
        response["success"] = success;
        return crow::response(response);
    });
    
}

} // namespace detector_service

