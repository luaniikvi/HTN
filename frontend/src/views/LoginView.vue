<template>
  <div class="auth-container">
    <div class="glass-card auth-card">
      <div class="auth-header">
        <div class="logo-box">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="shield-icon">
            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"></path>
          </svg>
        </div>
        <h2>ĐĂNG NHẬP HỆ THỐNG</h2>
        <p class="auth-sub">Hệ thống Giám sát & Quản lý Khóa Cửa Thông Minh ESP32-S3</p>
      </div>

      <div v-if="auth.error" class="error-banner">
        {{ auth.error }}
      </div>

      <form @submit.prevent="handleLogin" class="auth-form">
        <div class="form-group">
          <label>Tên đăng nhập (Username):</label>
          <input
            type="text"
            v-model="username"
            required
            placeholder="admin"
            class="input-control"
            autocomplete="username"
          />
        </div>

        <div class="form-group">
          <label>Mật khẩu (Password):</label>
          <input
            type="password"
            v-model="password"
            required
            placeholder="••••••••"
            class="input-control"
            autocomplete="current-password"
          />
        </div>

        <button type="submit" class="btn btn-primary btn-block" :disabled="auth.loading">
          {{ auth.loading ? 'Đang xác thực...' : 'ĐĂNG NHẬP' }}
        </button>
      </form>

      <div class="auth-footer">
        <span>Chưa có tài khoản quản trị?</span>
        <router-link to="/register" class="auth-link">Đăng ký mới</router-link>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue';
import { useRouter } from 'vue-router';
import { useAuthStore } from '../stores/auth';

const auth = useAuthStore();
const router = useRouter();

const username = ref('admin');
const password = ref('admin123');

async function handleLogin() {
  const success = await auth.login(username.value, password.value);
  if (success) {
    router.push('/');
  }
}
</script>

<style scoped>
.auth-container {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
}

.auth-card {
  width: 100%;
  max-width: 420px;
  padding: 32px 28px;
  display: flex;
  flex-direction: column;
  gap: 22px;
}

.auth-header {
  text-align: center;
}

.logo-box {
  width: 52px;
  height: 52px;
  background: rgba(59, 130, 246, 0.15);
  border-radius: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  margin: 0 auto 14px;
}

.shield-icon {
  width: 28px;
  height: 28px;
  color: var(--color-primary);
}

.auth-header h2 {
  font-size: 1.25rem;
  font-weight: 800;
  letter-spacing: 0.03em;
}

.auth-sub {
  font-size: 0.75rem;
  color: var(--text-muted);
  margin-top: 6px;
}

.auth-form {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.form-group {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.form-group label {
  font-size: 0.8rem;
  font-weight: 600;
  color: #d1d5db;
}

.btn-block {
  width: 100%;
  padding: 12px;
  margin-top: 6px;
}

.error-banner {
  padding: 10px 14px;
  background: rgba(239, 68, 68, 0.15);
  border: 1px solid rgba(239, 68, 68, 0.3);
  border-radius: 8px;
  color: #f87171;
  font-size: 0.85rem;
}

.auth-footer {
  text-align: center;
  font-size: 0.8rem;
  color: var(--text-muted);
  display: flex;
  justify-content: center;
  gap: 6px;
}

.auth-link {
  color: var(--color-primary);
  text-decoration: none;
  font-weight: 600;
}

.auth-link:hover {
  text-decoration: underline;
}
</style>
