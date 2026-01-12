import request from "@/utils/http";

export interface Alert {
  id: number;
  channel_id: number;
  channel_name: string;
  alert_type: string;
  alert_rule_id?: number;
  alert_rule_name?: string;
  image_path: string;
  confidence: number;
  detected_objects: string;
  bbox_x: number;
  bbox_y: number;
  bbox_w: number;
  bbox_h: number;
  report_status: string;
  report_url: string;
  created_at: string;
}

export interface GetAlertsParams {
  limit?: number;
  offset?: number;
}

export interface GetAlertsResponse {
  success: boolean;
  alerts: Alert[];
  total: number;
  error?: string;
}

export interface GetAlertResponse {
  success: boolean;
  alert: Alert;
  error?: string;
}

export interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
}

/**
 * 获取所有报警记录
 */
export function getAlerts(params?: GetAlertsParams) {
  return request<GetAlertsResponse>({
    url: "/alerts",
    method: "GET",
    params,
  });
}

/**
 * 获取单个报警记录
 */
export function getAlert(alertId: number) {
  return request<GetAlertResponse>({
    url: `/alerts/${alertId}`,
    method: "GET",
  });
}

/**
 * 获取通道的报警记录
 */
export function getAlertsByChannel(channelId: number, params?: GetAlertsParams) {
  return request<GetAlertsResponse>({
    url: `/channels/${channelId}/alerts`,
    method: "GET",
    params,
  });
}

/**
 * 删除报警记录
 */
export function deleteAlert(alertId: number) {
  return request<ApiResponse>({
    url: `/alerts/${alertId}`,
    method: "DELETE",
  });
}





