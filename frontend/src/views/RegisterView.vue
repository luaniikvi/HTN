<template>
  <div class="auth-container">
    <div class="glass-card auth-card">
      <div class="auth-header">
        <div class="logo-box">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="shield-icon">
            <path d="M16 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"></path>
            <circle cx="8.5" cy="7" r="4"></circle>
            <line x1="20" y1="8" x2="20" y2="14"></line>
            <line x1="23" y1="11" x2="17" y2="11"></line>
          </svg>
        </div>
        <h2>ĐĂNG KÝ TÀI KHOẢN MỚI</h2>
        <p class="auth-sub">Tạo tài khoản quản trị hệ thống giám sát an ninh</p>
      </div>

      <div v-if="auth.error" class="error-banner">
        {{ auth.error }}
      </div>

      <form @submit.prevent="handleRegister" class="auth-form">
        <div class="form-group">
          <label>Họ và Tên (Full Name):</label>
          <input
            type="text"
            v-model="fullname"
            required
            placeholder="Nguyễn Văn A"
            class="input-control"
          />
        </div>

        <div class="form-group">
          <label>Tên đăng nhập (Username):</label>
          <input
            type="text"
            v-model="username"
            required
            placeholder="admin01"
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
            autocomplete="new-password"
          />
        </div>

        <button type="submit" class="btn btn-primary btn-block" :disabled="auth.loading">
          {{ auth.loading ? 'Đang khởi tạo...' : 'TẠO TÀI KHOẢN' }}
        </button>
      </form>

      <div class="auth-footer">
        <span>Đã có tài khoản?</span>
        <router-link to="/login" class="auth-link">Đăng nhập</router-link>
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

const fullname = ref('');
const username = ref('');
const password = ref('');

async function handleRegister() {
  const success = await auth.register(username.value, password.value, fullname.value);
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
