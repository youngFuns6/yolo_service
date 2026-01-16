import { useState, useEffect } from "react";
import { Card, message, Radio } from "antd";
import { ProForm, ProFormText, ProFormDigit, ProFormSwitch } from "@ant-design/pro-components";
import {
  getGB28181Config,
  updateGB28181Config,
  type GB28181Config,
} from "@/api/config";

function GB28181ConfigPage() {
  const [loading, setLoading] = useState(false);
  const [streamMode, setStreamMode] = useState<"PS" | "H264">("PS");
  const [sipTransport, setSipTransport] = useState<"TCP" | "UDP">("UDP");

  const [initialValues, setInitialValues] = useState<Partial<GB28181Config>>({
    enabled: false,
    sip_server_ip: "",
    sip_server_port: 5060,
    sip_server_id: "",
    sip_server_domain: "",
    device_id: "",
    device_password: "",
    device_name: "",
    manufacturer: "",
    model: "",
    local_sip_port: 5061,
    rtp_port_start: 30000,
    rtp_port_end: 30100,
    heartbeat_interval: 60,
    heartbeat_count: 3,
    register_expires: 3600,
    stream_mode: "PS",
    max_channels: 32,
    sip_transport: "UDP",
  });

  // 加载配置
  function loadConfig() {
    setLoading(true);
    getGB28181Config()
      .then((response) => {
        if (response.success) {
          const values: Partial<GB28181Config> = {
            enabled: response.enabled ?? false,
            sip_server_ip: response.sip_server_ip || "",
            sip_server_port: response.sip_server_port || 5060,
            sip_server_id: response.sip_server_id || "",
            sip_server_domain: response.sip_server_domain || "",
            device_id: response.device_id || "",
            device_password: response.device_password || "",
            device_name: response.device_name || "",
            manufacturer: response.manufacturer || "",
            model: response.model || "",
            local_sip_port: response.local_sip_port || 5061,
            rtp_port_start: response.rtp_port_start || 30000,
            rtp_port_end: response.rtp_port_end || 30100,
            heartbeat_interval: response.heartbeat_interval || 60,
            heartbeat_count: response.heartbeat_count || 3,
            register_expires: response.register_expires || 3600,
            stream_mode: response.stream_mode || "PS",
            max_channels: response.max_channels || 32,
            sip_transport: response.sip_transport || "UDP",
          };
          setStreamMode((values.stream_mode as "PS" | "H264") || "PS");
          setSipTransport((values.sip_transport as "TCP" | "UDP") || "UDP");
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
  function handleSubmit(values: Partial<GB28181Config>) {
    const submitValues: Partial<GB28181Config> = {
      enabled: values.enabled ?? false,
      sip_server_ip: values.sip_server_ip || "",
      sip_server_port: values.sip_server_port || 5060,
      sip_server_id: values.sip_server_id || "",
      sip_server_domain: values.sip_server_domain || "",
      device_id: values.device_id || "",
      device_password: values.device_password || "",
      device_name: values.device_name || "",
      manufacturer: values.manufacturer || "",
      model: values.model || "",
      local_sip_port: values.local_sip_port || 5061,
      rtp_port_start: values.rtp_port_start || 30000,
      rtp_port_end: values.rtp_port_end || 30100,
      heartbeat_interval: values.heartbeat_interval || 60,
      heartbeat_count: values.heartbeat_count || 3,
      register_expires: values.register_expires || 3600,
      stream_mode: streamMode,
      max_channels: values.max_channels || 32,
      sip_transport: sipTransport,
    };

    return updateGB28181Config(submitValues)
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
    <Card title="GB28181 国标配置" loading={loading}>
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
          label="启用 GB28181"
          tooltip="开启后，系统将支持GB28181国标协议推流"
        />

        <Card type="inner" title="SIP 服务器配置" style={{ marginBottom: 16 }}>
          <ProFormText
            name="sip_server_ip"
            label="SIP 服务器 IP"
            placeholder="请输入 SIP 服务器 IP 地址"
            tooltip="GB28181平台SIP服务器IP地址"
            rules={[
              {
                required: true,
                message: "请输入 SIP 服务器 IP 地址",
              },
            ]}
          />
          <ProFormDigit
            name="sip_server_port"
            label="SIP 服务器端口"
            placeholder="请输入 SIP 服务器端口"
            tooltip="GB28181平台SIP服务器端口，默认5060"
            min={1}
            max={65535}
            initialValue={5060}
            fieldProps={{
              style: { width: "100%" },
            }}
          />
          <ProForm.Item
            name="sip_transport"
            label="SIP 传输协议"
            tooltip="选择TCP或UDP传输协议，默认UDP"
          >
            <Radio.Group
              value={sipTransport}
              onChange={(e) => {
                setSipTransport(e.target.value);
              }}
            >
              <Radio value="UDP">UDP</Radio>
              <Radio value="TCP">TCP</Radio>
            </Radio.Group>
          </ProForm.Item>
          <ProFormText
            name="sip_server_id"
            label="SIP 服务器 ID"
            placeholder="请输入 SIP 服务器 ID（20位国标编码）"
            tooltip="GB28181平台SIP服务器ID，20位国标编码，例如：34020000002000000001"
            rules={[
              {
                required: true,
                message: "请输入 SIP 服务器 ID",
              },
              {
                len: 20,
                message: "SIP 服务器 ID 必须为20位",
              },
            ]}
          />
          <ProFormText
            name="sip_server_domain"
            label="SIP 服务器域"
            placeholder="请输入 SIP 服务器域"
            tooltip="GB28181平台SIP服务器域，例如：3402000000"
          />
          <ProFormDigit
            name="local_sip_port"
            label="本地 SIP 端口"
            placeholder="请输入本地 SIP 端口"
            tooltip="本地SIP监听端口，默认5061"
            min={1}
            max={65535}
            initialValue={5061}
            fieldProps={{
              style: { width: "100%" },
            }}
          />
        </Card>

        <Card type="inner" title="设备信息配置" style={{ marginBottom: 16 }}>
          <ProFormText
            name="device_id"
            label="设备 ID"
            placeholder="请输入设备 ID（20位国标编码）"
            tooltip="设备国标编码，20位，例如：34020000001320000001"
            rules={[
              {
                required: true,
                message: "请输入设备 ID",
              },
              {
                len: 20,
                message: "设备 ID 必须为20位",
              },
            ]}
          />
          <ProFormText.Password
            name="device_password"
            label="设备密码"
            placeholder="请输入设备密码"
            tooltip="设备认证密码"
            rules={[
              {
                required: true,
                message: "请输入设备密码",
              },
            ]}
          />
          <ProFormText
            name="device_name"
            label="设备名称"
            placeholder="请输入设备名称"
            tooltip="设备显示名称"
          />
          <ProFormText
            name="manufacturer"
            label="设备厂商"
            placeholder="请输入设备厂商"
            tooltip="设备制造商名称"
          />
          <ProFormText
            name="model"
            label="设备型号"
            placeholder="请输入设备型号"
            tooltip="设备型号"
          />
        </Card>

        <Card type="inner" title="媒体传输配置" style={{ marginBottom: 16 }}>
          <ProForm.Item
            name="stream_mode"
            label="流模式"
            tooltip="选择PS流或H.264流格式"
          >
            <Radio.Group
              value={streamMode}
              onChange={(e) => {
                setStreamMode(e.target.value);
              }}
            >
              <Radio value="PS">PS 流</Radio>
              <Radio value="H264">H.264 流</Radio>
            </Radio.Group>
          </ProForm.Item>
          <ProFormDigit
            name="rtp_port_start"
            label="RTP 端口起始"
            placeholder="请输入 RTP 端口起始"
            tooltip="RTP媒体流端口范围起始值，默认30000"
            min={1024}
            max={65535}
            initialValue={30000}
            fieldProps={{
              style: { width: "100%" },
            }}
          />
          <ProFormDigit
            name="rtp_port_end"
            label="RTP 端口结束"
            placeholder="请输入 RTP 端口结束"
            tooltip="RTP媒体流端口范围结束值，默认30100"
            min={1024}
            max={65535}
            initialValue={30100}
            fieldProps={{
              style: { width: "100%" },
            }}
          />
          <ProFormDigit
            name="max_channels"
            label="最大通道数"
            placeholder="请输入最大通道数"
            tooltip="设备支持的最大通道数量，默认32"
            min={1}
            max={256}
            initialValue={32}
            fieldProps={{
              style: { width: "100%" },
            }}
          />
        </Card>

        <Card type="inner" title="心跳配置" style={{ marginBottom: 16 }}>
          <ProFormDigit
            name="heartbeat_interval"
            label="心跳间隔（秒）"
            placeholder="请输入心跳间隔"
            tooltip="设备向平台发送心跳的时间间隔，默认60秒"
            min={10}
            max={3600}
            initialValue={60}
            fieldProps={{
              style: { width: "100%" },
            }}
          />
          <ProFormDigit
            name="heartbeat_count"
            label="心跳超时次数"
            placeholder="请输入心跳超时次数"
            tooltip="连续多少次心跳超时后判定为离线，默认3次"
            min={1}
            max={10}
            initialValue={3}
            fieldProps={{
              style: { width: "100%" },
            }}
          />
          <ProFormDigit
            name="register_expires"
            label="注册有效期（秒）"
            placeholder="请输入注册有效期"
            tooltip="设备注册到平台的有效期，默认3600秒"
            min={60}
            max={86400}
            initialValue={3600}
            fieldProps={{
              style: { width: "100%" },
            }}
          />
        </Card>
      </ProForm>
    </Card>
  );
}

export default GB28181ConfigPage;

