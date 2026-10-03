<template>
  <div class="settings-page">
    <!-- Header -->
    <div class="page-title-row">
      <div>
        <h1 class="page-title">System & Telemetry</h1>
        <p class="page-subtitle">ESP32-S3 hardware specifications, broker connectivity, and edge runtime parameters</p>
      </div>
    </div>

    <div class="settings-grid">
      <!-- 1. Edge Hardware & Microcontroller -->
      <div class="clean-card spec-card">
        <div class="card-head">
          <div class="icon-wrap blue">
            <Cpu :size="20" />
          </div>
          <div>
            <h3 class="card-title">Edge Microcontroller (MCU)</h3>
            <span class="card-sub">ESP32-S3 N16R8 Dual-Core Xtensa LX7</span>
          </div>
        </div>

        <div class="spec-list font-mono">
          <div class="spec-row">
            <span class="spec-label">Device Identifier</span>
            <span class="spec-val font-bold">{{ system.deviceId }}</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">System State</span>
            <span class="badge" :class="system.status === 'ONLINE' ? 'badge-online' : 'badge-offline'">
              {{ system.status }}
            </span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Memory Architecture</span>
            <span class="spec-val">16MB Flash / 8MB Octal PSRAM</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">FreeRTOS Core 0</span>
            <span class="spec-val">Wi-Fi, MQTT, WebSocket, I/O Tasks</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">FreeRTOS Core 1</span>
            <span class="spec-val">OV5640 DMA & ESP-WHO Vector AI</span>
          </div>
        </div>
      </div>

      <!-- 2. Dual-Channel Communication Gateway -->
      <div class="clean-card spec-card">
        <div class="card-head">
          <div class="icon-wrap emerald">
            <Network :size="20" />
          </div>
          <div>
            <h3 class="card-title">Dual-Channel Gateway</h3>
            <span class="card-sub">MQTT Control Plane & WebSocket Stream</span>
          </div>
        </div>

        <div class="spec-list font-mono">
          <div class="spec-row">
            <span class="spec-label">MQTT Broker</span>
            <span class="spec-val">Eclipse Mosquitto (QoS 1)</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Events WebSocket</span>
            <span class="badge" :class="system.wsConnected ? 'badge-disarmed' : 'badge-offline'">
              {{ system.wsConnected ? 'CONNECTED' : 'RECONNECTING' }}
            </span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Binary Video Channel</span>
            <span class="spec-val">Direct ArrayBuffer over WS</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">MQTT Status Topic</span>
            <span class="spec-val text-xs">device/{{ system.deviceId }}/status</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">MQTT Events Topic</span>
            <span class="spec-val text-xs">device/{{ system.deviceId }}/events/#</span>
          </div>
        </div>
      </div>

      <!-- 3. Storage & Partition Topology -->
      <div class="clean-card spec-card">
        <div class="card-head">
          <div class="icon-wrap purple">
            <HardDrive :size="20" />
          </div>
          <div>
            <h3 class="card-title">Flash NVS & OTA Partitioning</h3>
            <span class="card-sub">Non-Volatile Storage and asymmetric failover</span>
          </div>
        </div>

        <div class="spec-list">
          <div class="spec-row">
            <span class="spec-label font-mono">Biometric Storage</span>
            <span class="spec-val font-mono">NVS Flash Partition (Offline-capable)</span>
          </div>
          <div class="spec-row">
            <span class="spec-label font-mono">OTA Partitions</span>
            <span class="spec-val font-mono">ota_0 (3.5MB) / ota_1 (3.5MB)</span>
          </div>
          <div class="spec-row">
            <span class="spec-label font-mono">Offline Fallback</span>
            <span class="badge badge-disarmed">Enabled</span>
          </div>
          <p class="nvs-note">
            The lock controller performs edge biometric matching entirely on-device using NVS Flash templates, ensuring uninterrupted access even during network outages.
          </p>
        </div>
      </div>

      <!-- 4. Security & Privacy Policy -->
      <div class="clean-card spec-card">
        <div class="card-head">
          <div class="icon-wrap red">
            <ShieldCheck :size="20" />
          </div>
          <div>
            <h3 class="card-title">Security & Privacy Protocol</h3>
            <span class="card-sub">Zero-disk frame policy and cryptographic credentials</span>
          </div>
        </div>

        <div class="spec-list">
          <div class="spec-row">
            <span class="spec-label font-mono">Live Video Retention</span>
            <span class="badge badge-disarmed">Zero Disk Footprint</span>
          </div>
          <div class="spec-row">
            <span class="spec-label font-mono">Web Authentication</span>
            <span class="spec-val font-mono">JWT Bearer Token (HMAC-SHA256)</span>
          </div>
          <div class="spec-row">
            <span class="spec-label font-mono">Physical Sensor</span>
            <span class="spec-val font-mono">MC-38 Magnetic Reed Switch</span>
          </div>
          <p class="nvs-note">
            All facial feature extractions are vectorized mathematical embeddings. Raw video stream frames are immediately destroyed in volatile RAM after transmission.
          </p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { useSystemStore } from '../stores/system';
import {
  Cpu,
  Network,
  HardDrive,
  ShieldCheck
} from 'lucide-vue-next';

const system = useSystemStore();
</script>

<style scoped>
.settings-page {
  display: flex;
  flex-direction: column;
  gap: 24px;
}

.page-title-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
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

.settings-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 20px;
}

.spec-card {
  padding: 22px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.card-head {
  display: flex;
  align-items: center;
  gap: 12px;
}

.icon-wrap {
  width: 42px;
  height: 42px;
  border-radius: var(--radius-md);
  display: flex;
  align-items: center;
  justify-content: center;
}

.icon-wrap.blue {
  background: var(--color-primary-subtle);
  color: var(--color-primary);
  border: 1px solid var(--color-primary-border);
}

.icon-wrap.emerald {
  background: var(--color-disarmed-subtle);
  color: var(--color-disarmed);
  border: 1px solid var(--color-disarmed-border);
}

.icon-wrap.purple {
  background: #f5f3ff;
  color: #7c3aed;
  border: 1px solid #ddd6fe;
}

.icon-wrap.red {
  background: var(--color-armed-subtle);
  color: var(--color-armed);
  border: 1px solid var(--color-armed-border);
}

.card-title {
  font-size: 0.95rem;
  font-weight: 700;
  color: var(--text-main);
}

.card-sub {
  font-size: 0.75rem;
  color: var(--text-secondary);
}

.spec-list {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.spec-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding-bottom: 8px;
  border-bottom: 1px solid var(--border-subtle);
  font-size: 0.825rem;
}

.spec-row:last-of-type {
  border-bottom: none;
  padding-bottom: 0;
}

.spec-label {
  color: var(--text-secondary);
  font-size: 0.78rem;
}

.spec-val {
  color: var(--text-main);
  font-weight: 600;
}

.nvs-note {
  font-size: 0.75rem;
  color: var(--text-muted);
  line-height: 1.45;
  background: var(--bg-subtle);
  padding: 10px 12px;
  border-radius: var(--radius-sm);
  margin-top: 4px;
}

@media (max-width: 960px) {
  .settings-grid {
    grid-template-columns: 1fr;
  }
}
</style>
