import axios, {
  type InternalAxiosRequestConfig,
  type AxiosError,
  type AxiosResponse,
  type AxiosRequestConfig,
} from "axios";
import type { AxiosInstance } from "axios";
import qs from "qs";
// import Nprogress from "nprogress";
// import "nprogress/nprogress.css";
// import { message } from "antd";
import Config from "@/config/index";

interface RequestInstance extends AxiosInstance {
  <T = any>(config: AxiosRequestConfig): Promise<T>;
}

const request = axios.create({
  timeout: 20000,
  baseURL: `http://${Config.BASE_URL_HOST}/api`,
  headers: {
    "Content-Type": "application/json; charset=UTF-8",
  },
  withCredentials: true, // 支持session管理，发送cookie
}) as RequestInstance;

request.interceptors.request.use(
  (config: InternalAxiosRequestConfig) => {
    // Nprogress.start();
    if (config.method === "get") {
      config.paramsSerializer = function (params) {
        return qs.stringify(params, { arrayFormat: "repeat" });
      };
    }
    // 如果使用 FormData，删除默认的 Content-Type，让 axios 自动设置正确的 boundary
    if (config.data instanceof FormData) {
      delete config.headers?.["Content-Type"];
    }
    // 使用session管理，不需要手动添加token
    // session cookie会自动由浏览器发送
    return config;
  },
  (error: AxiosError) => {
    return Promise.reject(error);
  }
);

request.interceptors.response.use(
  (config: AxiosResponse) => {
    // Nprogress.done();
    // if( config.status >= 200 && config.status <= 300){
    //   if(config.data && config.config.method !== 'get'){
    //     message.success(config.data.message || '操作成功');
    //   }
    // }
    return config.data;
  },
  (error: AxiosError) => {
    // Nprogress.done();
    // session过期或未登录，重定向到登录页
    if (error.response?.status === 401 && !error.response.config.url?.match("login")) {
      setTimeout(() => {
        window.location.href = window.location.origin + "/login";
      }, 2000);
    } else {
      // if (error.response?.config && error.response?.config?.method !== "get") {
      //   message.error((error.response?.data as any)?.message || "未知错误");
      // }
    }
    return Promise.reject(error);
  }
);

export default request;
