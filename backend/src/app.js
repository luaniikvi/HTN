const express = require('express');
const cors = require('cors');
const http = require('http');
const https = require('https');
require('dotenv').config();

const { sequelize } = require('./models');
const { getSslCredentials } = require('./config/ssl');
const mqttGateway = require('./services/mqttGateway.service');
const videoStreamService = require('./services/videoStream.service');
const cronEngineService = require('./services/cronEngine.service');

// Import routes
const authRoutes = require('./routes/auth.routes');
const faceRoutes = require('./routes/face.routes');
const logRoutes = require('./routes/log.routes');
const systemRoutes = require('./routes/system.routes');

const app = express();

// Middleware
app.use(cors({ origin: true, credentials: true }));
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// RESTful API Routing
app.use('/api/auth', authRoutes);
app.use('/api/faces', faceRoutes);
app.use('/api/logs', logRoutes);
app.use('/api/system', systemRoutes);

// Health check endpoint
app.get('/api/health', (req, res) => {
  res.json({
    status: 'ONLINE',
    time: new Date().toISOString(),
    viewersCount: videoStreamService.viewers.size
  });
});

const PORT_HTTP = parseInt(process.env.PORT_HTTP || '3000');
const PORT_HTTPS = parseInt(process.env.PORT_HTTPS || '3443');

// 1. Tạo HTTP Server (Phục vụ cổng 3000 cho ESP32 WS Stream và REST API)
const httpServer = http.createServer(app);

// 2. Tạo HTTPS Server (Phục vụ cổng 3443 cho Web Client bảo mật)
const sslCredentials = getSslCredentials();
let httpsServer = null;
if (sslCredentials) {
  httpsServer = https.createServer(sslCredentials, app);
}

// 3. Gắn dịch vụ WebSocket Stream Relay vào cả hai server
videoStreamService.attachServers(httpServer, httpsServer);

// 4. Khởi động toàn bộ dịch vụ
async function startServer() {
  try {
    // Kết nối CSDL
    await sequelize.authenticate();
    console.log('✅ Database connected successfully');
    await sequelize.sync(); // Đồng bộ bảng dữ liệu

    // Tự động khởi tạo và đảm bảo tài khoản admin / admin123
    const bcrypt = require('bcryptjs');
    const { User, SystemState } = require('./models');
    const adminUser = await User.findOne({ where: { username: 'admin' } });
    const adminHash = await bcrypt.hash('admin123', 10);
    if (!adminUser) {
      await User.create({
        username: 'admin',
        password: adminHash,
        fullname: 'System Administrator',
        role: 'admin'
      });
      console.log('👤 Initialized default admin user (admin / admin123)');
    } else {
      adminUser.password = adminHash;
      await adminUser.save();
      console.log('👤 Synchronized admin password (admin / admin123)');
    }

    await SystemState.findOrCreate({
      where: { device_id: process.env.DEVICE_ID || 'dev_01' },
      defaults: {
        status: 'OFFLINE',
        door_state: 'CLOSED',
        security_mode: 'DISARMED',
        grace_period: 10
      }
    });

    // Kết nối MQTT Broker
    mqttGateway.connect();

    // Khởi động Cron Engine quét mặt tạm thời hết hạn
    cronEngineService.start();

    // Lắng nghe cổng HTTP (3000)
    httpServer.listen(PORT_HTTP, '0.0.0.0', () => {
      console.log(`🚀 HTTP Server & ESP32 Camera WS listening on http://0.0.0.0:${PORT_HTTP}`);
      console.log(`   ESP32 Camera Stream endpoint: ws://<IP>:${PORT_HTTP}/ws/camera/stream`);
    });

    // Lắng nghe cổng HTTPS (3443)
    if (httpsServer) {
      httpsServer.listen(PORT_HTTPS, '0.0.0.0', () => {
        console.log(`🔒 HTTPS Secure Server listening on https://0.0.0.0:${PORT_HTTPS}`);
        console.log(`   Web Client WSS Stream endpoint: wss://<IP>:${PORT_HTTPS}/ws/camera/stream?viewer=true`);
      });
    } else {
      console.warn('⚠️ Running without HTTPS (SSL certs missing)');
    }
  } catch (err) {
    console.error('❌ Failed to start server:', err);
    process.exit(1);
  }
}

startServer();
