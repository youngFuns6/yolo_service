#include "frame_callback.h"
#include "ws_handler.h"
#include "alert.h"
#include "channel.h"
#include "image_utils.h"
#include "algorithm_config.h"
#include "report_config.h"
#include "report_service.h"
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <future>

namespace detector_service {

void processFrameCallback(int channel_id, const cv::Mat& frame, 
                         const std::vector<Detection>& detections) {
    auto& ws_handler = WebSocketHandler::getInstance();
    auto& alert_manager = AlertManager::getInstance();
    auto& channel_manager = ChannelManager::getInstance();
    auto& config_manager = AlgorithmConfigManager::getInstance();
    
    // 发送帧到 WebSocket
    ws_handler.broadcastFrame(channel_id, frame);
    
    // 如果没有检测结果，直接返回
    if (detections.empty()) {
        return;
    }
    
    // 加载通道的算法配置（包含告警规则）
    AlgorithmConfig config;
    if (!config_manager.getAlgorithmConfig(channel_id, config)) {
        // 如果加载失败，使用默认配置
        config = config_manager.getDefaultConfig(channel_id);
    }
    
    // 检查告警规则并触发告警
    int frame_width = frame.cols;
    int frame_height = frame.rows;
    
    for (const auto& rule : config.alert_rules) {
        // 检查是否应该触发告警
        if (!config_manager.shouldTriggerAlert(rule, detections, config.rois, frame_width, frame_height)) {
            continue;
        }
        
        // 检查告警抑制（时间窗口）
        if (alert_manager.isAlertSuppressed(channel_id, rule.id, rule.suppression_window_seconds)) {
            continue;  // 在抑制窗口内，跳过
        }
        
        // 获取满足规则的检测结果
        std::vector<Detection> matched_detections = 
            config_manager.evaluateAlertRule(rule, detections, config.rois, frame_width, frame_height);
        
        if (matched_detections.empty()) {
            continue;
        }
        
        auto channel = channel_manager.getChannel(channel_id);
        if (!channel) {
            continue;
        }
        
        // 构建检测对象 JSON（只包含匹配的检测结果）
        nlohmann::json detected_objects = nlohmann::json::array();
        for (const auto& det : matched_detections) {
            nlohmann::json obj;
            obj["class_id"] = det.class_id;
            obj["class_name"] = det.class_name;
            obj["confidence"] = det.confidence;
            obj["bbox"] = {
                {"x", det.bbox.x},
                {"y", det.bbox.y},
                {"w", det.bbox.width},
                {"h", det.bbox.height}
            };
            detected_objects.push_back(obj);
        }
        
        // 找到置信度最高的检测结果
        const Detection* highest_conf_det = &matched_detections[0];
        for (const auto& det : matched_detections) {
            if (det.confidence > highest_conf_det->confidence) {
                highest_conf_det = &det;
            }
        }
        
        // 构建告警类型：使用规则名称或类别名称
        std::string alert_type = rule.name;
        if (alert_type.empty()) {
            // 如果没有规则名称，使用类别名称
            if (matched_detections.size() == 1) {
                alert_type = matched_detections[0].class_name;
            } else {
                // 收集所有不同的类别名称
                std::vector<std::string> unique_classes;
                for (const auto& det : matched_detections) {
                    bool found = false;
                    for (const auto& cls : unique_classes) {
                        if (cls == det.class_name) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        unique_classes.push_back(det.class_name);
                    }
                }
                // 组合所有类别名称
                for (size_t i = 0; i < unique_classes.size(); ++i) {
                    if (i > 0) {
                        alert_type += ",";
                    }
                    alert_type += unique_classes[i];
                }
            }
        }
        
        // 快速生成低质量 Base64 图片用于立即发送到 WebSocket（减少延迟）
        std::string quick_image_base64 = ImageUtils::matToBase64(frame, ".jpg", 50);
        
        // 立即发送报警信息到 WebSocket（使用低质量图片以减少延迟）
        AlertMessage alert_msg;
        alert_msg.channel_id = channel_id;
        alert_msg.channel_name = channel->name;
        alert_msg.alert_type = alert_type;
        alert_msg.image_base64 = quick_image_base64;
        alert_msg.confidence = highest_conf_det->confidence;
        alert_msg.detected_objects = detected_objects.dump();
        
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        alert_msg.timestamp = oss.str();
        
        ws_handler.broadcastAlert(alert_msg);
        
        // 在后台线程中处理耗时的图片保存、高质量编码和数据库操作
        // 注意：需要克隆 frame，因为原始 frame 可能在函数返回后被释放
        cv::Mat frame_clone = frame.clone();
        std::thread([channel_id, frame_clone, matched_detections, rule, channel, alert_type, 
                     highest_conf_det, detected_objects]() {
            auto& alert_manager = AlertManager::getInstance();
            auto& report_service = ReportService::getInstance();
            // 保存报警图片
            std::string alert_dir = "alerts";
            std::filesystem::create_directories(alert_dir);
            
            std::string timestamp = std::to_string(std::time(nullptr));
            std::string image_path = alert_dir + "/alert_" + std::to_string(channel_id) + 
                                    "_" + std::to_string(rule.id) + "_" + timestamp + ".jpg";
            
            if (ImageUtils::saveImage(frame_clone, image_path)) {
                // 生成高质量 Base64 图片用于数据库存储
                std::string image_base64 = ImageUtils::matToBase64(frame_clone, ".jpg", 90);
                
                // 创建报警记录
                AlertRecord alert;
                alert.channel_id = channel_id;
                alert.channel_name = channel->name;
                alert.alert_type = alert_type;
                alert.alert_rule_id = rule.id;
                alert.alert_rule_name = rule.name;
                alert.image_path = image_path;
                alert.image_data = image_base64;
                alert.confidence = highest_conf_det->confidence;
                alert.detected_objects = detected_objects.dump();
                alert.bbox_x = highest_conf_det->bbox.x;
                alert.bbox_y = highest_conf_det->bbox.y;
                alert.bbox_w = highest_conf_det->bbox.width;
                alert.bbox_h = highest_conf_det->bbox.height;
                alert.report_status = "pending";
                alert.report_url = "";
                
                int alert_id = alert_manager.createAlert(alert);
                alert.id = alert_id;
                
                // 记录告警触发时间（用于抑制机制）
                alert_manager.recordAlertTrigger(channel_id, rule.id);
                
                // 如果通道的上报开关开启，进行上报
                if (channel->report_enabled.load()) {
                    auto& report_config_manager = ReportConfigManager::getInstance();
                    const auto& report_config = report_config_manager.getReportConfig();
                    
                    if (report_config.enabled.load()) {
                        // 设置上报地址
                        std::string report_url;
                        if (report_config.type == ReportType::HTTP) {
                            report_url = report_config.http_url;
                        } else if (report_config.type == ReportType::MQTT) {
                            report_url = report_config.mqtt_broker + ":" + std::to_string(report_config.mqtt_port) + "/" + report_config.mqtt_topic;
                        }
                        
                        // 执行上报（异步非阻塞，立即返回）
                        // 实际上报结果由后台线程处理并更新数据库状态
                        report_service.reportAlert(alert, report_config);
                    }
                }
            }
        }).detach();  // 分离线程，不等待完成
    }
    
    // 如果没有告警规则，使用旧的逻辑（向后兼容）
    if (config.alert_rules.empty() && !detections.empty()) {
        auto channel = channel_manager.getChannel(channel_id);
        if (!channel) {
            return;
        }
        
        // 构建检测对象 JSON
        nlohmann::json detected_objects = nlohmann::json::array();
        for (const auto& det : detections) {
            nlohmann::json obj;
            obj["class_id"] = det.class_id;
            obj["class_name"] = det.class_name;
            obj["confidence"] = det.confidence;
            obj["bbox"] = {
                {"x", det.bbox.x},
                {"y", det.bbox.y},
                {"w", det.bbox.width},
                {"h", det.bbox.height}
            };
            detected_objects.push_back(obj);
        }
        
        // 找到置信度最高的检测结果
        const Detection* highest_conf_det = &detections[0];
        for (const auto& det : detections) {
            if (det.confidence > highest_conf_det->confidence) {
                highest_conf_det = &det;
            }
        }
        
        // 构建告警类型
        std::string alert_type;
        if (detections.size() == 1) {
            alert_type = detections[0].class_name;
        } else {
            std::vector<std::string> unique_classes;
            for (const auto& det : detections) {
                bool found = false;
                for (const auto& cls : unique_classes) {
                    if (cls == det.class_name) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    unique_classes.push_back(det.class_name);
                }
            }
            for (size_t i = 0; i < unique_classes.size(); ++i) {
                if (i > 0) {
                    alert_type += ",";
                }
                alert_type += unique_classes[i];
            }
        }
        
        // 快速生成低质量 Base64 图片用于立即发送到 WebSocket（减少延迟）
        std::string quick_image_base64 = ImageUtils::matToBase64(frame, ".jpg", 50);
        
        // 立即发送报警信息到 WebSocket（使用低质量图片以减少延迟）
        AlertMessage alert_msg;
        alert_msg.channel_id = channel_id;
        alert_msg.channel_name = channel->name;
        alert_msg.alert_type = alert_type;
        alert_msg.image_base64 = quick_image_base64;
        alert_msg.confidence = highest_conf_det->confidence;
        alert_msg.detected_objects = detected_objects.dump();
        
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        alert_msg.timestamp = oss.str();
        
        ws_handler.broadcastAlert(alert_msg);
        
        // 在后台线程中处理耗时的图片保存、高质量编码和数据库操作
        cv::Mat frame_clone = frame.clone();
        std::thread([channel_id, frame_clone, detections, channel, alert_type, 
                     highest_conf_det, detected_objects]() {
            auto& alert_manager = AlertManager::getInstance();
            auto& report_service = ReportService::getInstance();
            
            // 保存报警图片
            std::string alert_dir = "alerts";
            std::filesystem::create_directories(alert_dir);
            
            std::string timestamp = std::to_string(std::time(nullptr));
            std::string image_path = alert_dir + "/alert_" + std::to_string(channel_id) + 
                                    "_" + timestamp + ".jpg";
            
            if (ImageUtils::saveImage(frame_clone, image_path)) {
                // 生成高质量 Base64 图片用于数据库存储
                std::string image_base64 = ImageUtils::matToBase64(frame_clone, ".jpg", 90);
                
                // 创建报警记录（没有告警规则ID）
                AlertRecord alert;
                alert.channel_id = channel_id;
                alert.channel_name = channel->name;
                alert.alert_type = alert_type;
                alert.alert_rule_id = 0;  // 没有规则
                alert.alert_rule_name = "";  // 没有规则名称
                alert.image_path = image_path;
                alert.image_data = image_base64;
                alert.confidence = highest_conf_det->confidence;
                alert.detected_objects = detected_objects.dump();
                alert.bbox_x = highest_conf_det->bbox.x;
                alert.bbox_y = highest_conf_det->bbox.y;
                alert.bbox_w = highest_conf_det->bbox.width;
                alert.bbox_h = highest_conf_det->bbox.height;
                alert.report_status = "pending";
                alert.report_url = "";
                
                int alert_id = alert_manager.createAlert(alert);
                alert.id = alert_id;
                
                // 如果通道的上报开关开启，进行上报
                if (channel->report_enabled.load()) {
                    auto& report_config_manager = ReportConfigManager::getInstance();
                    const auto& report_config = report_config_manager.getReportConfig();
                    
                    if (report_config.enabled.load()) {
                        // 设置上报地址
                        std::string report_url;
                        if (report_config.type == ReportType::HTTP) {
                            report_url = report_config.http_url;
                        } else if (report_config.type == ReportType::MQTT) {
                            report_url = report_config.mqtt_broker + ":" + std::to_string(report_config.mqtt_port) + "/" + report_config.mqtt_topic;
                        }
                        
                        // 执行上报（异步非阻塞，立即返回）
                        // 实际上报结果由后台线程处理并更新数据库状态
                        report_service.reportAlert(alert, report_config);
                    }
                }
            }
        }).detach();  // 分离线程，不等待完成
    }
}

} // namespace detector_service

