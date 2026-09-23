<template>
  <div class="glass-card logs-card">
    <div class="logs-header">
      <div class="tab-buttons">
        <button
          class="tab-btn"
          :class="{ active: activeTab === 'access' }"
          @click="activeTab = 'access'"
        >
          NHẬT KÝ RA VÀO (ACCESS LOGS)
        </button>
        <button
          class="tab-btn"
          :class="{ active: activeTab === 'alarm' }"
          @click="activeTab = 'alarm'"
        >
          CẢNH BÁO VI PHẠM (ALARM LOGS)
          <span v-if="alarmLogs.length > 0" class="badge-dot"></span>
        </button>
      </div>

      <button class="btn btn-secondary btn-sm" @click="fetchCurrentTabLogs" :disabled="isLoading">
        Làm mới
      </button>
    </div>

    <!-- Access Logs Table -->
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

    <!-- Alarm Logs Table -->
    <div v-else class="logs-content">
      <table class="data-table">
        <thead>
          <tr>
            <th>THỜI GIAN</th>
            <th>CHẾ ĐỘ AN NINH</th>
            <th>LOẠI VI PHẠM</th>
            <th>CHI TIẾT</th>
          </tr>
        </thead>
        <tbody>
          <tr v-if="alarmLogs.length === 0">
            <td colspan="4" class="empty-cell">Không có cảnh báo vi phạm an ninh.</td>
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
            <td class="alarm-details">{{ log.details }}</td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, onMounted } from 'vue';
import api from '../api/client';

const activeTab = ref('access');
const accessLogs = ref([]);
const alarmLogs = ref([]);
const isLoading = ref(false);

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
  if (tab === 'alarm' && alarmLogs.value.length === 0) fetchAlarmLogs();
});

onMounted(() => {
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
</style>
