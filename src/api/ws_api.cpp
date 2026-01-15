#include "ws_api.h"
#include "ws_handler.h"
#include <iostream>

namespace detector_service {

void setupWebSocketRoutes(crow::SimpleApp& app) {
    auto& ws_handler = WebSocketHandler::getInstance();
    
    // 通道数据订阅端点
    CROW_WEBSOCKET_ROUTE(app, "/ws/channel")
    .onopen([&ws_handler](crow::websocket::connection& conn) {
        ws_handler.handleChannelConnection(conn);
    })
    .onclose([&ws_handler](crow::websocket::connection& conn, const std::string& reason, uint16_t status_code) {
        ws_handler.handleDisconnection(conn);
    })
    .onmessage([&ws_handler](crow::websocket::connection& conn, const std::string& message, bool is_binary) {
        ws_handler.handleChannelMessage(conn, message, is_binary);
    });
    
    // 报警数据订阅端点
    CROW_WEBSOCKET_ROUTE(app, "/ws/alert")
    .onopen([&ws_handler](crow::websocket::connection& conn) {
        ws_handler.handleAlertConnection(conn);
    })
    .onclose([&ws_handler](crow::websocket::connection& conn, const std::string& reason, uint16_t status_code) {
        ws_handler.handleDisconnection(conn);
    })
    .onmessage([&ws_handler](crow::websocket::connection& conn, const std::string& message, bool is_binary) {
        ws_handler.handleAlertMessage(conn, message, is_binary);
    });
}

} // namespace detector_service

