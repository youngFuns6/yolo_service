#include "model_api.h"
#include "yolov11_detector.h"
#include "config.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace detector_service {

// 获取模型目录
std::string getModelsDirectory() {
    // 优先使用项目根目录的models文件夹
    std::filesystem::path models_dir = "models";
    if (std::filesystem::exists(models_dir) && std::filesystem::is_directory(models_dir)) {
        return models_dir.string();
    }
    // 如果不存在，使用当前目录
    return ".";
}

// 获取模型列表
void setupModelRoutes(httplib::Server& svr) {
    // 获取模型列表 - GET
    svr.Get("/api/models", [](const httplib::Request& req, httplib::Response& res) {
        std::vector<std::map<std::string, std::string>> models;
        std::string models_dir = getModelsDirectory();
        
        try {
            if (std::filesystem::exists(models_dir) && std::filesystem::is_directory(models_dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(models_dir)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        if (ext == ".onnx") {
                            std::map<std::string, std::string> model_info;
                            model_info["name"] = entry.path().filename().string();
                            model_info["path"] = entry.path().string();
                            model_info["size"] = std::to_string(std::filesystem::file_size(entry.path()));
                            model_info["modified"] = std::to_string(
                                std::chrono::duration_cast<std::chrono::seconds>(
                                    entry.last_write_time().time_since_epoch()).count()
                            );
                            models.push_back(model_info);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "获取模型列表失败: " << e.what() << std::endl;
        }
        
        nlohmann::json response;
        response["success"] = true;
        response["data"] = nlohmann::json::array();
        for (const auto& model : models) {
            nlohmann::json model_json;
            model_json["name"] = model.at("name");
            model_json["path"] = model.at("path");
            model_json["size"] = model.at("size");
            model_json["modified"] = model.at("modified");
            response["data"].push_back(model_json);
        }
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });
    
    // 上传模型 - POST
    svr.Post("/api/models/upload", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // 解析multipart/form-data
            auto& body = req.body;
            
            // 查找文件数据
            std::string boundary = "";
            auto content_type_it = req.headers.find("Content-Type");
            if (content_type_it != req.headers.end()) {
                std::string content_type = content_type_it->second;
                size_t boundary_pos = content_type.find("boundary=");
                if (boundary_pos != std::string::npos) {
                    boundary = content_type.substr(boundary_pos + 9);
                }
            }
            
            if (boundary.empty()) {
                nlohmann::json response;
                response["success"] = false;
                response["error"] = "无效的请求格式";
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            // 简化实现：直接保存文件（实际应该解析multipart）
            // 这里需要更完善的multipart解析，暂时返回错误提示
            nlohmann::json response;
            response["success"] = false;
            response["error"] = "模型上传功能需要完善multipart解析";
            res.status = 501;
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            nlohmann::json response;
            response["success"] = false;
            response["error"] = std::string("上传失败: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });
    
    // 删除模型 - DELETE
    svr.Delete(R"(/api/models/(.+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string model_name = req.matches[1];
            std::string models_dir = getModelsDirectory();
            std::filesystem::path model_path = std::filesystem::path(models_dir) / model_name;
            
            // 安全检查：确保文件在models目录内
            std::filesystem::path canonical_model = std::filesystem::canonical(model_path);
            std::filesystem::path canonical_models_dir = std::filesystem::canonical(models_dir);
            
            if (canonical_model.string().find(canonical_models_dir.string()) != 0) {
                nlohmann::json response;
                response["success"] = false;
                response["error"] = "无效的模型路径";
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            if (!std::filesystem::exists(model_path)) {
                nlohmann::json response;
                response["success"] = false;
                response["error"] = "模型文件不存在";
                res.status = 404;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            std::filesystem::remove(model_path);
            
            nlohmann::json response;
            response["success"] = true;
            response["message"] = "模型删除成功";
            res.status = 200;
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            nlohmann::json response;
            response["success"] = false;
            response["error"] = std::string("删除失败: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });
    
    // 获取类别列表 - GET
    svr.Get("/api/models/classes", [](const httplib::Request& req, httplib::Response& res) {
        // 创建临时检测器以获取类别列表
        try {
            YOLOv11Detector detector("yolov11n.onnx", 0.5f, 0.4f, 640, 640);
            if (!detector.initialize()) {
                nlohmann::json response;
                response["success"] = false;
                response["error"] = "无法初始化检测器";
                res.status = 500;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            const auto& class_names = detector.getClassNames();
            nlohmann::json response;
            response["success"] = true;
            response["data"] = nlohmann::json::array();
            for (size_t i = 0; i < class_names.size(); i++) {
                nlohmann::json class_json;
                class_json["id"] = static_cast<int>(i);
                class_json["name"] = class_names[i];
                response["data"].push_back(class_json);
            }
            res.status = 200;
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            nlohmann::json response;
            response["success"] = false;
            response["error"] = std::string("获取类别列表失败: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });
}

} // namespace detector_service
