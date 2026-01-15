import { useState, useEffect } from "react";
import {
  Table,
  Button,
  Space,
  message,
  Tag,
  Popconfirm,
  Card,
  Upload,
  Spin,
} from "antd";
import type { ColumnsType } from "antd/es/table";
import type { UploadProps, UploadFile } from "antd/es/upload";
import dayjs from "dayjs";
import {
  FaTrash,
  FaUpload,
  FaFile,
} from "react-icons/fa";
import {
  getModelList,
  uploadModel,
  deleteModel,
  type Model,
  type ApiResponse,
} from "@/api/model";

function ModelList() {
  const [models, setModels] = useState<Model[]>([]);
  const [loading, setLoading] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [fileList, setFileList] = useState<UploadFile[]>([]);

  // 加载模型列表
  function loadModels() {
    setLoading(true);
    getModelList()
      .then((response: ApiResponse<Model[]>) => {
        if (response.success && response.data) {
          setModels(response.data);
        } else {
          message.error(response.error || "获取模型列表失败");
        }
      })
      .catch((error) => {
        console.error("获取模型列表失败:", error);
        message.error("获取模型列表失败");
      })
      .finally(() => {
        setLoading(false);
      });
  }

  useEffect(() => {
    loadModels();
  }, []);

  // 处理文件上传
  const uploadProps: UploadProps = {
    name: "file",
    accept: ".onnx",
    maxCount: 1,
    fileList,
    beforeUpload: (file) => {
      // 验证文件类型
      const isOnnx = file.name.toLowerCase().endsWith(".onnx");
      if (!isOnnx) {
        message.error("只能上传 ONNX 格式的模型文件！");
        return Upload.LIST_IGNORE;
      }

      // 验证文件大小（例如：最大 500MB）
      const isLt500M = file.size / 1024 / 1024 < 500;
      if (!isLt500M) {
        message.error("模型文件大小不能超过 500MB！");
        return Upload.LIST_IGNORE;
      }

      return true;
    },
    onChange: (info) => {
      setFileList(info.fileList);
    },
    customRequest: async ({ file, onSuccess, onError }) => {
      if (!(file instanceof File)) {
        onError?.(new Error("无效的文件"));
        return;
      }

      setUploading(true);
      try {
        const response = await uploadModel(file);
        if (response.success) {
          message.success("模型上传成功");
          setFileList([]);
          loadModels();
          onSuccess?.(response);
        } else {
          const error = new Error(response.error || "上传失败");
          message.error(response.error || "模型上传失败");
          onError?.(error);
        }
      } catch (error) {
        console.error("上传模型失败:", error);
        message.error("模型上传失败");
        onError?.(error as Error);
      } finally {
        setUploading(false);
      }
    },
  };

  // 删除模型
  function handleDelete(modelName: string) {
    deleteModel(modelName)
      .then((response: ApiResponse) => {
        if (response.success) {
          message.success("删除模型成功");
          loadModels();
        } else {
          message.error(response.error || "删除模型失败");
        }
      })
      .catch((error) => {
        console.error("删除模型失败:", error);
        message.error("删除模型失败");
      });
  }

  // 格式化文件大小
  function formatFileSize(size: string): string {
    const numSize = parseFloat(size);
    if (isNaN(numSize)) return size;
    
    if (numSize < 1024) {
      return `${numSize.toFixed(2)} B`;
    } else if (numSize < 1024 * 1024) {
      return `${(numSize / 1024).toFixed(2)} KB`;
    } else if (numSize < 1024 * 1024 * 1024) {
      return `${(numSize / (1024 * 1024)).toFixed(2)} MB`;
    } else {
      return `${(numSize / (1024 * 1024 * 1024)).toFixed(2)} GB`;
    }
  }

  const columns: ColumnsType<Model> = [
    {
      title: "模型名称",
      dataIndex: "name",
      key: "name",
      width: 200,
      render: (name: string) => (
        <Space>
          <FaFile style={{ color: "#1890ff" }} />
          <span>{name}</span>
        </Space>
      ),
    },
    {
      title: "文件路径",
      dataIndex: "path",
      key: "path",
      ellipsis: true,
      render: (path: string) => (
        <Tag color="blue" style={{ maxWidth: "100%" }}>
          {path}
        </Tag>
      ),
    },
    {
      title: "文件大小",
      dataIndex: "size",
      key: "size",
      width: 120,
      render: (size: string) => formatFileSize(size),
    },
    {
      title: "修改时间",
      dataIndex: "modified",
      key: "modified",
      width: 180,
      render: (modified: string) => dayjs.unix(+modified).format("YYYY-MM-DD HH:mm:ss"),
    },
    {
      title: "操作",
      key: "action",
      width: 120,
      fixed: "right",
      render: (_, record) => (
        <Popconfirm
          title="确定要删除此模型吗？"
          description="删除后无法恢复，请谨慎操作！"
          onConfirm={() => handleDelete(record.name)}
          okText="确定"
          cancelText="取消"
        >
          <Button type="link" danger icon={<FaTrash />}>
            删除
          </Button>
        </Popconfirm>
      ),
    },
  ];

  return (
    <div>
      <Card>
        <div style={{ marginBottom: 16 }}>
          <Space>
            <Upload {...uploadProps}>
              <Button
                type="primary"
                icon={<FaUpload />}
                loading={uploading}
                disabled={uploading}
              >
                上传模型
              </Button>
            </Upload>
            <span style={{ color: "#999", fontSize: "12px" }}>
              支持 ONNX 格式，最大 500MB
            </span>
          </Space>
        </div>

        <Spin spinning={loading}>
          <Table
            columns={columns}
            dataSource={models}
            rowKey="name"
            loading={loading}
            scroll={{ x: 1000 }}
            pagination={{
              pageSize: 10,
              showSizeChanger: true,
              showTotal: (total) => `共 ${total} 个模型`,
            }}
            locale={{
              emptyText: "暂无模型，请上传模型文件",
            }}
          />
        </Spin>
      </Card>
    </div>
  );
}

export default ModelList;

