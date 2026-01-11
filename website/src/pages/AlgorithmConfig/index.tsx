import { useState, useEffect } from "react";
import { useParams, useNavigate } from "react-router-dom";
import {
  Card,
  Form,
  Input,
  InputNumber,
  Button,
  message,
  Space,
  Divider,
  Switch,
  Select,
  Checkbox,
  Row,
  Col,
  Tag,
  Spin,
  Alert,
} from "antd";
import {
  ProForm,
  ProFormDigit,
  ProFormSelect,
} from "@ant-design/pro-components";
import {
  FaUndo,
  FaArrowLeft,
  FaDrawPolygon,
} from "react-icons/fa";
import {
  getAlgorithmConfig,
  updateAlgorithmConfig,
  deleteAlgorithmConfig,
  getDefaultAlgorithmConfig,
  type UpdateAlgorithmConfigParams,
  type ROI,
  type AlertRule,
} from "@/api/algorithm";
import { getChannel } from "@/api/channel";
import { getModelList, getClassList, type Model, type ClassInfo } from "@/api/model";
import ROIDrawer from "@/components/ROIDrawer";

function AlgorithmConfigPage() {
  const { channelId } = useParams<{ channelId: string }>();
  const navigate = useNavigate();
  const [form] = Form.useForm();
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [channelName, setChannelName] = useState("");
  const [channelWidth, setChannelWidth] = useState(1920);
  const [channelHeight, setChannelHeight] = useState(1080);
  const [isDefault, setIsDefault] = useState(false);
  const [models, setModels] = useState<Model[]>([]);
  const [classes, setClasses] = useState<ClassInfo[]>([]);
  const [rois, setRois] = useState<ROI[]>([]);
  const [alertRules, setAlertRules] = useState<AlertRule[]>([]);
  const [drawerVisible, setDrawerVisible] = useState(false);
  const [editingRoiIndex, setEditingRoiIndex] = useState<number | null>(null);

  // 加载配置
  function loadConfig() {
    if (!channelId) return;

    setLoading(true);
    const channelIdNum = parseInt(channelId, 10);

    // 同时加载通道信息和算法配置
    Promise.all([
      getChannel(channelIdNum),
      getAlgorithmConfig(channelIdNum),
    ])
      .then(([channelResponse, configResponse]) => {
        if (channelResponse.success && channelResponse.channel) {
          setChannelName(channelResponse.channel.name);
          setChannelWidth(channelResponse.channel.width || 1920);
          setChannelHeight(channelResponse.channel.height || 1080);
        }

        if (configResponse.success && configResponse.data) {
          setIsDefault(false);
          // ROI坐标在数据库中存储为归一化坐标（0-1之间）
          // 这里直接使用，因为前端显示时不需要转换（除非在画布上绘制）
          setRois(configResponse.data.rois || []);
          setAlertRules(configResponse.data.alert_rules || []);
          // 将 enabled_classes 数组转换为数字数组（用于Select）
          const enabledClasses = configResponse.data.enabled_classes || [];
          form.setFieldsValue({
            model_path: configResponse.data.model_path,
            conf_threshold: configResponse.data.conf_threshold,
            nms_threshold: configResponse.data.nms_threshold,
            input_width: configResponse.data.input_width,
            input_height: configResponse.data.input_height,
            detection_interval: configResponse.data.detection_interval,
            enabled_classes: enabledClasses,
          });
        } else {
          // 如果没有配置，加载默认配置
          loadDefaultConfig();
        }
      })
      .catch((error) => {
        console.error("加载配置失败:", error);
        message.error("加载配置失败");
        loadDefaultConfig();
      })
      .finally(() => {
        setLoading(false);
      });
  }

  // 加载默认配置
  function loadDefaultConfig() {
    getDefaultAlgorithmConfig()
      .then((response) => {
        if (response.success && response.data) {
          setIsDefault(true);
          form.setFieldsValue({
            model_path: response.data.model_path,
            conf_threshold: response.data.conf_threshold,
            nms_threshold: response.data.nms_threshold,
            input_width: response.data.input_width,
            input_height: response.data.input_height,
            detection_interval: response.data.detection_interval,
            enabled_classes: [],
          });
        }
      })
      .catch((error) => {
        console.error("加载默认配置失败:", error);
      });
  }

  // 加载模型列表和类别列表
  useEffect(() => {
    getModelList()
      .then((response) => {
        if (response.success && response.data) {
          setModels(response.data);
        }
      })
      .catch((error) => {
        console.error("加载模型列表失败:", error);
      });

    getClassList()
      .then((response) => {
        if (response.success && response.data) {
          setClasses(response.data);
        }
      })
      .catch((error) => {
        console.error("加载类别列表失败:", error);
      });
  }, []);

  useEffect(() => {
    loadConfig();
  }, [channelId]);

  // 保存配置
  function handleSave(values: any) {
    if (!channelId) return;

    setSaving(true);
    const channelIdNum = parseInt(channelId, 10);

    // enabled_classes 已经是数字数组
    const enabledClasses = (values.enabled_classes || []).filter((id: number) => id >= 0);

    // 将ROI坐标归一化（如果坐标大于1，说明是像素坐标，需要归一化）
    // 使用input_width和input_height作为参考尺寸
    const normalizedRois = rois.map((roi) => {
      const refWidth = values.input_width || channelWidth;
      const refHeight = values.input_height || channelHeight;
      
      return {
        ...roi,
        points: roi.points.map((point) => {
          // 如果坐标大于1，假设是像素坐标，进行归一化
          // 否则，假设已经是归一化坐标，直接使用
          let normX = point.x;
          let normY = point.y;
          if (point.x > 1.0 || point.y > 1.0) {
            normX = point.x / refWidth;
            normY = point.y / refHeight;
          }
          // 确保归一化坐标在0-1范围内
          normX = Math.max(0, Math.min(1, normX));
          normY = Math.max(0, Math.min(1, normY));
          return { x: normX, y: normY };
        }),
      };
    });

    const params: UpdateAlgorithmConfigParams = {
      model_path: values.model_path,
      conf_threshold: values.conf_threshold,
      nms_threshold: values.nms_threshold,
      input_width: values.input_width,
      input_height: values.input_height,
      detection_interval: values.detection_interval,
      enabled_classes: enabledClasses,
      rois: normalizedRois,
      alert_rules: alertRules,
    };

    updateAlgorithmConfig(channelIdNum, params)
      .then((response) => {
        if (response.success) {
          message.success("保存配置成功");
          setIsDefault(false);
          loadConfig();
        } else {
          message.error(response.error || "保存配置失败");
        }
      })
      .catch((error) => {
        console.error("保存配置失败:", error);
        message.error("保存配置失败");
      })
      .finally(() => {
        setSaving(false);
      });
  }

  // 恢复默认配置
  function handleReset() {
    if (!channelId) return;

    deleteAlgorithmConfig(parseInt(channelId, 10))
      .then((response) => {
        if (response.success) {
          message.success("已恢复默认配置");
          loadConfig();
        } else {
          message.error(response.error || "恢复默认配置失败");
        }
      })
      .catch((error) => {
        console.error("恢复默认配置失败:", error);
        message.error("恢复默认配置失败");
      });
  }

  return (
    <div style={{ padding: "24px" }}>
      <Card>
        <Space style={{ marginBottom: 16 }}>
          <Button
            icon={<FaArrowLeft />}
            onClick={() => navigate("/channel")}
          >
            返回
          </Button>
          <h2 style={{ margin: 0 }}>
            算法配置 - {channelName || `通道 ${channelId}`}
          </h2>
          {isDefault && (
            <Tag color="orange">当前使用默认配置</Tag>
          )}
        </Space>

        {isDefault && (
          <Alert
            message="当前使用默认配置"
            description="您可以修改以下参数并保存，为当前通道创建专属配置。"
            type="info"
            showIcon
            style={{ marginBottom: 24 }}
          />
        )}

        <Spin spinning={loading}>
          <ProForm
            form={form}
            onFinish={handleSave}
            submitter={{
              render: (_, dom) => (
                <div style={{ textAlign: "right", marginTop: 24 }}>
                  <Space>
                    <Button
                      onClick={handleReset}
                      disabled={isDefault || saving}
                    >
                      <FaUndo /> 恢复默认
                    </Button>
                    {dom}
                  </Space>
                </div>
              ),
            }}
            layout="vertical"
          >
            {/* @ts-expect-error - Ant Design type definition issue */}
            <Divider orientation="left">模型配置</Divider>
            <Row gutter={16}>
              <Col span={12}>
                <ProFormSelect
                  name="model_path"
                  label="模型选择"
                  placeholder="请选择模型"
                  options={models.map((model) => ({
                    label: model.name,
                    value: model.path,
                  }))}
                  rules={[{ required: true, message: "请选择模型" }]}
                  tooltip="从已上传的模型中选择"
                  fieldProps={{
                    showSearch: true,
                    filterOption: (input, option) =>
                      (option?.label ?? "").toLowerCase().includes(input.toLowerCase()),
                  }}
                />
              </Col>
            </Row>

            {/* @ts-expect-error - Ant Design type definition issue */}
            <Divider orientation="left">检测参数</Divider>
            <Row gutter={16}>
              <Col span={8}>
                <ProFormDigit
                  name="conf_threshold"
                  label="置信度阈值"
                  placeholder="0.65"
                  min={0}
                  max={1}
                  step={0.01}
                  rules={[
                    { required: true, message: "请输入置信度阈值" },
                    { type: "number", min: 0, max: 1, message: "阈值必须在0-1之间" },
                  ]}
                  tooltip="检测框的置信度阈值，范围0-1，值越大要求越严格"
                  fieldProps={{
                    precision: 2,
                    style: { width: "100%" },
                  }}
                />
              </Col>
              <Col span={8}>
                <ProFormDigit
                  name="nms_threshold"
                  label="NMS阈值"
                  placeholder="0.45"
                  min={0}
                  max={1}
                  step={0.01}
                  rules={[
                    { required: true, message: "请输入NMS阈值" },
                    { type: "number", min: 0, max: 1, message: "阈值必须在0-1之间" },
                  ]}
                  tooltip="非极大值抑制阈值，用于去除重复检测框，范围0-1"
                  fieldProps={{
                    precision: 2,
                    style: { width: "100%" },
                  }}
                />
              </Col>
              <Col span={8}>
                <ProFormDigit
                  name="detection_interval"
                  label="检测间隔"
                  placeholder="3"
                  min={1}
                  rules={[
                    { required: true, message: "请输入检测间隔" },
                    { type: "number", min: 1, message: "检测间隔必须大于等于1" },
                  ]}
                  tooltip="每N帧检测一次，值越大性能越好但实时性越差"
                  fieldProps={{
                    style: { width: "100%" },
                  }}
                />
              </Col>
            </Row>

            <Row gutter={16}>
              <Col span={12}>
                <ProFormDigit
                  name="input_width"
                  label="输入宽度"
                  placeholder="640"
                  min={1}
                  rules={[
                    { required: true, message: "请输入输入宽度" },
                    { type: "number", min: 1, message: "宽度必须大于0" },
                  ]}
                  tooltip="模型输入图像的宽度（像素）"
                  fieldProps={{
                    style: { width: "100%" },
                  }}
                />
              </Col>
              <Col span={12}>
                <ProFormDigit
                  name="input_height"
                  label="输入高度"
                  placeholder="640"
                  min={1}
                  rules={[
                    { required: true, message: "请输入输入高度" },
                    { type: "number", min: 1, message: "高度必须大于0" },
                  ]}
                  tooltip="模型输入图像的高度（像素）"
                  fieldProps={{
                    style: { width: "100%" },
                  }}
                />
              </Col>
            </Row>

            {/* @ts-expect-error - Ant Design type definition issue */}
            <Divider orientation="left">类别过滤</Divider>
            <ProForm.Item
              name="enabled_classes"
              label="启用的类别"
              tooltip="留空表示检测所有类别，否则只检测选中的类别"
            >
              <Checkbox.Group style={{ width: "100%" }}>
                <Row gutter={[16, 8]}>
                  {classes.map((cls) => (
                    <Col span={6} key={cls.id}>
                      <Checkbox value={cls.id}>{`${cls.id}: ${cls.name}`}</Checkbox>
                    </Col>
                  ))}
                </Row>
              </Checkbox.Group>
            </ProForm.Item>

            {/* @ts-expect-error - Ant Design type definition issue */}
            <Divider orientation="left">ROI区域设置</Divider>
            <Alert
              message="ROI区域配置"
              description="ROI（感兴趣区域）用于限制检测范围。可以在告警规则中关联ROI区域，实现只在特定区域内检测和告警。"
              type="info"
              showIcon
              style={{ marginBottom: 16 }}
            />
            <ProForm.Item label="检测区域">
              <div style={{ marginBottom: 16 }}>
                <Button
                  type="dashed"
                  icon={<FaDrawPolygon />}
                  onClick={() => {
                    setEditingRoiIndex(null);
                    setDrawerVisible(true);
                  }}
                  style={{ width: "100%" }}
                >
                  绘制新区域
                </Button>
              </div>
              {rois.map((roi, index) => (
                <Card
                  key={roi.id}
                  size="small"
                  style={{ marginBottom: 8 }}
                  title={
                    <Space>
                      <Input
                        value={roi.name}
                        onChange={(e) => {
                          const newRois = [...rois];
                          newRois[index].name = e.target.value;
                          setRois(newRois);
                        }}
                        style={{ width: 150 }}
                      />
                      <Switch
                        checked={roi.enabled}
                        onChange={(checked) => {
                          const newRois = [...rois];
                          newRois[index].enabled = checked;
                          setRois(newRois);
                        }}
                      />
                      <Button
                        type="link"
                        size="small"
                        icon={<FaDrawPolygon />}
                        onClick={() => {
                          setEditingRoiIndex(index);
                          setDrawerVisible(true);
                        }}
                      >
                        重新绘制
                      </Button>
                      <Button
                        type="link"
                        danger
                        size="small"
                        onClick={() => {
                          setRois(rois.filter((_, idx) => idx !== index));
                        }}
                      >
                        删除
                      </Button>
                    </Space>
                  }
                >
                  <Row gutter={16}>
                    <Col span={24}>
                      <Space direction="vertical" style={{ width: "100%" }}>
                        <div>
                          <Tag color="blue">类型: {roi.type === "RECTANGLE" ? "矩形" : "多边形"}</Tag>
                          <Tag color={roi.enabled ? "green" : "default"}>
                            {roi.enabled ? "已启用" : "已禁用"}
                          </Tag>
                        </div>
                        <div style={{ fontSize: 12, color: "#666" }}>
                          提示: 点击"重新绘制"按钮可以在画布上绘制区域。
                        </div>
                        {roi.points && roi.points.length > 0 && (
                          <div style={{ fontSize: 12, color: "#999" }}>
                            坐标点（归一化）: {roi.points.map((p) => {
                              // 显示归一化坐标（0-1之间）
                              const displayX = p.x > 1 ? (p.x / (form.getFieldValue("input_width") || channelWidth)).toFixed(3) : p.x.toFixed(3);
                              const displayY = p.y > 1 ? (p.y / (form.getFieldValue("input_height") || channelHeight)).toFixed(3) : p.y.toFixed(3);
                              return `(${displayX}, ${displayY})`;
                            }).join(", ")}
                            <div style={{ fontSize: 11, color: "#999", marginTop: 4 }}>
                              提示: ROI坐标已归一化（0-1之间），可适配不同分辨率的视频流
                            </div>
                          </div>
                        )}
                      </Space>
                    </Col>
                  </Row>
                </Card>
              ))}
              {rois.length === 0 && (
                <Alert
                  message="暂无ROI区域"
                  description="ROI（感兴趣区域）用于限制检测范围。如果没有配置ROI区域，系统将检测整个画面。点击上方「添加矩形区域」按钮创建ROI区域。"
                  type="info"
                  showIcon
                  style={{ marginTop: 16 }}
                />
              )}
            </ProForm.Item>

            {/* @ts-expect-error - Ant Design type definition issue */}
            <Divider orientation="left">告警规则设置</Divider>
            <Alert
              message="告警规则配置"
              description="告警规则用于定义何时触发告警。可以配置多个规则，每个规则独立评估。满足规则条件时，系统会创建告警记录并发送通知。"
              type="info"
              showIcon
              style={{ marginBottom: 16 }}
            />
            <ProForm.Item label="告警规则">
              <div style={{ marginBottom: 16 }}>
                <Button
                  type="dashed"
                  onClick={() => {
                    const newRule: AlertRule = {
                      id: Date.now(),
                      name: `规则${alertRules.length + 1}`,
                      enabled: true,
                      target_classes: [],
                      min_confidence: 0.5,
                      min_count: 1,
                      max_count: 0,
                      suppression_window_seconds: 60,
                      roi_ids: [],
                    };
                    setAlertRules([...alertRules, newRule]);
                  }}
                  style={{ width: "100%" }}
                >
                  添加告警规则
                </Button>
              </div>
              {alertRules.map((rule, index) => (
                <Card
                  key={rule.id}
                  size="small"
                  style={{ marginBottom: 8 }}
                  title={
                    <Space>
                      <Input
                        value={rule.name}
                        onChange={(e) => {
                          const newRules = [...alertRules];
                          newRules[index].name = e.target.value;
                          setAlertRules(newRules);
                        }}
                        placeholder="规则名称"
                        style={{ width: 200 }}
                      />
                      <Tag color={rule.enabled ? "green" : "default"}>
                        {rule.enabled ? "已启用" : "已禁用"}
                      </Tag>
                      <Switch
                        checked={rule.enabled}
                        onChange={(checked) => {
                          const newRules = [...alertRules];
                          newRules[index].enabled = checked;
                          setAlertRules(newRules);
                        }}
                        checkedChildren="启用"
                        unCheckedChildren="禁用"
                      />
                      <Button
                        type="link"
                        danger
                        size="small"
                        onClick={() => {
                          setAlertRules(alertRules.filter((_, i) => i !== index));
                        }}
                      >
                        删除
                      </Button>
                    </Space>
                  }
                >
                  <Row gutter={16}>
                    <Col span={24}>
                      <div style={{ marginBottom: 12 }}>
                        <label style={{ display: "block", marginBottom: 4 }}>
                          目标类别 <span style={{ color: "#999", fontSize: 12 }}>(留空表示所有类别)</span>
                        </label>
                        <Select
                          mode="multiple"
                          value={rule.target_classes}
                          onChange={(values) => {
                            const newRules = [...alertRules];
                            newRules[index].target_classes = values;
                            setAlertRules(newRules);
                          }}
                          style={{ width: "100%" }}
                          placeholder="选择目标类别，留空表示检测所有类别"
                          options={classes.map((cls) => ({
                            label: `${cls.id}: ${cls.name}`,
                            value: cls.id,
                          }))}
                        />
                      </div>
                    </Col>
                  </Row>
                  
                  <Row gutter={16}>
                    <Col span={8}>
                      <div style={{ marginBottom: 12 }}>
                        <label style={{ display: "block", marginBottom: 4 }}>
                          最小置信度
                        </label>
                        <InputNumber
                          value={rule.min_confidence}
                          onChange={(value) => {
                            const newRules = [...alertRules];
                            newRules[index].min_confidence = value || 0.5;
                            setAlertRules(newRules);
                          }}
                          min={0}
                          max={1}
                          step={0.01}
                          precision={2}
                          style={{ width: "100%" }}
                          placeholder="0.5"
                        />
                        <div style={{ fontSize: 12, color: "#999", marginTop: 4 }}>
                          检测框置信度必须≥此值
                        </div>
                      </div>
                    </Col>
                    <Col span={8}>
                      <div style={{ marginBottom: 12 }}>
                        <label style={{ display: "block", marginBottom: 4 }}>
                          最小检测数量
                        </label>
                        <InputNumber
                          value={rule.min_count}
                          onChange={(value) => {
                            const newRules = [...alertRules];
                            newRules[index].min_count = value || 1;
                            setAlertRules(newRules);
                          }}
                          min={1}
                          style={{ width: "100%" }}
                          placeholder="1"
                        />
                        <div style={{ fontSize: 12, color: "#999", marginTop: 4 }}>
                          至少检测到N个目标才触发
                        </div>
                      </div>
                    </Col>
                    <Col span={8}>
                      <div style={{ marginBottom: 12 }}>
                        <label style={{ display: "block", marginBottom: 4 }}>
                          最大检测数量 <span style={{ color: "#999", fontSize: 12 }}>(0=不限制)</span>
                        </label>
                        <InputNumber
                          value={rule.max_count}
                          onChange={(value) => {
                            const newRules = [...alertRules];
                            newRules[index].max_count = value || 0;
                            setAlertRules(newRules);
                          }}
                          min={0}
                          style={{ width: "100%" }}
                          placeholder="0"
                        />
                        <div style={{ fontSize: 12, color: "#999", marginTop: 4 }}>
                          超过此数量也触发告警
                        </div>
                      </div>
                    </Col>
                  </Row>
                  
                  <Row gutter={16}>
                    <Col span={12}>
                      <div style={{ marginBottom: 12 }}>
                        <label style={{ display: "block", marginBottom: 4 }}>
                          关联ROI区域 <span style={{ color: "#999", fontSize: 12 }}>(留空表示全图)</span>
                        </label>
                        <Select
                          mode="multiple"
                          value={rule.roi_ids}
                          onChange={(values) => {
                            const newRules = [...alertRules];
                            newRules[index].roi_ids = values;
                            setAlertRules(newRules);
                          }}
                          style={{ width: "100%" }}
                          placeholder="选择ROI区域，留空表示检测全图"
                          options={rois
                            .filter((roi) => roi.enabled)
                            .map((roi) => ({
                              label: `${roi.name} (ID: ${roi.id})`,
                              value: roi.id,
                            }))}
                          disabled={rois.filter((roi) => roi.enabled).length === 0}
                        />
                        {rois.filter((roi) => roi.enabled).length === 0 && (
                          <div style={{ fontSize: 12, color: "#ff4d4f", marginTop: 4 }}>
                            请先在上方添加并启用ROI区域
                          </div>
                        )}
                      </div>
                    </Col>
                    <Col span={12}>
                      <div style={{ marginBottom: 12 }}>
                        <label style={{ display: "block", marginBottom: 4 }}>
                          抑制时间窗口(秒)
                        </label>
                        <InputNumber
                          value={rule.suppression_window_seconds}
                          onChange={(value) => {
                            const newRules = [...alertRules];
                            newRules[index].suppression_window_seconds = value || 60;
                            setAlertRules(newRules);
                          }}
                          min={1}
                          style={{ width: "100%" }}
                          placeholder="60"
                        />
                        <div style={{ fontSize: 12, color: "#999", marginTop: 4 }}>
                          相同告警在此时间内只触发一次
                        </div>
                      </div>
                    </Col>
                  </Row>
                  
                  <Alert
                    message="告警规则说明"
                    description={
                      <div style={{ fontSize: 12 }}>
                        <div>• 目标类别：留空表示检测所有类别，否则只检测选中的类别</div>
                        <div>• 最小/最大数量：满足最小数量或超过最大数量都会触发告警</div>
                        <div>• ROI区域：留空表示检测全图，否则只在选中的ROI区域内检测</div>
                        <div>• 抑制窗口：防止短时间内重复告警，建议设置为60-300秒</div>
                      </div>
                    }
                    type="info"
                    showIcon
                    style={{ marginTop: 8 }}
                  />
                </Card>
              ))}
              {alertRules.length === 0 && (
                <Alert
                  message="暂无告警规则"
                  description="点击上方「添加告警规则」按钮创建告警规则。如果没有配置告警规则，系统将在检测到任何目标时都触发告警。"
                  type="warning"
                  showIcon
                  style={{ marginTop: 16 }}
                />
              )}
            </ProForm.Item>
          </ProForm>
        </Spin>
      </Card>

      <ROIDrawer
        visible={drawerVisible}
        onClose={() => {
          setDrawerVisible(false);
          setEditingRoiIndex(null);
        }}
        onConfirm={(roi) => {
          if (editingRoiIndex !== null && editingRoiIndex >= 0 && editingRoiIndex < rois.length) {
            // 编辑现有ROI
            const newRois = [...rois];
            newRois[editingRoiIndex] = {
              ...newRois[editingRoiIndex],
              type: roi.type,
              points: roi.points,
            };
            setRois(newRois);
            message.success("区域已更新");
          } else {
            // 添加新ROI
            setRois([...rois, roi]);
            message.success("区域已添加");
          }
          setEditingRoiIndex(null);
        }}
        width={form.getFieldValue("input_width") || channelWidth}
        height={form.getFieldValue("input_height") || channelHeight}
        existingRois={rois}
        editingRoi={
          editingRoiIndex !== null && editingRoiIndex >= 0 && editingRoiIndex < rois.length
            ? rois[editingRoiIndex]
            : null
        }
        channelId={channelId ? parseInt(channelId, 10) : null}
      />
    </div>
  );
}

export default AlgorithmConfigPage;

