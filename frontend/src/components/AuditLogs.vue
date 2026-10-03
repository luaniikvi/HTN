<template>
  <div class="glass-card logs-card">
    <div class="logs-header">
      <div class="tab-buttons">
        <button
          class="tab-btn"
          :class="{ active: activeTab === 'access' }"
          @click="activeTab = 'access'"
        >
          NHẬT KÝ RA VÀO
        </button>
        <button
          class="tab-btn"
          :class="{ active: activeTab === 'alarm' }"
          @click="activeTab = 'alarm'"
        >
          CẢNH BÁO VI PHẠM
          <span v-if="alarmLogs.length > 0" class="badge-dot"></span>
        </button>
        <button
          class="tab-btn tab-btn-gallery"
          :class="{ active: activeTab === 'gallery' }"
          @click="activeTab = 'gallery'"
        >
          📸 THƯ VIỆN ẢNH VI PHẠM
          <span v-if="breachImages.length > 0" class="gallery-count-badge">{{ breachImages.length }}</span>
        </button>
      </div>

      <button class="btn btn-secondary btn-sm" @click="fetchCurrentTabLogs" :disabled="isLoading">
        Làm mới
      </button>
    </div>

    <!-- 1. Access Logs Table -->
    <div v-if="activeTab === 'access'" class="logs-content">
      <table class="data-table">
        <thead>
          <tr>
            <th>THỜI GIAN</th>
            <th>THIẾT BỊ</th>
            <th>SỰ KIỆN</th>
            <th>CHI TIẾT</th>
          </tr>
        </thead>
        <tbody>
          <tr v-if="accessLogs.length === 0">
            <td colspan="4" class="empty-cell">Chưa có nhật ký ra vào.</td>
          </tr>
          <tr v-for="log in accessLogs" :key="log.id">
            <td class="font-mono time-col">{{ formatTime(log.timestamp) }}</td>
            <td class="font-mono text-muted">{{ log.device_id }}</td>
            <td>
              <span
                class="badge"
                :class="log.event_type === 'DOOR_OPENED' ? 'badge-stay' : (log.event_type === 'FACE_AUTH_SUCCESS' ? 'badge-disarmed' : 'badge-offline')"
              >
                {{ log.event_type }}
              </span>
            </td>
            <td>{{ log.details }}</td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- 2. Alarm Logs Table -->
    <div v-else-if="activeTab === 'alarm'" class="logs-content">
      <table class="data-table">
        <thead>
          <tr>
            <th>THỜI GIAN</th>
            <th>CHẾ ĐỘ AN NINH</th>
            <th>LOẠI VI PHẠM</th>
            <th>HÌNH ẢNH VI PHẠM</th>
            <th>CHI TIẾT</th>
          </tr>
        </thead>
        <tbody>
          <tr v-if="alarmLogs.length === 0">
            <td colspan="5" class="empty-cell">Không có cảnh báo vi phạm an ninh.</td>
          </tr>
          <tr v-for="log in alarmLogs" :key="log.id" class="alarm-row">
            <td class="font-mono time-col">{{ formatTime(log.timestamp) }}</td>
            <td>
              <span class="badge" :class="log.mode === 'ARMED' ? 'badge-armed' : 'badge-stay'">
                {{ log.mode }}
              </span>
            </td>
            <td>
              <span class="badge badge-armed font-bold">{{ log.event }}</span>
            </td>
            <td>
              <div v-if="log.image_url" class="image-cell" @click="openImageLightbox(log)">
                <img :src="log.image_url" alt="Bằng chứng" class="table-thumbnail" />
                <span class="zoom-badge">🔍 Xem ảnh</span>
              </div>
              <span v-else class="text-muted text-xs">—</span>
            </td>
            <td class="alarm-details">{{ log.details }}</td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- 3. Snapshot Gallery Grid (Thư viện hình ảnh vi phạm trực quan) -->
    <div v-else-if="activeTab === 'gallery'" class="logs-content gallery-content">
      <div v-if="breachImages.length === 0" class="empty-gallery">
        <div class="empty-gallery-icon">📸</div>
        <h4>Chưa có hình ảnh vi phạm nào được ghi nhận</h4>
        <p>Khi hệ thống phát hiện vi phạm mở cửa ở chế độ ARMED, camera ESP32 sẽ tự động chụp ảnh bằng chứng và lưu trữ tại đây.</p>
      </div>

      <div v-else class="gallery-grid">
        <div
          v-for="item in breachImages"
          :key="item.id"
          class="gallery-card"
          @click="openImageLightbox(item)"
        >
          <div class="gallery-card-img-wrap">
            <img :src="item.image_url" alt="Ảnh vi phạm đột nhập" class="gallery-card-img" />
            <div class="gallery-card-overlay">
              <span class="overlay-text">🔍 Click để phóng to</span>
            </div>
            <span class="gallery-badge-armed">🚨 {{ item.mode }} VI PHẠM</span>
          </div>

          <div class="gallery-card-info">
            <div class="gallery-card-time font-mono">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="icon-small">
                <circle cx="12" cy="12" r="10"></circle>
                <polyline points="12 6 12 12 16 14"></polyline>
              </svg>
              {{ formatTime(item.timestamp) }}
            </div>
            <div class="gallery-card-actions">
              <span class="device-tag font-mono">{{ item.device_id }}</span>
              <a
                :href="item.image_url"
                target="_blank"
                download
                class="btn-download-icon"
                title="Tải ảnh về máy"
                @click.stop
              >
                ⬇
              </a>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- Image Lightbox Modal xem ảnh chất lượng cao -->
    <div v-if="selectedLog" class="lightbox-backdrop" @click="selectedLog = null">
      <div class="lightbox-modal glass-card" @click.stop>
        <div class="lightbox-header">
          <div>
            <span class="badge badge-armed">HÌNH ẢNH BẰNG CHỨNG VI PHẠM (SVGA COLOR)</span>
            <h3 class="lightbox-title">Thời điểm: {{ formatTime(selectedLog.timestamp) }}</h3>
          </div>
          <button class="btn-close" @click="selectedLog = null">✕</button>
        </div>

        <div class="lightbox-body">
          <img :src="selectedLog.image_url" alt="Bằng chứng vi phạm chất lượng cao" class="lightbox-image" />
        </div>

        <div class="lightbox-footer">
          <div class="lightbox-info font-mono">
            <span>Thiết bị: <strong>{{ selectedLog.device_id }}</strong></span>
            <span>Chế độ: <strong>{{ selectedLog.mode }}</strong></span>
          </div>
          <div class="lightbox-btns">
            <a :href="selectedLog.image_url" target="_blank" download class="btn btn-secondary btn-sm">
              ⬇ Tải ảnh về
            </a>
            <a :href="selectedLog.image_url" target="_blank" class="btn btn-primary btn-sm">
              Mở tab mới ↗
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

const activeTab = ref('gallery'); // Mặc định mở tab Thư viện ảnh hoặc Access
const accessLogs = ref([]);
const alarmLogs = ref([]);
const isLoading = ref(false);
const selectedLog = ref(null);

// Lọc toàn bộ danh sách các bản ghi có hình ảnh bằng chứng vi phạm
const breachImages = computed(() => {
  return alarmLogs.value.filter(log => Boolean(log.image_url));
});

function openImageLightbox(log) {
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
  return d.toLocaleString('vi-VN');
}

watch(activeTab, (tab) => {
  if (tab === 'access' && accessLogs.value.length === 0) fetchAccessLogs();
  if ((tab === 'alarm' || tab === 'gallery') && alarmLogs.value.length === 0) fetchAlarmLogs();
});

onMounted(() => {
  fetchAlarmLogs(); // Tải ngay danh sách ảnh vi phạm để hiển thị số lượng trên tab
  fetchAccessLogs();
});
</script>

<style scoped>
.logs-card {
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.logs-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  border-bottom: 1px solid var(--border-color);
  padding-bottom: 10px;
}

.tab-buttons {
  display: flex;
  gap: 12px;
}

.tab-btn {
  background: transparent;
  border: none;
  font-size: 0.85rem;
  font-weight: 700;
  color: var(--text-muted);
  cursor: pointer;
  padding: 6px 4px;
  position: relative;
  letter-spacing: 0.03em;
  transition: color 0.2s;
}

.tab-btn:hover {
  color: #f3f4f6;
}

.tab-btn.active {
  color: var(--color-primary);
}

.tab-btn.active::after {
  content: '';
  position: absolute;
  bottom: -11px;
  left: 0;
  right: 0;
  height: 2px;
  background: var(--color-primary);
}

.badge-dot {
  display: inline-block;
  width: 6px;
  height: 6px;
  background: #ef4444;
  border-radius: 50%;
  margin-left: 4px;
}

.logs-content {
  overflow-x: auto;
  max-height: 400px;
}

.data-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.85rem;
  text-align: left;
}

.data-table th {
  padding: 10px 12px;
  background: rgba(31, 41, 55, 0.4);
  color: var(--text-muted);
  font-weight: 600;
  border-bottom: 1px solid var(--border-color);
  font-size: 0.75rem;
  letter-spacing: 0.05em;
}

.data-table td {
  padding: 10px 12px;
  border-bottom: 1px solid rgba(75, 85, 99, 0.2);
}

.time-col {
  font-size: 0.75rem;
  color: #9ca3af;
  white-space: nowrap;
}

.alarm-row td {
  background: rgba(239, 68, 68, 0.05);
}

.alarm-details {
  color: #fca5a5;
  font-size: 0.8rem;
}

.empty-cell {
  text-align: center;
  padding: 30px;
  color: var(--text-muted);
}

/* Thumbnail ảnh trong bảng */
.image-cell {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  padding: 4px 6px;
  border-radius: 6px;
  background: rgba(0, 0, 0, 0.3);
  border: 1px solid rgba(239, 68, 68, 0.3);
  transition: all 0.2s ease;
}

.image-cell:hover {
  background: rgba(239, 68, 68, 0.15);
  border-color: #ef4444;
  transform: translateY(-1px);
}

.table-thumbnail {
  width: 52px;
  height: 38px;
  object-fit: cover;
  border-radius: 4px;
  border: 1px solid rgba(255, 255, 255, 0.15);
}

.zoom-badge {
  font-size: 0.75rem;
  color: #fca5a5;
  font-weight: 600;
  white-space: nowrap;
}

/* Lightbox Modal */
.lightbox-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.88);
  backdrop-filter: blur(8px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 2500;
  padding: 24px;
}

.lightbox-modal {
  width: 100%;
  max-width: 760px;
  background: #14161d;
  border: 2px solid rgba(239, 68, 68, 0.6);
  border-radius: 12px;
  padding: 20px;
  display: flex;
  flex-direction: column;
  gap: 16px;
  box-shadow: 0 20px 40px rgba(0, 0, 0, 0.6);
}

.lightbox-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  border-bottom: 1px solid var(--border-color);
  padding-bottom: 12px;
}

.lightbox-title {
  font-size: 1rem;
  font-weight: 700;
  color: #f3f4f6;
  margin-top: 4px;
}

.btn-close {
  background: transparent;
  border: none;
  font-size: 1.25rem;
  color: #9ca3af;
  cursor: pointer;
  padding: 4px 8px;
  border-radius: 4px;
  transition: all 0.2s;
}

.btn-close:hover {
  color: #ffffff;
  background: rgba(255, 255, 255, 0.1);
}

.lightbox-body {
  width: 100%;
  max-height: 480px;
  background: #000;
  border-radius: 8px;
  overflow: hidden;
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid rgba(255, 255, 255, 0.1);
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
  padding-top: 10px;
  border-top: 1px solid rgba(75, 85, 99, 0.2);
}

.lightbox-info {
  display: flex;
  gap: 16px;
  font-size: 0.8rem;
  color: #9ca3af;
}

.lightbox-info strong {
  color: #e5e7eb;
}

.lightbox-btns {
  display: flex;
  gap: 10px;
}

/* =========================================================================
   SNAPSHOT GALLERY GRID STYLES
   ========================================================================= */
.tab-btn-gallery {
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

.gallery-count-badge {
  background: #ef4444;
  color: #fff;
  font-size: 0.7rem;
  padding: 1px 6px;
  border-radius: 10px;
  font-weight: 800;
}

.gallery-content {
  padding: 8px 4px;
}

.gallery-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
  gap: 16px;
}

.gallery-card {
  background: rgba(17, 24, 39, 0.6);
  border: 1px solid rgba(239, 68, 68, 0.3);
  border-radius: 10px;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  cursor: pointer;
  transition: all 0.25s ease;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
}

.gallery-card:hover {
  transform: translateY(-3px);
  border-color: #ef4444;
  box-shadow: 0 8px 20px rgba(239, 68, 68, 0.25);
}

.gallery-card-img-wrap {
  position: relative;
  width: 100%;
  height: 150px;
  background: #000;
  overflow: hidden;
}

.gallery-card-img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  transition: transform 0.3s ease;
}

.gallery-card:hover .gallery-card-img {
  transform: scale(1.05);
}

.gallery-card-overlay {
  position: absolute;
  inset: 0;
  background: rgba(0, 0, 0, 0.45);
  display: flex;
  align-items: center;
  justify-content: center;
  opacity: 0;
  transition: opacity 0.2s ease;
}

.gallery-card:hover .gallery-card-overlay {
  opacity: 1;
}

.overlay-text {
  font-size: 0.8rem;
  font-weight: 700;
  color: #fff;
  background: rgba(0, 0, 0, 0.7);
  padding: 4px 10px;
  border-radius: 20px;
  border: 1px solid rgba(255, 255, 255, 0.2);
}

.gallery-badge-armed {
  position: absolute;
  top: 8px;
  left: 8px;
  background: rgba(239, 68, 68, 0.9);
  color: #fff;
  font-size: 0.65rem;
  font-weight: 800;
  padding: 2px 7px;
  border-radius: 4px;
  letter-spacing: 0.05em;
  backdrop-filter: blur(4px);
}

.gallery-card-info {
  padding: 10px 12px;
  display: flex;
  flex-direction: column;
  gap: 8px;
  background: rgba(31, 41, 55, 0.3);
}

.gallery-card-time {
  font-size: 0.75rem;
  color: #d1d5db;
  display: flex;
  align-items: center;
  gap: 6px;
}

.icon-small {
  width: 14px;
  height: 14px;
  color: #9ca3af;
}

.gallery-card-actions {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.device-tag {
  font-size: 0.7rem;
  color: #9ca3af;
  background: rgba(0, 0, 0, 0.4);
  padding: 2px 6px;
  border-radius: 4px;
}

.btn-download-icon {
  width: 26px;
  height: 26px;
  border-radius: 6px;
  background: rgba(59, 130, 246, 0.2);
  color: #60a5fa;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  text-decoration: none;
  font-size: 0.8rem;
  font-weight: bold;
  transition: all 0.2s;
}

.btn-download-icon:hover {
  background: #3b82f6;
  color: #fff;
}

.empty-gallery {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 50px 20px;
  text-align: center;
  gap: 12px;
}

.empty-gallery-icon {
  font-size: 3rem;
  opacity: 0.8;
}

.empty-gallery h4 {
  font-size: 1rem;
  font-weight: 700;
  color: #f3f4f6;
}

.empty-gallery p {
  font-size: 0.85rem;
  color: var(--text-muted);
  max-width: 440px;
  line-height: 1.5;
}
</style>
