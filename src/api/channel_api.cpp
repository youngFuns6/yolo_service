#include "channel_api.h"
#include "channel_utils.h"
#include "yolov11_detector.h"
#include "stream_manager.h"
#include "config.h"
#include "database.h"
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace detector_service {

void setupChannelRoutes(httplib::Server& svr,
                       std::shared_ptr<YOLOv11Detector> detector,
                       StreamManager* stream_manager) {
    // 创建通道
    svr.Post("/api/channels", [detector, stream_manager](const httplib::Request& req, httplib::Response& res) {
        try {
            nlohmann::json json_body;
            try {
                json_body = nlohmann::json::parse(req.body);
            } catch (const nlohmann::json::exception&) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
                return;
            }
            
            Channel channel;
            if (json_body.contains("id")) {
                channel.id = json_body["id"].get<int>();
            }
            if (json_body.contains("name")) {
                channel.name = json_body["name"].get<std::string>();
            }
            if (json_body.contains("source_url")) {
                channel.source_url = json_body["source_url"].get<std::string>();
            }
            if (json_body.contains("enabled")) {
                channel.enabled = json_body["enabled"].get<bool>();
            }
            if (json_body.contains("report_enabled")) {
                channel.report_enabled = json_body["report_enabled"].get<bool>();
            }
            
            auto& channel_manager = ChannelManager::getInstance();
            int channel_id = channel_manager.createChannel(channel);
            
            if (channel_id == -1) {
                nlohmann::json response;
                response["success"] = false;
                response["error"] = "Channel ID already exists";
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                return;
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
            
            nlohmann::json response;
            response["success"] = true;
            response["channel_id"] = channel_id;
            res.status = 200;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(std::string("Error: ") + e.what(), "text/plain");
        }
    });
    
    // 获取所有通道
    svr.Get("/api/channels", [](const httplib::Request& req, httplib::Response& res) {
        auto& channel_manager = ChannelManager::getInstance();
        auto channels = channel_manager.getAllChannels();
        
        nlohmann::json channel_list = nlohmann::json::array();
        for (const auto& channel : channels) {
            nlohmann::json ch;
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
        
        nlohmann::json response;
        response["success"] = true;
        response["channels"] = channel_list;
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
    // 获取单个通道
    svr.Get(R"(/api/channels/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int channel_id = std::stoi(req.matches[1]);
        auto& channel_manager = ChannelManager::getInstance();
        auto channel = channel_manager.getChannel(channel_id);
        
        if (!channel) {
            res.status = 404;
            res.set_content("Channel not found", "text/plain");
            return;
        }
        
        nlohmann::json response;
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
        
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
    // 更新通道
    svr.Put(R"(/api/channels/(\d+))", [detector, stream_manager](const httplib::Request& req, httplib::Response& res) {
        try {
            int channel_id = std::stoi(req.matches[1]);
            nlohmann::json json_body;
            try {
                json_body = nlohmann::json::parse(req.body);
            } catch (const nlohmann::json::exception&) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
                return;
            }
            
            auto& channel_manager = ChannelManager::getInstance();
            auto existing = channel_manager.getChannel(channel_id);
            if (!existing) {
                res.status = 404;
                res.set_content("Channel not found", "text/plain");
                return;
            }
            
            Channel channel = *existing;
            bool old_enabled = channel.enabled.load();
            std::string old_source_url = channel.source_url;  // 保存旧的拉流地址
            bool source_url_changed = false;  // 标记拉流地址是否变化
            
            if (json_body.contains("id")) {
                channel.id = json_body["id"].get<int>();
            }
            if (json_body.contains("name")) {
                channel.name = json_body["name"].get<std::string>();
            }
            if (json_body.contains("source_url")) {
                std::string new_source_url = json_body["source_url"].get<std::string>();
                if (new_source_url != old_source_url) {
                    source_url_changed = true;
                }
                channel.source_url = new_source_url;
            }
            if (json_body.contains("enabled")) {
                channel.enabled = json_body["enabled"].get<bool>();
            }
            if (json_body.contains("report_enabled")) {
                channel.report_enabled = json_body["report_enabled"].get<bool>();
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
            
            nlohmann::json response;
            response["success"] = success;
            if (!success) {
                // 检查是否是因为id冲突
                if (json_body.contains("id") && json_body["id"].get<int>() != channel_id) {
                    response["error"] = "Channel ID already exists";
                    res.status = 400;
                    res.set_content(response.dump(), "application/json");
                    return;
                }
            }
            res.status = 200;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(std::string("Error: ") + e.what(), "text/plain");
        }
    });
    
    // 删除通道
    svr.Delete(R"(/api/channels/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int channel_id = std::stoi(req.matches[1]);
        auto& channel_manager = ChannelManager::getInstance();
        bool success = channel_manager.deleteChannel(channel_id);
        
        nlohmann::json response;
        response["success"] = success;
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
}

} // namespace detector_service

