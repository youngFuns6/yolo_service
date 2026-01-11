import { useState, useRef, useEffect } from "react";
import { Modal, Button, Space, Radio, message, Spin } from "antd";
import type { ROI } from "@/api/algorithm";
import channelWsManager, { type WebSocketMessage } from "@/utils/websocket";

interface ROIDrawerProps {
  visible: boolean;
  onClose: () => void;
  onConfirm: (roi: ROI) => void;
  width: number;
  height: number;
  existingRois?: ROI[];
  editingRoi?: ROI | null;
  channelId?: number | null;
}

type DrawMode = "RECTANGLE" | "POLYGON" | "NONE";

interface Point {
  x: number;
  y: number;
}

function ROIDrawer({
  visible,
  onClose,
  onConfirm,
  width,
  height,
  existingRois = [],
  editingRoi = null,
  channelId = null,
}: ROIDrawerProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const backgroundImageRef = useRef<HTMLImageElement | null>(null);
  const [drawMode, setDrawMode] = useState<DrawMode>("NONE");
  const [currentRoi, setCurrentRoi] = useState<ROI | null>(null);
  const [isDrawing, setIsDrawing] = useState(false);
  const [startPoint, setStartPoint] = useState<Point | null>(null);
  const [currentPoints, setCurrentPoints] = useState<Point[]>([]);
  const [hoverPoint, setHoverPoint] = useState<Point | null>(null);
  const [streamLoading, setStreamLoading] = useState(false);
  const [hasStreamImage, setHasStreamImage] = useState(false);

  // 计算画布尺寸（保持宽高比，最大800x600）
  const maxCanvasWidth = 800;
  const maxCanvasHeight = 600;
  const aspectRatio = width / height;
  let canvasWidth = maxCanvasWidth;
  let canvasHeight = maxCanvasWidth / aspectRatio;

  if (canvasHeight > maxCanvasHeight) {
    canvasHeight = maxCanvasHeight;
    canvasWidth = maxCanvasHeight * aspectRatio;
  }

  const scaleX = canvasWidth / width;
  const scaleY = canvasHeight / height;

  // WebSocket连接和视频流处理
  useEffect(() => {
    if (!visible || !channelId) {
      return;
    }

    setStreamLoading(true);
    setHasStreamImage(false);

    // 连接WebSocket并订阅通道
    channelWsManager.connect(channelId);

    // 消息处理函数
    function handleMessage(message: WebSocketMessage) {
      // 处理订阅确认消息
      if (message.type === "subscription_confirmed") {
        console.log(`已成功订阅通道 ${message.channel_id}`);
        return;
      }

      // 处理帧数据
      if (message.type === "frame" && message.channel_id === channelId) {
        if (message.image_base64) {
          const img = new Image();
          img.onload = () => {
            backgroundImageRef.current = img;
            setHasStreamImage(true);
            setStreamLoading(false);
            drawCanvas();
          };
          img.onerror = () => {
            setStreamLoading(false);
            console.error("加载视频流图片失败");
          };
          img.src = `data:image/jpeg;base64,${message.image_base64}`;
        }
      }
    }

    // 添加消息处理器
    channelWsManager.addMessageHandler(handleMessage);

    // 如果连接已建立但还未订阅，则订阅通道
    const onOpenCallback = () => {
      if (channelWsManager.getSubscribedChannelId() !== channelId) {
        channelWsManager.subscribeChannel(channelId);
      }
    };

    channelWsManager.onOpen(onOpenCallback);

    // 清理函数
    return () => {
      channelWsManager.removeMessageHandler(handleMessage);
      channelWsManager.removeOnOpenCallback(onOpenCallback);
      // 注意：不在这里断开连接，因为可能其他地方也在使用
    };
  }, [visible, channelId]);

  // 当编辑现有ROI时，初始化当前ROI
  useEffect(() => {
    if (visible && editingRoi) {
      setCurrentRoi(editingRoi);
      // 如果编辑的是多边形，将归一化坐标转换为画布坐标
      if (editingRoi.type === "POLYGON" && editingRoi.points.length > 0) {
        const canvasPoints = editingRoi.points.map((p) => ({
          x: p.x * scaleX,
          y: p.y * scaleY,
        }));
        setCurrentPoints(canvasPoints);
      }
    } else if (visible && !editingRoi) {
      setCurrentRoi(null);
      setCurrentPoints([]);
    }
  }, [visible, editingRoi, scaleX, scaleY]);

  // 初始化画布
  useEffect(() => {
    if (!visible || !canvasRef.current) return;

    const canvas = canvasRef.current;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    canvas.width = canvasWidth;
    canvas.height = canvasHeight;

    drawCanvas();
  }, [visible, canvasWidth, canvasHeight]);

  // 绘制画布
  function drawCanvas() {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    // 清空画布
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // 绘制背景图片（如果有）
    if (backgroundImageRef.current && hasStreamImage) {
      // 计算图片缩放以适应画布
      const img = backgroundImageRef.current;
      const imgAspectRatio = img.width / img.height;
      const canvasAspectRatio = canvas.width / canvas.height;

      let drawWidth = canvas.width;
      let drawHeight = canvas.height;
      let drawX = 0;
      let drawY = 0;

      if (imgAspectRatio > canvasAspectRatio) {
        // 图片更宽，以高度为准
        drawHeight = canvas.height;
        drawWidth = drawHeight * imgAspectRatio;
        drawX = (canvas.width - drawWidth) / 2;
      } else {
        // 图片更高，以宽度为准
        drawWidth = canvas.width;
        drawHeight = drawWidth / imgAspectRatio;
        drawY = (canvas.height - drawHeight) / 2;
      }

      ctx.drawImage(img, drawX, drawY, drawWidth, drawHeight);
    } else {
      // 如果没有背景图片，绘制网格
      ctx.strokeStyle = "#e0e0e0";
      ctx.lineWidth = 1;
      const gridSize = 20;
      for (let x = 0; x <= canvas.width; x += gridSize) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, canvas.height);
        ctx.stroke();
      }
      for (let y = 0; y <= canvas.height; y += gridSize) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(canvas.width, y);
        ctx.stroke();
      }
    }

    // 绘制已存在的ROI区域（排除正在编辑的ROI）
    existingRois
      .filter((roi) => !editingRoi || roi.id !== editingRoi.id)
      .forEach((roi) => {
        if (!roi.enabled || !roi.points || roi.points.length === 0) return;

        ctx.strokeStyle = "#52c41a";
        ctx.fillStyle = "rgba(82, 196, 26, 0.2)";
        ctx.lineWidth = 2;

      if (roi.type === "RECTANGLE" && roi.points.length >= 2) {
        const p1 = roi.points[0];
        const p2 = roi.points[1];
        const x = Math.min(p1.x, p2.x) * scaleX;
        const y = Math.min(p1.y, p2.y) * scaleY;
        const w = Math.abs(p2.x - p1.x) * scaleX;
        const h = Math.abs(p2.y - p1.y) * scaleY;

        ctx.fillRect(x, y, w, h);
        ctx.strokeRect(x, y, w, h);
      } else if (roi.type === "POLYGON" && roi.points.length >= 3) {
        ctx.beginPath();
        const firstPoint = roi.points[0];
        ctx.moveTo(firstPoint.x * scaleX, firstPoint.y * scaleY);
        for (let i = 1; i < roi.points.length; i++) {
          ctx.lineTo(roi.points[i].x * scaleX, roi.points[i].y * scaleY);
        }
        ctx.closePath();
        ctx.fill();
        ctx.stroke();
      }
    });

    // 绘制当前正在绘制的区域
    if (drawMode === "RECTANGLE" && startPoint && hoverPoint) {
      ctx.strokeStyle = "#1890ff";
      ctx.fillStyle = "rgba(24, 144, 255, 0.2)";
      ctx.lineWidth = 2;

      const x = Math.min(startPoint.x, hoverPoint.x);
      const y = Math.min(startPoint.y, hoverPoint.y);
      const w = Math.abs(hoverPoint.x - startPoint.x);
      const h = Math.abs(hoverPoint.y - startPoint.y);

      ctx.fillRect(x, y, w, h);
      ctx.strokeRect(x, y, w, h);
    } else if (drawMode === "POLYGON" && currentPoints.length > 0) {
      ctx.strokeStyle = "#1890ff";
      ctx.fillStyle = "rgba(24, 144, 255, 0.2)";
      ctx.lineWidth = 2;

      // 绘制已完成的线段
      if (currentPoints.length >= 2) {
        ctx.beginPath();
        ctx.moveTo(currentPoints[0].x, currentPoints[0].y);
        for (let i = 1; i < currentPoints.length; i++) {
          ctx.lineTo(currentPoints[i].x, currentPoints[i].y);
        }
        ctx.stroke();
      }

      // 绘制从最后一个点到鼠标位置的预览线
      if (hoverPoint && currentPoints.length > 0) {
        const lastPoint = currentPoints[currentPoints.length - 1];
        ctx.strokeStyle = "#52c41a";
        ctx.lineWidth = 2;
        ctx.setLineDash([5, 5]);
        ctx.beginPath();
        ctx.moveTo(lastPoint.x, lastPoint.y);
        ctx.lineTo(hoverPoint.x, hoverPoint.y);
        ctx.stroke();
        ctx.setLineDash([]);

        // 如果至少有2个点，绘制从鼠标位置到第一个点的预览线（形成闭合预览）
        if (currentPoints.length >= 2) {
          const firstPoint = currentPoints[0];
          ctx.strokeStyle = "#ff9800";
          ctx.lineWidth = 1.5;
          ctx.setLineDash([3, 3]);
          ctx.beginPath();
          ctx.moveTo(hoverPoint.x, hoverPoint.y);
          ctx.lineTo(firstPoint.x, firstPoint.y);
          ctx.stroke();
          ctx.setLineDash([]);

          // 绘制预览填充区域
          ctx.fillStyle = "rgba(255, 152, 0, 0.15)";
          ctx.beginPath();
          ctx.moveTo(currentPoints[0].x, currentPoints[0].y);
          for (let i = 1; i < currentPoints.length; i++) {
            ctx.lineTo(currentPoints[i].x, currentPoints[i].y);
          }
          ctx.lineTo(hoverPoint.x, hoverPoint.y);
          ctx.closePath();
          ctx.fill();
        }
      }

      // 绘制已完成的点
      currentPoints.forEach((point, index) => {
        ctx.fillStyle = index === 0 ? "#ff4d4f" : "#1890ff";
        ctx.strokeStyle = "#fff";
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(point.x, point.y, 6, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();
      });

      // 绘制鼠标位置的预览点
      if (hoverPoint) {
        ctx.fillStyle = "#52c41a";
        ctx.strokeStyle = "#fff";
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(hoverPoint.x, hoverPoint.y, 6, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();
      }
    }
  }

  // 重新绘制
  useEffect(() => {
    if (visible) {
      drawCanvas();
    }
  }, [visible, existingRois, editingRoi, currentRoi, drawMode, startPoint, hoverPoint, currentPoints, hasStreamImage]);

  // 获取鼠标在画布上的坐标
  function getCanvasPoint(e: React.MouseEvent<HTMLCanvasElement>): Point {
    const canvas = canvasRef.current;
    if (!canvas) return { x: 0, y: 0 };

    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    return { x, y };
  }

  // 将画布坐标转换为归一化坐标
  function canvasToNormalized(point: Point): Point {
    return {
      x: point.x / scaleX,
      y: point.y / scaleY,
    };
  }

  // 鼠标按下
  function handleMouseDown(e: React.MouseEvent<HTMLCanvasElement>) {
    if (drawMode === "NONE") return;

    const point = getCanvasPoint(e);
    setStartPoint(point);

    if (drawMode === "RECTANGLE") {
      setIsDrawing(true);
    } else if (drawMode === "POLYGON") {
      setCurrentPoints([...currentPoints, point]);
    }
  }

  // 鼠标移动
  function handleMouseMove(e: React.MouseEvent<HTMLCanvasElement>) {
    const point = getCanvasPoint(e);

    if (drawMode === "RECTANGLE" && isDrawing && startPoint) {
      setHoverPoint(point);
    } else if (drawMode === "POLYGON") {
      setHoverPoint(point);
    } else {
      setHoverPoint(null);
    }
  }

  // 鼠标抬起
  function handleMouseUp(e: React.MouseEvent<HTMLCanvasElement>) {
    if (drawMode === "RECTANGLE" && isDrawing && startPoint) {
      const endPoint = getCanvasPoint(e);
      setIsDrawing(false);
      setStartPoint(null);
      setHoverPoint(null);

      // 创建矩形ROI
      const normalizedStart = canvasToNormalized(startPoint);
      const normalizedEnd = canvasToNormalized(endPoint);

      const newRoi: ROI = {
        id: editingRoi?.id || Date.now(),
        type: "RECTANGLE",
        name: editingRoi?.name || `区域${existingRois.length + 1}`,
        enabled: editingRoi?.enabled ?? true,
        points: [normalizedStart, normalizedEnd],
      };

      setCurrentRoi(newRoi);
      setDrawMode("NONE");
    }
  }

  // 双击完成多边形绘制
  function handleDoubleClick() {
    if (drawMode === "POLYGON" && currentPoints.length >= 3) {
      // 将画布坐标转换为归一化坐标
      const normalizedPoints = currentPoints.map((p) => canvasToNormalized(p));

      const newRoi: ROI = {
        id: editingRoi?.id || Date.now(),
        type: "POLYGON",
        name: editingRoi?.name || `区域${existingRois.length + 1}`,
        enabled: editingRoi?.enabled ?? true,
        points: normalizedPoints,
      };

      setCurrentRoi(newRoi);
      setCurrentPoints([]);
      setHoverPoint(null);
      setDrawMode("NONE");
    }
  }

  // 确认绘制
  function handleConfirm() {
    // 如果正在编辑ROI且已有currentRoi，直接使用
    if (editingRoi && currentRoi) {
      onConfirm(currentRoi);
      handleCancel();
      return;
    }

    // 否则需要绘制新区域
    if (!currentRoi) {
      message.warning("请先绘制一个区域");
      return;
    }

    onConfirm(currentRoi);
    handleCancel();
  }

  // 取消绘制
  function handleCancel() {
    setCurrentRoi(null);
    setDrawMode("NONE");
    setCurrentPoints([]);
    setStartPoint(null);
    setHoverPoint(null);
    setIsDrawing(false);
    onClose();
  }

  // 使用编辑的ROI作为初始值
  useEffect(() => {
    if (visible && editingRoi && editingRoi.points.length > 0) {
      // 如果编辑的是矩形，设置起始点以便重新绘制
      if (editingRoi.type === "RECTANGLE" && editingRoi.points.length >= 2) {
        const p1 = editingRoi.points[0];
        const p2 = editingRoi.points[1];
        // 转换为画布坐标
        const canvasP1 = { x: p1.x * scaleX, y: p1.y * scaleY };
        const canvasP2 = { x: p2.x * scaleX, y: p2.y * scaleY };
        setStartPoint(canvasP1);
        setHoverPoint(canvasP2);
      }
    }
  }, [visible, editingRoi]);

  // 开始绘制矩形
  function startDrawRectangle() {
    setDrawMode("RECTANGLE");
    setCurrentRoi(null);
    setCurrentPoints([]);
  }

  // 开始绘制多边形
  function startDrawPolygon() {
    setDrawMode("POLYGON");
    setCurrentRoi(null);
    setCurrentPoints([]);
  }

  return (
    <Modal
      title="绘制ROI区域"
      open={visible}
      onCancel={handleCancel}
      width={Math.min(canvasWidth + 100, 1000)}
      footer={
        <Space>
          <Button onClick={handleCancel}>取消</Button>
          <Button
            type="primary"
            onClick={handleConfirm}
            disabled={!currentRoi && !editingRoi}
          >
            确认
          </Button>
        </Space>
      }
    >
      <div style={{ marginBottom: 16 }}>
        <Space>
          <span>绘制模式：</span>
          <Radio.Group
            value={drawMode}
            onChange={(e) => {
              if (e.target.value === "RECTANGLE") {
                startDrawRectangle();
              } else if (e.target.value === "POLYGON") {
                startDrawPolygon();
              } else {
                setDrawMode("NONE");
                setCurrentPoints([]);
                setStartPoint(null);
                setHoverPoint(null);
              }
            }}
          >
            <Radio value="NONE">无</Radio>
            <Radio value="RECTANGLE">矩形</Radio>
            <Radio value="POLYGON">多边形</Radio>
          </Radio.Group>
        </Space>
      </div>

      <div style={{ marginBottom: 8, fontSize: 12, color: "#666" }}>
        {drawMode === "RECTANGLE" && "在画布上按住鼠标左键拖动绘制矩形"}
        {drawMode === "POLYGON" && (
          <div>
            <div>在画布上点击鼠标左键添加顶点，双击完成绘制（至少3个顶点）</div>
            <div style={{ marginTop: 4, color: "#999" }}>
              绿色虚线：当前预览线 | 橙色虚线：闭合预览线 | 橙色半透明区域：预览区域
            </div>
          </div>
        )}
        {drawMode === "NONE" && "请选择绘制模式"}
      </div>

      <Spin spinning={streamLoading && !hasStreamImage} tip="正在加载视频流...">
        <div
          style={{
            border: "1px solid #d9d9d9",
            borderRadius: 4,
            padding: 8,
            backgroundColor: "#fafafa",
            display: "inline-block",
            position: "relative",
          }}
        >
          <canvas
            ref={canvasRef}
            onMouseDown={handleMouseDown}
            onMouseMove={handleMouseMove}
            onMouseUp={handleMouseUp}
            onDoubleClick={handleDoubleClick}
            style={{
              cursor: drawMode !== "NONE" ? "crosshair" : "default",
              display: "block",
            }}
          />
          {!hasStreamImage && channelId && (
            <div
              style={{
                position: "absolute",
                top: "50%",
                left: "50%",
                transform: "translate(-50%, -50%)",
                color: "#999",
                fontSize: 12,
              }}
            >
              等待视频流...
            </div>
          )}
        </div>
      </Spin>

      <div style={{ marginTop: 8, fontSize: 12, color: "#999" }}>
        <div>画布尺寸: {canvasWidth.toFixed(0)} × {canvasHeight.toFixed(0)}</div>
        <div>参考尺寸: {width} × {height}</div>
        {editingRoi && (
          <div style={{ marginTop: 4, color: "#ff9800" }}>
            正在编辑: {editingRoi.name} ({editingRoi.type === "RECTANGLE" ? "矩形" : "多边形"})
          </div>
        )}
        {currentRoi && currentRoi !== editingRoi && (
          <div style={{ marginTop: 4, color: "#1890ff" }}>
            已绘制 {currentRoi.type === "RECTANGLE" ? "矩形" : "多边形"} 区域
            {currentRoi.points.length > 0 && ` (${currentRoi.points.length}个点)`}
          </div>
        )}
        {!currentRoi && !editingRoi && (
          <div style={{ marginTop: 4, color: "#999" }}>
            请选择绘制模式并绘制区域
          </div>
        )}
      </div>
    </Modal>
  );
}

export default ROIDrawer;

