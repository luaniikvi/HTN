import { createRouter, createWebHistory } from 'vue-router';
import LoginView from '../views/LoginView.vue';
import RegisterView from '../views/RegisterView.vue';
import AppLayout from '../components/layout/AppLayout.vue';
import LiveMonitorView from '../views/LiveMonitorView.vue';
import SecurityView from '../views/SecurityView.vue';
import FacesView from '../views/FacesView.vue';
import LogsView from '../views/LogsView.vue';
import SettingsView from '../views/SettingsView.vue';

const routes = [
  {
    path: '/login',
    name: 'Login',
    component: LoginView
  },
  {
    path: '/register',
    name: 'Register',
    component: RegisterView
  },
  {
    path: '/',
    component: AppLayout,
    meta: { requiresAuth: true },
    children: [
      {
        path: '',
        redirect: '/security'
      },
      {
        path: 'security',
        name: 'Security',
        component: SecurityView
      },
      {
        path: 'monitor',
        name: 'LiveMonitor',
        component: LiveMonitorView
      },
      {
        path: 'faces',
        name: 'Faces',
        component: FacesView
      },
      {
        path: 'logs',
        name: 'Logs',
        component: LogsView
      },
      {
        path: 'settings',
        name: 'Settings',
        component: SettingsView
      }
    ]
  }
];

const router = createRouter({
  history: createWebHistory(),
  routes
});

router.beforeEach((to, from, next) => {
  const token = localStorage.getItem('jwt_token');
  if (to.meta.requiresAuth && !token) {
    next('/login');
  } else if ((to.path === '/login' || to.path === '/register') && token) {
    next('/security');
  } else {
    next();
  }
});

export default router;
