#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sqlite3.h>
#include "alert.h"

namespace detector_service {

class Database {
public:
    static Database& getInstance() {
        static Database instance;
        return instance;
    }

    bool initialize(const std::string& db_path);
    void close();

    // 报警记录管理
    int insertAlert(const AlertRecord& alert);
    bool deleteAlert(int alert_id);
    bool deleteAlertsByChannel(int channel_id);
    std::vector<AlertRecord> getAlerts(int limit = 100, int offset = 0);
    std::vector<AlertRecord> getAlertsByChannel(int channel_id, int limit = 100, int offset = 0);
    AlertRecord getAlert(int alert_id);
    int getAlertCount();
    int getAlertCountByChannel(int channel_id);

    // 清理旧数据
    bool cleanupOldAlerts(int days);

    // 通道管理
    int insertChannel(int id, const std::string& name, const std::string& source_url, 
                      bool enabled, bool push_enabled, const std::string& created_at, const std::string& updated_at);
    bool deleteChannel(int channel_id);
    bool updateChannel(int channel_id, const std::string& name, const std::string& source_url,
                       bool enabled, bool push_enabled, const std::string& updated_at);
    bool updateChannelStatus(int channel_id, const std::string& status, const std::string& updated_at);
    bool updateChannelPushEnabled(int channel_id, bool push_enabled, const std::string& updated_at);
    bool updateChannelId(int old_id, int new_id);
    std::vector<std::pair<int, std::string>> getAllChannelsFromDB();  // 返回 (id, name) 对
    bool loadChannelFromDB(int channel_id, std::string& name, std::string& source_url,
                          std::string& status, bool& enabled, bool& push_enabled, std::string& created_at, std::string& updated_at);
    int getMaxChannelId();

    // 推流配置管理
    bool saveStreamConfig(const std::string& rtmp_url, int width, int height, int fps, int bitrate);
    bool loadStreamConfig(std::string& rtmp_url, int& width, int& height, int& fps, int& bitrate);

private:
    Database() : db_(nullptr) {}
    ~Database() { close(); }
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* db_;
    bool createTables();
};

} // namespace detector_service

