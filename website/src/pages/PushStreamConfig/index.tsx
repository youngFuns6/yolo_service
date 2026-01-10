import { useState, useEffect } from "react";
import { Card, message } from "antd";
import { ProForm, ProFormText, ProFormDigit } from "@ant-design/pro-components";
import {
  getPushStreamConfig,
  updatePushStreamConfig,
  type PushStreamConfig,
} from "@/api/config";

function PushStreamConfigPage() {
  const [loading, setLoading] = useState(false);

  const [initialValues, setInitialValues] = useState<Partial<PushStreamConfig>>({
    rtmp_url: "",
  });

  // 加载配置
  function loadConfig() {
    setLoading(true);
    getPushStreamConfig()
      .then((response) => {
        if (response.success) {
          const values: Partial<PushStreamConfig> = {
            rtmp_url: response.rtmp_url || "",
          };
          if (response.width != null) {
            values.width = response.width;
          }
          if (response.height != null) {
            values.height = response.height;
          }
          if (response.fps != null) {
            values.fps = response.fps;
          }
          if (response.bitrate != null) {
            values.bitrate = Math.round(response.bitrate / 1000); // 转换为kbps
          }
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
  function handleSubmit(values: Partial<PushStreamConfig>) {
    // 将bitrate从kbps转换为bps（如果提供了值）
    const submitValues: Partial<PushStreamConfig> = {
      rtmp_url: values.rtmp_url || "",
    };
    if (values.width != null) {
      submitValues.width = values.width;
    }
    if (values.height != null) {
      submitValues.height = values.height;
    }
    if (values.fps != null) {
      submitValues.fps = values.fps;
    }
    if (values.bitrate != null) {
      submitValues.bitrate = values.bitrate * 1000; // 转换为bps
    }
    return updatePushStreamConfig(submitValues)
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
          placeholder="请输入 RTMP 推流地址"
          tooltip="RTMP 推流服务器地址，例如：rtmp://example.com/live/stream。留空则只进行拉流分析，不推流。"
          required
        />
        <ProFormDigit
          name="width"
          label="推流宽度"
          placeholder="请输入推流宽度（可选）"
          min={1}
          max={7680}
          tooltip="推流视频的宽度（像素），可选"
          fieldProps={{
            style: { width: "100%" },
          }}
        />
        <ProFormDigit
          name="height"
          label="推流高度"
          placeholder="请输入推流高度（可选）"
          min={1}
          max={4320}
          tooltip="推流视频的高度（像素），可选"
          fieldProps={{
            style: { width: "100%" },
          }}
        />
        <ProFormDigit
          name="fps"
          label="帧率"
          placeholder="请输入帧率（可选）"
          min={1}
          max={120}
          tooltip="推流视频的帧率（FPS），可选"
          fieldProps={{
            style: { width: "100%" },
          }}
        />
        <ProFormDigit
          name="bitrate"
          label="比特率"
          placeholder="请输入比特率（kbps，可选）"
          min={100}
          max={50000}
          tooltip="推流视频的比特率（kbps），可选"
          fieldProps={{
            style: { width: "100%" },
            addonAfter: "kbps",
          }}
        />
      </ProForm>
    </Card>
  );
}

export default PushStreamConfigPage;

