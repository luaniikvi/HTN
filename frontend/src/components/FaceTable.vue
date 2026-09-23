<template>
  <div class="glass-card table-card">
    <div class="table-header">
      <div class="title-group">
        <h3>DANH SÁCH KHUÔN MẶT ĐĂNG KÝ (NVS FLASH)</h3>
        <span class="count-badge font-mono">{{ faces.length }} bản ghi</span>
      </div>
      <button class="btn btn-secondary btn-sm" @click="fetchFaces" :disabled="isLoading">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="refresh-icon" :class="{ spinning: isLoading }">
          <polyline points="23 4 23 10 17 10"></polyline>
          <polyline points="1 20 1 14 7 14"></polyline>
          <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"></path>
        </svg>
        Làm mới
      </button>
    </div>

    <div class="table-wrapper">
      <table class="data-table">
        <thead>
          <tr>
            <th>FACE ID</th>
            <th>HỌ VÀ TÊN</th>
            <th>LOẠI QUYỀN</th>
            <th>HẠN HIỆU LỰC</th>
            <th>TRẠNG THÁI MCU</th>
            <th>THAO TÁC</th>
          </tr>
        </thead>
        <tbody>
          <tr v-if="faces.length === 0">
            <td colspan="6" class="empty-cell">Chưa có khuôn mặt nào được đăng ký trong hệ thống.</td>
          </tr>
          <tr v-for="face in faces" :key="face.id" :class="{ 'row-inactive': !face.is_active }">
            <td class="font-mono font-bold">#{{ face.face_id }}</td>
            <td class="name-cell">{{ face.name }}</td>
            <td>
              <span class="badge" :class="face.role_type === 'PERMANENT' ? 'badge-disarmed' : 'badge-stay'">
                {{ face.role_type }}
              </span>
            </td>
            <td class="font-mono time-cell">
              {{ face.role_type === 'PERMANENT' ? 'Vĩnh viễn' : formatTime(face.valid_until) }}
            </td>
            <td>
              <span class="badge" :class="face.is_active ? 'badge-online' : 'badge-offline'">
                {{ face.is_active ? 'ACTIVE' : 'DELETED' }}
              </span>
            </td>
            <td>
              <button
                v-if="face.is_active"
                class="btn btn-danger btn-xs"
                @click="confirmDelete(face)"
                :disabled="isDeletingId === face.face_id"
              >
                {{ isDeletingId === face.face_id ? 'Đang xóa...' : 'Xóa' }}
              </button>
              <span v-else class="text-muted text-xs">Đã xóa</span>
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import api from '../api/client';

const faces = ref([]);
const isLoading = ref(false);
const isDeletingId = ref(null);

defineExpose({ fetchFaces });

async function fetchFaces() {
  isLoading.value = true;
  try {
    const res = await api.get('/faces');
    if (res.data.success) {
      faces.value = res.data.faces;
    }
  } catch (err) {
    console.error('Failed to fetch faces:', err);
  } finally {
    isLoading.value = false;
  }
}

async function confirmDelete(face) {
  if (!confirm(`Bạn có chắc muốn xóa khuôn mặt "${face.name}" (ID #${face.face_id}) khỏi Flash NVS trên MCU?`)) {
    return;
  }

  isDeletingId.value = face.face_id;
  try {
    await api.delete(`/faces/${face.face_id}`);
    alert(`Lệnh xóa đã được gửi xuống ESP32. Cơ sở dữ liệu sẽ cập nhật trạng thái khi MCU gửi ACK.`);
    setTimeout(fetchFaces, 1500);
  } catch (err) {
    alert('Lỗi xóa khuôn mặt: ' + (err.response?.data?.message || err.message));
  } finally {
    isDeletingId.value = null;
  }
}

function formatTime(iso) {
  if (!iso) return 'N/A';
  const d = new Date(iso);
  return d.toLocaleString('vi-VN');
}

onMounted(() => {
  fetchFaces();
});
</script>

<style scoped>
.table-card {
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.table-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.title-group {
  display: flex;
  align-items: center;
  gap: 10px;
}

.title-group h3 {
  font-size: 0.95rem;
  font-weight: 700;
  letter-spacing: 0.05em;
}

.count-badge {
  font-size: 0.75rem;
  color: var(--color-primary);
  background: rgba(59, 130, 246, 0.15);
  padding: 2px 8px;
  border-radius: 4px;
}

.table-wrapper {
  overflow-x: auto;
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
  padding: 12px;
  border-bottom: 1px solid rgba(75, 85, 99, 0.2);
}

.data-table tr:hover td {
  background: rgba(31, 41, 55, 0.3);
}

.row-inactive td {
  opacity: 0.5;
}

.name-cell {
  font-weight: 600;
  color: #f3f4f6;
}

.time-cell {
  font-size: 0.75rem;
  color: #9ca3af;
}

.empty-cell {
  text-align: center;
  padding: 30px;
  color: var(--text-muted);
}

.refresh-icon {
  width: 14px;
  height: 14px;
}

.spinning {
  animation: spin 0.8s linear infinite;
}

.btn-xs {
  padding: 4px 10px;
  font-size: 0.75rem;
}

.text-xs {
  font-size: 0.75rem;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}
</style>
