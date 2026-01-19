#include "ws_api.h"
#include "ws_handler.h"
#include <iostream>

namespace detector_service {

void setupWebSocketRoutes(httplib::Server& svr) {
    auto& ws_handler = WebSocketHandler::getInstance();
    
    // 通道数据订阅端点
    svr.GetWebSocket("/ws/channel", [&ws_handler](const httplib::Request& req, httplib::WebSocket& ws) {
        ws.onopen = [&ws_handler](httplib::WebSocket& ws) {
            ws_handler.handleChannelConnection(&ws);
        };
        
        ws.onmessage = [&ws_handler](httplib::WebSocket& ws, const std::string& message, bool is_binary) {
            ws_handler.handleChannelMessage(&ws, message, is_binary);
        };
        
        ws.onclose = [&ws_handler](httplib::WebSocket& ws, int status, const std::string& reason) {
            ws_handler.handleDisconnection(&ws);
        };
    });
    
    // 报警数据订阅端点
    svr.GetWebSocket("/ws/alert", [&ws_handler](const httplib::Request& req, httplib::WebSocket& ws) {
        ws.onopen = [&ws_handler](httplib::WebSocket& ws) {
            ws_handler.handleAlertConnection(&ws);
        };
        
        ws.onmessage = [&ws_handler](httplib::WebSocket& ws, const std::string& message, bool is_binary) {
            ws_handler.handleAlertMessage(&ws, message, is_binary);
        };
        
        ws.onclose = [&ws_handler](httplib::WebSocket& ws, int status, const std::string& reason) {
            ws_handler.handleDisconnection(&ws);
        };
    });
}

} // namespace detector_service

