<template>
  <div class="dashboard-layout">
    <!-- Top Navigation Header -->
    <header class="glass-card nav-header">
      <div class="nav-brand">
        <div class="brand-icon">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"></path>
          </svg>
        </div>
        <div>
          <h2>ESP32-S3 EDGE AI SECURITY</h2>
          <span class="brand-sub">Hệ thống Giám sát & Quản lý Khóa Cửa Cục bộ Thông minh</span>
        </div>
      </div>

      <!-- Center Status Indicators -->
      <div class="nav-status font-mono">
        <div class="status-indicator">
          <span class="dot" :class="system.status === 'ONLINE' ? 'dot-online' : 'dot-offline'"></span>
          <span>Thiết bị: <strong>{{ system.deviceId }} ({{ system.status }})</strong></span>
        </div>

        <div class="status-indicator">
          <span>Chế độ hiện tại: </span>
          <span
            class="badge"
            :class="system.securityMode === 'ARMED' ? 'badge-armed' : (system.securityMode === 'STAY' ? 'badge-stay' : 'badge-disarmed')"
          >
            {{ system.securityMode }}
          </span>
        </div>
      </div>

      <!-- Right User Menu -->
      <div class="nav-user">
        <span class="user-greeting">
          Xin chào, <strong>{{ auth.user?.fullname || auth.user?.username || 'Admin' }}</strong>
        </span>
        <button class="btn btn-secondary btn-sm" @click="handleLogout">
          Đăng xuất
        </button>
      </div>
    </header>

    <!-- Main Dashboard Grid -->
    <main class="dashboard-main">
      <!-- Left Column: Video Monitor & Controls -->
      <div class="dashboard-col col-left">
        <LiveMonitor />
        <QuickControlPanel @open-enroll="isEnrollModalOpen = true" />
      </div>

      <!-- Right Column: Face Management & Audit Logs -->
      <div class="dashboard-col col-right">
        <FaceTable ref="faceTableRef" />
        <AuditLogs />
      </div>
    </main>

    <!-- Modals -->
    <EnrollmentWizard
      v-if="isEnrollModalOpen"
      @close="isEnrollModalOpen = false"
      @face-saved="handleFaceSaved"
    />

    <BreachAlertModal />
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useRouter } from 'vue-router';
import { useAuthStore } from '../stores/auth';
import { useSystemStore } from '../stores/system';

import LiveMonitor from '../components/LiveMonitor.vue';
import QuickControlPanel from '../components/QuickControlPanel.vue';
import FaceTable from '../components/FaceTable.vue';
import AuditLogs from '../components/AuditLogs.vue';
import EnrollmentWizard from '../components/EnrollmentWizard.vue';
import BreachAlertModal from '../components/BreachAlertModal.vue';

const auth = useAuthStore();
const system = useSystemStore();
const router = useRouter();

const isEnrollModalOpen = ref(false);
const faceTableRef = ref(null);

function handleLogout() {
  auth.logout();
  router.push('/login');
}

function handleFaceSaved() {
  if (faceTableRef.value) {
    faceTableRef.value.fetchFaces();
  }
}

onMounted(() => {
  system.fetchSystemState();
  system.initWebSocket();
});
</script>

<style scoped>
.dashboard-layout {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  padding: 16px 20px;
  gap: 16px;
  max-width: 1600px;
  margin: 0 auto;
}

.nav-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 14px 22px;
  flex-wrap: wrap;
  gap: 14px;
}

.nav-brand {
  display: flex;
  align-items: center;
  gap: 12px;
}

.brand-icon {
  width: 40px;
  height: 40px;
  border-radius: 10px;
  background: rgba(59, 130, 246, 0.15);
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--color-primary);
}

.brand-icon svg {
  width: 24px;
  height: 24px;
}

.nav-brand h2 {
  font-size: 1.1rem;
  font-weight: 800;
  letter-spacing: 0.05em;
}

.brand-sub {
  font-size: 0.7rem;
  color: var(--text-muted);
}

.nav-status {
  display: flex;
  align-items: center;
  gap: 18px;
  font-size: 0.8rem;
}

.status-indicator {
  display: flex;
  align-items: center;
  gap: 6px;
}

.dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
}

.dot-online {
  background: #10b981;
  box-shadow: 0 0 8px #10b981;
}

.dot-offline {
  background: #6b7280;
}

.nav-user {
  display: flex;
  align-items: center;
  gap: 12px;
  font-size: 0.85rem;
}

.user-greeting {
  color: #e5e7eb;
}

/* Main Grid */
.dashboard-main {
  display: grid;
  grid-template-columns: 1.1fr 1fr;
  gap: 16px;
  flex: 1;
}

.dashboard-col {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

@media (max-width: 1100px) {
  .dashboard-main {
    grid-template-columns: 1fr;
  }
}
</style>
