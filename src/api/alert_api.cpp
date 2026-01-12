#include "alert_api.h"
#include "alert.h"
#include <iostream>

namespace detector_service {

void setupAlertRoutes(crow::SimpleApp& app) {
    // 获取所有报警记录
    CROW_ROUTE(app, "/api/alerts").methods("GET"_method)
    ([](const crow::request& req) {
        int limit = 100;
        int offset = 0;
        
        if (req.url_params.get("limit")) {
            limit = std::stoi(req.url_params.get("limit"));
        }
        if (req.url_params.get("offset")) {
            offset = std::stoi(req.url_params.get("offset"));
        }
        
        auto& alert_manager = AlertManager::getInstance();
        auto alerts = alert_manager.getAlerts(limit, offset);
        
        crow::json::wvalue::list alert_list;
        for (const auto& alert : alerts) {
            crow::json::wvalue a;
            a["id"] = alert.id;
            a["channel_id"] = alert.channel_id;
            a["channel_name"] = alert.channel_name;
            a["alert_type"] = alert.alert_type;
            a["alert_rule_id"] = alert.alert_rule_id;
            a["alert_rule_name"] = alert.alert_rule_name;
            a["image_path"] = alert.image_path;
            a["confidence"] = alert.confidence;
            a["detected_objects"] = alert.detected_objects;
            a["bbox_x"] = alert.bbox_x;
            a["bbox_y"] = alert.bbox_y;
            a["bbox_w"] = alert.bbox_w;
            a["bbox_h"] = alert.bbox_h;
            a["report_status"] = alert.report_status;
            a["report_url"] = alert.report_url;
            a["created_at"] = alert.created_at;
            alert_list.push_back(a);
        }
        
        crow::json::wvalue response;
        response["success"] = true;
        response["alerts"] = std::move(alert_list);
        response["total"] = alert_manager.getAlertCount();
        return crow::response(response);
    });
    
    // 获取单个报警记录
    CROW_ROUTE(app, "/api/alerts/<int>").methods("GET"_method)
    ([](int alert_id) {
        auto& alert_manager = AlertManager::getInstance();
        auto alert = alert_manager.getAlert(alert_id);
        
        if (alert.id == 0) {
            return crow::response(404, "Alert not found");
        }
        
        crow::json::wvalue response;
        response["success"] = true;
        response["alert"]["id"] = alert.id;
        response["alert"]["channel_id"] = alert.channel_id;
        response["alert"]["channel_name"] = alert.channel_name;
        response["alert"]["alert_type"] = alert.alert_type;
        response["alert"]["alert_rule_id"] = alert.alert_rule_id;
        response["alert"]["alert_rule_name"] = alert.alert_rule_name;
        response["alert"]["image_path"] = alert.image_path;
        response["alert"]["confidence"] = alert.confidence;
        response["alert"]["detected_objects"] = alert.detected_objects;
        response["alert"]["bbox_x"] = alert.bbox_x;
        response["alert"]["bbox_y"] = alert.bbox_y;
        response["alert"]["bbox_w"] = alert.bbox_w;
        response["alert"]["bbox_h"] = alert.bbox_h;
        response["alert"]["report_status"] = alert.report_status;
        response["alert"]["report_url"] = alert.report_url;
        response["alert"]["created_at"] = alert.created_at;
        
        return crow::response(response);
    });
    
    // 获取通道的报警记录
    CROW_ROUTE(app, "/api/channels/<int>/alerts").methods("GET"_method)
    ([](const crow::request& req, int channel_id) {
        int limit = 100;
        int offset = 0;
        
        if (req.url_params.get("limit")) {
            limit = std::stoi(req.url_params.get("limit"));
        }
        if (req.url_params.get("offset")) {
            offset = std::stoi(req.url_params.get("offset"));
        }
        
        auto& alert_manager = AlertManager::getInstance();
        auto alerts = alert_manager.getAlertsByChannel(channel_id, limit, offset);
        
        crow::json::wvalue::list alert_list;
        for (const auto& alert : alerts) {
            crow::json::wvalue a;
            a["id"] = alert.id;
            a["channel_id"] = alert.channel_id;
            a["channel_name"] = alert.channel_name;
            a["alert_type"] = alert.alert_type;
            a["alert_rule_id"] = alert.alert_rule_id;
            a["alert_rule_name"] = alert.alert_rule_name;
            a["image_path"] = alert.image_path;
            a["confidence"] = alert.confidence;
            a["detected_objects"] = alert.detected_objects;
            a["bbox_x"] = alert.bbox_x;
            a["bbox_y"] = alert.bbox_y;
            a["bbox_w"] = alert.bbox_w;
            a["bbox_h"] = alert.bbox_h;
            a["report_status"] = alert.report_status;
            a["report_url"] = alert.report_url;
            a["created_at"] = alert.created_at;
            alert_list.push_back(a);
        }
        
        crow::json::wvalue response;
        response["success"] = true;
        response["alerts"] = std::move(alert_list);
        response["total"] = alert_manager.getAlertCountByChannel(channel_id);
        return crow::response(response);
    });
    
    // 删除报警记录
    CROW_ROUTE(app, "/api/alerts/<int>").methods("DELETE"_method)
    ([](int alert_id) {
        auto& alert_manager = AlertManager::getInstance();
        bool success = alert_manager.deleteAlert(alert_id);
        
        crow::json::wvalue response;
        response["success"] = success;
        return crow::response(response);
    });
}

} // namespace detector_service

