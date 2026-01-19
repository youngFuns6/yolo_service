#include "alert_api.h"
#include "alert.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace detector_service {

void setupAlertRoutes(httplib::Server& svr) {
    // 获取所有报警记录
    svr.Get("/api/alerts", [](const httplib::Request& req, httplib::Response& res) {
        int limit = 100;
        int offset = 0;
        
        if (req.has_param("limit")) {
            limit = std::stoi(req.get_param_value("limit"));
        }
        if (req.has_param("offset")) {
            offset = std::stoi(req.get_param_value("offset"));
        }
        
        auto& alert_manager = AlertManager::getInstance();
        auto alerts = alert_manager.getAlerts(limit, offset);
        
        nlohmann::json alert_list = nlohmann::json::array();
        for (const auto& alert : alerts) {
            nlohmann::json a;
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
        
        nlohmann::json response;
        response["success"] = true;
        response["alerts"] = alert_list;
        response["total"] = alert_manager.getAlertCount();
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
    // 获取单个报警记录
    svr.Get(R"(/api/alerts/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int alert_id = std::stoi(req.matches[1]);
        auto& alert_manager = AlertManager::getInstance();
        auto alert = alert_manager.getAlert(alert_id);
        
        if (alert.id == 0) {
            res.status = 404;
            res.set_content("Alert not found", "text/plain");
            return;
        }
        
        nlohmann::json response;
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
        
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
    // 获取通道的报警记录
    svr.Get(R"(/api/channels/(\d+)/alerts)", [](const httplib::Request& req, httplib::Response& res) {
        int channel_id = std::stoi(req.matches[1]);
        int limit = 100;
        int offset = 0;
        
        if (req.has_param("limit")) {
            limit = std::stoi(req.get_param_value("limit"));
        }
        if (req.has_param("offset")) {
            offset = std::stoi(req.get_param_value("offset"));
        }
        
        auto& alert_manager = AlertManager::getInstance();
        auto alerts = alert_manager.getAlertsByChannel(channel_id, limit, offset);
        
        nlohmann::json alert_list = nlohmann::json::array();
        for (const auto& alert : alerts) {
            nlohmann::json a;
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
        
        nlohmann::json response;
        response["success"] = true;
        response["alerts"] = alert_list;
        response["total"] = alert_manager.getAlertCountByChannel(channel_id);
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
    // 删除报警记录
    svr.Delete(R"(/api/alerts/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int alert_id = std::stoi(req.matches[1]);
        auto& alert_manager = AlertManager::getInstance();
        bool success = alert_manager.deleteAlert(alert_id);
        
        nlohmann::json response;
        response["success"] = success;
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
}

} // namespace detector_service

