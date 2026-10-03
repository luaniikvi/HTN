const dgram = require('dgram');
const os = require('os');

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

class DiscoveryService {
  start(port = 3000, intervalMs = 3000) {
    const socket = dgram.createSocket({ type: 'udp4', reuseAddr: true });
    const msg = Buffer.from(JSON.stringify({ service: 'esp32_security_backend', port }));

    socket.on('error', () => {}); // ponytail: ignore socket errors to never crash backend
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
        console.log(`📡 UDP Discovery Service on port 8888 broadcasting to: ${targets.join(', ')} (every ${intervalMs / 1000}s)`);
        setInterval(() => {
          targets.forEach(addr => socket.send(msg, 0, msg.length, 8888, addr, () => {}));
        }, intervalMs);
      } catch (_) {}
    });
  }
}

module.exports = new DiscoveryService();
if (require.main === module) {
  new DiscoveryService().start();
}
