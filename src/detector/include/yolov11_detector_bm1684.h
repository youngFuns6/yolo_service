#pragma once

#ifdef ENABLE_BM1684

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include "image_utils.h"
#include "algorithm_config.h"
#include "config.h"

// BM1684 SDK headers
#include "bmruntime_interface.h"
#include "bmruntime_cpp.h"
#include "bmlib_runtime.h"
#include "bmcv_api.h"

namespace detector_service {

class YOLOv11DetectorBM1684 {
public:
    YOLOv11DetectorBM1684(const std::string& model_path, 
                         float conf_threshold = 0.5f,
                         float nms_threshold = 0.4f,
                         int input_width = 640,
                         int input_height = 640,
                         int device_id = 0);
    
    ~YOLOv11DetectorBM1684();
    
    bool initialize();
    std::vector<Detection> detect(const cv::Mat& image);
    cv::Mat processFrame(const cv::Mat& frame);
    
    // 动态配置更新
    void updateConfThreshold(float threshold) { conf_threshold_ = threshold; }
    void updateNmsThreshold(float threshold) { nms_threshold_ = threshold; }
    float getConfThreshold() const { return conf_threshold_; }
    float getNmsThreshold() const { return nms_threshold_; }
    
    // 应用过滤（类别、ROI等）
    std::vector<Detection> applyFilters(const std::vector<Detection>& detections,
                                       const std::vector<int>& enabled_classes = {},
                                       const std::vector<ROI>& rois = {},
                                       int frame_width = 0,
                                       int frame_height = 0);
    
    // 获取类别名称列表
    const std::vector<std::string>& getClassNames() const { return class_names_; }
    
private:
    std::string model_path_;
    float conf_threshold_;
    float nms_threshold_;
    int input_width_;
    int input_height_;
    int device_id_;
    
    // BM1684 runtime
    bm_handle_t bm_handle_;
    void* bmrt_;
    const bm_net_info_t* net_info_;
    
    // Input/Output tensors
    bm_tensor_t* input_tensors_;
    bm_tensor_t* output_tensors_;
    int input_num_;
    int output_num_;
    
    // Preprocessing images
    bm_image* resized_imgs_;
    bm_image* converto_imgs_;
    int max_batch_;
    
    std::vector<std::string> class_names_;
    
    bool loadModel();
    cv::Mat preprocess(const cv::Mat& image, float& scale, int& pad_x, int& pad_y);
    std::vector<Detection> postprocess(const float* output_data, 
                                     const bm_shape_t& output_shape,
                                     const cv::Size& original_size,
                                     float scale, int pad_x, int pad_y);
    std::vector<Detection> applyNMS(const std::vector<Detection>& detections);
    void loadClassNames();
    
    // BM1684 specific helper functions
    int preprocess_bmcv(const cv::Mat& image, bm_image& input_bmimg);
    float get_aspect_scaled_ratio(int src_w, int src_h, int dst_w, int dst_h, bool *pIsAligWidth);
};

} // namespace detector_service

#endif // ENABLE_BM1684

