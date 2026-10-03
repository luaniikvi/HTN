<template>
  <div class="auth-page">
    <div ref="authCardRef" class="clean-card auth-card">
      <!-- Header -->
      <div class="auth-header">
        <div class="brand-box">
          <Shield :size="28" stroke-width="2.2" />
        </div>
        <h2 class="auth-title">Sign In to EdgeGuard</h2>
        <p class="auth-desc">ESP32-S3 Edge AI Biometric Security Console</p>
      </div>

      <!-- Error Alert -->
      <div v-if="auth.error" class="error-alert">
        <AlertCircle :size="16" />
        <span>{{ auth.error }}</span>
      </div>

      <!-- Form -->
      <form @submit.prevent="handleLogin" class="auth-form">
        <div class="form-group">
          <label class="form-label">Username</label>
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
          <label class="form-label">Password</label>
          <input
            type="password"
            v-model="password"
            required
            placeholder="••••••••"
            class="input-control"
            autocomplete="current-password"
          />
        </div>

        <button type="submit" class="btn btn-primary auth-submit-btn" :disabled="auth.loading">
          <LogIn :size="16" />
          <span>{{ auth.loading ? 'Authenticating...' : 'Sign In' }}</span>
        </button>
      </form>

      <!-- Footer Link -->
      <div class="auth-footer">
        <span>Need a new account?</span>
        <router-link to="/register" class="auth-link">Create administrator</router-link>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useRouter } from 'vue-router';
import { useAuthStore } from '../stores/auth';
import gsap from 'gsap';
import { Shield, AlertCircle, LogIn } from 'lucide-vue-next';

const auth = useAuthStore();
const router = useRouter();
const authCardRef = ref(null);

const username = ref('admin');
const password = ref('admin123');

async function handleLogin() {
  const success = await auth.login(username.value, password.value);
  if (success) {
    router.push('/security');
  }
}

onMounted(() => {
  if (authCardRef.value) {
    gsap.fromTo(
      authCardRef.value,
      { opacity: 0, y: 16 },
      { opacity: 1, y: 0, duration: 0.35, ease: 'power2.out' }
    );
  }
});
</script>

<style scoped>
.auth-page {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  background-color: var(--bg-body);
  padding: 24px;
}

.auth-card {
  width: 100%;
  max-width: 400px;
  padding: 36px 30px;
  display: flex;
  flex-direction: column;
  gap: 22px;
  box-shadow: var(--shadow-modal);
}

.auth-header {
  text-align: center;
}

.brand-box {
  width: 54px;
  height: 54px;
  border-radius: var(--radius-md);
  background: var(--color-primary-subtle);
  border: 1px solid var(--color-primary-border);
  color: var(--color-primary);
  display: flex;
  align-items: center;
  justify-content: center;
  margin: 0 auto 14px;
}

.auth-title {
  font-size: 1.3rem;
  font-weight: 800;
  color: var(--text-main);
  letter-spacing: -0.02em;
}

.auth-desc {
  font-size: 0.78rem;
  color: var(--text-secondary);
  margin-top: 4px;
}

.error-alert {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 14px;
  background: var(--color-armed-subtle);
  border: 1px solid var(--color-armed-border);
  border-radius: var(--radius-md);
  color: var(--color-armed-text);
  font-size: 0.8rem;
  font-weight: 500;
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

.form-label {
  font-size: 0.78rem;
  font-weight: 600;
  color: var(--text-main);
}

.auth-submit-btn {
  width: 100%;
  padding: 12px;
  margin-top: 6px;
}

.auth-footer {
  text-align: center;
  font-size: 0.8rem;
  color: var(--text-secondary);
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
