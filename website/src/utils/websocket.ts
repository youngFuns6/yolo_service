import Config from "@/config/index";

export interface WebSocketMessage {
  type: "frame" | "alert" | "subscription_confirmed" | "alert_subscription_confirmed";
  channel_id: number;
  image_base64?: string;
  channel_name?: string;
  alert_type?: string;
  confidence?: number;
  detected_objects?: string;
  timestamp: string;
}

export type WebSocketMessageHandler = (message: WebSocketMessage) => void;

class ChannelWebSocketManager {
  private ws: WebSocket | null = null;
  private reconnectTimer: number | null = null;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 5;
  private reconnectDelay = 3000;
  private messageHandlers: Set<WebSocketMessageHandler> = new Set();
  private isConnecting = false;
  private subscribedChannelId: number | null = null;
  private pendingChannelId: number | null = null;
  private onOpenCallbacks: Set<() => void> = new Set();

  connect(channelId?: number) {
    if (this.isConnecting || (this.ws && this.ws.readyState === WebSocket.OPEN)) {
      // 如果已连接且有新的 channelId，直接订阅
      if (channelId !== undefined && this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.subscribeChannel(channelId);
      }
      return;
    }

    if (channelId !== undefined) {
      this.pendingChannelId = channelId;
    }

    this.isConnecting = true;
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const wsUrl = `${protocol}//${window.location.host}/ws/channel`;

    try {
      this.ws = new WebSocket(wsUrl);

      this.ws.onopen = () => {
        console.log("通道数据 WebSocket 连接已建立");
        this.isConnecting = false;
        this.reconnectAttempts = 0;
        
        // 如果有待订阅的通道，自动订阅
        if (this.pendingChannelId !== null) {
          this.subscribeChannel(this.pendingChannelId);
          this.pendingChannelId = null;
        }
        
        // 执行所有 onopen 回调
        this.onOpenCallbacks.forEach((callback) => callback());
        this.onOpenCallbacks.clear();
      };

      this.ws.onmessage = (event) => {
        try {
          const message: WebSocketMessage = JSON.parse(event.data);
          
          // 处理订阅确认消息
          if (message.type === "subscription_confirmed") {
            console.log(`已成功订阅通道 ${message.channel_id}`);
            this.subscribedChannelId = message.channel_id;
          }
          
          // 处理帧数据和其他消息
          this.messageHandlers.forEach((handler) => handler(message));
        } catch (error) {
          console.error("解析 WebSocket 消息失败:", error);
        }
      };

      this.ws.onerror = (error) => {
        console.error("WebSocket 错误:", error);
        this.isConnecting = false;
      };

      this.ws.onclose = () => {
        console.log("通道数据 WebSocket 连接已关闭");
        this.isConnecting = false;
        const channelIdToResubscribe = this.subscribedChannelId;
        this.ws = null;
        this.subscribedChannelId = null;
        // 如果之前有订阅的通道，重连后需要重新订阅
        if (channelIdToResubscribe !== null) {
          this.pendingChannelId = channelIdToResubscribe;
        }
        this.attemptReconnect();
      };
    } catch (error) {
      console.error("创建 WebSocket 连接失败:", error);
      this.isConnecting = false;
      this.attemptReconnect();
    }
  }

  private attemptReconnect() {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error("WebSocket 重连次数已达上限");
      return;
    }

    if (this.reconnectTimer) {
      return;
    }

    this.reconnectAttempts++;
    console.log(`尝试重连 WebSocket (${this.reconnectAttempts}/${this.maxReconnectAttempts})...`);

    // 保存当前订阅的通道ID，以便重连后重新订阅
    const channelIdToResubscribe = this.subscribedChannelId || this.pendingChannelId;

    this.reconnectTimer = window.setTimeout(() => {
      this.reconnectTimer = null;
      if (channelIdToResubscribe !== null) {
        this.connect(channelIdToResubscribe);
      } else {
        this.connect();
      }
    }, this.reconnectDelay);
  }

  disconnect() {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }

    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }

    this.messageHandlers.clear();
    this.reconnectAttempts = 0;
    this.subscribedChannelId = null;
  }

  subscribeChannel(channelId: number) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      console.warn("WebSocket 未连接，无法订阅通道");
      return;
    }

    const subscribeMessage = {
      action: "subscribe",
      channel_id: channelId,
    };

    try {
      this.ws.send(JSON.stringify(subscribeMessage));
      console.log(`正在订阅通道 ${channelId}...`);
    } catch (error) {
      console.error("发送订阅消息失败:", error);
    }
  }

  addMessageHandler(handler: WebSocketMessageHandler) {
    this.messageHandlers.add(handler);
  }

  removeMessageHandler(handler: WebSocketMessageHandler) {
    this.messageHandlers.delete(handler);
  }

  isConnected(): boolean {
    return this.ws !== null && this.ws.readyState === WebSocket.OPEN;
  }

  getSubscribedChannelId(): number | null {
    return this.subscribedChannelId;
  }

  onOpen(callback: () => void) {
    if (this.isConnected()) {
      callback();
    } else {
      this.onOpenCallbacks.add(callback);
    }
  }

  removeOnOpenCallback(callback: () => void) {
    this.onOpenCallbacks.delete(callback);
  }
}

// 单例实例
const channelWsManager = new ChannelWebSocketManager();

export default channelWsManager;

