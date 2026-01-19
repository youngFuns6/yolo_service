#include "ws_api.h"
#include "ws_handler.h"
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXConnectionState.h>
#include "config.h"
#include <iostream>
#include <memory>
#include <atomic>
#include <map>
#include <mutex>
#include <algorithm>
#include <cctype>

namespace detector_service {

// 全局 WebSocket 服务器实例（使用 IXWebSocket）
static std::unique_ptr<ix::WebSocketServer> ws_server;
static std::atomic<bool> ws_server_started(false);
// 存储连接路径映射：ConnectionState ID -> path
static std::map<std::string, std::string> connection_paths;
static std::mutex connection_paths_mutex;
// 存储 WebSocket 连接：ConnectionState ID -> shared_ptr<ix::WebSocket>
static std::map<std::string, std::shared_ptr<ix::WebSocket>> ws_connections;
static std::mutex ws_connections_mutex;

void setupWebSocketRoutes(httplib::Server& svr) {
    auto& ws_handler = WebSocketHandler::getInstance();
    
    // 从配置获取 HTTP 端口，WebSocket 使用 HTTP 端口 + 1
    auto& config = Config::getInstance();
    int http_port = config.getServerConfig().http_port;
    int ws_port = http_port + 1;
    
    // 创建 WebSocket 服务器（如果还没有创建）
    if (!ws_server && !ws_server_started.load()) {
        ws_server = std::make_unique<ix::WebSocketServer>(ws_port, "0.0.0.0");
        
        // 设置消息回调
        ws_server->setOnClientMessageCallback(
            [&ws_handler](std::shared_ptr<ix::ConnectionState> connectionState,
                         ix::WebSocket& ws,
                         const ix::WebSocketMessagePtr& msg) {
                std::string conn_id = connectionState->getId();
                
                // 获取或查找 shared_ptr<ix::WebSocket>
                // 注意：回调中的 ws 是引用，我们需要从服务器获取对应的 shared_ptr
                std::shared_ptr<ix::WebSocket> ws_ptr;
                {
                    std::lock_guard<std::mutex> lock(ws_connections_mutex);
                    auto it = ws_connections.find(conn_id);
                    if (it == ws_connections.end()) {
                        // 从服务器获取所有客户端，查找对应的 shared_ptr
                        auto clients = ws_server->getClients();
                        for (auto& client : clients) {
                            if (client.get() == &ws) {
                                ws_ptr = client;
                                ws_connections[conn_id] = ws_ptr;
                                break;
                            }
                        }
                        // 如果没找到，创建一个临时的 shared_ptr（不应该发生）
                        if (!ws_ptr) {
                            std::cerr << "警告：无法找到对应的 WebSocket shared_ptr" << std::endl;
                            return;
                        }
                    } else {
                        ws_ptr = it->second;
                    }
                }
                
                std::string path;
                {
                    std::lock_guard<std::mutex> lock(connection_paths_mutex);
                    auto it = connection_paths.find(conn_id);
                    if (it != connection_paths.end()) {
                        path = it->second;
                    }
                }
                
                switch (msg->type) {
                    case ix::WebSocketMessageType::Open: {
                        // 从 URI 获取路径
                        std::string uri = msg->openInfo.uri;
                        std::cout << "WebSocket 连接打开，URI: " << uri << std::endl;
                        
                        // 保存路径映射
                        {
                            std::lock_guard<std::mutex> lock(connection_paths_mutex);
                            connection_paths[conn_id] = uri;
                        }
                        
                        if (uri == "/ws/channel" || uri.find("/ws/channel") == 0) {
                            ws_handler.handleChannelConnection(ws_ptr);
                        } else if (uri == "/ws/alert" || uri.find("/ws/alert") == 0) {
                            ws_handler.handleAlertConnection(ws_ptr);
                        }
                        break;
                    }
                    case ix::WebSocketMessageType::Message: {
                        bool is_binary = msg->binary;
                        std::string message = msg->str;
                        
                        if (path == "/ws/channel" || path.find("/ws/channel") == 0) {
                            ws_handler.handleChannelMessage(ws_ptr, message, is_binary);
                        } else if (path == "/ws/alert" || path.find("/ws/alert") == 0) {
                            ws_handler.handleAlertMessage(ws_ptr, message, is_binary);
                        }
                        break;
                    }
                    case ix::WebSocketMessageType::Close: {
                        ws_handler.handleDisconnection(ws_ptr);
                        // 清理
                        {
                            std::lock_guard<std::mutex> lock(ws_connections_mutex);
                            ws_connections.erase(conn_id);
                        }
                        {
                            std::lock_guard<std::mutex> lock(connection_paths_mutex);
                            connection_paths.erase(conn_id);
                        }
                        break;
                    }
                    case ix::WebSocketMessageType::Error: {
                        std::cerr << "WebSocket 错误: " << msg->errorInfo.reason << std::endl;
                        ws_handler.handleDisconnection(ws_ptr);
                        // 清理
                        {
                            std::lock_guard<std::mutex> lock(ws_connections_mutex);
                            ws_connections.erase(conn_id);
                        }
                        {
                            std::lock_guard<std::mutex> lock(connection_paths_mutex);
                            connection_paths.erase(conn_id);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        );
        
        // 启动服务器
        auto res = ws_server->listen();
        if (!res.first) {
            std::cerr << "WebSocket 服务器启动失败: " << res.second << std::endl;
            return;
        }
        
        ws_server->start();
        ws_server_started = true;
        std::cout << "WebSocket 服务器已启动在端口 " << ws_port << " (使用 IXWebSocket)" << std::endl;
    }
    
    // 在 HTTP 服务器上注册路由，用于处理 WebSocket 升级请求
    svr.Get("/ws/channel", [ws_port](const httplib::Request& req, httplib::Response& res) {
        // 检查是否是 WebSocket 升级请求
        bool is_upgrade = false;
        auto upgrade_it = req.headers.find("Upgrade");
        auto connection_it = req.headers.find("Connection");
        if (upgrade_it != req.headers.end() && connection_it != req.headers.end()) {
            std::string upgrade = upgrade_it->second;
            std::string connection = connection_it->second;
            std::transform(upgrade.begin(), upgrade.end(), upgrade.begin(), ::tolower);
            std::transform(connection.begin(), connection.end(), connection.begin(), ::tolower);
            if (upgrade == "websocket" && connection.find("upgrade") != std::string::npos) {
                is_upgrade = true;
            }
        }
        
        if (is_upgrade) {
            res.status = 426; // Upgrade Required
            res.set_content("WebSocket upgrade required. Please connect to ws://host:" + 
                          std::to_string(ws_port) + "/ws/channel", "text/plain");
        } else {
            res.status = 400;
            res.set_content("This endpoint requires WebSocket connection", "text/plain");
        }
    });
    
    svr.Get("/ws/alert", [ws_port](const httplib::Request& req, httplib::Response& res) {
        bool is_upgrade = false;
        auto upgrade_it = req.headers.find("Upgrade");
        auto connection_it = req.headers.find("Connection");
        if (upgrade_it != req.headers.end() && connection_it != req.headers.end()) {
            std::string upgrade = upgrade_it->second;
            std::string connection = connection_it->second;
            std::transform(upgrade.begin(), upgrade.end(), upgrade.begin(), ::tolower);
            std::transform(connection.begin(), connection.end(), connection.begin(), ::tolower);
            if (upgrade == "websocket" && connection.find("upgrade") != std::string::npos) {
                is_upgrade = true;
            }
        }
        
        if (is_upgrade) {
            res.status = 426;
            res.set_content("WebSocket upgrade required. Please connect to ws://host:" + 
                          std::to_string(ws_port) + "/ws/alert", "text/plain");
        } else {
            res.status = 400;
            res.set_content("This endpoint requires WebSocket connection", "text/plain");
        }
    });
}

// 获取 WebSocket 服务器端口（用于前端连接）
int getWebSocketPort() {
    if (ws_server && ws_server_started.load()) {
        auto& config = Config::getInstance();
        return config.getServerConfig().http_port + 1;
    }
    auto& config = Config::getInstance();
    return config.getServerConfig().http_port + 1;
}

} // namespace detector_service
