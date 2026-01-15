import request from "@/utils/http";

export interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
}

export interface GB28181Config {
  enabled: boolean;
  sip_server_ip: string;
  sip_server_port: number;
  sip_server_id: string;
  sip_server_domain: string;
  device_id: string;
  device_password: string;
  device_name: string;
  manufacturer: string;
  model: string;
  local_sip_port: number;
  rtp_port_start: number;
  rtp_port_end: number;
  heartbeat_interval: number;
  heartbeat_count: number;
  register_expires: number;
  stream_mode: "PS" | "H264";
  max_channels: number;
}

/**
 * 获取GB28181配置
 */
export function getGB28181Config() {
  return request<GB28181Config & { success: boolean }>({
    url: "/config/gb28181",
    method: "GET",
  });
}

/**
 * 更新GB28181配置
 */
export function updateGB28181Config(params: Partial<GB28181Config>) {
  return request<ApiResponse>({
    url: "/config/gb28181",
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

