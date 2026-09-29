import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import path from 'path';
import dgram from 'dgram';
import os from 'os';

function getBroadcastTargets() {
  const targets = ['255.255.255.255'];
  try {
    const ifaces = os.networkInterfaces();
    for (const name of Object.keys(ifaces)) {
      for (const iface of ifaces[name]) {
        if (iface.family === 'IPv4' && !iface.internal) {
          const ip = iface.address.split('.').map(Number);
          const mask = iface.netmask.split('.').map(Number);
          const bcast = ip.map((b, i) => b | (~mask[i] & 255)).join('.');
          targets.push(bcast);
        }
      }
    }
  } catch (_) {}
  return [...new Set(targets)];
}

// ponytail: UDP discovery responder & broadcaster for ESP32
function esp32DiscoveryPlugin() {
  return {
    name: 'esp32-discovery-broadcaster',
    configureServer(server) {
      const socket = dgram.createSocket({ type: 'udp4', reuseAddr: true });
      const msg = Buffer.from(JSON.stringify({ service: 'esp32_security_backend', port: 3000 }));
      socket.on('error', () => {});
      socket.on('message', (data, rinfo) => {
        try {
          if (data.toString().includes('DISCOVER_SERVER')) {
            socket.send(msg, 0, msg.length, 8888, rinfo.address, () => {});
          }
        } catch (_) {}
      });
      socket.bind(8888, () => {
        try {
          socket.setBroadcast(true);
          const targets = getBroadcastTargets();
          console.log('\x1b[36m%s\x1b[0m', `📡 [ESP32 Discovery] Listening on 8888 & broadcasting to: ${targets.join(', ')}`);
          const timer = setInterval(() => {
            targets.forEach(addr => socket.send(msg, 0, msg.length, 8888, addr, () => {}));
          }, 3000);
          server.httpServer?.on('close', () => {
            clearInterval(timer);
            socket.close();
          });
        } catch (_) {}
      });
    }
  };
}

export default defineConfig({
  plugins: [vue(), esp32DiscoveryPlugin()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src')
    }
  },
  server: {
    port: 5173,
    host: '0.0.0.0',
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:3000',
        changeOrigin: true,
        secure: false
      },
      '/uploads': {
        target: 'http://127.0.0.1:3000',
        changeOrigin: true,
        secure: false
      },
      '/ws': {
        target: 'ws://127.0.0.1:3000',
        ws: true,
        changeOrigin: true,
        secure: false
      }
    }
  }
});
