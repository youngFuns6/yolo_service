#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <map>
#include "models/include/gb28181_config.h"

// Forward declarations for eXosip2
struct eXosip_t;

namespace detector_service {

/**
 * @brief GB28181 媒体会话信息
 */
struct GB28181Session {
    int call_id;                        // 通话ID
    int dialog_id;                      // 对话ID
    std::string channel_id;             // 通道编码
    std::string dest_ip;                // 目标IP
    int dest_port;                      // 目标端口
    std::string ssrc;                   // SSRC
    std::string session_name;           // 会话名称
    int64_t start_time;                 // 开始时间（时间戳）
    bool is_active;                     // 是否活跃
};

/**
 * @brief GB28181 SIP客户端
 * 负责处理SIP信令交互：注册、心跳、Invite响应、目录查询等
 */
class GB28181SipClient {
public:
    using InviteCallback = std::function<void(const GB28181Session&)>;
    using ByeCallback = std::function<void(const std::string& channel_id)>;
    
    GB28181SipClient();
    ~GB28181SipClient();
    
    /**
     * @brief 初始化SIP客户端
     * @param config GB28181配置
     * @return 成功返回true
     */
    bool initialize(const GB28181Config& config);
    
    /**
     * @brief 启动SIP客户端（注册、心跳等）
     * @return 成功返回true
     */
    bool start();
    
    /**
     * @brief 停止SIP客户端
     */
    void stop();
    
    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const { return initialized.load(); }
    
    /**
     * @brief 检查是否正在运行
     */
    bool isRunning() const { return running.load(); }
    
    /**
     * @brief 设置Invite回调（上级平台请求视频流时触发）
     */
    void setInviteCallback(InviteCallback callback) {
        invite_callback = callback;
    }
    
    /**
     * @brief 设置Bye回调（上级平台停止视频流时触发）
     */
    void setByeCallback(ByeCallback callback) {
        bye_callback = callback;
    }
    
    /**
     * @brief 获取通道编码
     * @param channel_index 通道索引（从1开始）
     * @return 20位国标编码
     */
    std::string getChannelId(int channel_index) const;
    
    /**
     * @brief 发送200 OK响应（用于Invite确认）
     */
    bool sendInviteOk(const GB28181Session& session);
    
    /**
     * @brief 获取所有活跃会话
     */
    std::vector<GB28181Session> getActiveSessions() const;

private:
    // SIP事件处理线程
    void eventLoop();
    
    // 处理SIP事件
    void handleEvent();
    
    // 注册到SIP服务器
    bool doRegister();
    
    // 发送心跳
    bool sendHeartbeat();
    
    // 处理Invite请求
    void handleInvite(void* event);
    
    // 处理Bye请求
    void handleBye(void* event);
    
    // 处理Message请求（目录查询、设备信息查询等）
    void handleMessage(void* event);
    
    // 响应目录查询
    void respondCatalogQuery(void* event);
    
    // 响应设备信息查询
    void respondDeviceInfo(void* event);
    
    // 响应设备状态查询
    void respondDeviceStatus(void* event);
    
    // 生成SSRC
    std::string generateSSRC(const std::string& channel_id) const;
    
    // 生成SDP
    std::string generateSDP(const GB28181Session& session) const;
    
    // 解析SDP获取媒体信息
    bool parseInviteSDP(const std::string& sdp, std::string& ip, int& port);

private:
    GB28181Config config;
    eXosip_t* exosip_context;           // eXosip上下文
    
    std::atomic<bool> initialized;      // 是否已初始化
    std::atomic<bool> running;          // 是否正在运行
    std::thread event_thread;           // 事件处理线程
    std::thread heartbeat_thread;       // 心跳线程
    
    int register_id;                    // 注册ID
    int64_t last_heartbeat_time;        // 上次心跳时间
    
    InviteCallback invite_callback;     // Invite回调
    ByeCallback bye_callback;           // Bye回调
    
    mutable std::mutex sessions_mutex;
    std::map<std::string, GB28181Session> active_sessions;  // 活跃会话（key: channel_id）
};

} // namespace detector_service

