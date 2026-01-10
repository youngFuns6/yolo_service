import request from "@/utils/http";

export interface PushStreamConfig {
  rtmp_url: string;
  width?: number | null;
  height?: number | null;
  fps?: number | null;
  bitrate?: number | null;
}

export interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
}

/**
 * 获取全局推流配置
 */
export function getPushStreamConfig() {
  return request<PushStreamConfig & { success: boolean }>({
    url: "/config/push_stream",
    method: "GET",
  });
}

/**
 * 更新全局推流配置
 */
export function updatePushStreamConfig(params: Partial<PushStreamConfig>) {
  return request<ApiResponse>({
    url: "/config/push_stream",
    method: "PUT",
    data: params,
  });
}

export interface ReportConfig {
  type: "HTTP" | "MQTT";
  http_url: string;
  mqtt_broker: string;
  mqtt_port: number;
  mqtt_topic: string;
  mqtt_username: string;
  mqtt_password: string;
  mqtt_client_id: string;
  enabled: boolean;
}

/**
 * 获取上报配置
 */
export function getReportConfig() {
  return request<{ success: boolean; config: ReportConfig }>({
    url: "/report-config",
    method: "GET",
  });
}

/**
 * 更新上报配置
 */
export function updateReportConfig(params: Partial<ReportConfig>) {
  return request<ApiResponse>({
    url: "/report-config",
    method: "PUT",
    data: params,
  });
}

