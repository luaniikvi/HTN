<template>
  <div class="app-shell">
    <!-- Top Sticky Navigation Bar -->
    <header class="navbar">
      <div class="nav-container">
        <!-- Navigation Tabs -->
        <nav class="nav-tabs">
          <router-link to="/security" class="nav-tab" active-class="active">
            <Lock :size="18" stroke-width="2" />
            <span>Security</span>
          </router-link>
          <router-link to="/monitor" class="nav-tab" active-class="active">
            <Video :size="18" stroke-width="2" />
            <span>Live Monitor</span>
          </router-link>
          <router-link to="/faces" class="nav-tab" active-class="active">
            <Users :size="18" stroke-width="2" />
            <span>Face ID</span>
          </router-link>
          <router-link to="/logs" class="nav-tab" active-class="active">
            <FileText :size="18" stroke-width="2" />
            <span>Audit & Logs</span>
          </router-link>
          <router-link to="/settings" class="nav-tab" active-class="active">
            <Sliders :size="18" stroke-width="2" />
            <span>System</span>
          </router-link>
        </nav>

        <!-- Status & User Actions -->
        <div class="nav-meta">
          <!-- Device Status Pill -->
          <div class="device-pill font-mono">
            <span class="status-dot" :class="system.status === 'ONLINE' ? 'online' : 'offline'"></span>
            <span>{{ system.deviceId }}</span>
            <span class="pill-divider">•</span>
            <span :class="system.status === 'ONLINE' ? 'text-online' : 'text-offline'">
              {{ system.status }}
            </span>
          </div>

          <!-- Security Mode Chip -->
          <div class="mode-chip" :class="modeBadgeClass">
            <MorphIcon :icon="modeIcon" :size="14" spring="snappy" />
            <span>{{ system.securityMode }}</span>
          </div>

          <!-- User Menu -->
          <div class="user-block">
            <span class="user-name">
              {{ auth.user?.fullname || auth.user?.username || 'Operator' }}
            </span>
            <button class="btn btn-secondary btn-xs logout-btn" @click="handleLogout" title="Sign out">
              <LogOut :size="13" />
              <span>Logout</span>
            </button>
          </div>
        </div>
      </div>
    </header>

    <!-- Global Breach Alert Sticky Strip (if active) -->
    <transition name="slide-banner">
      <div v-if="system.activeBreachAlert" class="breach-strip">
        <div class="breach-strip-content">
          <div class="breach-strip-left">
            <AlertTriangle :size="20" class="strip-icon" />
            <div>
              <strong>SECURITY BREACH DETECTED:</strong>
              <span>
                Unauthorized door opening in <strong>{{ system.activeBreachAlert.mode }}</strong> mode!
              </span>
            </div>
          </div>
          <div class="breach-strip-actions">
            <button class="btn btn-danger btn-xs" @click="silenceAlarm">
              Silence Siren
            </button>
            <button class="btn btn-secondary btn-xs" @click="disarmSystem">
              Disarm System
            </button>
          </div>
        </div>
      </div>
    </transition>

    <!-- Main Content Area with GSAP Page View -->
    <main class="page-container">
      <router-view v-slot="{ Component }">
        <transition @before-enter="beforePageEnter" @enter="onPageEnter" @leave="onPageLeave" :css="false" mode="out-in">
          <component :is="Component" />
        </transition>
      </router-view>
    </main>

    <!-- Global Breach Alert Full Modal -->
    <BreachAlertModal />
  </div>
</template>

<script setup>
import { computed, onMounted } from 'vue';
import { useRouter } from 'vue-router';
import { useAuthStore } from '../../stores/auth';
import { useSystemStore } from '../../stores/system';
import gsap from 'gsap';
import { MorphIcon } from 'morphicons/vue';
import {
  ShieldCheck,
  Home,
  ShieldAlert
} from 'lucide';
import {
  Shield,
  Video,
  Lock,
  Users,
  FileText,
  Sliders,
  LogOut,
  AlertTriangle
} from 'lucide-vue-next';

import BreachAlertModal from '../BreachAlertModal.vue';

const auth = useAuthStore();
const system = useSystemStore();
const router = useRouter();

const modeBadgeClass = computed(() => {
  if (system.securityMode === 'ARMED') return 'chip-armed';
  if (system.securityMode === 'STAY') return 'chip-stay';
  return 'chip-disarmed';
});

const modeIcon = computed(() => {
  if (system.securityMode === 'ARMED') return ShieldAlert;
  if (system.securityMode === 'STAY') return Home;
  return ShieldCheck;
});

function handleLogout() {
  auth.logout();
  router.push('/login');
}

async function silenceAlarm() {
  try {
    await system.toggleAlarm(false);
  } catch (err) {
    console.error('Failed to silence siren:', err);
  }
}

async function disarmSystem() {
  try {
    await system.changeMode('DISARMED');
  } catch (err) {
    console.error('Failed to disarm system:', err);
  }
}

// GSAP Page Transition
function beforePageEnter(el) {
  gsap.set(el, { opacity: 0, y: 10 });
}

function onPageEnter(el, done) {
  gsap.to(el, {
    opacity: 1,
    y: 0,
    duration: 0.25,
    ease: 'power2.out',
    onComplete: done
  });
}

function onPageLeave(el, done) {
  gsap.to(el, {
    opacity: 0,
    y: -8,
    duration: 0.15,
    ease: 'power2.in',
    onComplete: done
  });
}

onMounted(() => {
  system.fetchSystemState();
  system.initWebSocket();
});
</script>

<style scoped>
.app-shell {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  background-color: var(--bg-body);
}

.navbar {
  position: sticky;
  top: 0;
  z-index: 100;
  background: rgba(255, 255, 255, 0.92);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
  border-bottom: 1px solid var(--border-subtle);
  box-shadow: 0 1px 2px 0 rgba(0, 0, 0, 0.03);
}

.nav-container {
  max-width: 1440px;
  margin: 0 auto;
  padding: 0 24px;
  height: 68px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
}

/* Nav Tabs */
.nav-tabs {
  display: flex;
  align-items: center;
  gap: 6px;
  background: var(--bg-subtle);
  padding: 5px;
  border-radius: var(--radius-lg);
  border: 1px solid var(--border-subtle);
}

.nav-tab {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 18px;
  border-radius: var(--radius-md);
  font-size: 0.95rem;
  font-weight: 600;
  color: var(--text-secondary);
  text-decoration: none;
  transition: all 0.15s ease;
  white-space: nowrap;
}

.nav-tab:hover {
  color: var(--text-main);
}

.nav-tab.active {
  background: #ffffff;
  color: var(--color-primary);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.08);
}

/* Meta / Right Section */
.nav-meta {
  display: flex;
  align-items: center;
  gap: 14px;
  flex-shrink: 0;
}

.device-pill {
  display: flex;
  align-items: center;
  gap: 7px;
  padding: 6px 12px;
  background: #ffffff;
  border: 1px solid var(--border-subtle);
  border-radius: var(--radius-full);
  font-size: 0.825rem;
  font-weight: 600;
  color: var(--text-secondary);
}

.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
}

.status-dot.online {
  background: var(--color-online);
  box-shadow: 0 0 6px var(--color-online);
}

.status-dot.offline {
  background: var(--color-offline);
}

.text-online {
  color: var(--color-online);
  font-weight: 700;
}

.text-offline {
  color: var(--text-muted);
}

.pill-divider {
  color: var(--border-strong);
}

.mode-chip {
  display: inline-flex;
  align-items: center;
  gap: 7px;
  padding: 6px 14px;
  border-radius: var(--radius-full);
  font-size: 0.8rem;
  font-weight: 700;
  letter-spacing: 0.04em;
  text-transform: uppercase;
}

.chip-disarmed {
  background: var(--color-disarmed-subtle);
  color: var(--color-disarmed-text);
  border: 1px solid var(--color-disarmed-border);
}

.chip-stay {
  background: var(--color-stay-subtle);
  color: var(--color-stay-text);
  border: 1px solid var(--color-stay-border);
}

.chip-armed {
  background: var(--color-armed-subtle);
  color: var(--color-armed-text);
  border: 1px solid var(--color-armed-border);
}

.user-block {
  display: flex;
  align-items: center;
  gap: 10px;
  padding-left: 10px;
  border-left: 1px solid var(--border-subtle);
}

.user-name {
  font-size: 0.9rem;
  font-weight: 600;
  color: var(--text-main);
  max-width: 140px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.logout-btn {
  padding: 5px 10px;
  font-size: 0.825rem;
  color: var(--text-secondary);
}

/* Breach Sticky Strip */
.breach-strip {
  background: #fef2f2;
  border-bottom: 1px solid #fecaca;
  padding: 8px 16px;
}

.breach-strip-content {
  max-width: 1440px;
  margin: 0 auto;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  flex-wrap: wrap;
}

.breach-strip-left {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 0.85rem;
  color: #991b1b;
}

.strip-icon {
  color: #ef4444;
  animation: pulse-icon 1s infinite alternate;
}

@keyframes pulse-icon {
  from { transform: scale(1); }
  to { transform: scale(1.15); }
}

.breach-strip-actions {
  display: flex;
  gap: 8px;
}

/* Page Container */
.page-container {
  flex: 1;
  max-width: 1440px;
  width: 100%;
  margin: 0 auto;
  padding: 24px;
}

@media (max-width: 900px) {
  .nav-tabs {
    order: 3;
    width: 100%;
    overflow-x: auto;
    padding: 4px;
  }

  .nav-container {
    height: auto;
    padding: 12px 16px;
    flex-wrap: wrap;
  }
}
</style>
