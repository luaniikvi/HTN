const { WebSocketServer } = require('ws');
const mqttGateway = require('./mqttGateway.service');

class VideoStreamService {
  constructor() {
    this.streamWss = null;
    this.eventsWss = null;
    this.viewers = new Set();
    this.esp32CameraWs = null;
    this.lastFrame = null;
  }

  attachServers(httpServer, httpsServer) {
    // 1. WebSocket Server cho luồng Binary Stream (/ws/camera/stream)
    this.streamWss = new WebSocketServer({ noServer: true });

    // 2. WebSocket Server cho sự kiện thời gian thực (/ws/events)
    this.eventsWss = new WebSocketServer({ noServer: true });

    const handleUpgrade = (request, socket, head) => {
      const url = new URL(request.url, `https://${request.headers.host}`);
      const pathname = url.pathname;

      if (pathname === '/ws/camera/stream') {
        this.streamWss.handleUpgrade(request, socket, head, (ws) => {
          this.streamWss.emit('connection', ws, request);
        });
      } else if (pathname === '/ws/events') {
        this.eventsWss.handleUpgrade(request, socket, head, (ws) => {
          this.eventsWss.emit('connection', ws, request);
        });
      } else {
        socket.destroy();
      }
    };

    if (httpServer) httpServer.on('upgrade', handleUpgrade);
    if (httpsServer) httpsServer.on('upgrade', handleUpgrade);

    this.setupStreamHandler();
    this.setupEventsHandler();
  }

  setupStreamHandler() {
    this.streamWss.on('connection', (ws, req) => {
      const url = new URL(req.url, `https://${req.headers.host}`);
      const isViewer = url.searchParams.get('viewer') === 'true' || req.headers['sec-websocket-protocol'] === 'viewer';

      if (isViewer) {
        // Đây là Web Client kết nối để xem
        this.viewers.add(ws);
        console.log(`[Stream] New viewer connected. Total viewers: ${this.viewers.size}`);

        // Gửi ngay frame mới nhất nếu có để không bị màn hình đen
        if (this.lastFrame && ws.readyState === ws.OPEN) {
          ws.send(this.lastFrame, { binary: true });
        }

        // Auto-Throttling: Bật stream trên ESP32 nếu trước đó đang tắt
        if (this.viewers.size === 1) {
          mqttGateway.sendStreamCommand(true);
        }

        ws.on('close', () => {
          this.viewers.delete(ws);
          console.log(`[Stream] Viewer disconnected. Remaining: ${this.viewers.size}`);
          // Auto-Throttling: Hạ tải / tạm dừng stream trên ESP32 khi không có ai xem
          if (this.viewers.size === 0) {
            mqttGateway.sendStreamCommand(false);
          }
        });
      } else {
        // Đây là ESP32 kết nối đến để đẩy luồng hình ảnh
        console.log('📷 [Stream] ESP32 Camera sender connected');
        this.esp32CameraWs = ws;

        ws.on('message', (data, isBinary) => {
          if (!isBinary) return;

          // Lưu frame đệm tạm thời trong RAM (Zero-Copy relay, không ghi đĩa)
          this.lastFrame = data;

          // Chuyển tiếp tức thì mảng byte JPEG Grayscale đến tất cả Active Viewers (độ trễ <= 125ms)
          for (const viewer of this.viewers) {
            if (viewer.readyState === viewer.OPEN && viewer.bufferedAmount === 0) {
              viewer.send(data, { binary: true });
            }
          }
        });

        ws.on('close', () => {
          console.warn('⚠️ [Stream] ESP32 Camera sender disconnected');
          this.esp32CameraWs = null;
        });
      }
    });
  }

  setupEventsHandler() {
    this.eventsWss.on('connection', (ws) => {
      // Đăng ký nhận sự kiện từ MQTT Gateway để broadcast lên Web
      const sendJson = (type, payload) => {
        if (ws.readyState === ws.OPEN) {
          ws.send(JSON.stringify({ type, payload }));
        }
      };

      const onStatus = (data) => sendJson('DEVICE_STATUS', data);
      const onDoor = (data) => sendJson('DOOR_EVENT', data);
      const onAlarm = (data) => sendJson('ALARM_BREACH', data);
      const onEnrollStep = (data) => sendJson('ENROLL_STEP', data);
      const onEnrollDone = (data) => sendJson('ENROLL_DONE', data);
      const onDeletedDone = (data) => sendJson('DELETED_DONE', data);

      mqttGateway.on('device_status', onStatus);
      mqttGateway.on('door_event', onDoor);
      mqttGateway.on('alarm_event', onAlarm);
      mqttGateway.on('enroll_step', onEnrollStep);
      mqttGateway.on('enroll_done', onEnrollDone);
      mqttGateway.on('deleted_done', onDeletedDone);

      ws.on('close', () => {
        mqttGateway.off('device_status', onStatus);
        mqttGateway.off('door_event', onDoor);
        mqttGateway.off('alarm_event', onAlarm);
        mqttGateway.off('enroll_step', onEnrollStep);
        mqttGateway.off('enroll_done', onEnrollDone);
        mqttGateway.off('deleted_done', onDeletedDone);
      });
    });
  }

  broadcastEvent(type, payload) {
    if (this.eventsWss && this.eventsWss.clients) {
      const msg = JSON.stringify({ type, payload });
      for (const client of this.eventsWss.clients) {
        if (client.readyState === 1) { // 1 = OPEN
          client.send(msg);
        }
      }
    }
  }
}

const videoStreamService = new VideoStreamService();
module.exports = videoStreamService;
