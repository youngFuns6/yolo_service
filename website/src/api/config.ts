import request from "@/utils/http";

export interface StreamConfig {
  rtmp_url: string;
  width: number;
  height: number;
  fps: number;
  bitrate: number;
}

export interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
}

/**
 * 获取全局推流配置
 */
export function getStreamConfig() {
  return request<StreamConfig & { success: boolean }>({
    url: "/config/stream",
    method: "GET",
  });
}

/**
 * 更新全局推流配置
 */
export function updateStreamConfig(params: Partial<StreamConfig>) {
  return request<ApiResponse>({
    url: "/config/stream",
    method: "PUT",
    data: params,
  });
}

