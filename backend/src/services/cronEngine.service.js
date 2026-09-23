const cron = require('node-cron');
const { Op } = require('sequelize');
const { Face } = require('../models');
const mqttGateway = require('./mqttGateway.service');

class CronEngineService {
  start() {
    console.log('⏰ Face Expiration Cron Engine scheduled to run every 60 seconds');

    // Chạy ngầm mỗi 60 giây (* * * * *)
    cron.schedule('*/1 * * * *', async () => {
      try {
        const now = new Date();
        // Tìm các khuôn mặt TEMPORARY đã hết hạn nhưng vẫn đang active trên Flash MCU
        const expiredFaces = await Face.findAll({
          where: {
            role_type: 'TEMPORARY',
            is_active: true,
            valid_until: {
              [Op.lte]: now
            }
          }
        });

        if (expiredFaces.length > 0) {
          console.log(`[Cron] Found ${expiredFaces.length} expired temporary face(s). Triggering MQTT deletion...`);
          for (const face of expiredFaces) {
            console.log(`[Cron] Requesting ESP32 to delete expired face_id: ${face.face_id} (${face.name})`);
            mqttGateway.sendDeleteCommand(face.face_id);
            // Lưu ý: is_active = false chỉ được cập nhật khi nhận tin deleted_done từ ESP32 tại mqttGateway
          }
        }
      } catch (err) {
        console.error('[Cron] Error scanning expired faces:', err.message);
      }
    });
  }
}

const cronEngineService = new CronEngineService();
module.exports = cronEngineService;
