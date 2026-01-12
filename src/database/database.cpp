#include "database.h"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace detector_service {

std::string getCurrentTime() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

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
            push_enabled INTEGER NOT NULL DEFAULT 0,
            report_enabled INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        );
        
        -- 为现有数据库添加push_enabled字段（如果不存在）
        -- SQLite不支持IF NOT EXISTS for ALTER TABLE ADD COLUMN，需要先检查
        -- 这里使用PRAGMA table_info来检查列是否存在，如果不存在则添加
        
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

        CREATE TABLE IF NOT EXISTS push_stream_config (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            rtmp_url TEXT NOT NULL DEFAULT '',
            width INTEGER,
            height INTEGER,
            fps INTEGER,
            bitrate INTEGER,
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

    // 为现有数据库添加push_enabled字段（如果不存在）
    // SQLite 3.32.0+ 支持 ALTER TABLE ADD COLUMN IF NOT EXISTS，但为了兼容性，先检查
    const char* alter_sql = "ALTER TABLE channels ADD COLUMN push_enabled INTEGER NOT NULL DEFAULT 0";
    err_msg = nullptr;
    rc = sqlite3_exec(db_, alter_sql, nullptr, nullptr, &err_msg);
    // 如果列已存在，会返回错误，这是正常的，可以忽略
    if (rc != SQLITE_OK && err_msg) {
        // 检查是否是"duplicate column name"错误，如果是则忽略
        std::string error_str = err_msg;
        if (error_str.find("duplicate column name") == std::string::npos) {
            std::cerr << "添加push_enabled字段失败: " << err_msg << std::endl;
        }
        sqlite3_free(err_msg);
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
                             bool enabled, bool push_enabled, bool report_enabled, const std::string& created_at, const std::string& updated_at) {
    const char* sql = R"(
        INSERT INTO channels (id, name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at)
        VALUES (?, ?, ?, 'idle', ?, ?, ?, ?, ?)
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
    sqlite3_bind_int(stmt, 5, push_enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 6, report_enabled ? 1 : 0);
    sqlite3_bind_text(stmt, 7, created_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, updated_at.c_str(), -1, SQLITE_STATIC);

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
                             bool enabled, bool push_enabled, bool report_enabled, const std::string& updated_at) {
    const char* sql = R"(
        UPDATE channels 
        SET name = ?, source_url = ?, enabled = ?, push_enabled = ?, report_enabled = ?, updated_at = ?
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
    sqlite3_bind_int(stmt, 4, push_enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 5, report_enabled ? 1 : 0);
    sqlite3_bind_text(stmt, 6, updated_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, channel_id);
    
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

bool Database::updateChannelPushEnabled(int channel_id, bool push_enabled, const std::string& updated_at) {
    const char* sql = "UPDATE channels SET push_enabled = ?, updated_at = ? WHERE id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, push_enabled ? 1 : 0);
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
    bool enabled, push_enabled, report_enabled;
    
    if (!loadChannelFromDB(old_id, name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at)) {
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
        INSERT INTO channels (id, name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
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
    sqlite3_bind_int(insert_stmt, 6, push_enabled ? 1 : 0);
    sqlite3_bind_int(insert_stmt, 7, report_enabled ? 1 : 0);
    sqlite3_bind_text(insert_stmt, 8, created_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insert_stmt, 9, updated_at.c_str(), -1, SQLITE_STATIC);
    
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
                                  std::string& status, bool& enabled, bool& push_enabled, bool& report_enabled, std::string& created_at, std::string& updated_at) {
    const char* sql = "SELECT name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at FROM channels WHERE id = ?";
    
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
        // 如果push_enabled列不存在（旧数据库），默认为false
        push_enabled = (col_count > 4) ? (sqlite3_column_int(stmt, 4) != 0) : false;
        // 如果report_enabled列不存在（旧数据库），默认为false
        report_enabled = (col_count > 5) ? (sqlite3_column_int(stmt, 5) != 0) : false;
        // 根据列数确定created_at和updated_at的位置
        int created_at_idx = (col_count > 5) ? 6 : ((col_count > 4) ? 5 : 4);
        int updated_at_idx = (col_count > 5) ? 7 : ((col_count > 4) ? 6 : 5);
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

bool Database::savePushStreamConfig(const std::string& rtmp_url, 
                                     std::optional<int> width, 
                                     std::optional<int> height, 
                                     std::optional<int> fps, 
                                     std::optional<int> bitrate) {
    // 先加载现有配置，只更新提供的字段
    std::string existing_rtmp_url;
    std::optional<int> existing_width, existing_height, existing_fps, existing_bitrate;
    
    bool exists = loadPushStreamConfig(existing_rtmp_url, existing_width, existing_height, existing_fps, existing_bitrate);
    
    // 使用提供的值，如果没有提供则使用现有值
    std::optional<int> final_width = width.has_value() ? width : existing_width;
    std::optional<int> final_height = height.has_value() ? height : existing_height;
    std::optional<int> final_fps = fps.has_value() ? fps : existing_fps;
    std::optional<int> final_bitrate = bitrate.has_value() ? bitrate : existing_bitrate;
    
    // 使用 INSERT OR REPLACE 来确保只有一条记录
    const char* sql = R"(
        INSERT OR REPLACE INTO push_stream_config (id, rtmp_url, width, height, fps, bitrate, updated_at)
        VALUES (1, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "准备语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    std::string updated_at = getCurrentTime();
    sqlite3_bind_text(stmt, 1, rtmp_url.c_str(), -1, SQLITE_STATIC);
    
    // 如果提供了值则绑定，否则绑定 NULL
    if (final_width.has_value()) {
        sqlite3_bind_int(stmt, 2, final_width.value());
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    if (final_height.has_value()) {
        sqlite3_bind_int(stmt, 3, final_height.value());
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    if (final_fps.has_value()) {
        sqlite3_bind_int(stmt, 4, final_fps.value());
    } else {
        sqlite3_bind_null(stmt, 4);
    }
    if (final_bitrate.has_value()) {
        sqlite3_bind_int(stmt, 5, final_bitrate.value());
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    
    sqlite3_bind_text(stmt, 6, updated_at.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool Database::loadPushStreamConfig(std::string& rtmp_url, 
                                     std::optional<int>& width, 
                                     std::optional<int>& height, 
                                     std::optional<int>& fps, 
                                     std::optional<int>& bitrate) {
    const char* sql = "SELECT rtmp_url, width, height, fps, bitrate FROM push_stream_config WHERE id = 1";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rtmp_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        
        // 检查每个字段是否为 NULL
        if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
            width = sqlite3_column_int(stmt, 1);
        } else {
            width = std::nullopt;
        }
        
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            height = sqlite3_column_int(stmt, 2);
        } else {
            height = std::nullopt;
        }
        
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            fps = sqlite3_column_int(stmt, 3);
        } else {
            fps = std::nullopt;
        }
        
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
            bitrate = sqlite3_column_int(stmt, 4);
        } else {
            bitrate = std::nullopt;
        }
        
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
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

} // namespace detector_service

