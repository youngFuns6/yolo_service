import { useState, useEffect, useRef } from "react";
import {
  Table,
  Button,
  Space,
  message,
  Tag,
  Popconfirm,
  Card,
  Modal,
  Spin,
  Switch,
} from "antd";
import type { ColumnsType } from "antd/es/table";
import { ProForm, ProFormText, ProFormDigit, ProFormSwitch, ModalForm } from "@ant-design/pro-components";
import {
  FaPlus,
  FaEdit,
  FaTrash,
  FaEye,
} from "react-icons/fa";
import wsManager, { type WebSocketMessage } from "@/utils/websocket";
import {
  getChannelList,
  createChannel,
  updateChannel,
  deleteChannel,
  setChannelPush,
  type Channel,
  type CreateChannelParams,
  type UpdateChannelParams,
} from "@/api/channel";

function ChannelList() {
  const [channels, setChannels] = useState<Channel[]>([]);
  const [loading, setLoading] = useState(false);
  const [modalVisible, setModalVisible] = useState(false);
  const [editingChannel, setEditingChannel] = useState<Channel | null>(null);
  const [viewModalVisible, setViewModalVisible] = useState(false);
  const [viewingChannel, setViewingChannel] = useState<Channel | null>(null);
  const [streamLoading, setStreamLoading] = useState(false);
  const imageRef = useRef<HTMLImageElement>(null);

  // 加载通道列表
  function loadChannels() {
    setLoading(true);
    getChannelList()
      .then((response) => {
        if (response.success && response.channels) {
          setChannels(response.channels);
        } else {
          message.error("获取通道列表失败");
        }
      })
      .catch((error) => {
        console.error("获取通道列表失败:", error);
        message.error("获取通道列表失败");
      })
      .finally(() => {
        setLoading(false);
      });
  }

  useEffect(() => {
    loadChannels();
    // 每5秒刷新一次状态
    const interval = setInterval(loadChannels, 5000);
    return () => clearInterval(interval);
  }, []);

  // 打开创建/编辑对话框
  function handleOpenModal(channel?: Channel) {
    setEditingChannel(channel || null);
    setModalVisible(true);
  }

  // 关闭对话框
  function handleCloseModal() {
    setModalVisible(false);
    setEditingChannel(null);
  }

  // 提交表单
  function handleSubmit(values: any) {
    const params: CreateChannelParams | UpdateChannelParams = {
      name: values.name,
      source_url: values.source_url,
      enabled: values.enabled ?? true,
      push_enabled: values.push_enabled ?? false,
    };

    // 如果指定了id，添加到参数中
    if (values.id !== undefined && values.id !== null && values.id !== '') {
      params.id = Number(values.id);
    }

    if (editingChannel) {
      // 更新通道
      return updateChannel(editingChannel.id, params)
        .then((response) => {
          if (response.success) {
            message.success("更新通道成功");
            handleCloseModal();
            loadChannels();
            return true;
          } else {
            message.error(response.error || "更新通道失败");
            return false;
          }
        })
        .catch((error) => {
          console.error("更新通道失败:", error);
          message.error("更新通道失败");
          return false;
        });
    } else {
      // 创建通道
      return createChannel(params)
        .then((response) => {
          if (response.success) {
            message.success("创建通道成功");
            handleCloseModal();
            loadChannels();
            return true;
          } else {
            message.error(response.error || "创建通道失败");
            return false;
          }
        })
        .catch((error) => {
          console.error("创建通道失败:", error);
          message.error("创建通道失败");
          return false;
        });
    }
  }

  // 删除通道
  function handleDelete(channelId: number) {
    deleteChannel(channelId)
      .then((response) => {
        if (response.success) {
          message.success("删除通道成功");
          loadChannels();
        } else {
          message.error(response.error || "删除通道失败");
        }
      })
      .catch((error) => {
        console.error("删除通道失败:", error);
        message.error("删除通道失败");
      });
  }

  // 切换推流开关
  function handlePushToggle(channelId: number, checked: boolean) {
    setChannelPush(channelId, checked)
      .then((response) => {
        if (response.success) {
          message.success(checked ? "开启推流成功" : "关闭推流成功");
          loadChannels();
        } else {
          message.error(response.error || (checked ? "开启推流失败" : "关闭推流失败"));
          // 如果失败，重新加载以恢复原状态
          loadChannels();
        }
      })
      .catch((error) => {
        console.error("切换推流开关失败:", error);
        message.error(checked ? "开启推流失败" : "关闭推流失败");
        // 如果失败，重新加载以恢复原状态
        loadChannels();
      });
  }

  // 打开查看弹窗
  function handleOpenView(channel: Channel) {
    if (channel.status !== "running") {
      message.warning("通道未运行，无法查看");
      return;
    }
    setViewingChannel(channel);
    setViewModalVisible(true);
    setStreamLoading(true);
  }

  // 关闭查看弹窗
  function handleCloseView() {
    setViewModalVisible(false);
    setViewingChannel(null);
    setStreamLoading(false);
  }

  // WebSocket消息处理
  useEffect(() => {
    if (!viewModalVisible || !viewingChannel) {
      return;
    }

    // 连接WebSocket并订阅通道
    wsManager.connect(viewingChannel.id);

    // 消息处理函数
    const handleMessage = (message: WebSocketMessage) => {
      // 处理订阅确认消息
      if (message.type === "subscription_confirmed") {
        console.log(`已成功订阅通道 ${message.channel_id}`);
        return;
      }

      // 处理帧数据
      if (message.type === "frame" && message.channel_id === viewingChannel.id) {
        if (message.image_base64 && imageRef.current) {
          imageRef.current.src = `data:image/jpeg;base64,${message.image_base64}`;
          setStreamLoading(false);
        }
      }
    };

    // 添加消息处理器
    wsManager.addMessageHandler(handleMessage);

    // 如果连接已建立但还未订阅，则订阅通道
    const onOpenCallback = () => {
      if (wsManager.getSubscribedChannelId() !== viewingChannel.id) {
        wsManager.subscribeChannel(viewingChannel.id);
      }
    };

    wsManager.onOpen(onOpenCallback);

    // 清理函数
    return () => {
      wsManager.removeMessageHandler(handleMessage);
      wsManager.removeOnOpenCallback(onOpenCallback);
      // 关闭连接（如果需要的话，可以保留连接以便快速切换通道）
      // wsManager.disconnect();
    };
  }, [viewModalVisible, viewingChannel]);

  // 状态标签颜色
  function getStatusColor(status: string) {
    switch (status) {
      case "running":
        return "success";
      case "error":
        return "error";
      case "stopped":
        return "default";
      case "idle":
        return "processing";
      default:
        return "default";
    }
  }

  // 状态标签文本
  function getStatusText(status: string) {
    const statusMap: Record<string, string> = {
      idle: "空闲",
      running: "运行中",
      error: "错误",
      stopped: "已停止",
    };
    return statusMap[status] || status;
  }

  const columns: ColumnsType<Channel> = [
    {
      title: "ID",
      dataIndex: "id",
      key: "id",
      width: 80,
    },
    {
      title: "通道名称",
      dataIndex: "name",
      key: "name",
      width: 150,
    },
    {
      title: "源地址",
      dataIndex: "source_url",
      key: "source_url",
      ellipsis: true,
    },
    {
      title: "状态",
      dataIndex: "status",
      key: "status",
      width: 100,
      render: (status: string) => (
        <Tag color={getStatusColor(status)}>{getStatusText(status)}</Tag>
      ),
    },
    {
      title: "启用",
      dataIndex: "enabled",
      key: "enabled",
      width: 80,
      render: (enabled: boolean) => (
        <Tag color={enabled ? "success" : "default"}>
          {enabled ? "是" : "否"}
        </Tag>
      ),
    },
    {
      title: "推送开关",
      dataIndex: "push_enabled",
      key: "push_enabled",
      width: 100,
      render: (pushEnabled: boolean, record: Channel) => (
        <Switch
          checked={pushEnabled}
          onChange={(checked) => handlePushToggle(record.id, checked)}
          disabled={!record.enabled}
          checkedChildren="开"
          unCheckedChildren="关"
        />
      ),
    },
    {
      title: "创建时间",
      dataIndex: "created_at",
      key: "created_at",
      width: 180,
    },
    {
      title: "操作",
      key: "action",
      width: 240,
      fixed: "right",
      render: (_, record) => (
        <Space size="small">
          <Button
            type="link"
            icon={<FaEye />}
            onClick={() => handleOpenView(record)}
            disabled={record.status !== "running"}
          >
            查看
          </Button>
          <Button
            type="link"
            icon={<FaEdit />}
            onClick={() => handleOpenModal(record)}
          >
            编辑
          </Button>
          <Popconfirm
            title="确定要删除此通道吗？"
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

  return (
    <div>
      <Card>
        <div style={{ marginBottom: 16 }}>
          <Button
            type="primary"
            icon={<FaPlus />}
            onClick={() => handleOpenModal()}
          >
            创建通道
          </Button>
        </div>
        <Table
          columns={columns}
          dataSource={channels}
          rowKey="id"
          loading={loading}
          scroll={{ x: 1400 }}
          pagination={{
            pageSize: 10,
            showSizeChanger: true,
            showTotal: (total) => `共 ${total} 条记录`,
          }}
        />
      </Card>

      <ModalForm
        title={editingChannel ? "编辑通道" : "创建通道"}
        open={modalVisible}
        onFinish={handleSubmit}
        onOpenChange={setModalVisible}
        modalProps={{
          onCancel: handleCloseModal,
        }}
        width={600}
        initialValues={
          editingChannel
            ? {
                id: editingChannel.id,
                name: editingChannel.name,
                source_url: editingChannel.source_url,
                enabled: editingChannel.enabled,
                push_enabled: editingChannel.push_enabled,
              }
            : {
                enabled: true,
                push_enabled: false,
              }
        }
      >
        <ProFormDigit
          name="id"
          label="通道号"
          placeholder="请输入通道号（可选，不填则自动生成）"
          min={1}
          tooltip="通道的唯一标识符，如果不填写则系统自动生成"
          fieldProps={{
            style: { width: "100%" },
          }}
        />
        <ProFormText
          name="name"
          label="通道名称"
          placeholder="请输入通道名称"
          rules={[{ required: true, message: "请输入通道名称" }]}
        />
        <ProFormText
          name="source_url"
          label="源地址"
          placeholder="请输入源地址（如：rtsp://...）"
          rules={[{ required: true, message: "请输入源地址" }]}
        />
        <ProFormSwitch
          name="enabled"
          label="启用"
        />
        <ProFormSwitch
          name="push_enabled"
          label="推送开关"
          tooltip="是否启用推流功能"
        />
      </ModalForm>

      <Modal
        title={`查看通道 - ${viewingChannel?.name || ""}`}
        open={viewModalVisible}
        onCancel={handleCloseView}
        footer={null}
        width={1200}
        destroyOnClose
      >
        <div style={{ textAlign: "center", padding: "20px" }}>
          <Spin spinning={streamLoading} tip="正在加载视频流...">
            <img
              ref={imageRef}
              alt="视频流"
              style={{
                maxWidth: "100%",
                maxHeight: "70vh",
                objectFit: "contain",
                backgroundColor: "#000",
              }}
              onError={() => {
                setStreamLoading(false);
                message.error("视频流加载失败");
              }}
            />
          </Spin>
          {viewingChannel && (
            <div style={{ marginTop: "16px", color: "#666" }}>
              <Tag>通道ID: {viewingChannel.id}</Tag>
              <Tag>源地址: {viewingChannel.source_url}</Tag>
            </div>
          )}
        </div>
      </Modal>
    </div>
  );
}

export default ChannelList;

