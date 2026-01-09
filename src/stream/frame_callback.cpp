#include "frame_callback.h"
#include "ws_handler.h"
#include "alert.h"
#include "channel.h"
#include "image_utils.h"
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace detector_service {

void processFrameCallback(int channel_id, const cv::Mat& frame, 
                         const std::vector<Detection>& detections) {
    auto& ws_handler = WebSocketHandler::getInstance();
    auto& alert_manager = AlertManager::getInstance();
    auto& channel_manager = ChannelManager::getInstance();
    
    // 发送帧到 WebSocket
    ws_handler.broadcastFrame(channel_id, frame);
    
    // 如果有检测结果，保存报警记录
    if (!detections.empty()) {
        auto channel = channel_manager.getChannel(channel_id);
        if (!channel) {
            return;
        }
        
        // 保存报警图片
        std::string alert_dir = "alerts";
        std::filesystem::create_directories(alert_dir);
        
        std::string timestamp = std::to_string(std::time(nullptr));
        std::string image_path = alert_dir + "/alert_" + std::to_string(channel_id) + 
                                "_" + timestamp + ".jpg";
        
        if (ImageUtils::saveImage(frame, image_path)) {
            std::string image_base64 = ImageUtils::matToBase64(frame);
            
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
            
            // 找到置信度最高的检测结果，用于确定报警类型
            const Detection* highest_conf_det = &detections[0];
            for (const auto& det : detections) {
                if (det.confidence > highest_conf_det->confidence) {
                    highest_conf_det = &det;
                }
            }
            
            // 如果有多个不同的类别，组合成报警类型（如 "person,car"）
            std::string alert_type;
            if (detections.size() == 1) {
                alert_type = detections[0].class_name;
            } else {
                // 收集所有不同的类别名称
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
                // 组合所有类别名称
                for (size_t i = 0; i < unique_classes.size(); ++i) {
                    if (i > 0) {
                        alert_type += ",";
                    }
                    alert_type += unique_classes[i];
                }
            }
            
            // 创建报警记录
            AlertRecord alert;
            alert.channel_id = channel_id;
            alert.channel_name = channel->name;
            alert.alert_type = alert_type;  // 使用模型输出的分类作为报警类型
            alert.image_path = image_path;
            alert.image_data = image_base64;
            alert.confidence = highest_conf_det->confidence;  // 使用最高置信度
            alert.detected_objects = detected_objects.dump();
            alert.bbox_x = highest_conf_det->bbox.x;
            alert.bbox_y = highest_conf_det->bbox.y;
            alert.bbox_w = highest_conf_det->bbox.width;
            alert.bbox_h = highest_conf_det->bbox.height;
            
            alert_manager.createAlert(alert);
            
            // 发送报警信息到 WebSocket
            AlertMessage alert_msg;
            alert_msg.channel_id = channel_id;
            alert_msg.channel_name = channel->name;
            alert_msg.alert_type = alert_type;  // 使用模型输出的分类作为报警类型
            alert_msg.image_base64 = image_base64;
            alert_msg.confidence = alert.confidence;
            alert_msg.detected_objects = alert.detected_objects;
            
            auto now = std::time(nullptr);
            auto tm = *std::localtime(&now);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            alert_msg.timestamp = oss.str();
            
            ws_handler.broadcastAlert(alert_msg);
        }
    }
}

} // namespace detector_service

