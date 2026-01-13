#pragma once

#include <string>
#include <memory>
#include <optional>
#include <atomic>

namespace detector_service {

/**
 * @brief GB28181配置结构
 */
struct GB28181Config {
    std::atomic<bool> enabled;              // 是否启用GB28181
    std::string sip_server_ip;              // SIP服务器IP地址
    int sip_server_port;                    // SIP服务器端口，默认5060
    std::string sip_server_id;              // SIP服务器ID（20位国标编码）
    std::string sip_server_domain;          // SIP服务器域
    
    std::string device_id;                  // 设备ID（20位国标编码）
    std::string device_password;            // 设备密码
    std::string device_name;                // 设备名称
    std::string manufacturer;               // 设备厂商
    std::string model;                      // 设备型号
    
    int local_sip_port;                     // 本地SIP端口，默认5061
    int rtp_port_start;                     // RTP端口起始，默认30000
    int rtp_port_end;                       // RTP端口结束，默认30100
    
    int heartbeat_interval;                 // 心跳间隔（秒），默认60
    int heartbeat_count;                    // 心跳超时次数，默认3
    int register_expires;                   // 注册有效期（秒），默认3600
    
    std::string stream_mode;                // 流模式：PS或H264，默认PS
    int max_channels;                       // 最大通道数，默认32
    
    GB28181Config() 
        : enabled(false),
          sip_server_port(5060),
          local_sip_port(5061),
          rtp_port_start(30000),
          rtp_port_end(30100),
          heartbeat_interval(60),
          heartbeat_count(3),
          register_expires(3600),
          stream_mode("PS"),
          max_channels(32) {}
    
    // 拷贝构造函数，处理atomic成员
    GB28181Config(const GB28181Config& other)
        : enabled(other.enabled.load()),
          sip_server_ip(other.sip_server_ip),
          sip_server_port(other.sip_server_port),
          sip_server_id(other.sip_server_id),
          sip_server_domain(other.sip_server_domain),
          device_id(other.device_id),
          device_password(other.device_password),
          device_name(other.device_name),
          manufacturer(other.manufacturer),
          model(other.model),
          local_sip_port(other.local_sip_port),
          rtp_port_start(other.rtp_port_start),
          rtp_port_end(other.rtp_port_end),
          heartbeat_interval(other.heartbeat_interval),
          heartbeat_count(other.heartbeat_count),
          register_expires(other.register_expires),
          stream_mode(other.stream_mode),
          max_channels(other.max_channels) {}
    
    // 拷贝赋值运算符，处理atomic成员
    GB28181Config& operator=(const GB28181Config& other) {
        if (this != &other) {
            enabled.store(other.enabled.load());
            sip_server_ip = other.sip_server_ip;
            sip_server_port = other.sip_server_port;
            sip_server_id = other.sip_server_id;
            sip_server_domain = other.sip_server_domain;
            device_id = other.device_id;
            device_password = other.device_password;
            device_name = other.device_name;
            manufacturer = other.manufacturer;
            model = other.model;
            local_sip_port = other.local_sip_port;
            rtp_port_start = other.rtp_port_start;
            rtp_port_end = other.rtp_port_end;
            heartbeat_interval = other.heartbeat_interval;
            heartbeat_count = other.heartbeat_count;
            register_expires = other.register_expires;
            stream_mode = other.stream_mode;
            max_channels = other.max_channels;
        }
        return *this;
    }
};

/**
 * @brief GB28181配置管理器（单例）
 */
class GB28181ConfigManager {
public:
    static GB28181ConfigManager& getInstance() {
        static GB28181ConfigManager instance;
        return instance;
    }

    // 保存GB28181配置
    bool saveGB28181Config(const GB28181Config& config);
    
    // 加载GB28181配置
    bool loadGB28181Config(GB28181Config& config);
    
    // 获取GB28181配置（如果不存在则返回默认值）
    GB28181Config getGB28181Config();

private:
    GB28181ConfigManager() = default;
    ~GB28181ConfigManager() = default;
    GB28181ConfigManager(const GB28181ConfigManager&) = delete;
    GB28181ConfigManager& operator=(const GB28181ConfigManager&) = delete;
};

} // namespace detector_service

