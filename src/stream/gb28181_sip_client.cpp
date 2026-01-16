#include "gb28181_sip_client.h"
#include <eXosip2/eXosip.h>
#include <osip2/osip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <cstring>

namespace detector_service {

GB28181SipClient::GB28181SipClient()
    : exosip_context(nullptr),
      initialized(false),
      running(false),
      register_id(-1),
      last_heartbeat_time(0) {
}

GB28181SipClient::~GB28181SipClient() {
    stop();
}

bool GB28181SipClient::initialize(const GB28181Config& config) {
    if (initialized.load()) {
        std::cerr << "GB28181 SIP: 客户端已初始化" << std::endl;
        return false;
    }
    
    this->config = config;
    
    // 初始化eXosip
    exosip_context = eXosip_malloc();
    if (!exosip_context) {
        std::cerr << "GB28181 SIP: 无法分配eXosip上下文" << std::endl;
        return false;
    }
    
    if (eXosip_init(exosip_context) != 0) {
        std::cerr << "GB28181 SIP: eXosip初始化失败" << std::endl;
        eXosip_quit(exosip_context);
        exosip_context = nullptr;
        return false;
    }
    
    // 根据配置选择传输协议
    int protocol = IPPROTO_UDP;  // 默认UDP
    std::string transport_param = "udp";
    if (config.sip_transport == "TCP" || config.sip_transport == "tcp") {
        protocol = IPPROTO_TCP;
        transport_param = "tcp";
    }
    
    // 监听本地SIP端口
    if (eXosip_listen_addr(exosip_context, protocol, nullptr, 
                           config.local_sip_port, AF_INET, 0) != 0) {
        std::cerr << "GB28181 SIP: 无法监听端口 " << config.local_sip_port 
                  << " (协议: " << transport_param << ")" << std::endl;
        eXosip_quit(exosip_context);
        exosip_context = nullptr;
        return false;
    }
    
    std::cerr << "GB28181 SIP: 成功监听端口 " << config.local_sip_port 
              << " (协议: " << transport_param << ")" << std::endl;
    
    // 设置用户代理
    std::string user_agent = config.manufacturer + " " + config.model;
    eXosip_set_user_agent(exosip_context, user_agent.c_str());
    
    initialized = true;
    return true;
}

bool GB28181SipClient::start() {
    if (!initialized.load()) {
        std::cerr << "GB28181 SIP: 客户端未初始化" << std::endl;
        return false;
    }
    
    if (running.load()) {
        std::cerr << "GB28181 SIP: 客户端已在运行" << std::endl;
        return false;
    }
    
    // 启动事件处理线程
    running = true;
    event_thread = std::thread(&GB28181SipClient::eventLoop, this);
    
    // 启动心跳线程
    heartbeat_thread = std::thread([this]() {
        while (running.load()) {
            auto now = std::chrono::system_clock::now().time_since_epoch().count();
            auto elapsed = (now - last_heartbeat_time) / 1000000000; // 转换为秒
            
            if (elapsed >= config.heartbeat_interval) {
                sendHeartbeat();
                last_heartbeat_time = now;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    
    // 给线程一些时间启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 注册到SIP服务器
    if (!doRegister()) {
        std::cerr << "GB28181 SIP: 注册失败" << std::endl;
        stop();
        return false;
    }
    
    return true;
}

void GB28181SipClient::stop() {
    if (!running.load()) {
        return;
    }
    
    running = false;
    
    // 注销
    if (register_id > 0 && exosip_context) {
        eXosip_register_remove(exosip_context, register_id);
    }
    
    // 等待线程结束
    if (event_thread.joinable()) {
        event_thread.join();
    }
    if (heartbeat_thread.joinable()) {
        heartbeat_thread.join();
    }
    
    // 清理eXosip
    if (exosip_context) {
        eXosip_quit(exosip_context);
        exosip_context = nullptr;
    }
    
    initialized = false;
}

void GB28181SipClient::eventLoop() {
    while (running.load()) {
        handleEvent();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void GB28181SipClient::handleEvent() {
    if (!exosip_context) return;
    
    eXosip_event_t* event = eXosip_event_wait(exosip_context, 0, 50);
    if (!event) {
        eXosip_automatic_action(exosip_context);
        return;
    }
    
    switch (event->type) {
        case EXOSIP_REGISTRATION_SUCCESS:
            break;
            
        case EXOSIP_REGISTRATION_FAILURE:
            std::cerr << "GB28181 SIP: 注册失败" << std::endl;
            break;
            
        case EXOSIP_CALL_INVITE:
            handleInvite(event);
            break;
            
        case EXOSIP_CALL_CLOSED:
        case EXOSIP_CALL_RELEASED:
            handleBye(event);
            break;
            
        case EXOSIP_MESSAGE_NEW:
            handleMessage(event);
            break;
            
        default:
            break;
    }
    
    eXosip_event_free(event);
    eXosip_automatic_action(exosip_context);
}

bool GB28181SipClient::doRegister() {
    if (!exosip_context) {
        std::cerr << "GB28181 SIP: eXosip上下文为空" << std::endl;
        return false;
    }
    
    // 验证必需的配置字段
    if (config.device_id.empty()) {
        std::cerr << "GB28181 SIP: 设备ID为空" << std::endl;
        return false;
    }
    
    if (config.sip_server_domain.empty()) {
        std::cerr << "GB28181 SIP: SIP服务器域为空" << std::endl;
        return false;
    }
    
    if (config.sip_server_ip.empty()) {
        std::cerr << "GB28181 SIP: SIP服务器IP为空" << std::endl;
        return false;
    }
    
    if (config.sip_server_port <= 0 || config.sip_server_port > 65535) {
        std::cerr << "GB28181 SIP: SIP服务器端口无效: " << config.sip_server_port << std::endl;
        return false;
    }
    
    if (config.local_sip_port <= 0 || config.local_sip_port > 65535) {
        std::cerr << "GB28181 SIP: 本地SIP端口无效: " << config.local_sip_port << std::endl;
        return false;
    }
    
    if (config.register_expires <= 0) {
        std::cerr << "GB28181 SIP: 注册有效期无效: " << config.register_expires << std::endl;
        return false;
    }
    
    osip_message_t* reg = nullptr;
    
    // 确定传输协议参数
    std::string transport_param = "";
    if (config.sip_transport == "TCP" || config.sip_transport == "tcp") {
        transport_param = ";transport=tcp";
    } else {
        transport_param = ";transport=udp";
    }
    
    // 构建From URI: sip:device_id@domain
    std::string from_uri = "sip:" + config.device_id + "@" + config.sip_server_domain + transport_param;
    
    // 构建To URI: sip:server_id@domain (如果server_id为空，使用device_id)
    std::string server_id = config.sip_server_id.empty() ? config.device_id : config.sip_server_id;
    std::string to_uri = "sip:" + server_id + "@" + config.sip_server_domain + transport_param;
    
    // 构建Contact URI: sip:device_id@local_ip:local_port
    std::string contact_uri = "sip:" + config.device_id + "@127.0.0.1:" + 
                              std::to_string(config.local_sip_port) + transport_param;
    
    // 构建Proxy URI: sip:server_ip:server_port
    std::string proxy_uri = "sip:" + config.sip_server_ip + ":" + 
                           std::to_string(config.sip_server_port) + transport_param;
    
    // 打印调试信息
    std::cerr << "GB28181 SIP: 准备注册 - From: " << from_uri 
              << ", Proxy: " << proxy_uri 
              << ", Contact: " << contact_uri << std::endl;
    
    eXosip_lock(exosip_context);
    
    register_id = eXosip_register_build_initial_register(
        exosip_context,
        from_uri.c_str(),
        proxy_uri.c_str(),
        contact_uri.c_str(),
        config.register_expires,
        &reg
    );
    
    if (register_id < 0) {
        eXosip_unlock(exosip_context);
        std::cerr << "GB28181 SIP: 构建注册消息失败 (register_id=" << register_id 
                  << "), From: " << from_uri 
                  << ", Proxy: " << proxy_uri 
                  << ", Contact: " << contact_uri << std::endl;
        return false;
    }
    
    // 添加认证信息
    if (!config.device_password.empty()) {
        osip_message_set_authorization(reg, config.device_password.c_str());
    }
    
    int ret = eXosip_register_send_register(exosip_context, register_id, reg);
    eXosip_unlock(exosip_context);
    
    if (ret != 0) {
        std::cerr << "GB28181 SIP: 发送注册消息失败 (ret=" << ret << ")" << std::endl;
        return false;
    }
    
    std::cerr << "GB28181 SIP: 注册消息已发送" << std::endl;
    return true;
}

bool GB28181SipClient::sendHeartbeat() {
    if (!exosip_context || register_id < 0) return false;
    
    osip_message_t* message = nullptr;
    
    // 确定传输协议参数
    std::string transport_param = "";
    if (config.sip_transport == "TCP" || config.sip_transport == "tcp") {
        transport_param = ";transport=tcp";
    } else {
        transport_param = ";transport=udp";
    }
    
    std::string from_uri = "sip:" + config.device_id + "@" + config.sip_server_domain + transport_param;
    std::string to_uri = "sip:" + config.sip_server_id + "@" + config.sip_server_domain + transport_param;
    
    eXosip_lock(exosip_context);
    
    int ret = eXosip_message_build_request(
        exosip_context,
        &message,
        "MESSAGE",
        to_uri.c_str(),
        from_uri.c_str(),
        nullptr
    );
    
    if (ret != 0 || !message) {
        eXosip_unlock(exosip_context);
        return false;
    }
    
    // 构建心跳XML内容
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_r(&time_t_now, &tm_now);
    
    char time_str[32];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", &tm_now);
    
    std::ostringstream body;
    body << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n"
         << "<Notify>\r\n"
         << "<CmdType>Keepalive</CmdType>\r\n"
         << "<SN>" << (rand() % 100000) << "</SN>\r\n"
         << "<DeviceID>" << config.device_id << "</DeviceID>\r\n"
         << "<Status>OK</Status>\r\n"
         << "</Notify>\r\n";
    
    std::string body_str = body.str();
    osip_message_set_body(message, body_str.c_str(), body_str.length());
    osip_message_set_content_type(message, "Application/MANSCDP+xml");
    
    ret = eXosip_message_send_request(exosip_context, message);
    eXosip_unlock(exosip_context);
    
    if (ret != 0) {
        return false;
    }
    
    // 不打印日志，避免刷屏
    return true;
}

void GB28181SipClient::handleInvite(void* evt) {
    eXosip_event_t* event = static_cast<eXosip_event_t*>(evt);
    if (!event || !event->request) return;
    
    // 解析SDP获取目标IP和端口
    std::string dest_ip;
    int dest_port = 0;
    
    osip_body_t* sdp_body = nullptr;
    osip_message_get_body(event->request, 0, &sdp_body);
    
    if (sdp_body && sdp_body->body) {
        std::string sdp(sdp_body->body);
        parseInviteSDP(sdp, dest_ip, dest_port);
    }
    
    // 从Request-URI中获取通道ID
    osip_uri_t* req_uri = osip_message_get_uri(event->request);
    std::string channel_id;
    if (req_uri && req_uri->username) {
        channel_id = req_uri->username;
    }
    
    if (channel_id.empty() || dest_ip.empty() || dest_port == 0) {
        std::cerr << "GB28181 SIP: Invite请求参数不完整" << std::endl;
        eXosip_lock(exosip_context);
        eXosip_call_send_answer(exosip_context, event->tid, 400, nullptr);
        eXosip_unlock(exosip_context);
        return;
    }
    
    // 生成SSRC
    std::string ssrc = generateSSRC(channel_id);
    
    // 创建会话
    GB28181Session session;
    session.call_id = event->cid;
    session.dialog_id = event->did;
    session.channel_id = channel_id;
    session.dest_ip = dest_ip;
    session.dest_port = dest_port;
    session.ssrc = ssrc;
    session.session_name = "Play";
    session.start_time = std::chrono::system_clock::now().time_since_epoch().count();
    session.is_active = true;
    
    // 保存会话
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        active_sessions[channel_id] = session;
    }
    
    // 触发回调
    if (invite_callback) {
        invite_callback(session);
    }
    
    // 发送180 Ringing
    eXosip_lock(exosip_context);
    eXosip_call_send_answer(exosip_context, event->tid, 180, nullptr);
    eXosip_unlock(exosip_context);
}

bool GB28181SipClient::sendInviteOk(const GB28181Session& session) {
    if (!exosip_context) return false;
    
    osip_message_t* answer = nullptr;
    
    eXosip_lock(exosip_context);
    
    int ret = eXosip_call_build_answer(exosip_context, session.dialog_id, 200, &answer);
    if (ret != 0 || !answer) {
        eXosip_unlock(exosip_context);
        std::cerr << "GB28181 SIP: 构建200 OK失败" << std::endl;
        return false;
    }
    
    // 生成SDP
    std::string sdp = generateSDP(session);
    osip_message_set_body(answer, sdp.c_str(), sdp.length());
    osip_message_set_content_type(answer, "application/sdp");
    
    ret = eXosip_call_send_answer(exosip_context, session.dialog_id, 200, answer);
    eXosip_unlock(exosip_context);
    
    if (ret != 0) {
        std::cerr << "GB28181 SIP: 发送200 OK失败" << std::endl;
        return false;
    }
    
    return true;
}

void GB28181SipClient::handleBye(void* evt) {
    eXosip_event_t* event = static_cast<eXosip_event_t*>(evt);
    if (!event) return;
    
    // 查找对应的会话
    std::string channel_id;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        for (auto& pair : active_sessions) {
            if (pair.second.call_id == event->cid) {
                channel_id = pair.first;
                pair.second.is_active = false;
                break;
            }
        }
    }
    
    if (!channel_id.empty()) {
        // 触发回调
        if (bye_callback) {
            bye_callback(channel_id);
        }
        
        // 移除会话
        std::lock_guard<std::mutex> lock(sessions_mutex);
        active_sessions.erase(channel_id);
    }
}

void GB28181SipClient::handleMessage(void* evt) {
    eXosip_event_t* event = static_cast<eXosip_event_t*>(evt);
    if (!event || !event->request) return;
    
    osip_body_t* body = nullptr;
    osip_message_get_body(event->request, 0, &body);
    
    if (!body || !body->body) {
        eXosip_lock(exosip_context);
        eXosip_message_send_answer(exosip_context, event->tid, 400, nullptr);
        eXosip_unlock(exosip_context);
        return;
    }
    
    std::string xml_body(body->body);
    
    // 简单解析XML，判断CmdType
    if (xml_body.find("<CmdType>Catalog</CmdType>") != std::string::npos) {
        respondCatalogQuery(event);
    } else if (xml_body.find("<CmdType>DeviceInfo</CmdType>") != std::string::npos) {
        respondDeviceInfo(event);
    } else if (xml_body.find("<CmdType>DeviceStatus</CmdType>") != std::string::npos) {
        respondDeviceStatus(event);
    } else {
        // 其他类型，暂不处理
        eXosip_lock(exosip_context);
        eXosip_message_send_answer(exosip_context, event->tid, 200, nullptr);
        eXosip_unlock(exosip_context);
    }
}

void GB28181SipClient::respondCatalogQuery(void* evt) {
    eXosip_event_t* event = static_cast<eXosip_event_t*>(evt);
    if (!event) return;
    
    // 先回复200 OK
    eXosip_lock(exosip_context);
    eXosip_message_send_answer(exosip_context, event->tid, 200, nullptr);
    eXosip_unlock(exosip_context);
    
    // 构建目录响应
    osip_message_t* message = nullptr;
    
    // 确定传输协议参数
    std::string transport_param = "";
    if (config.sip_transport == "TCP" || config.sip_transport == "tcp") {
        transport_param = ";transport=tcp";
    } else {
        transport_param = ";transport=udp";
    }
    
    std::string from_uri = "sip:" + config.device_id + "@" + config.sip_server_domain + transport_param;
    std::string to_uri = "sip:" + config.sip_server_id + "@" + config.sip_server_domain + transport_param;
    
    eXosip_lock(exosip_context);
    
    int ret = eXosip_message_build_request(
        exosip_context,
        &message,
        "MESSAGE",
        to_uri.c_str(),
        from_uri.c_str(),
        nullptr
    );
    
    if (ret != 0 || !message) {
        eXosip_unlock(exosip_context);
        return;
    }
    
    // 构建目录XML
    std::ostringstream body;
    body << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n"
         << "<Response>\r\n"
         << "<CmdType>Catalog</CmdType>\r\n"
         << "<SN>" << (rand() % 100000) << "</SN>\r\n"
         << "<DeviceID>" << config.device_id << "</DeviceID>\r\n"
         << "<SumNum>" << config.max_channels << "</SumNum>\r\n"
         << "<DeviceList Num=\"" << config.max_channels << "\">\r\n";
    
    // 添加通道列表
    for (int i = 1; i <= config.max_channels; ++i) {
        std::string channel_id = getChannelId(i);
        body << "<Item>\r\n"
             << "<DeviceID>" << channel_id << "</DeviceID>\r\n"
             << "<Name>" << config.device_name << "-通道" << i << "</Name>\r\n"
             << "<Manufacturer>" << config.manufacturer << "</Manufacturer>\r\n"
             << "<Model>" << config.model << "</Model>\r\n"
             << "<Owner>Owner</Owner>\r\n"
             << "<CivilCode>" << config.sip_server_domain << "</CivilCode>\r\n"
             << "<Address>Address</Address>\r\n"
             << "<Parental>0</Parental>\r\n"
             << "<ParentID>" << config.device_id << "</ParentID>\r\n"
             << "<SafetyWay>0</SafetyWay>\r\n"
             << "<RegisterWay>1</RegisterWay>\r\n"
             << "<Secrecy>0</Secrecy>\r\n"
             << "<Status>ON</Status>\r\n"
             << "</Item>\r\n";
    }
    
    body << "</DeviceList>\r\n</Response>\r\n";
    
    std::string body_str = body.str();
    osip_message_set_body(message, body_str.c_str(), body_str.length());
    osip_message_set_content_type(message, "Application/MANSCDP+xml");
    
    ret = eXosip_message_send_request(exosip_context, message);
    eXosip_unlock(exosip_context);
}

void GB28181SipClient::respondDeviceInfo(void* evt) {
    eXosip_event_t* event = static_cast<eXosip_event_t*>(evt);
    if (!event) return;
    
    // 先回复200 OK
    eXosip_lock(exosip_context);
    eXosip_message_send_answer(exosip_context, event->tid, 200, nullptr);
    eXosip_unlock(exosip_context);
    
    // 构建设备信息响应
    osip_message_t* message = nullptr;
    
    // 确定传输协议参数
    std::string transport_param = "";
    if (config.sip_transport == "TCP" || config.sip_transport == "tcp") {
        transport_param = ";transport=tcp";
    } else {
        transport_param = ";transport=udp";
    }
    
    std::string from_uri = "sip:" + config.device_id + "@" + config.sip_server_domain + transport_param;
    std::string to_uri = "sip:" + config.sip_server_id + "@" + config.sip_server_domain + transport_param;
    
    eXosip_lock(exosip_context);
    
    int ret = eXosip_message_build_request(
        exosip_context,
        &message,
        "MESSAGE",
        to_uri.c_str(),
        from_uri.c_str(),
        nullptr
    );
    
    if (ret != 0 || !message) {
        eXosip_unlock(exosip_context);
        return;
    }
    
    // 构建设备信息XML
    std::ostringstream body;
    body << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n"
         << "<Response>\r\n"
         << "<CmdType>DeviceInfo</CmdType>\r\n"
         << "<SN>" << (rand() % 100000) << "</SN>\r\n"
         << "<DeviceID>" << config.device_id << "</DeviceID>\r\n"
         << "<DeviceName>" << config.device_name << "</DeviceName>\r\n"
         << "<Manufacturer>" << config.manufacturer << "</Manufacturer>\r\n"
         << "<Model>" << config.model << "</Model>\r\n"
         << "<Firmware>1.0.0</Firmware>\r\n"
         << "<Channel>" << config.max_channels << "</Channel>\r\n"
         << "</Response>\r\n";
    
    std::string body_str = body.str();
    osip_message_set_body(message, body_str.c_str(), body_str.length());
    osip_message_set_content_type(message, "Application/MANSCDP+xml");
    
    ret = eXosip_message_send_request(exosip_context, message);
    eXosip_unlock(exosip_context);
}

void GB28181SipClient::respondDeviceStatus(void* evt) {
    eXosip_event_t* event = static_cast<eXosip_event_t*>(evt);
    if (!event) return;
    
    // 先回复200 OK
    eXosip_lock(exosip_context);
    eXosip_message_send_answer(exosip_context, event->tid, 200, nullptr);
    eXosip_unlock(exosip_context);
    
    // 构建设备状态响应
    osip_message_t* message = nullptr;
    
    // 确定传输协议参数
    std::string transport_param = "";
    if (config.sip_transport == "TCP" || config.sip_transport == "tcp") {
        transport_param = ";transport=tcp";
    } else {
        transport_param = ";transport=udp";
    }
    
    std::string from_uri = "sip:" + config.device_id + "@" + config.sip_server_domain + transport_param;
    std::string to_uri = "sip:" + config.sip_server_id + "@" + config.sip_server_domain + transport_param;
    
    eXosip_lock(exosip_context);
    
    int ret = eXosip_message_build_request(
        exosip_context,
        &message,
        "MESSAGE",
        to_uri.c_str(),
        from_uri.c_str(),
        nullptr
    );
    
    if (ret != 0 || !message) {
        eXosip_unlock(exosip_context);
        return;
    }
    
    // 构建设备状态XML
    std::ostringstream body;
    body << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n"
         << "<Response>\r\n"
         << "<CmdType>DeviceStatus</CmdType>\r\n"
         << "<SN>" << (rand() % 100000) << "</SN>\r\n"
         << "<DeviceID>" << config.device_id << "</DeviceID>\r\n"
         << "<Result>OK</Result>\r\n"
         << "<Online>ONLINE</Online>\r\n"
         << "<Status>OK</Status>\r\n"
         << "</Response>\r\n";
    
    std::string body_str = body.str();
    osip_message_set_body(message, body_str.c_str(), body_str.length());
    osip_message_set_content_type(message, "Application/MANSCDP+xml");
    
    ret = eXosip_message_send_request(exosip_context, message);
    eXosip_unlock(exosip_context);
}

std::string GB28181SipClient::getChannelId(int channel_index) const {
    // 通道编码 = 设备ID前10位 + 类型码(131) + 通道序号(4位,补零) + 设备ID后3位
    if (config.device_id.length() != 20) {
        return "";
    }
    
    std::ostringstream oss;
    oss << config.device_id.substr(0, 10)   // 前10位
        << "131"                             // 类型码：131=前端设备通道
        << std::setw(4) << std::setfill('0') << channel_index  // 通道序号
        << config.device_id.substr(17, 3);   // 后3位
    
    return oss.str();
}

std::string GB28181SipClient::generateSSRC(const std::string& channel_id) const {
    // SSRC使用通道ID的后10位（数字部分）
    if (channel_id.length() >= 10) {
        return channel_id.substr(channel_id.length() - 10);
    }
    return channel_id;
}

std::string GB28181SipClient::generateSDP(const GB28181Session& session) const {
    std::ostringstream sdp;
    
    // SDP头部
    sdp << "v=0\r\n";
    sdp << "o=" << config.device_id << " 0 0 IN IP4 127.0.0.1\r\n";
    sdp << "s=" << session.session_name << "\r\n";
    sdp << "c=IN IP4 " << session.dest_ip << "\r\n";
    sdp << "t=0 0\r\n";
    
    // 媒体描述
    if (config.stream_mode == "PS") {
        // PS流
        sdp << "m=video " << session.dest_port << " RTP/AVP 96\r\n";
        sdp << "a=rtpmap:96 PS/90000\r\n";
    } else {
        // H.264流
        sdp << "m=video " << session.dest_port << " RTP/AVP 96\r\n";
        sdp << "a=rtpmap:96 H264/90000\r\n";
    }
    
    sdp << "a=sendonly\r\n";
    sdp << "a=ssrc:" << session.ssrc << "\r\n";
    sdp << "y=" << session.ssrc << "\r\n";
    
    return sdp.str();
}

bool GB28181SipClient::parseInviteSDP(const std::string& sdp, 
                                      std::string& ip, int& port) {
    // 简单解析SDP
    std::istringstream stream(sdp);
    std::string line;
    
    while (std::getline(stream, line)) {
        // 去除\r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // 解析c=行获取IP
        if (line.substr(0, 2) == "c=") {
            size_t pos = line.rfind(' ');
            if (pos != std::string::npos) {
                ip = line.substr(pos + 1);
            }
        }
        
        // 解析m=行获取端口
        if (line.substr(0, 2) == "m=") {
            std::istringstream mline(line);
            std::string m, media, port_str;
            mline >> m >> media >> port_str;
            try {
                port = std::stoi(port_str);
            } catch (...) {
                return false;
            }
        }
    }
    
    return !ip.empty() && port > 0;
}

std::vector<GB28181Session> GB28181SipClient::getActiveSessions() const {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    std::vector<GB28181Session> sessions;
    for (const auto& pair : active_sessions) {
        if (pair.second.is_active) {
            sessions.push_back(pair.second);
        }
    }
    return sessions;
}

} // namespace detector_service

