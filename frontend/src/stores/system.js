import { defineStore } from 'pinia';
import api from '../api/client';

export const useSystemStore = defineStore('system', {
  state: () => ({
    deviceId: 'dev_01',
    status: 'OFFLINE',
    doorState: 'CLOSED',
    securityMode: 'DISARMED',
    forcedAlarm: false,
    armedLatched: false,
    gracePeriod: 10,
    wsConnected: false,
    
    // Sự kiện cảnh báo đột nhập thời gian thực
    activeBreachAlert: null,

    // Tiến trình đăng ký khuôn mặt thời gian thực
    enrollStep: 0,
    enrollStepData: null,
    enrolledFaceId: null,
    isEnrolling: false
  }),

  actions: {
    async fetchSystemState() {
      try {
        const res = await api.get('/system/state');
        if (res.data.success && res.data.state) {
          const s = res.data.state;
          this.status = s.status;
          this.doorState = s.door_state;
          this.securityMode = s.security_mode;
          this.forcedAlarm = (s.security_mode === 'DISARMED') ? false : Boolean(s.forced_alarm);
          this.armedLatched = (s.security_mode === 'DISARMED') ? false : Boolean(s.armed_latched);
          this.gracePeriod = s.grace_period;
        }
      } catch (err) {
        console.error('Failed to fetch system state:', err);
      }
    },

    async changeMode(mode) {
      try {
        await api.post('/system/mode', { mode });
        this.securityMode = mode;
        this.armedLatched = false;
        this.activeBreachAlert = null;
        if (mode === 'DISARMED') {
          this.forcedAlarm = false;
        }
      } catch (err) {
        console.error('Failed to change mode:', err);
        throw err;
      }
    },

    async toggleAlarm(alarmState) {
      try {
        await api.post('/system/alarm', { alarm: alarmState });
        this.forcedAlarm = alarmState;
        if (!alarmState) {
          this.armedLatched = false;
          this.activeBreachAlert = null;
        }
      } catch (err) {
        console.error('Failed to toggle alarm:', err);
        throw err;
      }
    },

    async updateGracePeriod(period) {
      try {
        await api.post('/system/config', { grace_period: period });
        this.gracePeriod = period;
      } catch (err) {
        console.error('Failed to update grace period:', err);
        throw err;
      }
    },

    initWebSocket() {
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${protocol}//${window.location.host}/ws/events`;

      const socket = new WebSocket(wsUrl);

      socket.onopen = () => {
        this.wsConnected = true;
        console.log('✅ Connected to Dashboard Events WebSocket');
      };

      socket.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          const type = data.type;
          const payload = data.payload;

          if (type === 'DEVICE_STATUS') {
            const prevMode = this.securityMode;
            this.status = payload.status;
            this.doorState = payload.doorState;
            this.securityMode = payload.securityMode;
            if (payload.securityMode === 'DISARMED') {
              this.forcedAlarm = false;
              this.armedLatched = false;
              this.activeBreachAlert = null;
            } else if (prevMode !== payload.securityMode) {
              // Khi chuyển sang ARMED hoặc STAY từ bên ngoài (công tắc), reset còi chốt cũ
              this.armedLatched = false;
              this.activeBreachAlert = null;
            }
            if (payload.doorState === 'CLOSED' && payload.securityMode === 'STAY') {
              this.armedLatched = false;
              this.activeBreachAlert = null;
            }
          } else if (type === 'DOOR_EVENT') {
            this.doorState = payload.state;
            if (payload.state === 'CLOSED' && this.securityMode === 'STAY') {
              this.armedLatched = false;
              this.activeBreachAlert = null;
            }
          } else if (type === 'ALARM_BREACH') {
            this.activeBreachAlert = {
              ...(this.activeBreachAlert || {}),
              ...payload
            };
            this.armedLatched = true;
          } else if (type === 'ENROLL_STEP') {
            this.enrollStep = payload.step;
            this.enrollStepData = payload;
          } else if (type === 'ENROLL_DONE') {
            this.enrolledFaceId = payload.faceId;
            this.enrollStep = 4; // Hoàn tất
          }
        } catch (err) {
          console.error('Error parsing event message:', err);
        }
      };

      socket.onclose = () => {
        this.wsConnected = false;
        console.warn('⚠️ Events WebSocket closed, retrying in 3s...');
        setTimeout(() => this.initWebSocket(), 3000);
      };
    },

    dismissBreachAlert() {
      this.activeBreachAlert = null;
      this.armedLatched = false;
    }
  }
});
