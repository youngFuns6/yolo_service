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
     */
    static Ort::Env& getInstance() {
        static std::once_flag once_flag;
        std::call_once(once_flag, []() {
            instance_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "YOLOv11Detector");
        });
        return *instance_;
    }

private:
    static std::unique_ptr<Ort::Env> instance_;
};

} // namespace detector_service

