<template>
  <div class="glass-card monitor-card">
    <div class="monitor-header">
      <div class="monitor-title">
        <span class="live-indicator" :class="{ 'live-active': isReceivingFrames && isStreamingActive }"></span>
        <h3>LIVE CAMERA MONITOR</h3>
        <span class="badge" :class="isReceivingFrames && isStreamingActive ? 'badge-disarmed' : 'badge-offline'">
          {{ isStreamingActive ? (isReceivingFrames ? 'STREAMING' : 'CONNECTING...') : 'STANDBY' }}
        </span>
      </div>
      <div class="header-right">
        <div class="monitor-meta font-mono">
          <span>QVGA 320x240</span>
          <span class="divider">•</span>
          <span>Grayscale JPEG</span>
          <span class="divider">•</span>
          <span>{{ fps }} FPS</span>
        </div>
        <button 
          class="btn-stream-toggle" 
          :class="isStreamingActive ? 'btn-stop-stream' : 'btn-start-stream'"
          @click="toggleStreaming"
        >
          <svg v-if="!isStreamingActive" viewBox="0 0 24 24" width="14" height="14" fill="currentColor">
            <polygon points="5 3 19 12 5 21 5 3"></polygon>
          </svg>
          <svg v-else viewBox="0 0 24 24" width="14" height="14" fill="currentColor">
            <rect x="6" y="5" width="4" height="14" rx="1"></rect>
            <rect x="14" y="5" width="4" height="14" rx="1"></rect>
          </svg>
          {{ isStreamingActive ? 'Tắt Stream' : 'Bật Stream' }}
        </button>
      </div>
    </div>

    <div class="monitor-viewport">
      <img
        v-if="isStreamingActive && currentFrameUrl"
        :src="currentFrameUrl"
        alt="Camera Stream"
        class="stream-img"
      />
      <div v-else-if="isStreamingActive" class="stream-placeholder">
        <div class="loading-spinner"></div>
        <p>{{ wsStatusText }}</p>
        <span class="placeholder-sub">Đang yêu cầu ESP32 khởi động truyền hình ảnh...</span>
      </div>
      <div v-else class="stream-standby">
        <div class="standby-icon-wrap">
          <svg class="placeholder-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
            <path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"></path>
            <circle cx="12" cy="13" r="4"></circle>
          </svg>
        </div>
        <h4>Camera đang ở chế độ Chờ (Standby)</h4>
        <p class="standby-desc">Hệ thống chỉ truyền hình ảnh khi có yêu cầu từ Web nhằm tiết kiệm băng thông và tối ưu hiệu năng MCU.</p>
        <button class="btn-primary btn-start-large" @click="startStream">
          <svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor">
            <polygon points="5 3 19 12 5 21 5 3"></polygon>
          </svg>
          Bật Xem Trực Tiếp
        </button>
      </div>

      <!-- Overlay Status Bar -->
      <div class="viewport-overlay font-mono">
        <span>LATENCY &le; 125ms</span>
        <span v-if="isStreamingActive && framesReceived > 0">Frames: {{ framesReceived }}</span>
        <span v-else>CHẾ ĐỘ TIẾT KIỆM BĂNG THÔNG</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount } from 'vue';
import api from '../api/client';

const isStreamingActive = ref(false);
const currentFrameUrl = ref(null);
let previousFrameUrl = null;
const isReceivingFrames = ref(false);
const wsStatusText = ref('Đang kết nối cổng WebSocket Stream...');
const fps = ref(0);
const framesReceived = ref(0);

let ws = null;
let frameCounter = 0;
let fpsInterval = null;
let lastFrameTimeout = null;
let nextFrameBlob = null;
let rafId = null;

function scheduleRender() {
  if (nextFrameBlob) {
    const newUrl = URL.createObjectURL(nextFrameBlob);
    currentFrameUrl.value = newUrl;
    if (previousFrameUrl) {
      URL.revokeObjectURL(previousFrameUrl);
    }
    previousFrameUrl = newUrl;
    nextFrameBlob = null;
  }
}

function startStream() {
  if (isStreamingActive.value) return;
  isStreamingActive.value = true;
  wsStatusText.value = 'Đang kích hoạt camera ESP32...';

  // 1. Báo Backend gửi lệnh MQTT bật stream xuống ESP32
  api.post('/system/stream', { enable: true }).catch(err => {
    console.warn('Could not send stream enable via API, relying on WebSocket connection:', err.message);
  });

  // 2. Mở kết nối WebSocket
  connectStream();

  // 3. Khởi động bộ đếm FPS
  if (!fpsInterval) {
    fpsInterval = setInterval(() => {
      fps.value = frameCounter;
      frameCounter = 0;
    }, 1000);
  }
}

function stopStream() {
  isStreamingActive.value = false;
  isReceivingFrames.value = false;
  fps.value = 0;
  frameCounter = 0;

  // 1. Báo Backend gửi lệnh MQTT tắt stream xuống ESP32
  api.post('/system/stream', { enable: false }).catch(err => {
    console.warn('Could not send stream disable via API:', err.message);
  });

  // 2. Đóng kết nối WebSocket
  if (ws) {
    ws.close();
    ws = null;
  }

  // 3. Dừng bộ đếm và giải phóng bộ nhớ hình ảnh
  if (fpsInterval) {
    clearInterval(fpsInterval);
    fpsInterval = null;
  }
  if (lastFrameTimeout) clearTimeout(lastFrameTimeout);
  if (previousFrameUrl) {
    URL.revokeObjectURL(previousFrameUrl);
    previousFrameUrl = null;
  }
  if (currentFrameUrl.value) {
    URL.revokeObjectURL(currentFrameUrl.value);
    currentFrameUrl.value = null;
  }
}

function toggleStreaming() {
  if (isStreamingActive.value) {
    stopStream();
  } else {
    startStream();
  }
}

function connectStream() {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const streamUrl = `${protocol}//${window.location.host}/ws/camera/stream?viewer=true`;

  ws = new WebSocket(streamUrl);
  ws.binaryType = 'arraybuffer';

  ws.onopen = () => {
    wsStatusText.value = 'Đã kết nối máy chủ. Đang chờ khung hình từ ESP32...';
    console.log('✅ Connected to Binary Video Stream Gateway');
  };

  ws.onmessage = (event) => {
    if (!isStreamingActive.value) return;

    if (event.data instanceof ArrayBuffer) {
      framesReceived.value++;
      frameCounter++;
      isReceivingFrames.value = true;

      // Lưu trữ frame nhị phân mới nhất vào buffer để requestAnimationFrame vẽ đồng bộ màn hình
      nextFrameBlob = new Blob([event.data], { type: 'image/jpeg' });
      if (!rafId) {
        rafId = requestAnimationFrame(() => {
          scheduleRender();
          rafId = null;
        });
      }

      // Reset timer kiểm tra mất frame
      clearTimeout(lastFrameTimeout);
      lastFrameTimeout = setTimeout(() => {
        isReceivingFrames.value = false;
        wsStatusText.value = 'Tạm ngừng nhận frame từ ESP32';
      }, 3000);
    }
  };

  ws.onerror = (err) => {
    console.error('Stream WebSocket error:', err);
    wsStatusText.value = 'Lỗi kết nối cổng Video Stream';
  };

  ws.onclose = () => {
    isReceivingFrames.value = false;
    if (isStreamingActive.value) {
      wsStatusText.value = 'Mất kết nối máy chủ. Thử lại sau 3s...';
      setTimeout(() => {
        if (isStreamingActive.value) connectStream();
      }, 3000);
    }
  };
}

onMounted(() => {
  // MẶC ĐỊNH KHÔNG TỰ ĐỘNG STREAM: Để camera ở chế độ chờ (Standby)
  // Chỉ kích hoạt khi người dùng bấm "Bật Stream"
});

onBeforeUnmount(() => {
  stopStream();
});
</script>

<style scoped>
.monitor-card {
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.monitor-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 8px;
}

.monitor-title {
  display: flex;
  align-items: center;
  gap: 10px;
}

.monitor-title h3 {
  font-size: 0.95rem;
  font-weight: 700;
  letter-spacing: 0.05em;
}

.live-indicator {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: #6b7280;
}

.live-active {
  background: #ef4444;
  box-shadow: 0 0 10px #ef4444;
  animation: pulse-live 1.5s infinite;
}

@keyframes pulse-live {
  0%, 100% { opacity: 1; transform: scale(1); }
  50% { opacity: 0.4; transform: scale(1.2); }
}

.monitor-meta {
  font-size: 0.75rem;
  color: var(--text-muted);
}

.divider {
  margin: 0 4px;
}

.monitor-viewport {
  position: relative;
  width: 100%;
  aspect-ratio: 4 / 3;
  background: #000000;
  border-radius: 10px;
  overflow: hidden;
  border: 1px solid rgba(255, 255, 255, 0.1);
  display: flex;
  align-items: center;
  justify-content: center;
}

.stream-img {
  width: 100%;
  height: 100%;
  object-fit: contain;
  image-rendering: -webkit-optimize-contrast;
}

.stream-placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  color: var(--text-muted);
  text-align: center;
  padding: 20px;
}

.placeholder-icon {
  width: 48px;
  height: 48px;
  color: #4b5563;
}

.placeholder-sub {
  font-size: 0.75rem;
  color: #6b7280;
}

.header-right {
  display: flex;
  align-items: center;
  gap: 12px;
}

.btn-stream-toggle {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 5px 12px;
  border-radius: 6px;
  font-size: 0.78rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s ease;
  border: 1px solid transparent;
}

.btn-start-stream {
  background: rgba(16, 185, 129, 0.15);
  color: #10b981;
  border-color: rgba(16, 185, 129, 0.3);
}

.btn-start-stream:hover {
  background: #10b981;
  color: #ffffff;
  box-shadow: 0 0 12px rgba(16, 185, 129, 0.4);
}

.btn-stop-stream {
  background: rgba(239, 68, 68, 0.15);
  color: #ef4444;
  border-color: rgba(239, 68, 68, 0.3);
}

.btn-stop-stream:hover {
  background: #ef4444;
  color: #ffffff;
  box-shadow: 0 0 12px rgba(239, 68, 68, 0.4);
}

.stream-standby {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 10px;
  text-align: center;
  padding: 24px;
  max-width: 380px;
}

.standby-icon-wrap {
  width: 56px;
  height: 56px;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.05);
  border: 1px solid rgba(255, 255, 255, 0.1);
  display: flex;
  align-items: center;
  justify-content: center;
  margin-bottom: 4px;
}

.stream-standby h4 {
  font-size: 0.95rem;
  font-weight: 600;
  color: #e5e7eb;
}

.standby-desc {
  font-size: 0.78rem;
  color: #9ca3af;
  line-height: 1.4;
}

.btn-start-large {
  margin-top: 6px;
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 18px;
  font-size: 0.85rem;
  font-weight: 600;
  background: linear-gradient(135deg, #10b981 0%, #059669 100%);
  color: #ffffff;
  border: none;
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.25s ease;
  box-shadow: 0 4px 14px rgba(16, 185, 129, 0.35);
}

.btn-start-large:hover {
  transform: translateY(-2px);
  box-shadow: 0 6px 20px rgba(16, 185, 129, 0.5);
}

.loading-spinner {
  width: 36px;
  height: 36px;
  border: 3px solid rgba(255, 255, 255, 0.1);
  border-top-color: #3b82f6;
  border-radius: 50%;
  animation: spin 1s linear infinite;
  margin-bottom: 8px;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

.viewport-overlay {
  position: absolute;
  bottom: 8px;
  left: 8px;
  right: 8px;
  display: flex;
  justify-content: space-between;
  font-size: 0.7rem;
  color: rgba(255, 255, 255, 0.6);
  background: rgba(0, 0, 0, 0.5);
  padding: 4px 8px;
  border-radius: 4px;
  backdrop-filter: blur(4px);
}
</style>
