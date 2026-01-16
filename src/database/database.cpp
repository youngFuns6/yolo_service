#include "database.h"
#include "common_utils.h"
#include <iostream>

namespace detector_service {

bool Database::initialize(const std::string& db_path) {
    if (db_ != nullptr) {
        close();
    }

    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "无法打开数据库: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return createTables();
}

void Database::close() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::createTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            channel_id INTEGER NOT NULL,
            channel_name TEXT NOT NULL,
            alert_type TEXT NOT NULL,
            alert_rule_id INTEGER NOT NULL DEFAULT 0,
            alert_rule_name TEXT NOT NULL DEFAULT '',
            image_path TEXT,
            confidence REAL,
            detected_objects TEXT,
            bbox_x REAL,
            bbox_y REAL,
            bbox_w REAL,
            bbox_h REAL,
            report_status TEXT NOT NULL DEFAULT 'pending',
            report_url TEXT NOT NULL DEFAULT '',
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS channels (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            source_url TEXT NOT NULL,
            status TEXT NOT NULL DEFAULT 'idle',
            enabled INTEGER NOT NULL DEFAULT 0,
            report_enabled INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        );
        
        CREATE TABLE IF NOT EXISTS report_config (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            type TEXT NOT NULL DEFAULT 'HTTP',
            http_url TEXT NOT NULL DEFAULT '',
            mqtt_broker TEXT NOT NULL DEFAULT '',
            mqtt_port INTEGER NOT NULL DEFAULT 1883,
            mqtt_topic TEXT NOT NULL DEFAULT '',
            mqtt_username TEXT NOT NULL DEFAULT '',
            mqtt_password TEXT NOT NULL DEFAULT '',
            mqtt_client_id TEXT NOT NULL DEFAULT 'detector_service',
            enabled INTEGER NOT NULL DEFAULT 0,
            updated_at TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_channel_id ON alerts(channel_id);
        CREATE INDEX IF NOT EXISTS idx_created_at ON alerts(created_at);

        CREATE TABLE IF NOT EXISTS gb28181_config (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            enabled INTEGER NOT NULL DEFAULT 0,
            sip_server_ip TEXT NOT NULL DEFAULT '',
            sip_server_port INTEGER NOT NULL DEFAULT 5060,
            sip_server_id TEXT NOT NULL DEFAULT '',
            sip_server_domain TEXT NOT NULL DEFAULT '',
            device_id TEXT NOT NULL DEFAULT '',
            device_password TEXT NOT NULL DEFAULT '',
            device_name TEXT NOT NULL DEFAULT '',
            manufacturer TEXT NOT NULL DEFAULT '',
            model TEXT NOT NULL DEFAULT '',
            local_sip_port INTEGER NOT NULL DEFAULT 5061,
            rtp_port_start INTEGER NOT NULL DEFAULT 30000,
            rtp_port_end INTEGER NOT NULL DEFAULT 30100,
            heartbeat_interval INTEGER NOT NULL DEFAULT 60,
            heartbeat_count INTEGER NOT NULL DEFAULT 3,
            register_expires INTEGER NOT NULL DEFAULT 3600,
            stream_mode TEXT NOT NULL DEFAULT 'PS',
            max_channels INTEGER NOT NULL DEFAULT 32,
            sip_transport TEXT NOT NULL DEFAULT 'UDP',
            updated_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS algorithm_configs (
            channel_id INTEGER PRIMARY KEY,
            model_path TEXT NOT NULL DEFAULT 'yolov11n.onnx',
            conf_threshold REAL NOT NULL DEFAULT 0.65,
            nms_threshold REAL NOT NULL DEFAULT 0.45,
            input_width INTEGER NOT NULL DEFAULT 640,
            input_height INTEGER NOT NULL DEFAULT 640,
            detection_interval INTEGER NOT NULL DEFAULT 3,
            enabled_classes TEXT NOT NULL DEFAULT '[]',
            rois_json TEXT NOT NULL DEFAULT '[]',
            alert_rules_json TEXT NOT NULL DEFAULT '[]',
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL,
            FOREIGN KEY (channel_id) REFERENCES channels(id) ON DELETE CASCADE
        );
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "创建表失败: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    // 为现有数据库添加report_enabled字段（如果不存在）
    const char* alter_report_sql = "ALTER TABLE channels ADD COLUMN report_enabled INTEGER NOT NULL DEFAULT 0";
    err_msg = nullptr;
    rc = sqlite3_exec(db_, alter_report_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK && err_msg) {
        std::string error_str = err_msg;
        if (error_str.find("duplicate column name") == std::string::npos) {
            std::cerr << "添加report_enabled字段失败: " << err_msg << std::endl;
        }
        sqlite3_free(err_msg);
    }

    // 为现有数据库添加report_status字段（如果不存在）
    // 注意：SQLite不支持直接删除列，所以如果image_data列存在，我们保留它但不使用
    const char* alter_report_status_sql = "ALTER TABLE alerts ADD COLUMN report_status TEXT NOT NULL DEFAULT 'pending'";
    err_msg = nullptr;
    rc = sqlite3_exec(db_, alter_report_status_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK && err_msg) {
        std::string error_str = err_msg;
        if (error_str.find("duplicate column name") == std::string::npos) {
            std::cerr << "添加report_status字段失败: " << err_msg << std::endl;
        }
        sqlite3_free(err_msg);
    }

    // 为现有数据库添加report_url字段（如果不存在）
    const char* alter_report_url_sql = "ALTER TABLE alerts ADD COLUMN report_url TEXT NOT NULL DEFAULT ''";
    err_msg = nullptr;
    rc = sqlite3_exec(db_, alter_report_url_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK && err_msg) {
        std::string error_str = err_msg;
        if (error_str.find("duplicate column name") == std::string::npos) {
            std::cerr << "添加report_url字段失败: " << err_msg << std::endl;
        }
        sqlite3_free(err_msg);
    }

    // 为现有数据库添加sip_transport字段（如果不存在）
    const char* alter_sip_transport_sql = "ALTER TABLE gb28181_config ADD COLUMN sip_transport TEXT NOT NULL DEFAULT 'UDP'";
    err_msg = nullptr;
    rc = sqlite3_exec(db_, alter_sip_transport_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK && err_msg) {
        std::string error_str = err_msg;
        if (error_str.find("duplicate column name") == std::string::npos) {
            std::cerr << "添加sip_transport字段失败: " << err_msg << std::endl;
        }
        sqlite3_free(err_msg);
    }

    return true;
}

int Database::insertAlert(const AlertRecord& alert) {
    const char* sql = R"(
        INSERT INTO alerts (
            channel_id, channel_name, alert_type, alert_rule_id, alert_rule_name,
            image_path, confidence, detected_objects, 
            bbox_x, bbox_y, bbox_w, bbox_h, report_status, report_url, created_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "准备语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_int(stmt, 1, alert.channel_id);
    sqlite3_bind_text(stmt, 2, alert.channel_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, alert.alert_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, alert.alert_rule_id);
    sqlite3_bind_text(stmt, 5, alert.alert_rule_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, alert.image_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 7, alert.confidence);
    sqlite3_bind_text(stmt, 8, alert.detected_objects.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 9, alert.bbox_x);
    sqlite3_bind_double(stmt, 10, alert.bbox_y);
    sqlite3_bind_double(stmt, 11, alert.bbox_w);
    sqlite3_bind_double(stmt, 12, alert.bbox_h);
    
    std::string report_status = alert.report_status.empty() ? "pending" : alert.report_status;
    sqlite3_bind_text(stmt, 13, report_status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 14, alert.report_url.c_str(), -1, SQLITE_STATIC);
    
    std::string created_at = alert.created_at.empty() ? getCurrentTime() : alert.created_at;
    sqlite3_bind_text(stmt, 15, created_at.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    int alert_id = -1;
    
    if (rc == SQLITE_DONE) {
        alert_id = sqlite3_last_insert_rowid(db_);
    } else {
        std::cerr << "插入失败: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return alert_id;
}

bool Database::deleteAlert(int alert_id) {
    const char* sql = "DELETE FROM alerts WHERE id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, alert_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::deleteAlertsByChannel(int channel_id) {
    const char* sql = "DELETE FROM alerts WHERE channel_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, channel_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::vector<AlertRecord> Database::getAlerts(int limit, int offset) {
    std::vector<AlertRecord> alerts;
    const char* sql = "SELECT * FROM alerts ORDER BY created_at DESC LIMIT ? OFFSET ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return alerts;
    }

    sqlite3_bind_int(stmt, 1, limit);
    sqlite3_bind_int(stmt, 2, offset);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AlertRecord alert;
        int col_count = sqlite3_column_count(stmt);
        alert.id = sqlite3_column_int(stmt, 0);
        alert.channel_id = sqlite3_column_int(stmt, 1);
        alert.channel_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        alert.alert_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        // 兼容新旧数据库结构
        if (col_count > 4) {
            alert.alert_rule_id = sqlite3_column_int(stmt, 4);
            if (col_count > 5) {
                const char* rule_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                alert.alert_rule_name = rule_name ? rule_name : "";
            }
        }
        int offset = (col_count > 5) ? 6 : 4;
        alert.image_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset));
        alert.confidence = sqlite3_column_double(stmt, offset + 1);
        alert.detected_objects = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 2));
        alert.bbox_x = sqlite3_column_double(stmt, offset + 3);
        alert.bbox_y = sqlite3_column_double(stmt, offset + 4);
        alert.bbox_w = sqlite3_column_double(stmt, offset + 5);
        alert.bbox_h = sqlite3_column_double(stmt, offset + 6);
        
        // 兼容新旧数据库结构：检查是否有 report_status 和 report_url 字段
        if (col_count > offset + 7) {
            const char* report_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 7));
            alert.report_status = report_status ? report_status : "pending";
        } else {
            alert.report_status = "pending";
        }
        if (col_count > offset + 8) {
            const char* report_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 8));
            alert.report_url = report_url ? report_url : "";
        } else {
            alert.report_url = "";
        }
        
        alert.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + (col_count > offset + 8 ? 9 : 8)));
        
        alerts.push_back(alert);
    }

    sqlite3_finalize(stmt);
    return alerts;
}

std::vector<AlertRecord> Database::getAlertsByChannel(int channel_id, int limit, int offset) {
    std::vector<AlertRecord> alerts;
    const char* sql = "SELECT * FROM alerts WHERE channel_id = ? ORDER BY created_at DESC LIMIT ? OFFSET ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return alerts;
    }

    sqlite3_bind_int(stmt, 1, channel_id);
    sqlite3_bind_int(stmt, 2, limit);
    sqlite3_bind_int(stmt, 3, offset);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AlertRecord alert;
        int col_count = sqlite3_column_count(stmt);
        alert.id = sqlite3_column_int(stmt, 0);
        alert.channel_id = sqlite3_column_int(stmt, 1);
        alert.channel_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        alert.alert_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        // 兼容新旧数据库结构
        if (col_count > 4) {
            alert.alert_rule_id = sqlite3_column_int(stmt, 4);
            if (col_count > 5) {
                const char* rule_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                alert.alert_rule_name = rule_name ? rule_name : "";
            }
        }
        int offset = (col_count > 5) ? 6 : 4;
        alert.image_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset));
        alert.confidence = sqlite3_column_double(stmt, offset + 1);
        alert.detected_objects = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 2));
        alert.bbox_x = sqlite3_column_double(stmt, offset + 3);
        alert.bbox_y = sqlite3_column_double(stmt, offset + 4);
        alert.bbox_w = sqlite3_column_double(stmt, offset + 5);
        alert.bbox_h = sqlite3_column_double(stmt, offset + 6);
        
        // 兼容新旧数据库结构：检查是否有 report_status 和 report_url 字段
        if (col_count > offset + 7) {
            const char* report_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 7));
            alert.report_status = report_status ? report_status : "pending";
        } else {
            alert.report_status = "pending";
        }
        if (col_count > offset + 8) {
            const char* report_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 8));
            alert.report_url = report_url ? report_url : "";
        } else {
            alert.report_url = "";
        }
        
        alert.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + (col_count > offset + 8 ? 9 : 8)));
        
        alerts.push_back(alert);
    }

    sqlite3_finalize(stmt);
    return alerts;
}

AlertRecord Database::getAlert(int alert_id) {
    AlertRecord alert;
    const char* sql = "SELECT * FROM alerts WHERE id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return alert;
    }

    sqlite3_bind_int(stmt, 1, alert_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int col_count = sqlite3_column_count(stmt);
        alert.id = sqlite3_column_int(stmt, 0);
        alert.channel_id = sqlite3_column_int(stmt, 1);
        alert.channel_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        alert.alert_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        // 兼容新旧数据库结构
        if (col_count > 4) {
            alert.alert_rule_id = sqlite3_column_int(stmt, 4);
            if (col_count > 5) {
                const char* rule_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                alert.alert_rule_name = rule_name ? rule_name : "";
            }
        }
        int offset = (col_count > 5) ? 6 : 4;
        alert.image_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset));
        alert.confidence = sqlite3_column_double(stmt, offset + 1);
        alert.detected_objects = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 2));
        alert.bbox_x = sqlite3_column_double(stmt, offset + 3);
        alert.bbox_y = sqlite3_column_double(stmt, offset + 4);
        alert.bbox_w = sqlite3_column_double(stmt, offset + 5);
        alert.bbox_h = sqlite3_column_double(stmt, offset + 6);
        
        // 兼容新旧数据库结构：检查是否有 report_status 和 report_url 字段
        if (col_count > offset + 7) {
            const char* report_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 7));
            alert.report_status = report_status ? report_status : "pending";
        } else {
            alert.report_status = "pending";
        }
        if (col_count > offset + 8) {
            const char* report_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 8));
            alert.report_url = report_url ? report_url : "";
        } else {
            alert.report_url = "";
        }
        
        alert.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + (col_count > offset + 8 ? 9 : 8)));
    }

    sqlite3_finalize(stmt);
    return alert;
}

bool Database::updateAlertReportStatus(int alert_id, const std::string& report_status, const std::string& report_url) {
    const char* sql = "UPDATE alerts SET report_status = ?, report_url = ? WHERE id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "准备语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, report_status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, report_url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, alert_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

int Database::getAlertCount() {
    const char* sql = "SELECT COUNT(*) FROM alerts";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

int Database::getAlertCountByChannel(int channel_id) {
    const char* sql = "SELECT COUNT(*) FROM alerts WHERE channel_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, channel_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

bool Database::cleanupOldAlerts(int days) {
    const char* sql = "DELETE FROM alerts WHERE created_at < datetime('now', '-' || ? || ' days')";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, days);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

// 通道管理方法实现
int Database::insertChannel(int id, const std::string& name, const std::string& source_url,
                             bool enabled, bool report_enabled, const std::string& created_at, const std::string& updated_at) {
    const char* sql = R"(
        INSERT INTO channels (id, name, source_url, status, enabled, report_enabled, created_at, updated_at)
        VALUES (?, ?, ?, 'idle', ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "准备语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, source_url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 5, report_enabled ? 1 : 0);
    sqlite3_bind_text(stmt, 6, created_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, updated_at.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    int result = -1;
    
    if (rc == SQLITE_DONE) {
        result = id;
    } else {
        std::cerr << "插入通道失败: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return result;
}

bool Database::deleteChannel(int channel_id) {
    const char* sql = "DELETE FROM channels WHERE id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, channel_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::updateChannel(int channel_id, const std::string& name, const std::string& source_url,
                             bool enabled, bool report_enabled, const std::string& updated_at) {
    const char* sql = R"(
        UPDATE channels 
        SET name = ?, source_url = ?, enabled = ?, report_enabled = ?, updated_at = ?
        WHERE id = ?
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, source_url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 4, report_enabled ? 1 : 0);
    sqlite3_bind_text(stmt, 5, updated_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, channel_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::updateChannelStatus(int channel_id, const std::string& status, const std::string& updated_at) {
    const char* sql = "UPDATE channels SET status = ?, updated_at = ? WHERE id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, updated_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, channel_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::updateChannelId(int old_id, int new_id) {
    // SQLite不支持直接更新PRIMARY KEY，需要使用事务删除并重新插入
    // 首先获取旧记录的所有数据
    std::string name, source_url, status, created_at, updated_at;
    bool enabled, report_enabled;
    
    if (!loadChannelFromDB(old_id, name, source_url, status, enabled, report_enabled, created_at, updated_at)) {
        return false;
    }
    
    // 开始事务
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            std::cerr << "开始事务失败: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        }
        return false;
    }
    
    // 删除旧记录
    const char* delete_sql = "DELETE FROM channels WHERE id = ?";
    sqlite3_stmt* delete_stmt;
    rc = sqlite3_prepare_v2(db_, delete_sql, -1, &delete_stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        return false;
    }
    
    sqlite3_bind_int(delete_stmt, 1, old_id);
    rc = sqlite3_step(delete_stmt);
    sqlite3_finalize(delete_stmt);
    
    if (rc != SQLITE_DONE) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        return false;
    }
    
    // 插入新记录
    const char* insert_sql = R"(
        INSERT INTO channels (id, name, source_url, status, enabled, report_enabled, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )";
    sqlite3_stmt* insert_stmt;
    rc = sqlite3_prepare_v2(db_, insert_sql, -1, &insert_stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        return false;
    }
    
    sqlite3_bind_int(insert_stmt, 1, new_id);
    sqlite3_bind_text(insert_stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insert_stmt, 3, source_url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insert_stmt, 4, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(insert_stmt, 5, enabled ? 1 : 0);
    sqlite3_bind_int(insert_stmt, 6, report_enabled ? 1 : 0);
    sqlite3_bind_text(insert_stmt, 7, created_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insert_stmt, 8, updated_at.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(insert_stmt);
    sqlite3_finalize(insert_stmt);
    
    if (rc != SQLITE_DONE) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        return false;
    }
    
    // 更新alerts表中的channel_id（外键）
    const char* update_alerts_sql = "UPDATE alerts SET channel_id = ? WHERE channel_id = ?";
    sqlite3_stmt* update_alerts_stmt;
    rc = sqlite3_prepare_v2(db_, update_alerts_sql, -1, &update_alerts_stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        return false;
    }
    
    sqlite3_bind_int(update_alerts_stmt, 1, new_id);
    sqlite3_bind_int(update_alerts_stmt, 2, old_id);
    rc = sqlite3_step(update_alerts_stmt);
    sqlite3_finalize(update_alerts_stmt);
    
    if (rc != SQLITE_DONE) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        return false;
    }
    
    // 提交事务
    rc = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            std::cerr << "提交事务失败: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        }
        return false;
    }
    
    return true;
}

std::vector<std::pair<int, std::string>> Database::getAllChannelsFromDB() {
    std::vector<std::pair<int, std::string>> channels;
    const char* sql = "SELECT id, name FROM channels";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return channels;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        channels.push_back({id, name ? name : ""});
    }

    sqlite3_finalize(stmt);
    return channels;
}

bool Database::loadChannelFromDB(int channel_id, std::string& name, std::string& source_url,
                                  std::string& status, bool& enabled, bool& report_enabled, std::string& created_at, std::string& updated_at) {
    const char* sql = "SELECT name, source_url, status, enabled, report_enabled, created_at, updated_at FROM channels WHERE id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, channel_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int col_count = sqlite3_column_count(stmt);
        name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        source_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        enabled = sqlite3_column_int(stmt, 3) != 0;
        // 如果report_enabled列不存在（旧数据库），默认为false
        report_enabled = (col_count > 4) ? (sqlite3_column_int(stmt, 4) != 0) : false;
        // 根据列数确定created_at和updated_at的位置
        int created_at_idx = (col_count > 4) ? 5 : 4;
        int updated_at_idx = (col_count > 4) ? 6 : 5;
        created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, created_at_idx));
        updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, updated_at_idx));
        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}

int Database::getMaxChannelId() {
    const char* sql = "SELECT MAX(id) FROM channels";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    int max_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        max_id = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return max_id;
}

bool Database::saveReportConfig(const ReportConfig& config) {
    const char* sql = R"(
        INSERT OR REPLACE INTO report_config (
            id, type, http_url, mqtt_broker, mqtt_port, mqtt_topic,
            mqtt_username, mqtt_password, mqtt_client_id, enabled, updated_at
        ) VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "准备语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    std::string type_str = (config.type == ReportType::HTTP) ? "HTTP" : "MQTT";
    std::string updated_at = getCurrentTime();
    
    sqlite3_bind_text(stmt, 1, type_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, config.http_url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, config.mqtt_broker.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, config.mqtt_port);
    sqlite3_bind_text(stmt, 5, config.mqtt_topic.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, config.mqtt_username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, config.mqtt_password.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, config.mqtt_client_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 9, config.enabled.load() ? 1 : 0);
    sqlite3_bind_text(stmt, 10, updated_at.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool Database::loadReportConfig(ReportConfig& config) {
    const char* sql = "SELECT type, http_url, mqtt_broker, mqtt_port, mqtt_topic, "
                      "mqtt_username, mqtt_password, mqtt_client_id, enabled "
                      "FROM report_config WHERE id = 1";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* type_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        config.type = (type_str && std::string(type_str) == "MQTT") ? ReportType::MQTT : ReportType::HTTP;
        
        const char* http_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        config.http_url = http_url ? http_url : "";
        
        const char* mqtt_broker = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        config.mqtt_broker = mqtt_broker ? mqtt_broker : "";
        
        config.mqtt_port = sqlite3_column_int(stmt, 3);
        
        const char* mqtt_topic = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        config.mqtt_topic = mqtt_topic ? mqtt_topic : "";
        
        const char* mqtt_username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        config.mqtt_username = mqtt_username ? mqtt_username : "";
        
        const char* mqtt_password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        config.mqtt_password = mqtt_password ? mqtt_password : "";
        
        const char* mqtt_client_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        config.mqtt_client_id = mqtt_client_id ? mqtt_client_id : "detector_service";
        
        config.enabled = sqlite3_column_int(stmt, 8) != 0;
        
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
}

bool Database::saveGB28181Config(const GB28181Config& config) {
    const char* sql = R"(
        INSERT OR REPLACE INTO gb28181_config (
            id, enabled, sip_server_ip, sip_server_port, sip_server_id, sip_server_domain,
            device_id, device_password, device_name, manufacturer, model,
            local_sip_port, rtp_port_start, rtp_port_end, heartbeat_interval, heartbeat_count,
            register_expires, stream_mode, max_channels, sip_transport, updated_at
        ) VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "准备语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    std::string updated_at = getCurrentTime();
    
    sqlite3_bind_int(stmt, 1, config.enabled.load() ? 1 : 0);
    sqlite3_bind_text(stmt, 2, config.sip_server_ip.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, config.sip_server_port);
    sqlite3_bind_text(stmt, 4, config.sip_server_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, config.sip_server_domain.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, config.device_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, config.device_password.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, config.device_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, config.manufacturer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, config.model.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 11, config.local_sip_port);
    sqlite3_bind_int(stmt, 12, config.rtp_port_start);
    sqlite3_bind_int(stmt, 13, config.rtp_port_end);
    sqlite3_bind_int(stmt, 14, config.heartbeat_interval);
    sqlite3_bind_int(stmt, 15, config.heartbeat_count);
    sqlite3_bind_int(stmt, 16, config.register_expires);
    sqlite3_bind_text(stmt, 17, config.stream_mode.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 18, config.max_channels);
    sqlite3_bind_text(stmt, 19, config.sip_transport.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 20, updated_at.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool Database::loadGB28181Config(GB28181Config& config) {
    const char* sql = R"(
        SELECT enabled, sip_server_ip, sip_server_port, sip_server_id, sip_server_domain,
               device_id, device_password, device_name, manufacturer, model,
               local_sip_port, rtp_port_start, rtp_port_end, heartbeat_interval, heartbeat_count,
               register_expires, stream_mode, max_channels, sip_transport
        FROM gb28181_config WHERE id = 1
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        config.enabled.store(sqlite3_column_int(stmt, 0) != 0);
        
        const char* sip_server_ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        config.sip_server_ip = sip_server_ip ? sip_server_ip : "";
        
        config.sip_server_port = sqlite3_column_int(stmt, 2);
        
        const char* sip_server_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        config.sip_server_id = sip_server_id ? sip_server_id : "";
        
        const char* sip_server_domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        config.sip_server_domain = sip_server_domain ? sip_server_domain : "";
        
        const char* device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        config.device_id = device_id ? device_id : "";
        
        const char* device_password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        config.device_password = device_password ? device_password : "";
        
        const char* device_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        config.device_name = device_name ? device_name : "";
        
        const char* manufacturer = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        config.manufacturer = manufacturer ? manufacturer : "";
        
        const char* model = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        config.model = model ? model : "";
        
        config.local_sip_port = sqlite3_column_int(stmt, 10);
        config.rtp_port_start = sqlite3_column_int(stmt, 11);
        config.rtp_port_end = sqlite3_column_int(stmt, 12);
        config.heartbeat_interval = sqlite3_column_int(stmt, 13);
        config.heartbeat_count = sqlite3_column_int(stmt, 14);
        config.register_expires = sqlite3_column_int(stmt, 15);
        
        const char* stream_mode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16));
        config.stream_mode = stream_mode ? stream_mode : "PS";
        
        config.max_channels = sqlite3_column_int(stmt, 17);
        
        const char* sip_transport = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 18));
        config.sip_transport = sip_transport ? sip_transport : "UDP";
        
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
}

} // namespace detector_service

