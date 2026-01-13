import request from "@/utils/http";

export type ChannelStatus = "idle" | "running" | "error" | "stopped";

export interface Channel {
  id: number;
  name: string;
  source_url: string;
  status: ChannelStatus;
  enabled: boolean;
  report_enabled: boolean;
  width: number;
  height: number;
  fps: number;
  created_at: string;
  updated_at: string;
}

export interface CreateChannelParams {
  id?: number;
  name: string;
  source_url: string;
  enabled?: boolean;
  report_enabled?: boolean;
}

export interface UpdateChannelParams {
  id?: number;
  name?: string;
  source_url?: string;
  enabled?: boolean;
  report_enabled?: boolean;
}

export interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
  channels?: T[];
  channel?: T;
  channel_id?: number;
}

/**
 * 获取通道列表
 */
export function getChannelList() {
  return request<ApiResponse<Channel>>({
    url: "/channels",
    method: "GET",
  });
}

/**
 * 获取单个通道
 */
export function getChannel(channelId: number) {
  return request<ApiResponse<Channel>>({
    url: `/channels/${channelId}`,
    method: "GET",
  });
}

/**
 * 创建通道
 */
export function createChannel(params: CreateChannelParams) {
  return request<ApiResponse<{ channel_id: number }>>({
    url: "/channels",
    method: "POST",
    data: params,
  });
}

/**
 * 更新通道
 */
export function updateChannel(channelId: number, params: UpdateChannelParams) {
  return request<ApiResponse>({
    url: `/channels/${channelId}`,
    method: "PUT",
    data: params,
  });
}

/**
 * 删除通道
 */
export function deleteChannel(channelId: number) {
  return request<ApiResponse>({
    url: `/channels/${channelId}`,
    method: "DELETE",
  });
}

/**
 * 启动通道
 */
export function startChannel(channelId: number) {
  return request<ApiResponse>({
    url: `/channels/${channelId}/start`,
    method: "POST",
  });
}

/**
 * 停止通道
 */
export function stopChannel(channelId: number) {
  return request<ApiResponse>({
    url: `/channels/${channelId}/stop`,
    method: "POST",
  });
}

