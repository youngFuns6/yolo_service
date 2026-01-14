#pragma once

#include <onnxruntime/onnxruntime_cxx_api.h>
#include <mutex>
#include <memory>

namespace detector_service {

/**
 * @brief ONNX Runtime 环境单例
 * 确保整个程序中只有一个 Ort::Env 实例，避免 schema 重复注册问题
 */
class OnnxEnvSingleton {
public:
    /**
     * @brief 获取单例实例
     * 使用函数内静态变量确保线程安全的单例初始化
     */
    static Ort::Env& getInstance() {
        static Ort::Env instance(ORT_LOGGING_LEVEL_WARNING, "YOLOv11Detector");
        return instance;
    }
};

} // namespace detector_service

