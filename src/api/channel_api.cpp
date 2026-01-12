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
            if (json_body.has("push_enabled")) {
                channel.push_enabled = json_body["push_enabled"].b();
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
                std::cout << "ChannelAPI: 通道 " << channel_id << " 已启用，准备启动分析" << std::endl;
                auto created_channel = channel_manager.getChannel(channel_id);
                if (created_channel) {
                    // 启动拉流分析
                    std::cout << "ChannelAPI: 启动分析模式，通道ID=" << channel_id << std::endl;
                    stream_manager->startAnalysis(channel_id, created_channel, detector);
                } else {
                    std::cerr << "ChannelAPI: 无法获取通道 " << channel_id << " 的实例" << std::endl;
                }
            } else {
                std::cout << "ChannelAPI: 通道 " << channel_id 
                          << " 未启用(enabled=" << channel.enabled.load() 
                          << ")或stream_manager/detector为空，不启动分析" << std::endl;
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
            ch["push_enabled"] = channel->push_enabled.load();
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
            response["channel"]["push_enabled"] = channel->push_enabled.load();
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
            if (json_body.has("push_enabled")) {
                channel.push_enabled = json_body["push_enabled"].b();
            }
            if (json_body.has("report_enabled")) {
                channel.report_enabled = json_body["report_enabled"].b();
            }
            
            bool success = channel_manager.updateChannel(channel_id, channel);
            
            // 处理enabled状态变化
            if (success && stream_manager && detector) {
                bool new_enabled = channel.enabled.load();
                auto updated_channel = channel_manager.getChannel(channel_id);
                
                if (old_enabled != new_enabled) {
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
    
    // 推流开关API（保留配置，但不执行推流逻辑）
    CROW_ROUTE(app, "/api/channels/<int>/push").methods("POST"_method)
    ([](const crow::request& req, int channel_id) {
        try {
            auto json_body = crow::json::load(req.body);
            if (!json_body) {
                return crow::response(400, "Invalid JSON");
            }
            
            if (!json_body.has("push_enabled")) {
                return crow::response(400, "Missing push_enabled field");
            }
            
            bool push_enabled = json_body["push_enabled"].b();
            
            auto& channel_manager = ChannelManager::getInstance();
            auto channel = channel_manager.getChannel(channel_id);
            
            if (!channel) {
                crow::json::wvalue response;
                response["success"] = false;
                response["error"] = "Channel not found";
                return crow::response(404, response);
            }
            
            // 更新push_enabled字段（仅保留配置）
            channel->push_enabled = push_enabled;
            bool success = channel_manager.updateChannel(channel_id, *channel);
            
            if (!success) {
                crow::json::wvalue response;
                response["success"] = false;
                response["error"] = "Failed to update push_enabled";
                return crow::response(500, response);
            }
            
            crow::json::wvalue response;
            response["success"] = true;
            return crow::response(response);
        } catch (const std::exception& e) {
            return crow::response(500, std::string("Error: ") + e.what());
        }
    });
    
}

} // namespace detector_service

