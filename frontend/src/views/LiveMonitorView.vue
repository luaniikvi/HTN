<template>
  <div class="monitor-page">
    <!-- Page Header -->
    <div class="page-title-row">
      <div>
        <h1 class="page-title">Live Camera Monitor</h1>
        <p class="page-subtitle">Real-time low-latency video feed streamed from ESP32-S3 OV5640 sensor</p>
      </div>

      <div class="header-actions">
        <!-- Live Stream Toggle with MorphIcon -->
        <button
          class="btn btn-lg"
          :class="isStreamingActive ? 'btn-danger' : 'btn-primary'"
          @click="toggleStreaming"
        >
          <MorphIcon :icon="streamButtonIcon" :size="20" spring="snappy" />
          <span>{{ isStreamingActive ? 'Stop Stream' : 'Start Live Stream' }}</span>
        </button>
      </div>
    </div>

    <!-- Main Stream Layout Grid -->
    <div class="monitor-grid">
      <!-- Left / Primary: Video Viewport -->
      <div class="clean-card viewport-card">
        <div class="viewport-header">
          <div class="stream-badge-group">
            <span class="live-pulse-dot" :class="{ active: isReceivingFrames && isStreamingActive }"></span>
            <span class="badge" :class="isReceivingFrames && isStreamingActive ? 'badge-disarmed' : 'badge-offline'">
              {{ isStreamingActive ? (isReceivingFrames ? 'LIVE' : 'CONNECTING...') : 'STANDBY' }}
            </span>
          </div>

          <div class="stream-telemetry font-mono">
            <span class="telemetry-item">QVGA 320×240</span>
            <span class="dot-sep">•</span>
            <span class="telemetry-item">Grayscale JPEG</span>
            <span class="dot-sep">•</span>
            <span class="telemetry-item fps-badge">
              <strong>{{ fps }}</strong> FPS
            </span>
          </div>
        </div>

        <div class="viewport-wrapper">
          <!-- Active Stream Canvas / Image -->
          <img
            v-if="isStreamingActive && currentFrameUrl"
            :src="currentFrameUrl"
            alt="Live Stream Frame"
            class="viewport-img"
          />

          <!-- Connecting / Waiting Placeholder -->
          <div v-else-if="isStreamingActive" class="viewport-placeholder">
            <div class="stream-spinner"></div>
            <p class="placeholder-text">{{ wsStatusText }}</p>
            <span class="placeholder-sub">Requesting camera frame buffer from ESP32-S3...</span>
          </div>

          <!-- Standby State -->
          <div v-else class="viewport-standby">
            <div class="standby-icon-box">
              <Camera :size="44" stroke-width="1.8" />
            </div>
            <h3 class="standby-title">Camera in Low-Power Standby</h3>
            <p class="standby-desc">
              Streaming runs on-demand to conserve MCU Core 1 computation and preserve Wi-Fi bandwidth for edge face recognition.
            </p>
            <button class="btn btn-primary btn-md" @click="startStream">
              <Play :size="18" />
              <span>Launch Live View</span>
            </button>
          </div>

          <!-- Bottom Viewport Info Bar -->
          <div class="viewport-bottom-bar font-mono">
            <span>LATENCY: &le; 125ms</span>
            <span v-if="isStreamingActive && framesReceived > 0">
              FRAMES RECEIVED: {{ framesReceived }}
            </span>
            <span v-else>ENERGY CONSERVATION ACTIVE</span>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onBeforeUnmount } from 'vue';
import api from '../api/client';
import { MorphIcon } from 'morphicons/vue';
import { Play as LucidePlay, Square as LucideSquare } from 'lucide';
import {
  Camera,
  Play
} from 'lucide-vue-next';

const isStreamingActive = ref(false);
const currentFrameUrl = ref(null);
let previousFrameUrl = null;
const isReceivingFrames = ref(false);
const wsStatusText = ref('Connecting to WebSocket Gateway...');
const fps = ref(0);
const framesReceived = ref(0);

let ws = null;
let frameCounter = 0;
let fpsInterval = null;
let lastFrameTimeout = null;
let nextFrameBlob = null;
let rafId = null;

const streamButtonIcon = computed(() => {
  return isStreamingActive.value ? LucideSquare : LucidePlay;
});

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
  wsStatusText.value = 'Requesting camera activation from ESP32-S3...';

  // 1. Notify Backend to publish MQTT stream cmd to ESP32
  api.post('/system/stream', { enable: true }).catch(err => {
    console.warn('Could not trigger stream via API:', err.message);
  });

  // 2. Open binary stream WebSocket
  connectStream();

  // 3. Start FPS counter
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

  // 1. Notify Backend to publish MQTT stop stream cmd
  api.post('/system/stream', { enable: false }).catch(err => {
    console.warn('Could not stop stream via API:', err.message);
  });

  // 2. Close WebSocket
  if (ws) {
    ws.close();
    ws = null;
  }

  // 3. Clear timers and revoke blob URLs
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
    wsStatusText.value = 'Connected to server. Awaiting video frames from ESP32...';
  };

  ws.onmessage = (event) => {
    if (!isStreamingActive.value) return;

    if (event.data instanceof ArrayBuffer) {
      framesReceived.value++;
      frameCounter++;
      isReceivingFrames.value = true;

      nextFrameBlob = new Blob([event.data], { type: 'image/jpeg' });
      if (!rafId) {
        rafId = requestAnimationFrame(() => {
          scheduleRender();
          rafId = null;
        });
      }

      clearTimeout(lastFrameTimeout);
      lastFrameTimeout = setTimeout(() => {
        isReceivingFrames.value = false;
        wsStatusText.value = 'Waiting for incoming frames from ESP32...';
      }, 3000);
    }
  };

  ws.onerror = (err) => {
    console.error('Stream WebSocket error:', err);
    wsStatusText.value = 'Video stream connection error';
  };

  ws.onclose = () => {
    isReceivingFrames.value = false;
    if (isStreamingActive.value) {
      wsStatusText.value = 'Lost connection to gateway. Reconnecting in 3s...';
      setTimeout(() => {
        if (isStreamingActive.value) connectStream();
      }, 3000);
    }
  };
}

onMounted(() => {
  // Camera defaults to standby to save power
});

onBeforeUnmount(() => {
  stopStream();
});
</script>

<style scoped>
.monitor-page {
  display: flex;
  flex-direction: column;
  gap: 24px;
}

.page-title-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 14px;
}

.page-title {
  font-size: 1.65rem;
  font-weight: 800;
  color: var(--text-main);
  letter-spacing: -0.02em;
}

.page-subtitle {
  font-size: 0.95rem;
  color: var(--text-secondary);
  margin-top: 4px;
}

/* Grid Layout */
.monitor-grid {
  display: flex;
  justify-content: center;
  width: 100%;
}

/* Viewport Card */
.viewport-card {
  width: 100%;
  max-width: 960px;
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.viewport-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 12px;
}

.stream-badge-group {
  display: flex;
  align-items: center;
  gap: 10px;
}

.live-pulse-dot {
  width: 11px;
  height: 11px;
  border-radius: 50%;
  background: var(--text-muted);
}

.live-pulse-dot.active {
  background: var(--color-disarmed);
  box-shadow: 0 0 10px var(--color-disarmed);
  animation: pulse-live 1.5s infinite;
}

@keyframes pulse-live {
  0%, 100% { opacity: 1; transform: scale(1); }
  50% { opacity: 0.4; transform: scale(1.2); }
}

.stream-telemetry {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 0.875rem;
  color: var(--text-secondary);
}

.dot-sep {
  color: var(--border-strong);
}

.fps-badge {
  color: var(--color-primary);
  background: var(--color-primary-subtle);
  padding: 4px 10px;
  border-radius: var(--radius-sm);
  border: 1px solid var(--color-primary-border);
  font-size: 0.9rem;
}

/* Viewport Canvas */
.viewport-wrapper {
  position: relative;
  width: 100%;
  aspect-ratio: 4 / 3;
  background: #0f172a;
  border-radius: var(--radius-md);
  overflow: hidden;
  border: 1px solid var(--border-subtle);
  display: flex;
  align-items: center;
  justify-content: center;
}

.viewport-img {
  width: 100%;
  height: 100%;
  object-fit: contain;
  image-rendering: -webkit-optimize-contrast;
}

.viewport-placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 12px;
  padding: 32px;
  text-align: center;
  color: #94a3b8;
}

.stream-spinner {
  width: 44px;
  height: 44px;
  border: 3.5px solid rgba(255, 255, 255, 0.15);
  border-top-color: #3b82f6;
  border-radius: 50%;
  animation: spin 0.85s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

.placeholder-text {
  font-size: 1.05rem;
  font-weight: 600;
  color: #f1f5f9;
}

.placeholder-sub {
  font-size: 0.875rem;
  color: #64748b;
}

/* Standby */
.viewport-standby {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 16px;
  padding: 40px 28px;
  text-align: center;
  max-width: 520px;
}

.standby-icon-box {
  width: 84px;
  height: 84px;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.06);
  border: 1px solid rgba(255, 255, 255, 0.12);
  display: flex;
  align-items: center;
  justify-content: center;
  color: #94a3b8;
}

.standby-title {
  font-size: 1.3rem;
  font-weight: 800;
  color: #f8fafc;
}

.standby-desc {
  font-size: 0.95rem;
  color: #94a3b8;
  line-height: 1.6;
}

.viewport-bottom-bar {
  position: absolute;
  bottom: 10px;
  left: 10px;
  right: 10px;
  display: flex;
  justify-content: space-between;
  font-size: 0.8rem;
  color: rgba(255, 255, 255, 0.8);
  background: rgba(0, 0, 0, 0.6);
  backdrop-filter: blur(8px);
  -webkit-backdrop-filter: blur(8px);
  padding: 6px 14px;
  border-radius: var(--radius-sm);
  border: 1px solid rgba(255, 255, 255, 0.08);
}
</style>
