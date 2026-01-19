#pragma once

#include <memory>
#include <httplib.h>
#include "config.h"
#include "yolov11_detector.h"
#include "stream_manager.h"

namespace detector_service {

struct InitializationResult {
    bool success;
    std::shared_ptr<YOLOv11Detector> detector;
    std::string error_message;
};

// 初始化配置
void initializeConfig(Config& config);

// 初始化数据库
bool initializeDatabase(Config& config);

// 初始化检测器
InitializationResult initializeDetector(Config& config);

// 初始化流管理器
void initializeStreamManager(StreamManager& stream_manager);

// 初始化所有组件
struct AppContext {
    std::shared_ptr<YOLOv11Detector> detector;
    StreamManager* stream_manager;
};

bool initializeApplication(AppContext& context, StreamManager& stream_manager);

// 设置所有路由
void setupAllRoutes(httplib::Server& svr, const AppContext& context);

// 启动服务器
void startServer(httplib::Server& svr, const Config& config);

// 启动服务（封装所有初始化、路由设置和服务器启动逻辑）
int startService();

} // namespace detector_service

