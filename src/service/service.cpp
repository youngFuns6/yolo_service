#include "service.h"
#include "database.h"
#include "channel.h"
#include "frame_callback.h"
#include "channel_api.h"
#include "alert_api.h"
#include "algorithm_config_api.h"
#include "model_api.h"
#include "ws_api.h"
#include "report_config_api.h"
#include "gb28181_config_api.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace detector_service {

void initializeConfig(Config& config) {
    DetectorConfig detector_config;
    detector_config.model_path = "yolov11n.onnx";  // 默认模型路径
    config.setDetectorConfig(detector_config);
    
    DatabaseConfig db_config;
    config.setDatabaseConfig(db_config);
    
    ServerConfig server_config;
    config.setServerConfig(server_config);
}

bool initializeDatabase(Config& config) {
    auto& db = Database::getInstance();
    if (!db.initialize(config.getDatabaseConfig().db_path)) {
        std::cerr << "数据库初始化失败" << std::endl;
        return false;
    }
    
    return true;
}

InitializationResult initializeDetector(Config& config) {
    InitializationResult result;
    
    const auto& detector_config = config.getDetectorConfig();
    result.detector = std::make_shared<YOLOv11Detector>(
        detector_config.model_path,
        detector_config.conf_threshold,
        detector_config.nms_threshold,
        detector_config.input_width,
        detector_config.input_height,
        detector_config.execution_provider,
        detector_config.device_id
    );
    
    if (!result.detector->initialize()) {
        result.success = false;
        result.error_message = "检测器初始化失败，请确保模型文件存在: " + 
                              detector_config.model_path;
        return result;
    }
    
    result.success = true;
    return result;
}


bool initializeApplication(AppContext& context, StreamManager& stream_manager) {
    // 初始化配置
    auto& config = Config::getInstance();
    initializeConfig(config);
    
    // 初始化数据库
    if (!initializeDatabase(config)) {
        return false;
    }
    
    // 初始化 StreamManager（数据库已初始化，可以加载配置）
    stream_manager.initialize();
    
    // 初始化检测器
    auto result = initializeDetector(config);
    if (!result.success) {
        std::cerr << result.error_message << std::endl;
        return false;
    }
    context.detector = result.detector;
    
    // 设置 stream_manager 指针
    context.stream_manager = &stream_manager;
    
    return true;
}

// 获取文件的 MIME 类型
std::string getMimeType(const std::string& file_path) {
    std::string ext = std::filesystem::path(file_path).extension().string();
    if (ext == ".html") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".woff") return "font/woff";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".ttf") return "font/ttf";
    if (ext == ".eot") return "application/vnd.ms-fontobject";
    return "application/octet-stream";
}

// 提供静态文件服务
crow::response serveStaticFile(const std::string& file_path) {
    // 清理路径，移除前导斜杠
    std::string clean_path = file_path;
    if (!clean_path.empty() && clean_path[0] == '/') {
        clean_path = clean_path.substr(1);
    }
    
    // 如果路径为空，默认为 index.html
    if (clean_path.empty()) {
        clean_path = "index.html";
    }
    
    std::filesystem::path full_path = std::filesystem::path("website") / clean_path;
    
    // 如果是目录，尝试查找 index.html
    if (std::filesystem::exists(full_path) && std::filesystem::is_directory(full_path)) {
        full_path = full_path / "index.html";
    }
    
    // 检查文件是否存在
    if (!std::filesystem::exists(full_path) || !std::filesystem::is_regular_file(full_path)) {
        return crow::response(404, "File not found");
    }
    
    // 读取文件内容
    std::ifstream file(full_path.string(), std::ios::binary);
    if (!file.is_open()) {
        return crow::response(500, "Failed to open file");
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // 创建响应
    crow::response res;
    res.code = 200;
    res.set_header("Content-Type", getMimeType(full_path.string()));
    res.body = content;
    
    return res;
}

void setupAllRoutes(crow::SimpleApp& app, const AppContext& context) {
    // 先设置 API 路由和 WebSocket 路由（优先级更高）
    setupChannelRoutes(app, context.detector, context.stream_manager);
    setupAlertRoutes(app);
    setupReportConfigRoutes(app);
    setupAlgorithmConfigRoutes(app);
    setupGB28181ConfigRoutes(app);
    setupModelRoutes(app);
    setupWebSocketRoutes(app);
    
    // 设置根路由 - 返回 website/index.html 或 API 信息
    CROW_ROUTE(app, "/")
    ([]() {
        return serveStaticFile("");
    });
    
    // 设置静态文件路由 - 处理 website 目录下的所有文件（作为 fallback）
    // 注意：这个路由应该在 API 路由之后注册，这样 API 路由会优先匹配
    CROW_ROUTE(app, "/<path>")
    ([](const crow::request& req, const std::string& path) {
        // 如果路径以 api/ 或 ws 开头，不处理（应该由 API/WebSocket 路由处理）
        // 如果这些路由没有匹配，说明路径不存在，返回 404
        if (path.find("api/") == 0 || path == "api" || path.find("ws") == 0) {
            return crow::response(404);
        }
        
        return serveStaticFile(path);
    });
}

void startServer(crow::SimpleApp& app, const Config& config) {
    int port = config.getServerConfig().http_port;
    app.port(port).multithreaded().run();
}

void startEnabledChannels(StreamManager* stream_manager, 
                         std::shared_ptr<YOLOv11Detector> detector) {
    if (!stream_manager || !detector) {
        std::cerr << "StreamManager 或 Detector 为空，无法启动已启用的通道" << std::endl;
        return;
    }
    
    auto& channel_manager = ChannelManager::getInstance();
    auto channels = channel_manager.getAllChannels();
    
    int started_count = 0;
    for (const auto& channel : channels) {
        if (channel && channel->enabled.load()) {
            if (stream_manager->startAnalysis(channel->id, channel, detector)) {
                started_count++;
            } else {
                std::cerr << "通道 " << channel->id << " 启动失败" << std::endl;
            }
        }
    }
}

int startService() {
    // 先初始化应用（包括数据库）
    AppContext context;
    StreamManager stream_manager;
    if (!initializeApplication(context, stream_manager)) {
        return 1;
    }
    
    // 注意：StreamManager 在 initializeApplication 之后创建，
    // 这样数据库已经初始化，可以正确加载 GB28181 配置
    
    // 设置帧回调函数
    stream_manager.setFrameCallback(processFrameCallback);
    
    // 创建 Crow 应用并设置路由
    crow::SimpleApp app;
    setupAllRoutes(app, context);
    
    // 启动所有已启用的通道
    startEnabledChannels(context.stream_manager, context.detector);
    
    // 启动服务器
    auto& config = Config::getInstance();
    startServer(app, config);
    
    return 0;
}

} // namespace detector_service

