import { useState, useEffect } from "react";
import { Card, message } from "antd";
import { ProForm, ProFormText, ProFormDigit } from "@ant-design/pro-components";
import {
  getStreamConfig,
  updateStreamConfig,
  type StreamConfig,
} from "@/api/config";

function ChannelConfig() {
  const [loading, setLoading] = useState(false);

  const [initialValues, setInitialValues] = useState<Partial<StreamConfig>>({
    width: 1920,
    height: 1080,
    fps: 25,
    bitrate: 2000,
  });

  // 加载配置
  function loadConfig() {
    setLoading(true);
    getStreamConfig()
      .then((response) => {
        if (response.success) {
          const values = {
            rtmp_url: response.rtmp_url || "",
            width: response.width || 1920,
            height: response.height || 1080,
            fps: response.fps || 25,
            bitrate: response.bitrate ? Math.round(response.bitrate / 1000) : 2000, // 转换为kbps
          };
          setInitialValues(values);
        } else {
          message.error("获取配置失败");
        }
      })
      .catch((error) => {
        console.error("获取配置失败:", error);
        message.error("获取配置失败");
      })
      .finally(() => {
        setLoading(false);
      });
  }

  useEffect(() => {
    loadConfig();
  }, []);

  // 保存配置
  function handleSubmit(values: Partial<StreamConfig>) {
    // 将bitrate从kbps转换为bps
    const submitValues = {
      ...values,
      bitrate: values.bitrate ? values.bitrate * 1000 : 2000000,
    };
    return updateStreamConfig(submitValues)
      .then((response) => {
        if (response.success) {
          message.success("保存配置成功");
          return true;
        } else {
          message.error(response.error || "保存配置失败");
          return false;
        }
      })
      .catch((error) => {
        console.error("保存配置失败:", error);
        message.error("保存配置失败");
        return false;
      });
  }

  return (
    <Card title="全局推流配置" loading={loading}>
      <ProForm
        onFinish={handleSubmit}
        initialValues={initialValues}
        submitter={{
          searchConfig: {
            submitText: "保存配置",
          },
          resetButtonProps: {
            onClick: () => {
              loadConfig();
            },
          },
        }}
      >
        <ProFormText
          name="rtmp_url"
          label="RTMP 推流地址"
          placeholder="请输入 RTMP 推流地址（留空则不推流）"
          tooltip="RTMP 推流服务器地址，例如：rtmp://example.com/live/stream。留空则只进行拉流分析，不推流。"
        />
        <ProFormDigit
          name="width"
          label="推流宽度"
          placeholder="请输入推流宽度"
          min={1}
          max={7680}
          rules={[{ required: true, message: "请输入推流宽度" }]}
          tooltip="推流视频的宽度（像素）"
          fieldProps={{
            style: { width: "100%" },
          }}
        />
        <ProFormDigit
          name="height"
          label="推流高度"
          placeholder="请输入推流高度"
          min={1}
          max={4320}
          rules={[{ required: true, message: "请输入推流高度" }]}
          tooltip="推流视频的高度（像素）"
          fieldProps={{
            style: { width: "100%" },
          }}
        />
        <ProFormDigit
          name="fps"
          label="帧率"
          placeholder="请输入帧率"
          min={1}
          max={120}
          rules={[{ required: true, message: "请输入帧率" }]}
          tooltip="推流视频的帧率（FPS）"
          fieldProps={{
            style: { width: "100%" },
          }}
        />
        <ProFormDigit
          name="bitrate"
          label="比特率"
          placeholder="请输入比特率（kbps）"
          min={100}
          max={50000}
          rules={[{ required: true, message: "请输入比特率" }]}
          tooltip="推流视频的比特率（kbps）"
          fieldProps={{
            style: { width: "100%" },
            addonAfter: "kbps",
          }}
        />
      </ProForm>
    </Card>
  );
}

export default ChannelConfig;

