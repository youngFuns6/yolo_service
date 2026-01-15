import request from "@/utils/http";

export interface Model {
  name: string;
  path: string;
  size: string;
  modified: string;
}

export interface ClassInfo {
  id: number;
  name: string;
}

export interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
  message?: string;
}

/**
 * 获取模型列表
 */
export function getModelList() {
  return request<ApiResponse<Model[]>>({
    url: "/models",
    method: "GET",
  });
}

/**
 * 上传模型
 */
export function uploadModel(file: File) {
  const formData = new FormData();
  formData.append("file", file);
  
  return request<ApiResponse>({
    url: "/models/upload",
    method: "POST",
    data: formData,
    // 不设置 Content-Type，让 axios 自动处理 FormData 的 boundary
  });
}

/**
 * 删除模型
 */
export function deleteModel(modelName: string) {
  return request<ApiResponse>({
    url: `/models/${modelName}`,
    method: "DELETE",
  });
}

/**
 * 获取类别列表
 */
export function getClassList() {
  return request<ApiResponse<ClassInfo[]>>({
    url: "/models/classes",
    method: "GET",
  });
}

