import { useState, useEffect } from "react";
import {
  Table,
  Button,
  Space,
  message,
  Tag,
  Popconfirm,
  Card,
  Modal,
  Image,
  Select,
  DatePicker,
  Row,
  Col,
  Statistic,
} from "antd";
import type { ColumnsType } from "antd/es/table";
import {
  FaTrash,
  FaEye,
  FaRedo,
} from "react-icons/fa";
import dayjs, { type Dayjs } from "dayjs";
import {
  getAlerts,
  getAlertsByChannel,
  deleteAlert,
  type Alert,
  type GetAlertsParams,
} from "@/api/alert";
import { getChannelList, type Channel } from "@/api/channel";

const { RangePicker } = DatePicker;

function AlertList() {
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const [loading, setLoading] = useState(false);
  const [total, setTotal] = useState(0);
  const [pagination, setPagination] = useState({
    current: 1,
    pageSize: 10,
  });
  const [viewModalVisible, setViewModalVisible] = useState(false);
  const [viewingAlert, setViewingAlert] = useState<Alert | null>(null);
  const [channels, setChannels] = useState<Channel[]>([]);
  const [filters, setFilters] = useState<{
    channelId?: number;
    alertType?: string;
    dateRange?: [Dayjs, Dayjs];
  }>({});

  // 加载通道列表
  function loadChannels() {
    getChannelList()
      .then((response) => {
        if (response.success && response.channels) {
          setChannels(response.channels);
        }
      })
      .catch((error) => {
        console.error("获取通道列表失败:", error);
      });
  }

  // 加载报警列表
  function loadAlerts() {
    setLoading(true);
    const params: GetAlertsParams = {
      limit: pagination.pageSize,
      offset: (pagination.current - 1) * pagination.pageSize,
    };

    const requestPromise = filters.channelId
      ? getAlertsByChannel(filters.channelId, params)
      : getAlerts(params);

    requestPromise
      .then((response) => {
        if (response.success && response.alerts) {
          // 如果有日期筛选，在前端进行过滤
          let filteredAlerts = response.alerts;
          if (filters.dateRange) {
            const [startDate, endDate] = filters.dateRange;
            filteredAlerts = response.alerts.filter((alert) => {
              const alertDate = dayjs(alert.created_at);
              return (
                alertDate.isAfter(startDate.startOf("day")) &&
                alertDate.isBefore(endDate.endOf("day"))
              );
            });
          }

          // 如果有报警类型筛选，在前端进行过滤
          if (filters.alertType) {
            filteredAlerts = filteredAlerts.filter(
              (alert) => alert.alert_type === filters.alertType
            );
          }

          setAlerts(filteredAlerts);
          setTotal(response.total);
        } else {
          message.error(response.error || "获取报警列表失败");
        }
      })
      .catch((error) => {
        console.error("获取报警列表失败:", error);
        message.error("获取报警列表失败");
      })
      .finally(() => {
        setLoading(false);
      });
  }

  useEffect(() => {
    loadChannels();
  }, []);

  useEffect(() => {
    loadAlerts();
  }, [pagination.current, pagination.pageSize, filters.channelId]);

  // 处理分页变化
  function handleTableChange(page: number, pageSize: number) {
    setPagination({
      current: page,
      pageSize,
    });
  }

  // 删除报警
  function handleDelete(alertId: number) {
    deleteAlert(alertId)
      .then((response) => {
        if (response.success) {
          message.success("删除报警成功");
          loadAlerts();
        } else {
          message.error(response.error || "删除报警失败");
        }
      })
      .catch((error) => {
        console.error("删除报警失败:", error);
        message.error("删除报警失败");
      });
  }

  // 打开查看弹窗
  function handleOpenView(alert: Alert) {
    setViewingAlert(alert);
    setViewModalVisible(true);
  }

  // 关闭查看弹窗
  function handleCloseView() {
    setViewModalVisible(false);
    setViewingAlert(null);
  }

  // 处理筛选
  function handleFilterChange(key: string, value: any) {
    setFilters((prev) => ({
      ...prev,
      [key]: value,
    }));
    // 重置到第一页
    setPagination((prev) => ({
      ...prev,
      current: 1,
    }));
  }

  // 重置筛选
  function handleResetFilters() {
    setFilters({});
    setPagination((prev) => ({
      ...prev,
      current: 1,
    }));
  }

  // 获取报警类型标签颜色
  function getAlertTypeColor(alertType: string) {
    const colorMap: Record<string, string> = {
      person: "blue",
      vehicle: "green",
      fire: "red",
      smoke: "orange",
      intrusion: "purple",
    };
    return colorMap[alertType.toLowerCase()] || "default";
  }

  // 格式化置信度
  function formatConfidence(confidence: number) {
    return `${(confidence * 100).toFixed(1)}%`;
  }

  // 获取图片 URL
  function getImageUrl(alert: Alert) {
    if (alert.image_path) {
      // 如果后端提供了图片路径，可以拼接完整 URL
      return alert.image_path;
    }
    return "";
  }

  // 获取上报状态标签颜色
  function getReportStatusColor(status: string) {
    const colorMap: Record<string, string> = {
      pending: "default",
      success: "success",
      failed: "error",
    };
    return colorMap[status.toLowerCase()] || "default";
  }

  const columns: ColumnsType<Alert> = [
    {
      title: "ID",
      dataIndex: "id",
      key: "id",
      width: 80,
      sorter: (a, b) => a.id - b.id,
    },
    {
      title: "通道",
      dataIndex: "channel_name",
      key: "channel_name",
      width: 150,
      render: (name: string, record: Alert) => (
        <div>
          <div>{name}</div>
          <Tag color="default" style={{ fontSize: "12px" }}>
            ID: {record.channel_id}
          </Tag>
        </div>
      ),
    },
    {
      title: "报警类型",
      dataIndex: "alert_type",
      key: "alert_type",
      width: 120,
      render: (alertType: string) => (
        <Tag color={getAlertTypeColor(alertType)}>{alertType}</Tag>
      ),
    },
    {
      title: "检测对象",
      dataIndex: "detected_objects",
      key: "detected_objects",
      width: 150,
      ellipsis: true,
    },
    {
      title: "置信度",
      dataIndex: "confidence",
      key: "confidence",
      width: 100,
      render: (confidence: number) => (
        <Tag color={confidence > 0.8 ? "success" : confidence > 0.5 ? "warning" : "default"}>
          {formatConfidence(confidence)}
        </Tag>
      ),
      sorter: (a, b) => a.confidence - b.confidence,
    },
    {
      title: "位置",
      key: "bbox",
      width: 200,
      render: (_, record: Alert) => (
        <div style={{ fontSize: "12px" }}>
          <div>X: {record.bbox_x.toFixed(0)}, Y: {record.bbox_y.toFixed(0)}</div>
          <div>W: {record.bbox_w.toFixed(0)}, H: {record.bbox_h.toFixed(0)}</div>
        </div>
      ),
    },
    {
      title: "上报状态",
      dataIndex: "report_status",
      key: "report_status",
      width: 100,
      render: (status: string) => (
        <Tag color={getReportStatusColor(status)}>
          {status === "pending" ? "待上报" : status === "success" ? "已上报" : "上报失败"}
        </Tag>
      ),
    },
    {
      title: "上报地址",
      dataIndex: "report_url",
      key: "report_url",
      width: 200,
      ellipsis: true,
      render: (url: string) => url || "-",
    },
    {
      title: "创建时间",
      dataIndex: "created_at",
      key: "created_at",
      width: 180,
      render: (createdAt: string) => dayjs(createdAt).format("YYYY-MM-DD HH:mm:ss"),
      sorter: (a, b) => dayjs(a.created_at).unix() - dayjs(b.created_at).unix(),
    },
    {
      title: "操作",
      key: "action",
      width: 150,
      fixed: "right",
      render: (_, record) => (
        <Space size="small">
          <Button
            type="link"
            icon={<FaEye />}
            onClick={() => handleOpenView(record)}
          >
            查看
          </Button>
          <Popconfirm
            title="确定要删除此报警记录吗？"
            onConfirm={() => handleDelete(record.id)}
          >
            <Button type="link" danger icon={<FaTrash />}>
              删除
            </Button>
          </Popconfirm>
        </Space>
      ),
    },
  ];

  // 统计信息
  const stats = {
    total: total,
    today: alerts.filter((alert) =>
      dayjs(alert.created_at).isSame(dayjs(), "day")
    ).length,
    highConfidence: alerts.filter((alert) => alert.confidence > 0.8).length,
  };

  return (
    <div>
      <Card>
        {/* 统计信息 */}
        <Row gutter={16} style={{ marginBottom: 16 }}>
          <Col span={6}>
            <Statistic title="总报警数" value={stats.total} />
          </Col>
          <Col span={6}>
            <Statistic title="今日报警" value={stats.today} />
          </Col>
          <Col span={6}>
            <Statistic
              title="高置信度报警"
              value={stats.highConfidence}
              valueStyle={{ color: "#3f8600" }}
            />
          </Col>
        </Row>

        {/* 筛选区域 */}
        <Card size="small" style={{ marginBottom: 16 }}>
          <Row gutter={16} align="middle">
            <Col>
              <span>通道筛选：</span>
              <Select
                style={{ width: 200, marginLeft: 8 }}
                placeholder="选择通道"
                allowClear
                value={filters.channelId}
                onChange={(value) => handleFilterChange("channelId", value)}
              >
                {channels.map((channel) => (
                  <Select.Option key={channel.id} value={channel.id}>
                    {channel.name} (ID: {channel.id})
                  </Select.Option>
                ))}
              </Select>
            </Col>
            <Col>
              <span>报警类型：</span>
              <Select
                style={{ width: 150, marginLeft: 8 }}
                placeholder="选择类型"
                allowClear
                value={filters.alertType}
                onChange={(value) => handleFilterChange("alertType", value)}
              >
                <Select.Option value="person">人员</Select.Option>
                <Select.Option value="vehicle">车辆</Select.Option>
                <Select.Option value="fire">火灾</Select.Option>
                <Select.Option value="smoke">烟雾</Select.Option>
                <Select.Option value="intrusion">入侵</Select.Option>
              </Select>
            </Col>
            <Col>
              <span>日期范围：</span>
              <RangePicker
                style={{ marginLeft: 8 }}
                value={filters.dateRange}
                onChange={(dates) => handleFilterChange("dateRange", dates || undefined)}
                format="YYYY-MM-DD"
              />
            </Col>
            <Col>
              <Button
                icon={<FaRedo />}
                onClick={handleResetFilters}
              >
                重置
              </Button>
            </Col>
            <Col flex="auto" style={{ textAlign: "right" }}>
              <Button
                icon={<FaRedo />}
                onClick={loadAlerts}
                loading={loading}
              >
                刷新
              </Button>
            </Col>
          </Row>
        </Card>

        {/* 报警列表表格 */}
        <Table
          columns={columns}
          dataSource={alerts}
          rowKey="id"
          loading={loading}
          scroll={{ x: 1400 }}
          pagination={{
            current: pagination.current,
            pageSize: pagination.pageSize,
            total: total,
            showSizeChanger: true,
            showTotal: (total) => `共 ${total} 条记录`,
            onChange: handleTableChange,
            onShowSizeChange: handleTableChange,
          }}
        />
      </Card>

      {/* 查看报警详情弹窗 */}
      <Modal
        title={`报警详情 - ID: ${viewingAlert?.id || ""}`}
        open={viewModalVisible}
        onCancel={handleCloseView}
        footer={null}
        width={1000}
        destroyOnClose
      >
        {viewingAlert && (
          <div>
            <Row gutter={16}>
              <Col span={12}>
                <div style={{ marginBottom: 16 }}>
                  <strong>通道信息：</strong>
                  <div>
                    {viewingAlert.channel_name} (ID: {viewingAlert.channel_id})
                  </div>
                </div>
                <div style={{ marginBottom: 16 }}>
                  <strong>报警类型：</strong>
                  <Tag color={getAlertTypeColor(viewingAlert.alert_type)}>
                    {viewingAlert.alert_type}
                  </Tag>
                </div>
                <div style={{ marginBottom: 16 }}>
                  <strong>检测对象：</strong>
                  <div>{viewingAlert.detected_objects}</div>
                </div>
                <div style={{ marginBottom: 16 }}>
                  <strong>置信度：</strong>
                  <Tag
                    color={
                      viewingAlert.confidence > 0.8
                        ? "success"
                        : viewingAlert.confidence > 0.5
                        ? "warning"
                        : "default"
                    }
                  >
                    {formatConfidence(viewingAlert.confidence)}
                  </Tag>
                </div>
                <div style={{ marginBottom: 16 }}>
                  <strong>检测框位置：</strong>
                  <div>
                    X: {viewingAlert.bbox_x.toFixed(2)}, Y: {viewingAlert.bbox_y.toFixed(2)}
                  </div>
                  <div>
                    宽: {viewingAlert.bbox_w.toFixed(2)}, 高: {viewingAlert.bbox_h.toFixed(2)}
                  </div>
                </div>
                <div style={{ marginBottom: 16 }}>
                  <strong>创建时间：</strong>
                  <div>{dayjs(viewingAlert.created_at).format("YYYY-MM-DD HH:mm:ss")}</div>
                </div>
                <div style={{ marginBottom: 16 }}>
                  <strong>上报状态：</strong>
                  <Tag color={getReportStatusColor(viewingAlert.report_status)}>
                    {viewingAlert.report_status === "pending" ? "待上报" : viewingAlert.report_status === "success" ? "已上报" : "上报失败"}
                  </Tag>
                </div>
                <div style={{ marginBottom: 16 }}>
                  <strong>上报地址：</strong>
                  <div>{viewingAlert.report_url || "-"}</div>
                </div>
              </Col>
              <Col span={12}>
                <div style={{ marginBottom: 8 }}>
                  <strong>报警图片：</strong>
                </div>
                {getImageUrl(viewingAlert) ? (
                  <Image
                    src={getImageUrl(viewingAlert)}
                    alt="报警图片"
                    style={{
                      maxWidth: "100%",
                      maxHeight: "500px",
                      objectFit: "contain",
                    }}
                    preview={{
                      mask: "查看大图",
                    }}
                  />
                ) : (
                  <div style={{ padding: "40px", textAlign: "center", color: "#999" }}>
                    暂无图片
                  </div>
                )}
              </Col>
            </Row>
          </div>
        )}
      </Modal>
    </div>
  );
}

export default AlertList;

