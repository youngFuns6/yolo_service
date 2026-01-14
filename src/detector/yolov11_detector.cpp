#include "yolov11_detector.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace detector_service {

YOLOv11Detector::YOLOv11Detector(const std::string& model_path,
                                 float conf_threshold,
                                 float nms_threshold,
                                 int input_width,
                                 int input_height,
                                 ExecutionProvider execution_provider,
                                 int device_id)
    : model_path_(model_path),
      conf_threshold_(conf_threshold),
      nms_threshold_(nms_threshold),
      input_width_(input_width),
      input_height_(input_height),
      execution_provider_(execution_provider),
      device_id_(device_id),
      env_(OnnxEnvSingleton::getInstance()) {  // 使用单例，确保整个程序只有一个Ort::Env实例
}

YOLOv11Detector::~YOLOv11Detector() {
}

bool YOLOv11Detector::initialize() {
    try {
        session_options_.SetIntraOpNumThreads(1);
        session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        
        // 配置执行提供者
        configureExecutionProvider();
        
        if (!loadModel()) {
            return false;
        }
        
        loadClassNames();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "初始化检测器失败: " << e.what() << std::endl;
        return false;
    }
}

void YOLOv11Detector::configureExecutionProvider() {
    ExecutionProvider provider = execution_provider_;
    
    // 如果是 AUTO，自动选择最佳执行提供者
    if (provider == ExecutionProvider::AUTO) {
        provider = selectExecutionProvider();
        execution_provider_ = provider;  // 更新为实际选择的提供者
    }
    
    try {
        switch (provider) {
            case ExecutionProvider::CUDA: {
                OrtCUDAProviderOptions cuda_options{};
                cuda_options.device_id = device_id_;
                cuda_options.arena_extend_strategy = 0;  // kNextPowerOfTwo
                cuda_options.gpu_mem_limit = 2 * 1024 * 1024 * 1024;  // 2GB
                cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
                cuda_options.do_copy_in_default_stream = 1;
                cuda_options.has_user_compute_stream = 0;
                cuda_options.default_memory_arena_cfg = nullptr;
                session_options_.AppendExecutionProvider_CUDA(cuda_options);
                std::cout << "[检测器] 使用 CUDA 执行提供者 (设备 ID: " << device_id_ << ")" << std::endl;
                break;
            }
            case ExecutionProvider::CoreML: {
                #ifdef __APPLE__
                // CoreML 在 macOS 上可用
                // 使用 AppendExecutionProvider 方法添加 CoreML 执行提供者
                session_options_.AppendExecutionProvider("CoreMLExecutionProvider");
                std::cout << "[检测器] 使用 CoreML 执行提供者" << std::endl;
                #else
                std::cerr << "[检测器] 警告: CoreML 仅在 macOS 上可用，回退到 CPU" << std::endl;
                #endif
                break;
            }
            case ExecutionProvider::TensorRT: {
                OrtTensorRTProviderOptions trt_options{};
                trt_options.device_id = device_id_;
                trt_options.has_user_compute_stream = 0;
                trt_options.trt_max_partition_iterations = 1000;
                trt_options.trt_min_subgraph_size = 1;
                trt_options.trt_max_workspace_size = 2 * 1024 * 1024 * 1024;  // 2GB
                trt_options.trt_fp16_enable = 0;  // 可以根据需要启用 FP16
                trt_options.trt_int8_enable = 0;
                trt_options.trt_int8_calibration_table_name = "";
                trt_options.trt_int8_use_native_calibration_table = 0;
                trt_options.trt_dla_enable = 0;
                trt_options.trt_dla_core = 0;
                trt_options.trt_dump_subgraphs = 0;
                trt_options.trt_engine_cache_enable = 0;
                trt_options.trt_engine_cache_path = "";
                trt_options.trt_engine_decryption_enable = 0;
                trt_options.trt_engine_decryption_lib_path = "";
                trt_options.trt_force_sequential_engine_build = 0;
                session_options_.AppendExecutionProvider_TensorRT(trt_options);
                std::cout << "[检测器] 使用 TensorRT 执行提供者 (设备 ID: " << device_id_ << ")" << std::endl;
                break;
            }
            case ExecutionProvider::ROCM: {
                OrtROCMProviderOptions rocm_options{};
                rocm_options.device_id = device_id_;
                rocm_options.miopen_conv_exhaustive_search = 0;
                rocm_options.do_copy_in_default_stream = 1;
                session_options_.AppendExecutionProvider_ROCM(rocm_options);
                std::cout << "[检测器] 使用 ROCm 执行提供者 (设备 ID: " << device_id_ << ")" << std::endl;
                break;
            }
            case ExecutionProvider::CPU:
            default:
                std::cout << "[检测器] 使用 CPU 执行提供者" << std::endl;
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "[检测器] 配置执行提供者失败: " << e.what() 
                  << "，回退到 CPU" << std::endl;
        // 发生错误时回退到 CPU
    }
}

ExecutionProvider YOLOv11Detector::selectExecutionProvider() {
    // 获取可用的执行提供者
    std::vector<std::string> available_providers = getAvailableProviders();
    
    std::cout << "[检测器] 可用的执行提供者: ";
    for (const auto& provider : available_providers) {
        std::cout << provider << " ";
    }
    std::cout << std::endl;
    
    // 按优先级选择：CoreML (macOS) > CUDA > TensorRT > ROCm > CPU
    #ifdef __APPLE__
    for (const auto& provider : available_providers) {
        if (provider == "CoreMLExecutionProvider") {
            std::cout << "[检测器] 自动选择: CoreML" << std::endl;
            return ExecutionProvider::CoreML;
        }
    }
    #endif
    
    for (const auto& provider : available_providers) {
        if (provider == "CUDAExecutionProvider") {
            std::cout << "[检测器] 自动选择: CUDA" << std::endl;
            return ExecutionProvider::CUDA;
        }
    }
    
    for (const auto& provider : available_providers) {
        if (provider == "TensorrtExecutionProvider") {
            std::cout << "[检测器] 自动选择: TensorRT" << std::endl;
            return ExecutionProvider::TensorRT;
        }
    }
    
    for (const auto& provider : available_providers) {
        if (provider == "ROCMExecutionProvider") {
            std::cout << "[检测器] 自动选择: ROCm" << std::endl;
            return ExecutionProvider::ROCM;
        }
    }
    
    std::cout << "[检测器] 自动选择: CPU" << std::endl;
    return ExecutionProvider::CPU;
}

std::vector<std::string> YOLOv11Detector::getAvailableProviders() {
    try {
        return Ort::GetAvailableProviders();
    } catch (const std::exception& e) {
        std::cerr << "[检测器] 获取可用执行提供者失败: " << e.what() << std::endl;
        return {"CPUExecutionProvider"};  // 至少返回 CPU
    }
}

bool YOLOv11Detector::loadModel() {
    try {
        session_ = std::make_unique<Ort::Session>(env_, model_path_.c_str(), session_options_);
        
        Ort::AllocatorWithDefaultOptions allocator;
        
        // 获取输入信息
        size_t num_input_nodes = session_->GetInputCount();
        input_names_.reserve(num_input_nodes);
        input_shapes_.reserve(num_input_nodes);
        
        for (size_t i = 0; i < num_input_nodes; i++) {
            auto input_name = session_->GetInputNameAllocated(i, allocator);
            input_names_.push_back(std::string(input_name.get()));
            
            auto input_type_info = session_->GetInputTypeInfo(i);
            auto tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
            auto shape = tensor_info.GetShape();
            input_shapes_.push_back(shape);
        }
        
        // 获取输出信息
        size_t num_output_nodes = session_->GetOutputCount();
        output_names_.reserve(num_output_nodes);
        output_shapes_.reserve(num_output_nodes);
        
        for (size_t i = 0; i < num_output_nodes; i++) {
            auto output_name = session_->GetOutputNameAllocated(i, allocator);
            output_names_.push_back(std::string(output_name.get()));
            
            auto output_type_info = session_->GetOutputTypeInfo(i);
            auto tensor_info = output_type_info.GetTensorTypeAndShapeInfo();
            auto shape = tensor_info.GetShape();
            output_shapes_.push_back(shape);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "加载模型失败: " << e.what() << std::endl;
        return false;
    }
}

void YOLOv11Detector::loadClassNames() {
    // COCO 数据集的类别名称
    class_names_ = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
        "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
        "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
        "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
        "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
        "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
        "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
        "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
        "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
        "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier",
        "toothbrush"
    };
}

cv::Mat YOLOv11Detector::preprocess(const cv::Mat& image, float& scale, int& pad_x, int& pad_y) {
    cv::Mat resized, rgb;
    
    // 转换为 RGB
    cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    
    // 计算缩放比例，保持宽高比
    scale = std::min(static_cast<float>(input_width_) / image.cols,
                     static_cast<float>(input_height_) / image.rows);
    int new_width = static_cast<int>(image.cols * scale);
    int new_height = static_cast<int>(image.rows * scale);
    
    cv::resize(rgb, resized, cv::Size(new_width, new_height));
    
    // 计算 padding（确保对称）
    pad_x = (input_width_ - new_width) / 2;
    pad_y = (input_height_ - new_height) / 2;
    
    // 计算右侧和底部的 padding（处理奇数情况）
    int pad_x_right = input_width_ - new_width - pad_x;
    int pad_y_bottom = input_height_ - new_height - pad_y;
    
    // 创建填充后的图像（top, bottom, left, right）
    cv::Mat padded;
    cv::copyMakeBorder(resized, padded, pad_y, pad_y_bottom, pad_x, pad_x_right,
                      cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));
    
    // 归一化到 [0, 1]
    cv::Mat normalized;
    padded.convertTo(normalized, CV_32F, 1.0 / 255.0);
    
    return normalized;
}

std::vector<Detection> YOLOv11Detector::detect(const cv::Mat& image) {
    if (!session_) {
        return {};
    }
    
    float scale;
    int pad_x, pad_y;
    cv::Mat preprocessed = preprocess(image, scale, pad_x, pad_y);
    cv::Size original_size(image.cols, image.rows);
    
    // 准备输入张量
    std::vector<int64_t> input_shape = {1, 3, input_height_, input_width_};
    size_t input_tensor_size = 1 * 3 * input_height_ * input_width_;
    std::vector<float> input_tensor_values(input_tensor_size);
    
    // 将图像数据转换为 CHW 格式
    for (int c = 0; c < 3; c++) {
        for (int h = 0; h < input_height_; h++) {
            for (int w = 0; w < input_width_; w++) {
                input_tensor_values[c * input_height_ * input_width_ + h * input_width_ + w] =
                    preprocessed.at<cv::Vec3f>(h, w)[c];
            }
        }
    }
    
    // 创建输入张量
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor_values.data(), input_tensor_size,
        input_shape.data(), input_shape.size());
    
    // 运行推理
    // 将字符串向量转换为 const char* 数组
    std::vector<const char*> input_name_ptrs;
    input_name_ptrs.reserve(input_names_.size());
    for (const auto& name : input_names_) {
        input_name_ptrs.push_back(name.c_str());
    }
    
    std::vector<const char*> output_name_ptrs;
    output_name_ptrs.reserve(output_names_.size());
    for (const auto& name : output_names_) {
        output_name_ptrs.push_back(name.c_str());
    }
    
    auto output_tensors = session_->Run(Ort::RunOptions{nullptr},
                                       input_name_ptrs.data(), &input_tensor, 1,
                                       output_name_ptrs.data(), output_name_ptrs.size());
    
    // 获取输出数据
    float* output_data = output_tensors[0].GetTensorMutableData<float>();
    auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
    
    size_t output_size = 1;
    for (auto dim : output_shape) {
        output_size *= dim;
    }
    
    std::vector<float> output(output_data, output_data + output_size);
    
    // 后处理，传递实际的输出形状和预处理信息
    return postprocess(output, original_size, output_shape, scale, pad_x, pad_y);
}

std::vector<Detection> YOLOv11Detector::postprocess(const std::vector<float>& output,
                                                    const cv::Size& original_size,
                                                    const std::vector<int64_t>& output_shape,
                                                    float scale, int pad_x, int pad_y) {
    std::vector<Detection> detections;
    
    // 获取输出形状信息
    if (output_shape.empty()) {
        std::cerr << "输出形状为空" << std::endl;
        return {};
    }
    
    int64_t num_anchors;
    int64_t features;
    bool has_objectness = false;
    bool is_transposed = false;  // 标记是否为转置格式 [batch, features, num_detections]
    int num_classes = 80;
    
    // 处理 YOLOv11 输出格式（3D格式）
    if (output_shape.size() != 3) {
        std::cerr << "错误: 不支持的输出形状维度: " << output_shape.size() << std::endl;
        return {};
    }
    
    // 判断是否为转置格式: [batch, features, num_detections] 或 [batch, num_detections, features]
    if (output_shape[1] < output_shape[2]) {
        // 转置格式: [1, 84, 8400]
        features = output_shape[1];
        num_anchors = output_shape[2];
        is_transposed = true;
    } else {
        // 标准格式: [1, 8400, 84]
        num_anchors = output_shape[1];
        features = output_shape[2];
        is_transposed = false;
    }
    has_objectness = (features == 85);
    
    for (int64_t i = 0; i < num_anchors; i++) {
        // 根据数据布局访问数据
        float center_x, center_y, width, height;
        
        if (is_transposed) {
            // 转置格式 [1, 84, 8400]: 第 i 个检测框的第 j 个特征在 output[j * 8400 + i]
            center_x = output[0 * num_anchors + i];
            center_y = output[1 * num_anchors + i];
            width = output[2 * num_anchors + i];
            height = output[3 * num_anchors + i];
        } else {
            // 标准格式 [1, 8400, 84]: 第 i 个检测框的第 j 个特征在 output[i * 84 + j]
            int offset = i * features;
            center_x = output[offset + 0];
            center_y = output[offset + 1];
            width = output[offset + 2];
            height = output[offset + 3];
        }
        
        // 获取 objectness（如果有）
        float objectness = 0.0f;
        if (has_objectness) {
            // 格式: [x, y, w, h, objectness, class_scores...]
            if (is_transposed) {
                objectness = output[4 * num_anchors + i];
            } else {
                int offset = i * features;
                objectness = output[offset + 4];
            }
            
            // 如果 objectness 太低，跳过
            if (objectness < 0.1f) {
                continue;
            }
        }
        
        // 找到最大类别 logit（不应用 sigmoid，提高效率）
        int class_start = has_objectness ? 5 : 4;
        float max_logit = -std::numeric_limits<float>::max();
        int class_id = 0;
        for (int j = 0; j < num_classes; j++) {
            float logit;
            if (is_transposed) {
                // 转置格式: 类别分数在 output[(class_start + j) * num_anchors + i]
                logit = output[(class_start + j) * num_anchors + i];
            } else {
                // 标准格式: 类别分数在 output[i * features + class_start + j]
                int offset = i * features;
                logit = output[offset + class_start + j];
            }
            
            if (logit > max_logit) {
                max_logit = logit;
                class_id = j;
            }
        }
        
        // 对最大 logit 应用 sigmoid 转换为概率
        float class_conf;
        if (max_logit >= 0.0f) {
            class_conf = 1.0f / (1.0f + std::exp(-max_logit));
        } else {
            float exp_logit = std::exp(max_logit);
            class_conf = exp_logit / (1.0f + exp_logit);
        }
        
        // 计算最终置信度
        float confidence;
        if (has_objectness) {
            // 对 objectness 应用 sigmoid
            float obj_sigmoid;
            if (objectness >= 0.0f) {
                obj_sigmoid = 1.0f / (1.0f + std::exp(-objectness));
            } else {
                float exp_obj = std::exp(objectness);
                obj_sigmoid = exp_obj / (1.0f + exp_obj);
            }
            confidence = obj_sigmoid * class_conf;
        } else {
            // 直接使用类别概率作为置信度
            confidence = class_conf;
        }
        
        // 应用置信度阈值过滤（YOLOv11 官方建议使用 0.5 或更高的阈值）
        float effective_threshold = std::max(conf_threshold_, 0.5f);
        if (confidence < effective_threshold) {
            continue;
        }
        
        // 转换坐标：去除 padding 并通过 scale 转换回原图
        float orig_center_x = (center_x - pad_x) / scale;
        float orig_center_y = (center_y - pad_y) / scale;
        float orig_width = width / scale;
        float orig_height = height / scale;
        
        // 转换为左上角坐标并限制在图像范围内
        float x = std::max(0.0f, std::min(static_cast<float>(original_size.width), 
                                         orig_center_x - orig_width / 2.0f));
        float y = std::max(0.0f, std::min(static_cast<float>(original_size.height), 
                                         orig_center_y - orig_height / 2.0f));
        float w = std::max(1.0f, std::min(static_cast<float>(original_size.width) - x, orig_width));
        float h = std::max(1.0f, std::min(static_cast<float>(original_size.height) - y, orig_height));
        
        // 转换为整数坐标
        int x_int = static_cast<int>(x);
        int y_int = static_cast<int>(y);
        int w_int = static_cast<int>(w);
        int h_int = static_cast<int>(h);
        
        Detection detection;
        detection.class_id = class_id;
        detection.class_name = (class_id < static_cast<int>(class_names_.size())) ? class_names_[class_id] : "unknown";
        detection.confidence = confidence;
        detection.bbox = cv::Rect(x_int, y_int, w_int, h_int);
        
        detections.push_back(detection);
    }
    
    return applyNMS(detections);
}

std::vector<Detection> YOLOv11Detector::applyNMS(const std::vector<Detection>& detections) {
    if (detections.empty()) {
        return {};
    }
    
    // 计算 IoU 并应用 NMS
    std::vector<Detection> result;
    std::vector<bool> suppressed(detections.size(), false);
    
    // 按置信度排序
    std::vector<size_t> indices(detections.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
              [&detections](size_t a, size_t b) {
                  return detections[a].confidence > detections[b].confidence;
              });
    
    for (size_t i = 0; i < indices.size(); i++) {
        if (suppressed[indices[i]]) {
            continue;
        }
        
        result.push_back(detections[indices[i]]);
        
        const auto& bbox1 = detections[indices[i]].bbox;
        float area1 = static_cast<float>(bbox1.width * bbox1.height);
        
        if (area1 <= 0.0f) {
            result.pop_back();
            continue;
        }
        
        for (size_t j = i + 1; j < indices.size(); j++) {
            if (suppressed[indices[j]]) {
                continue;
            }
            
            const auto& bbox2 = detections[indices[j]].bbox;
            float area2 = static_cast<float>(bbox2.width * bbox2.height);
            
            if (area2 <= 0.0f) {
                continue;
            }
            
            // 计算 IoU
            int x1 = std::max(bbox1.x, bbox2.x);
            int y1 = std::max(bbox1.y, bbox2.y);
            int x2 = std::min(bbox1.x + bbox1.width, bbox2.x + bbox2.width);
            int y2 = std::min(bbox1.y + bbox1.height, bbox2.y + bbox2.height);
            
            if (x2 <= x1 || y2 <= y1) {
                continue;  // 没有重叠
            }
            
            float intersection = static_cast<float>((x2 - x1) * (y2 - y1));
            float union_area = area1 + area2 - intersection;
            
            // 避免除零
            if (union_area <= 0.0f) {
                continue;
            }
            
            float iou = intersection / union_area;
            
            // 如果 IoU 超过阈值，抑制该检测框
            if (iou > nms_threshold_) {
                suppressed[indices[j]] = true;
            }
        }
    }
    
    return result;
}

std::vector<Detection> YOLOv11Detector::applyFilters(const std::vector<Detection>& detections,
                                                     const std::vector<int>& enabled_classes,
                                                     const std::vector<ROI>& rois,
                                                     int frame_width,
                                                     int frame_height) {
    std::vector<Detection> filtered;
    
    // 如果frame_width或frame_height为0，说明没有提供帧尺寸，跳过ROI过滤
    bool can_use_roi = (frame_width > 0 && frame_height > 0);
    
    for (const auto& detection : detections) {
        // 类别过滤
        if (!enabled_classes.empty()) {
            bool class_enabled = false;
            for (int class_id : enabled_classes) {
                if (detection.class_id == class_id) {
                    class_enabled = true;
                    break;
                }
            }
            if (!class_enabled) {
                continue;
            }
        }
        
        // ROI过滤
        if (!rois.empty() && can_use_roi) {
            bool in_roi = false;
            for (const auto& roi : rois) {
                if (roi.enabled && AlgorithmConfigManager::isDetectionInROI(detection.bbox, roi, frame_width, frame_height)) {
                    in_roi = true;
                    break;
                }
            }
            if (!in_roi) {
                continue;
            }
        }
        
        filtered.push_back(detection);
    }
    
    return filtered;
}

cv::Mat YOLOv11Detector::processFrame(const cv::Mat& frame) {
    auto detections = detect(frame);
    return ImageUtils::drawDetections(frame, detections);
}

} // namespace detector_service

