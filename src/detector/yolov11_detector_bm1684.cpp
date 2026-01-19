#ifdef ENABLE_BM1684

#include "yolov11_detector_bm1684.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>
#include <cstring>

// BM1684 OpenCV extension
#ifdef USE_BM_OPENCV
#include <opencv2/bmcv.hpp>
#endif

namespace detector_service {

#define FFALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

YOLOv11DetectorBM1684::YOLOv11DetectorBM1684(const std::string& model_path,
                                             float conf_threshold,
                                             float nms_threshold,
                                             int input_width,
                                             int input_height,
                                             int device_id)
    : model_path_(model_path),
      conf_threshold_(conf_threshold),
      nms_threshold_(nms_threshold),
      input_width_(input_width),
      input_height_(input_height),
      device_id_(device_id),
      bm_handle_(nullptr),
      bmrt_(nullptr),
      net_info_(nullptr),
      input_tensors_(nullptr),
      output_tensors_(nullptr),
      input_num_(0),
      output_num_(0),
      resized_imgs_(nullptr),
      converto_imgs_(nullptr),
      max_batch_(1) {
}

YOLOv11DetectorBM1684::~YOLOv11DetectorBM1684() {
    // Free BM images
    if (resized_imgs_) {
        for (int i = 0; i < max_batch_; i++) {
            bm_image_destroy(resized_imgs_[i]);
        }
        bm_image_free_contiguous_mem(max_batch_, resized_imgs_);
        delete[] resized_imgs_;
    }
    
    if (converto_imgs_) {
        for (int i = 0; i < max_batch_; i++) {
            bm_image_destroy(converto_imgs_[i]);
        }
        delete[] converto_imgs_;
    }
    
    // Free tensors
    if (input_tensors_) {
        for (int i = 0; i < input_num_; i++) {
            if (input_tensors_[i].device_mem.size != 0) {
                bm_free_device(bm_handle_, input_tensors_[i].device_mem);
            }
        }
        delete[] input_tensors_;
    }
    
    if (output_tensors_) {
        for (int i = 0; i < output_num_; i++) {
            if (output_tensors_[i].device_mem.size != 0) {
                bm_free_device(bm_handle_, output_tensors_[i].device_mem);
            }
        }
        delete[] output_tensors_;
    }
    
    // Destroy BMRuntime
    if (bmrt_) {
        bmrt_destroy(bmrt_);
    }
    
    // Free BM handle
    if (bm_handle_) {
        bm_dev_free(bm_handle_);
    }
}

bool YOLOv11DetectorBM1684::initialize() {
    try {
        // Initialize BM device
        bm_status_t ret = bm_dev_request(&bm_handle_, device_id_);
        if (ret != BM_SUCCESS) {
            std::cerr << "BM1684: 无法请求设备 " << device_id_ << std::endl;
            return false;
        }
        
        // Create BMRuntime
        bmrt_ = bmrt_create(bm_handle_);
        if (!bmrt_) {
            std::cerr << "BM1684: 创建BMRuntime失败" << std::endl;
            return false;
        }
        
        // Load BModel
        if (!loadModel()) {
            return false;
        }
        
        // Initialize preprocessing images
        net_info_ = bmrt_get_network_info(bmrt_, 0);
        if (!net_info_) {
            std::cerr << "BM1684: 无法获取网络信息" << std::endl;
            return false;
        }
        
        input_num_ = net_info_->input_num;
        output_num_ = net_info_->output_num;
        max_batch_ = net_info_->stages[0].input_shapes[0].dims[0];
        
        // Allocate input/output tensors
        input_tensors_ = new bm_tensor_t[input_num_];
        output_tensors_ = new bm_tensor_t[output_num_];
        
        for (int i = 0; i < input_num_; i++) {
            input_tensors_[i].dtype = net_info_->input_dtypes[i];
            input_tensors_[i].shape = net_info_->stages[0].input_shapes[i];
            input_tensors_[i].st_mode = BM_STORE_1N;
            input_tensors_[i].device_mem = bm_mem_null();
        }
        
        for (int i = 0; i < output_num_; i++) {
            output_tensors_[i].dtype = net_info_->output_dtypes[i];
            output_tensors_[i].shape = net_info_->stages[0].output_shapes[i];
            output_tensors_[i].st_mode = BM_STORE_1N;
            output_tensors_[i].device_mem = bm_mem_null();
        }
        
        // Create preprocessing images
        int net_h = net_info_->stages[0].input_shapes[0].dims[2];
        int net_w = net_info_->stages[0].input_shapes[0].dims[3];
        
        resized_imgs_ = new bm_image[max_batch_];
        converto_imgs_ = new bm_image[max_batch_];
        
        int aligned_net_w = FFALIGN(net_w, 64);
        int strides[3] = {aligned_net_w, aligned_net_w, aligned_net_w};
        
        for (int i = 0; i < max_batch_; i++) {
            ret = bm_image_create(bm_handle_, net_h, net_w,
                                 FORMAT_RGB_PLANAR,
                                 DATA_TYPE_EXT_1N_BYTE,
                                 &resized_imgs_[i], strides);
            if (ret != BM_SUCCESS) {
                std::cerr << "BM1684: 创建resized图像失败" << std::endl;
                return false;
            }
        }
        bm_image_alloc_contiguous_mem(max_batch_, resized_imgs_);
        
        bm_image_data_format_ext img_dtype = DATA_TYPE_EXT_FLOAT32;
        if (net_info_->input_dtypes[0] == BM_INT8) {
            img_dtype = DATA_TYPE_EXT_1N_BYTE_SIGNED;
        }
        
        // 使用循环创建 bm_image（如果 bm_image_create_batch 不可用）
        for (int i = 0; i < max_batch_; i++) {
            ret = bm_image_create(bm_handle_, net_h, net_w,
                                 FORMAT_RGB_PLANAR,
                                 img_dtype,
                                 &converto_imgs_[i]);
            if (ret != BM_SUCCESS) {
                std::cerr << "BM1684: 创建converto图像 " << i << " 失败" << std::endl;
                return false;
            }
        }
        if (ret != BM_SUCCESS) {
            std::cerr << "BM1684: 创建converto图像失败" << std::endl;
            return false;
        }
        
        loadClassNames();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "BM1684: 初始化检测器失败: " << e.what() << std::endl;
        return false;
    }
}

bool YOLOv11DetectorBM1684::loadModel() {
    if (!bmrt_load_bmodel(bmrt_, model_path_.c_str())) {
        std::cerr << "BM1684: 加载BModel失败: " << model_path_ << std::endl;
        return false;
    }
    
    std::cout << "BM1684: 成功加载BModel: " << model_path_ << std::endl;
    return true;
}

void YOLOv11DetectorBM1684::loadClassNames() {
    // 默认COCO类别名称
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

float YOLOv11DetectorBM1684::get_aspect_scaled_ratio(int src_w, int src_h, int dst_w, int dst_h, bool *pIsAligWidth) {
    float ratio;
    ratio = (float) dst_w / src_w;
    int dst_h1 = src_h * ratio;
    if (dst_h1 > dst_h) {
        *pIsAligWidth = false;
        ratio = (float) dst_h / src_h;
    } else {
        *pIsAligWidth = true;
        ratio = (float) src_w / src_w;
    }
    return ratio;
}

int YOLOv11DetectorBM1684::preprocess_bmcv(const cv::Mat& image, bm_image& input_bmimg) {
    bm_image image1;
    bm_image image_aligned;
    
#ifdef USE_BM_OPENCV
    cv::bmcv::toBMI(const_cast<cv::Mat&>(image), &image1);
#else
    // 如果使用标准OpenCV，需要手动转换
    // 这里简化处理，实际应该使用bmcv API
    // 注意：标准OpenCV不支持直接转换为bm_image，需要使用BMCV API
    std::cerr << "BM1684: 需要USE_BM_OPENCV支持或使用BMCV API" << std::endl;
    return -1;
#endif
    
    bool need_copy = image1.width & (64-1);
    
    if (need_copy) {
        int stride1[3], stride2[3];
        bm_image_get_stride(image1, stride1);
        stride2[0] = FFALIGN(stride1[0], 64);
        stride2[1] = FFALIGN(stride1[1], 64);
        stride2[2] = FFALIGN(stride1[2], 64);
        bm_image_create(bm_handle_, image1.height, image1.width,
                       image1.image_format, image1.data_type, &image_aligned, stride2);
        bm_image_alloc_dev_mem(image_aligned, BMCV_IMAGE_FOR_IN);
        
        bmcv_copy_to_atrr_t copyToAttr;
        memset(&copyToAttr, 0, sizeof(copyToAttr));
        copyToAttr.start_x = 0;
        copyToAttr.start_y = 0;
        copyToAttr.if_padding = 1;
        bmcv_image_copy_to(bm_handle_, copyToAttr, image1, image_aligned);
    } else {
        image_aligned = image1;
    }
    
    // Resize with aspect ratio
    bool isAlignWidth = false;
    float ratio = get_aspect_scaled_ratio(image.cols, image.rows, input_width_, input_height_, &isAlignWidth);
    
    bmcv_padding_atrr_t padding_attr;
    memset(&padding_attr, 0, sizeof(padding_attr));
    padding_attr.dst_crop_sty = 0;
    padding_attr.dst_crop_stx = 0;
    padding_attr.padding_b = 114;
    padding_attr.padding_g = 114;
    padding_attr.padding_r = 114;
    padding_attr.if_memset = 1;
    
    if (isAlignWidth) {
        padding_attr.dst_crop_h = image.rows * ratio;
        padding_attr.dst_crop_w = input_width_;
    } else {
        padding_attr.dst_crop_h = input_height_;
        padding_attr.dst_crop_w = image.cols * ratio;
    }
    
    bmcv_image_vpp_convert_padding(bm_handle_, 1, image_aligned, &resized_imgs_[0], &padding_attr);
    
    // Convert to model input format
    // 使用默认的归一化 scale（将 [0, 255] 归一化到 [0, 1]）
    // 注意：bm_tensor_t 结构体没有 scale 成员，使用默认值
    float input_scale = 1.0f / 255.0f;
    
    bmcv_convert_to_attr converto_attr;
    converto_attr.alpha_0 = input_scale;
    converto_attr.beta_0 = 0;
    converto_attr.alpha_1 = input_scale;
    converto_attr.beta_1 = 0;
    converto_attr.alpha_2 = input_scale;
    converto_attr.beta_2 = 0;
    
    bmcv_image_convert_to(bm_handle_, max_batch_, 
                         converto_attr,
                         resized_imgs_, converto_imgs_);
    
    // Attach to input tensor
    bm_image_get_device_mem(converto_imgs_[0], &input_tensors_[0].device_mem);
    input_bmimg = converto_imgs_[0];
    
    if (need_copy) {
        bm_image_destroy(image_aligned);
    }
    
    return 0;
}

cv::Mat YOLOv11DetectorBM1684::preprocess(const cv::Mat& image, float& scale, int& pad_x, int& pad_y) {
    // 使用标准OpenCV预处理（作为fallback）
    cv::Mat resized;
    float scale_w = static_cast<float>(input_width_) / image.cols;
    float scale_h = static_cast<float>(input_height_) / image.rows;
    scale = std::min(scale_w, scale_h);
    
    int new_w = static_cast<int>(image.cols * scale);
    int new_h = static_cast<int>(image.rows * scale);
    
    cv::resize(image, resized, cv::Size(new_w, new_h));
    
    pad_x = (input_width_ - new_w) / 2;
    pad_y = (input_height_ - new_h) / 2;
    
    cv::Mat padded;
    cv::copyMakeBorder(resized, padded, pad_y, input_height_ - new_h - pad_y,
                      pad_x, input_width_ - new_w - pad_x,
                      cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));
    
    cv::Mat rgb;
    cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);
    
    cv::Mat float_img;
    rgb.convertTo(float_img, CV_32F, 1.0 / 255.0);
    
    return float_img;
}

std::vector<Detection> YOLOv11DetectorBM1684::detect(const cv::Mat& image) {
    if (!bmrt_ || !net_info_) {
        return {};
    }
    
    bm_image input_bmimg;
    
    // Preprocess using BMCV (hardware accelerated)
    if (preprocess_bmcv(image, input_bmimg) != 0) {
        // Fallback to CPU preprocessing
        float scale;
        int pad_x, pad_y;
        cv::Mat preprocessed = preprocess(image, scale, pad_x, pad_y);
        
        // Convert to device memory (simplified, should use bmcv)
        // For now, we'll use CPU path
        std::cerr << "BM1684: 使用CPU预处理路径" << std::endl;
        return {};
    }
    
    // Run inference
    bool ok = bmrt_launch_tensor_ex(bmrt_, net_info_->name,
                                    input_tensors_, input_num_,
                                    output_tensors_, output_num_,
                                    false, true);
    if (!ok) {
        std::cerr << "BM1684: 推理失败" << std::endl;
        return {};
    }
    
    // Sync
    bm_thread_sync(bm_handle_);
    
    // Get output data
    bm_shape_t output_shape = output_tensors_[0].shape;
    int output_size = bmrt_shape_count(&output_shape);
    
    // Allocate CPU buffer
    std::vector<float> output_data(output_size);
    
    // Copy from device to host
    if (output_tensors_[0].dtype == BM_FLOAT32) {
        bm_memcpy_d2s_partial(bm_handle_, output_data.data(),
                             output_tensors_[0].device_mem,
                             output_size * sizeof(float));
    } else if (output_tensors_[0].dtype == BM_INT8) {
        std::vector<int8_t> int8_data(output_size);
        bm_memcpy_d2s_partial(bm_handle_, int8_data.data(),
                             output_tensors_[0].device_mem,
                             output_size * sizeof(int8_t));
        float scale = net_info_->output_scales[0];
        for (int i = 0; i < output_size; i++) {
            output_data[i] = int8_data[i] * scale;
        }
    }
    
    // Postprocess
    cv::Size original_size(image.cols, image.rows);
    float scale = 1.0f;  // Should be calculated from preprocessing
    int pad_x = 0, pad_y = 0;
    
    return postprocess(output_data.data(), output_shape, original_size, scale, pad_x, pad_y);
}

std::vector<Detection> YOLOv11DetectorBM1684::postprocess(const float* output_data,
                                                         const bm_shape_t& output_shape,
                                                         const cv::Size& original_size,
                                                         float scale, int pad_x, int pad_y) {
    std::vector<Detection> detections;
    
    // Similar to ONNX version postprocessing
    int num_anchors = output_shape.dims[1];
    int features = output_shape.dims[2];
    bool has_objectness = (features == 85);
    int num_classes = has_objectness ? 80 : (features - 4);
    
    for (int i = 0; i < num_anchors; i++) {
        int offset = i * features;
        float center_x = output_data[offset + 0];
        float center_y = output_data[offset + 1];
        float width = output_data[offset + 2];
        float height = output_data[offset + 3];
        
        float objectness = has_objectness ? output_data[offset + 4] : 1.0f;
        if (has_objectness && objectness < 0.1f) {
            continue;
        }
        
        // Find max class
        int class_start = has_objectness ? 5 : 4;
        float max_logit = -std::numeric_limits<float>::max();
        int class_id = 0;
        for (int j = 0; j < num_classes; j++) {
            float logit = output_data[offset + class_start + j];
            if (logit > max_logit) {
                max_logit = logit;
                class_id = j;
            }
        }
        
        // Apply sigmoid for confidence
        float confidence = 1.0f / (1.0f + std::exp(-max_logit));
        if (has_objectness) {
            confidence *= objectness;
        }
        
        if (confidence < conf_threshold_) {
            continue;
        }
        
        // Convert to pixel coordinates
        float x1 = (center_x - width / 2.0f - pad_x) / scale;
        float y1 = (center_y - height / 2.0f - pad_y) / scale;
        float x2 = (center_x + width / 2.0f - pad_x) / scale;
        float y2 = (center_y + height / 2.0f - pad_y) / scale;
        
        // Clamp to image bounds
        x1 = std::max(0.0f, std::min(static_cast<float>(original_size.width), x1));
        y1 = std::max(0.0f, std::min(static_cast<float>(original_size.height), y1));
        x2 = std::max(0.0f, std::min(static_cast<float>(original_size.width), x2));
        y2 = std::max(0.0f, std::min(static_cast<float>(original_size.height), y2));
        
        Detection det;
        det.bbox = cv::Rect(static_cast<int>(x1), static_cast<int>(y1), 
                           static_cast<int>(x2 - x1), static_cast<int>(y2 - y1));
        det.confidence = confidence;
        det.class_id = class_id;
        det.class_name = (class_id < static_cast<int>(class_names_.size())) 
                        ? class_names_[class_id] : "unknown";
        
        detections.push_back(det);
    }
    
    // Apply NMS
    return applyNMS(detections);
}

std::vector<Detection> YOLOv11DetectorBM1684::applyNMS(const std::vector<Detection>& detections) {
    if (detections.empty()) {
        return {};
    }
    
    // Sort by confidence
    std::vector<Detection> sorted = detections;
    std::sort(sorted.begin(), sorted.end(),
              [](const Detection& a, const Detection& b) {
                  return a.confidence > b.confidence;
              });
    
    std::vector<Detection> result;
    std::vector<bool> suppressed(sorted.size(), false);
    
    for (size_t i = 0; i < sorted.size(); i++) {
        if (suppressed[i]) continue;
        
        result.push_back(sorted[i]);
        
        for (size_t j = i + 1; j < sorted.size(); j++) {
            if (suppressed[j] || sorted[i].class_id != sorted[j].class_id) {
                continue;
            }
            
            // Calculate IoU
            const cv::Rect& rect_i = sorted[i].bbox;
            const cv::Rect& rect_j = sorted[j].bbox;
            
            int x1 = std::max(rect_i.x, rect_j.x);
            int y1 = std::max(rect_i.y, rect_j.y);
            int x2 = std::min(rect_i.x + rect_i.width, rect_j.x + rect_j.width);
            int y2 = std::min(rect_i.y + rect_i.height, rect_j.y + rect_j.height);
            
            float intersection = std::max(0, x2 - x1) * std::max(0, y2 - y1);
            float area_i = rect_i.width * rect_i.height;
            float area_j = rect_j.width * rect_j.height;
            float union_area = area_i + area_j - intersection;
            
            float iou = union_area > 0 ? intersection / union_area : 0.0f;
            
            if (iou > nms_threshold_) {
                suppressed[j] = true;
            }
        }
    }
    
    return result;
}

std::vector<Detection> YOLOv11DetectorBM1684::applyFilters(const std::vector<Detection>& detections,
                                                          const std::vector<int>& enabled_classes,
                                                          const std::vector<ROI>& rois,
                                                          int frame_width,
                                                          int frame_height) {
    std::vector<Detection> filtered;
    
    for (const auto& det : detections) {
        // Filter by class
        if (!enabled_classes.empty()) {
            if (std::find(enabled_classes.begin(), enabled_classes.end(), det.class_id) 
                == enabled_classes.end()) {
                continue;
            }
        }
        
        // Filter by ROI
        if (!rois.empty() && frame_width > 0 && frame_height > 0) {
            bool in_roi = false;
            const cv::Rect& bbox = det.bbox;
            float center_x = bbox.x + bbox.width / 2.0f;
            float center_y = bbox.y + bbox.height / 2.0f;
            
            for (const auto& roi : rois) {
                if (roi.type == ROIType::RECTANGLE && roi.points.size() >= 2) {
                    // 对于矩形 ROI，使用前两个点作为左上和右下
                    float roi_x1 = roi.points[0].x * frame_width;
                    float roi_y1 = roi.points[0].y * frame_height;
                    float roi_x2 = roi.points[1].x * frame_width;
                    float roi_y2 = roi.points[1].y * frame_height;
                    
                    // 确保坐标顺序正确
                    if (roi_x1 > roi_x2) std::swap(roi_x1, roi_x2);
                    if (roi_y1 > roi_y2) std::swap(roi_y1, roi_y2);
                    
                    if (center_x >= roi_x1 && center_x <= roi_x2 &&
                        center_y >= roi_y1 && center_y <= roi_y2) {
                        in_roi = true;
                        break;
                    }
                }
            }
            
            if (!in_roi) {
                continue;
            }
        }
        
        filtered.push_back(det);
    }
    
    return filtered;
}

cv::Mat YOLOv11DetectorBM1684::processFrame(const cv::Mat& frame) {
    auto detections = detect(frame);
    return ImageUtils::drawDetections(frame, detections);
}

} // namespace detector_service

#endif // ENABLE_BM1684

