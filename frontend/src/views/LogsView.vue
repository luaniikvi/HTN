<template>
  <div class="logs-page">
    <!-- Header -->
    <div class="page-title-row">
      <div>
        <h1 class="page-title">Audit & Incident Logs</h1>
        <p class="page-subtitle">Track physical door access events, unauthorized alarm breaches, and snapshot evidence</p>
      </div>

      <div class="header-actions">
        <button class="btn btn-secondary" @click="fetchCurrentTabLogs" :disabled="isLoading">
          <RefreshCw :size="16" :class="{ spinning: isLoading }" />
          <span>Refresh</span>
        </button>
      </div>
    </div>

    <!-- Main Container with Tabs -->
    <div class="clean-card logs-container-card">
      <!-- Tab Header Strip -->
      <div class="tabs-header">
        <div class="tabs-list">
          <button
            class="tab-item"
            :class="{ active: activeTab === 'access' }"
            @click="activeTab = 'access'"
          >
            <Clock :size="15" />
            <span>Access History</span>
            <span class="count-pill">{{ accessLogs.length }}</span>
          </button>

          <button
            class="tab-item"
            :class="{ active: activeTab === 'alarms' }"
            @click="activeTab = 'alarms'"
          >
            <AlertTriangle :size="15" />
            <span>Breach Incidents</span>
            <span v-if="alarmLogs.length > 0" class="count-pill red">{{ alarmLogs.length }}</span>
          </button>

          <button
            class="tab-item"
            :class="{ active: activeTab === 'gallery' }"
            @click="activeTab = 'gallery'"
          >
            <Camera :size="15" />
            <span>Evidence Gallery</span>
            <span v-if="breachImages.length > 0" class="count-pill blue">{{ breachImages.length }}</span>
          </button>
        </div>
      </div>

      <!-- Tab 1: Access History Table -->
      <div v-if="activeTab === 'access'" class="tab-pane">
        <div class="table-responsive">
          <table class="data-table">
            <thead>
              <tr>
                <th>Timestamp</th>
                <th>Device ID</th>
                <th>Event Type</th>
                <th>Log Details</th>
              </tr>
            </thead>
            <tbody>
              <tr v-if="accessLogs.length === 0">
                <td colspan="4" class="empty-cell">No access events recorded yet.</td>
              </tr>
              <tr v-for="log in accessLogs" :key="log.id">
                <td class="font-mono text-muted text-xs">{{ formatTime(log.timestamp) }}</td>
                <td class="font-mono font-bold">{{ log.device_id }}</td>
                <td>
                  <span
                    class="badge"
                    :class="log.event_type === 'DOOR_OPENED' ? 'badge-stay' : (log.event_type === 'FACE_AUTH_SUCCESS' ? 'badge-disarmed' : 'badge-offline')"
                  >
                    {{ log.event_type }}
                  </span>
                </td>
                <td class="details-cell">{{ log.details }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

      <!-- Tab 2: Breach Incidents Table -->
      <div v-else-if="activeTab === 'alarms'" class="tab-pane">
        <div class="table-responsive">
          <table class="data-table">
            <thead>
              <tr>
                <th>Timestamp</th>
                <th>Mode</th>
                <th>Trigger Event</th>
                <th>Evidence Photo</th>
                <th>Details</th>
              </tr>
            </thead>
            <tbody>
              <tr v-if="alarmLogs.length === 0">
                <td colspan="5" class="empty-cell">No security breaches detected. All perimeters secure.</td>
              </tr>
              <tr v-for="log in alarmLogs" :key="log.id" class="alarm-row">
                <td class="font-mono text-xs">{{ formatTime(log.timestamp) }}</td>
                <td>
                  <span class="badge" :class="log.mode === 'ARMED' ? 'badge-armed' : 'badge-stay'">
                    {{ log.mode }}
                  </span>
                </td>
                <td>
                  <span class="badge badge-armed font-bold">{{ log.event }}</span>
                </td>
                <td>
                  <div v-if="log.image_url" class="thumb-cell" @click="openLightbox(log)">
                    <img :src="log.image_url" alt="Breach Evidence" class="log-thumbnail" />
                    <span class="thumb-zoom-tag">
                      <ZoomIn :size="12" />
                      <span>View</span>
                    </span>
                  </div>
                  <span v-else class="text-muted text-xs">—</span>
                </td>
                <td class="alarm-details-text">{{ log.details }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

      <!-- Tab 3: Snapshot Evidence Gallery -->
      <div v-else-if="activeTab === 'gallery'" class="tab-pane gallery-pane">
        <div v-if="breachImages.length === 0" class="empty-gallery-box">
          <Camera :size="40" class="empty-icon" />
          <h4>No Evidence Photos Stored</h4>
          <p>
            When an unauthorized door breach occurs in ARMED mode, the ESP32-S3 automatically captures a high-resolution snapshot and uploads it to this gallery.
          </p>
        </div>

        <div v-else class="gallery-grid">
          <div
            v-for="item in breachImages"
            :key="item.id"
            class="gallery-card"
            @click="openLightbox(item)"
          >
            <div class="gallery-img-wrapper">
              <img :src="item.image_url" alt="Breach Snapshot" class="gallery-img" />
              <div class="gallery-overlay">
                <span class="zoom-pill">
                  <ZoomIn :size="14" />
                  <span>Enlarge</span>
                </span>
              </div>
              <span class="mode-tag">{{ item.mode }} BREACH</span>
            </div>

            <div class="gallery-meta">
              <div class="gallery-time font-mono">
                {{ formatTime(item.timestamp) }}
              </div>
              <div class="gallery-footer-row">
                <span class="device-chip font-mono">{{ item.device_id }}</span>
                <a
                  :href="item.image_url"
                  target="_blank"
                  download
                  class="download-btn"
                  title="Download Image"
                  @click.stop
                >
                  <Download :size="14" />
                </a>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- Image Lightbox Modal -->
    <div v-if="selectedLog" class="modal-backdrop" @click="selectedLog = null">
      <div class="clean-card lightbox-card" @click.stop>
        <div class="lightbox-header">
          <div>
            <span class="badge badge-armed">High-Resolution Evidence Capture</span>
            <h3 class="lightbox-title">{{ formatTime(selectedLog.timestamp) }}</h3>
          </div>
          <button class="btn-close" @click="selectedLog = null">
            <X :size="18" />
          </button>
        </div>

        <div class="lightbox-body">
          <img :src="selectedLog.image_url" alt="Breach Evidence" class="lightbox-image" />
        </div>

        <div class="lightbox-footer">
          <div class="lightbox-meta font-mono">
            <span>Device: <strong>{{ selectedLog.device_id }}</strong></span>
            <span class="sep">•</span>
            <span>Security Mode: <strong>{{ selectedLog.mode }}</strong></span>
          </div>

          <div class="lightbox-actions">
            <a :href="selectedLog.image_url" download class="btn btn-secondary btn-sm">
              <Download :size="14" />
              <span>Download</span>
            </a>
            <a :href="selectedLog.image_url" target="_blank" class="btn btn-primary btn-sm">
              <ExternalLink :size="14" />
              <span>Open Full Tab</span>
            </a>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue';
import api from '../api/client';
import {
  RefreshCw,
  Clock,
  AlertTriangle,
  Camera,
  ZoomIn,
  Download,
  X,
  ExternalLink
} from 'lucide-vue-next';

const activeTab = ref('gallery');
const accessLogs = ref([]);
const alarmLogs = ref([]);
const isLoading = ref(false);
const selectedLog = ref(null);

const breachImages = computed(() => {
  return alarmLogs.value.filter(log => Boolean(log.image_url));
});

function openLightbox(log) {
  selectedLog.value = log;
}

async function fetchAccessLogs() {
  isLoading.value = true;
  try {
    const res = await api.get('/logs/access?limit=50');
    if (res.data.success) {
      accessLogs.value = res.data.logs;
    }
  } catch (err) {
    console.error('Failed to fetch access logs:', err);
  } finally {
    isLoading.value = false;
  }
}

async function fetchAlarmLogs() {
  isLoading.value = true;
  try {
    const res = await api.get('/logs/alarms?limit=50');
    if (res.data.success) {
      alarmLogs.value = res.data.logs;
    }
  } catch (err) {
    console.error('Failed to fetch alarm logs:', err);
  } finally {
    isLoading.value = false;
  }
}

function fetchCurrentTabLogs() {
  if (activeTab.value === 'access') fetchAccessLogs();
  else fetchAlarmLogs();
}

function formatTime(iso) {
  if (!iso) return 'N/A';
  const d = new Date(iso);
  return d.toLocaleString('en-US', {
    month: 'short',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });
}

watch(activeTab, (tab) => {
  if (tab === 'access' && accessLogs.value.length === 0) fetchAccessLogs();
  if ((tab === 'alarms' || tab === 'gallery') && alarmLogs.value.length === 0) fetchAlarmLogs();
});

onMounted(() => {
  fetchAlarmLogs();
  fetchAccessLogs();
});
</script>

<style scoped>
.logs-page {
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

.spinning {
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

/* Container Card */
.logs-container-card {
  padding: 0;
  overflow: hidden;
}

.tabs-header {
  border-bottom: 1px solid var(--border-subtle);
  background: #ffffff;
  padding: 0 16px;
}

.tabs-list {
  display: flex;
  gap: 8px;
}

.tab-item {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 14px 12px;
  background: transparent;
  border: none;
  border-bottom: 2px solid transparent;
  color: var(--text-secondary);
  font-size: 0.825rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.15s ease;
}

.tab-item:hover {
  color: var(--text-main);
}

.tab-item.active {
  color: var(--color-primary);
  border-bottom-color: var(--color-primary);
}

.count-pill {
  font-size: 0.7rem;
  padding: 1px 6px;
  border-radius: var(--radius-full);
  background: var(--bg-subtle);
  color: var(--text-secondary);
  font-weight: 700;
}

.count-pill.red {
  background: var(--color-armed-subtle);
  color: var(--color-armed-text);
}

.count-pill.blue {
  background: var(--color-primary-subtle);
  color: var(--color-primary);
}

/* Tab Content */
.tab-pane {
  padding: 0;
}

.table-responsive {
  overflow-x: auto;
}

.data-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.825rem;
  text-align: left;
}

.data-table th {
  padding: 12px 18px;
  background: var(--bg-subtle);
  color: var(--text-secondary);
  font-weight: 600;
  font-size: 0.72rem;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  border-bottom: 1px solid var(--border-subtle);
}

.data-table td {
  padding: 14px 18px;
  border-bottom: 1px solid var(--border-subtle);
  vertical-align: middle;
}

.data-table tr:last-child td {
  border-bottom: none;
}

.data-table tr:hover td {
  background: var(--bg-card-hover);
}

.alarm-row td {
  background: #fff8f8;
}

.alarm-row:hover td {
  background: #fee2e2;
}

.alarm-details-text {
  color: var(--color-armed-text);
  font-weight: 500;
}

.empty-cell {
  text-align: center;
  padding: 40px;
  color: var(--text-muted);
}

/* Thumbnail Cell */
.thumb-cell {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  padding: 3px 6px;
  border-radius: var(--radius-sm);
  background: #ffffff;
  border: 1px solid var(--border-subtle);
  transition: all 0.15s ease;
}

.thumb-cell:hover {
  border-color: var(--color-armed);
  box-shadow: var(--shadow-xs);
}

.log-thumbnail {
  width: 48px;
  height: 36px;
  object-fit: cover;
  border-radius: 4px;
}

.thumb-zoom-tag {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 0.7rem;
  font-weight: 600;
  color: var(--color-armed);
}

/* Gallery Grid */
.gallery-pane {
  padding: 20px;
}

.empty-gallery-box {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  padding: 48px 24px;
  gap: 10px;
  max-width: 460px;
  margin: 0 auto;
}

.empty-gallery-box h4 {
  font-size: 1rem;
  font-weight: 700;
  color: var(--text-main);
}

.empty-gallery-box p {
  font-size: 0.8rem;
  color: var(--text-secondary);
  line-height: 1.5;
}

.gallery-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(240px, 1fr));
  gap: 18px;
}

.gallery-card {
  border: 1px solid var(--border-subtle);
  border-radius: var(--radius-md);
  overflow: hidden;
  background: #ffffff;
  box-shadow: var(--shadow-card);
  cursor: pointer;
  transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
  display: flex;
  flex-direction: column;
}

.gallery-card:hover {
  transform: translateY(-2px);
  border-color: var(--color-armed-border);
  box-shadow: var(--shadow-card-hover);
}

.gallery-img-wrapper {
  position: relative;
  width: 100%;
  aspect-ratio: 4 / 3;
  background: #0f172a;
  overflow: hidden;
}

.gallery-img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  transition: transform 0.3s ease;
}

.gallery-card:hover .gallery-img {
  transform: scale(1.04);
}

.gallery-overlay {
  position: absolute;
  inset: 0;
  background: rgba(0, 0, 0, 0.35);
  display: flex;
  align-items: center;
  justify-content: center;
  opacity: 0;
  transition: opacity 0.2s ease;
}

.gallery-card:hover .gallery-overlay {
  opacity: 1;
}

.zoom-pill {
  display: flex;
  align-items: center;
  gap: 5px;
  background: rgba(255, 255, 255, 0.95);
  padding: 5px 12px;
  border-radius: var(--radius-full);
  font-size: 0.75rem;
  font-weight: 700;
  color: var(--text-main);
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2);
}

.mode-tag {
  position: absolute;
  top: 8px;
  left: 8px;
  background: rgba(239, 68, 68, 0.92);
  color: #ffffff;
  font-size: 0.65rem;
  font-weight: 800;
  padding: 2px 7px;
  border-radius: var(--radius-sm);
  letter-spacing: 0.04em;
}

.gallery-meta {
  padding: 10px 12px;
  display: flex;
  flex-direction: column;
  gap: 6px;
  background: #ffffff;
}

.gallery-time {
  font-size: 0.72rem;
  color: var(--text-secondary);
}

.gallery-footer-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.device-chip {
  font-size: 0.68rem;
  padding: 2px 6px;
  border-radius: var(--radius-sm);
  background: var(--bg-subtle);
  color: var(--text-secondary);
}

.download-btn {
  width: 26px;
  height: 26px;
  border-radius: var(--radius-sm);
  background: var(--color-primary-subtle);
  color: var(--color-primary);
  display: flex;
  align-items: center;
  justify-content: center;
  text-decoration: none;
  transition: background 0.15s ease;
}

.download-btn:hover {
  background: var(--color-primary);
  color: #ffffff;
}

/* Lightbox Modal */
.modal-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(15, 23, 42, 0.7);
  backdrop-filter: blur(8px);
  -webkit-backdrop-filter: blur(8px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  padding: 20px;
}

.lightbox-card {
  width: 100%;
  max-width: 740px;
  padding: 20px;
  display: flex;
  flex-direction: column;
  gap: 16px;
  box-shadow: var(--shadow-modal);
}

.lightbox-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
}

.lightbox-title {
  font-size: 1.05rem;
  font-weight: 700;
  color: var(--text-main);
  margin-top: 4px;
}

.btn-close {
  background: transparent;
  border: none;
  cursor: pointer;
  color: var(--text-muted);
  padding: 4px;
  border-radius: var(--radius-sm);
}

.btn-close:hover {
  color: var(--text-main);
  background: var(--bg-subtle);
}

.lightbox-body {
  width: 100%;
  max-height: 480px;
  background: #0f172a;
  border-radius: var(--radius-md);
  overflow: hidden;
  display: flex;
  align-items: center;
  justify-content: center;
}

.lightbox-image {
  width: 100%;
  height: auto;
  max-height: 480px;
  object-fit: contain;
}

.lightbox-footer {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 12px;
}

.lightbox-meta {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 0.78rem;
  color: var(--text-secondary);
}

.lightbox-actions {
  display: flex;
  gap: 8px;
}
</style>
