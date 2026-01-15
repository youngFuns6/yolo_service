import request from "@/utils/http";

export interface ROI {
  id: number;
  type: "RECTANGLE" | "POLYGON";
  name: string;
  enabled: boolean;
  points: Array<{ x: number; y: number }>;
}

export interface AlertRule {
  id: number;
  name: string;
  enabled: boolean;
  target_classes: number[];
  min_confidence: number;
  min_count: number;
  max_count: number;
  suppression_window_seconds: number;
  roi_ids: number[];
}

export interface AlgorithmConfig {
  channel_id: number;
  model_path: string;
  conf_threshold: number;
  nms_threshold: number;
  input_width: number;
  input_height: number;
  detection_interval: number;
  enabled_classes: number[];
  rois: ROI[];
  alert_rules: AlertRule[];
  created_at?: string;
  updated_at?: string;
}

export interface UpdateAlgorithmConfigParams {
  model_path?: string;
  conf_threshold?: number;
  nms_threshold?: number;
  input_width?: number;
  input_height?: number;
  detection_interval?: number;
  enabled_classes?: number[];
  rois?: ROI[];
  alert_rules?: AlertRule[];
}

export interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
  message?: string;
}

/**
 * 获取通道的算法配置
 */
export function getAlgorithmConfig(channelId: number) {
  return request<ApiResponse<AlgorithmConfig>>({
    url: `/algorithm-configs/${channelId}`,
    method: "GET",
  });
}

/**
 * 更新通道的算法配置
 */
export function updateAlgorithmConfig(
  channelId: number,
  params: UpdateAlgorithmConfigParams
) {
  return request<ApiResponse>({
    url: `/algorithm-configs/${channelId}`,
    method: "PUT",
    data: params,
  });
}

/**
 * 删除通道的算法配置（恢复默认配置）
 */
export function deleteAlgorithmConfig(channelId: number) {
  return request<ApiResponse>({
    url: `/algorithm-configs/${channelId}`,
    method: "DELETE",
  });
}

/**
 * 获取默认算法配置
 */
export function getDefaultAlgorithmConfig() {
  return request<ApiResponse<AlgorithmConfig>>({
    url: "/algorithm-configs/default",
    method: "GET",
  });
}

