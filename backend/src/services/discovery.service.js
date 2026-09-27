const dgram = require('dgram');

class DiscoveryService {
  start(port = 3000, intervalMs = 3000) {
    const socket = dgram.createSocket({ type: 'udp4', reuseAddr: true });
    const msg = Buffer.from(JSON.stringify({ service: 'esp32_security_backend', port }));

    socket.on('error', () => {}); // ponytail: ignore socket errors to never crash backend
    socket.bind(() => {
      try {
        socket.setBroadcast(true);
        console.log(`📡 UDP Discovery Service broadcasting on port 8888 (every ${intervalMs / 1000}s)`);
        setInterval(() => {
          socket.send(msg, 0, msg.length, 8888, '255.255.255.255', () => {});
        }, intervalMs);
      } catch (_) {}
    });
  }
}

module.exports = new DiscoveryService();
