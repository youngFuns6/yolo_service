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
void setupModelRoutes(crow::SimpleApp& app) {
    // 获取模型列表
    CROW_ROUTE(app, "/api/models").methods("GET"_method)
    ([]() {
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
        
        crow::json::wvalue response;
        response["success"] = true;
        crow::json::wvalue models_array;
        for (size_t i = 0; i < models.size(); i++) {
            models_array[i]["name"] = models[i]["name"];
            models_array[i]["path"] = models[i]["path"];
            models_array[i]["size"] = models[i]["size"];
            models_array[i]["modified"] = models[i]["modified"];
        }
        response["data"] = std::move(models_array);
        return crow::response(200, response);
    });
    
    // 上传模型
    CROW_ROUTE(app, "/api/models/upload").methods("POST"_method)
    ([](const crow::request& req) {
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
                crow::json::wvalue response;
                response["success"] = false;
                response["error"] = "无效的请求格式";
                return crow::response(400, response);
            }
            
            // 简化实现：直接保存文件（实际应该解析multipart）
            // 这里需要更完善的multipart解析，暂时返回错误提示
            crow::json::wvalue response;
            response["success"] = false;
            response["error"] = "模型上传功能需要完善multipart解析";
            return crow::response(501, response);
            
        } catch (const std::exception& e) {
            crow::json::wvalue response;
            response["success"] = false;
            response["error"] = std::string("上传失败: ") + e.what();
            return crow::response(500, response);
        }
    });
    
    // 删除模型
    CROW_ROUTE(app, "/api/models/<string>").methods("DELETE"_method)
    ([](const std::string& model_name) {
        try {
            std::string models_dir = getModelsDirectory();
            std::filesystem::path model_path = std::filesystem::path(models_dir) / model_name;
            
            // 安全检查：确保文件在models目录内
            std::filesystem::path canonical_model = std::filesystem::canonical(model_path);
            std::filesystem::path canonical_models_dir = std::filesystem::canonical(models_dir);
            
            if (canonical_model.string().find(canonical_models_dir.string()) != 0) {
                crow::json::wvalue response;
                response["success"] = false;
                response["error"] = "无效的模型路径";
                return crow::response(400, response);
            }
            
            if (!std::filesystem::exists(model_path)) {
                crow::json::wvalue response;
                response["success"] = false;
                response["error"] = "模型文件不存在";
                return crow::response(404, response);
            }
            
            std::filesystem::remove(model_path);
            
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "模型删除成功";
            return crow::response(200, response);
            
        } catch (const std::exception& e) {
            crow::json::wvalue response;
            response["success"] = false;
            response["error"] = std::string("删除失败: ") + e.what();
            return crow::response(500, response);
        }
    });
    
    // 获取类别列表
    CROW_ROUTE(app, "/api/models/classes").methods("GET"_method)
    ([]() {
        // 创建临时检测器以获取类别列表
        try {
            YOLOv11Detector detector("yolov11n.onnx", 0.5f, 0.4f, 640, 640);
            if (!detector.initialize()) {
                crow::json::wvalue response;
                response["success"] = false;
                response["error"] = "无法初始化检测器";
                return crow::response(500, response);
            }
            
            const auto& class_names = detector.getClassNames();
            crow::json::wvalue response;
            response["success"] = true;
            crow::json::wvalue classes_array;
            for (size_t i = 0; i < class_names.size(); i++) {
                classes_array[i]["id"] = static_cast<int>(i);
                classes_array[i]["name"] = class_names[i];
            }
            response["data"] = std::move(classes_array);
            return crow::response(200, response);
            
        } catch (const std::exception& e) {
            crow::json::wvalue response;
            response["success"] = false;
            response["error"] = std::string("获取类别列表失败: ") + e.what();
            return crow::response(500, response);
        }
    });
}

} // namespace detector_service

