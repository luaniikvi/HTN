import { defineStore } from 'pinia';
import api from '../api/client';

export const useAuthStore = defineStore('auth', {
  state: () => ({
    token: localStorage.getItem('jwt_token') || null,
    user: JSON.parse(localStorage.getItem('user_info') || 'null'),
    loading: false,
    error: null
  }),

  getters: {
    isAuthenticated: (state) => !!state.token
  },

  actions: {
    async login(username, password) {
      this.loading = true;
      this.error = null;
      try {
        const res = await api.post('/auth/login', { username, password });
        this.token = res.data.token;
        this.user = res.data.user;
        localStorage.setItem('jwt_token', this.token);
        localStorage.setItem('user_info', JSON.stringify(this.user));
        return true;
      } catch (err) {
        this.error = err.response?.data?.message || 'Login failed';
        return false;
      } finally {
        this.loading = false;
      }
    },

    async register(username, password, fullname) {
      this.loading = true;
      this.error = null;
      try {
        const res = await api.post('/auth/register', { username, password, fullname });
        this.token = res.data.token;
        this.user = res.data.user;
        localStorage.setItem('jwt_token', this.token);
        localStorage.setItem('user_info', JSON.stringify(this.user));
        return true;
      } catch (err) {
        this.error = err.response?.data?.message || 'Registration failed';
        return false;
      } finally {
        this.loading = false;
      }
    },

    logout() {
      this.token = null;
      this.user = null;
      localStorage.removeItem('jwt_token');
      localStorage.removeItem('user_info');
    }
  }
});
