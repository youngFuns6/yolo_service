#pragma once

#include <string>
#include <map>

namespace detector_service {

// 执行提供者类型
enum class ExecutionProvider {
    CPU,        // CPU 执行提供者（默认）
    CUDA,       // NVIDIA CUDA GPU
    CoreML,     // Apple CoreML (macOS/iOS)
    TensorRT,   // NVIDIA TensorRT GPU 优化
    ROCM,       // AMD ROCm GPU
    AUTO        // 自动选择（优先 GPU，回退到 CPU）
};

struct DetectorConfig {
    std::string model_path;
    float conf_threshold = 0.65f;
    float nms_threshold = 0.45f;
    int input_width = 640;
    int input_height = 640;
    ExecutionProvider execution_provider = ExecutionProvider::AUTO;  // 执行提供者，默认为自动选择
    int device_id = 0;  // GPU 设备 ID（仅对 CUDA/TensorRT/ROCM 有效）
};

struct DatabaseConfig {
    std::string db_path = "detector.db";
    int max_storage_days = 30;
};

struct ServerConfig {
    int http_port = 9090;
    std::string ws_path = "/ws";
    int max_connections = 100;
};

class Config {
public:
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    void loadFromFile(const std::string& config_path);
    void setDetectorConfig(const DetectorConfig& config) { detector_config_ = config; }
    void setDatabaseConfig(const DatabaseConfig& config) { database_config_ = config; }
    void setServerConfig(const ServerConfig& config) { server_config_ = config; }

    const DetectorConfig& getDetectorConfig() const { return detector_config_; }
    const DatabaseConfig& getDatabaseConfig() const { return database_config_; }
    const ServerConfig& getServerConfig() const { return server_config_; }

private:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    DetectorConfig detector_config_;
    DatabaseConfig database_config_;
    ServerConfig server_config_;
};

} // namespace detector_service

