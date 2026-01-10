import { useState, useEffect } from "react";
import { Card, message, Radio, Space } from "antd";
import { ProForm, ProFormText, ProFormDigit, ProFormSwitch } from "@ant-design/pro-components";
import {
  getReportConfig,
  updateReportConfig,
  type ReportConfig,
} from "@/api/config";

function ReportConfigPage() {
  const [loading, setLoading] = useState(false);
  const [reportType, setReportType] = useState<"HTTP" | "MQTT">("HTTP");

  const [initialValues, setInitialValues] = useState<Partial<ReportConfig>>({
    type: "HTTP",
    http_url: "",
    mqtt_broker: "",
    mqtt_port: 1883,
    mqtt_topic: "",
    mqtt_username: "",
    mqtt_password: "",
    mqtt_client_id: "detector_service",
    enabled: false,
  });

  // 加载配置
  function loadConfig() {
    setLoading(true);
    getReportConfig()
      .then((response) => {
        if (response.success && response.config) {
          const config = response.config;
          setReportType(config.type || "HTTP");
          setInitialValues({
            type: config.type || "HTTP",
            http_url: config.http_url || "",
            mqtt_broker: config.mqtt_broker || "",
            mqtt_port: config.mqtt_port || 1883,
            mqtt_topic: config.mqtt_topic || "",
            mqtt_username: config.mqtt_username || "",
            mqtt_password: config.mqtt_password || "",
            mqtt_client_id: config.mqtt_client_id || "detector_service",
            enabled: config.enabled ?? false,
          });
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
  function handleSubmit(values: Partial<ReportConfig>) {
    const submitValues: Partial<ReportConfig> = {
      type: reportType,
      enabled: values.enabled ?? false,
    };

    if (reportType === "HTTP") {
      submitValues.http_url = values.http_url || "";
    } else if (reportType === "MQTT") {
      submitValues.mqtt_broker = values.mqtt_broker || "";
      submitValues.mqtt_port = values.mqtt_port || 1883;
      submitValues.mqtt_topic = values.mqtt_topic || "";
      submitValues.mqtt_username = values.mqtt_username || "";
      submitValues.mqtt_password = values.mqtt_password || "";
      submitValues.mqtt_client_id = values.mqtt_client_id || "detector_service";
    }

    return updateReportConfig(submitValues)
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
    <Card title="报警上报配置" loading={loading}>
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
        <ProFormSwitch
          name="enabled"
          label="启用上报"
          tooltip="开启后，当通道的上报开关开启时，报警信息将自动上报"
        />

        <ProForm.Item
          name="type"
          label="上报方式"
          tooltip="选择 HTTP 或 MQTT 上报方式"
        >
          <Radio.Group
            value={reportType}
            onChange={(e) => {
              setReportType(e.target.value);
            }}
          >
            <Radio value="HTTP">HTTP</Radio>
            <Radio value="MQTT">MQTT</Radio>
          </Radio.Group>
        </ProForm.Item>

        {reportType === "HTTP" && (
          <>
            <ProFormText
              name="http_url"
              label="HTTP 上报地址"
              placeholder="请输入 HTTP 上报地址"
              tooltip="报警信息将通过 POST 请求发送到此地址，Content-Type: application/json"
              rules={[
                {
                  required: true,
                  message: "请输入 HTTP 上报地址",
                },
                {
                  type: "url",
                  message: "请输入有效的 URL 地址",
                },
              ]}
            />
          </>
        )}

        {reportType === "MQTT" && (
          <>
            <ProFormText
              name="mqtt_broker"
              label="MQTT Broker 地址"
              placeholder="请输入 MQTT Broker 地址"
              tooltip="MQTT 服务器地址，例如：broker.example.com 或 192.168.1.100"
              rules={[
                {
                  required: true,
                  message: "请输入 MQTT Broker 地址",
                },
              ]}
            />
            <ProFormDigit
              name="mqtt_port"
              label="MQTT 端口"
              placeholder="请输入 MQTT 端口"
              tooltip="MQTT 服务器端口，默认 1883"
              min={1}
              max={65535}
              initialValue={1883}
              fieldProps={{
                style: { width: "100%" },
              }}
            />
            <ProFormText
              name="mqtt_topic"
              label="MQTT 主题"
              placeholder="请输入 MQTT 主题"
              tooltip="报警信息将发布到此 MQTT 主题"
              rules={[
                {
                  required: true,
                  message: "请输入 MQTT 主题",
                },
              ]}
            />
            <ProFormText
              name="mqtt_username"
              label="MQTT 用户名"
              placeholder="请输入 MQTT 用户名（可选）"
              tooltip="MQTT 认证用户名，如果服务器需要认证则必填"
            />
            <ProFormText.Password
              name="mqtt_password"
              label="MQTT 密码"
              placeholder="请输入 MQTT 密码（可选）"
              tooltip="MQTT 认证密码，如果服务器需要认证则必填"
            />
            <ProFormText
              name="mqtt_client_id"
              label="MQTT 客户端 ID"
              placeholder="请输入 MQTT 客户端 ID"
              tooltip="MQTT 客户端标识符，默认为 detector_service"
              initialValue="detector_service"
            />
          </>
        )}
      </ProForm>
    </Card>
  );
}

export default ReportConfigPage;

