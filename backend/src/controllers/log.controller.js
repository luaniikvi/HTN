const fs = require('fs').promises;
const path = require('path');
const { Op } = require('sequelize');
const { AccessLog, AlarmLog, Face } = require('../models');
const videoStreamService = require('../services/videoStream.service');

// Lấy danh sách nhật ký ra vào
exports.getAccessLogs = async (req, res) => {
  try {
    const limit = parseInt(req.query.limit) || 50;
    const logs = await AccessLog.findAll({
      order: [['timestamp', 'DESC']],
      limit
    });
    res.json({ success: true, logs });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Lấy danh sách nhật ký cảnh báo vi phạm
exports.getAlarmLogs = async (req, res) => {
  try {
    const limit = parseInt(req.query.limit) || 50;
    const logs = await AlarmLog.findAll({
      order: [['timestamp', 'DESC']],
      limit
    });
    res.json({ success: true, logs });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Tiếp nhận và lưu trữ ảnh chụp vi phạm từ ESP32
exports.captureBreachImage = async (req, res) => {
  try {
    const imageBuffer = req.body;
    if (!imageBuffer || imageBuffer.length === 0) {
      return res.status(400).json({ success: false, message: 'Dữ liệu ảnh trống (Empty image payload)' });
    }

    const deviceId = req.headers['x-device-id'] || req.query.device_id || 'dev_01';
    const mode = req.headers['x-mode'] || req.query.mode || 'ARMED';
    const filename = `breach_${deviceId}_${Date.now()}.jpg`;
    const uploadDir = path.join(__dirname, '../../uploads/breach_images');
    const filePath = path.join(uploadDir, filename);

    await fs.writeFile(filePath, imageBuffer);
    const imageUrl = `/uploads/breach_images/${filename}`;

    console.log(`📸 [BREACH IMAGE] Đã nhận và lưu ảnh vi phạm từ ${deviceId}: ${imageUrl} (${imageBuffer.length} bytes)`);

    // Tìm bản ghi cảnh báo gần nhất của deviceId (trong vòng 10 giây) để gắn ảnh vào
    const recentAlarm = await AlarmLog.findOne({
      where: {
        device_id: deviceId,
        event: 'BREACH'
      },
      order: [['id', 'DESC']]
    });

    let isRecent = false;
    if (recentAlarm && recentAlarm.timestamp) {
      const diffMs = Math.abs(Date.now() - new Date(recentAlarm.timestamp).getTime());
      if (diffMs < 10000 || Math.abs(diffMs - 7 * 3600 * 1000) < 10000) {
        isRecent = true;
      }
    }

    let alarm = null;
    if (isRecent && recentAlarm) {
      alarm = recentAlarm;
      alarm.image_url = imageUrl;
      alarm.details = `Phát hiện vi phạm đột nhập mở cửa ở chế độ ${mode} (kèm ảnh chụp bằng chứng)`;
      await alarm.save();
    } else {
      alarm = await AlarmLog.create({
        device_id: deviceId,
        mode: mode,
        event: 'BREACH',
        details: `Phát hiện vi phạm đột nhập mở cửa ở chế độ ${mode} (kèm ảnh chụp bằng chứng)`,
        image_url: imageUrl,
        timestamp: new Date()
      });
    }

    // Dọn dẹp bất kỳ bản ghi rác nào không có ảnh được tạo sát thời điểm này (nếu có)
    await AlarmLog.destroy({
      where: {
        device_id: deviceId,
        event: 'BREACH',
        image_url: null,
        id: { [Op.ne]: alarm.id }
      },
      limit: 2
    });

    // Broadcast sự kiện cảnh báo thời gian thực kèm ảnh tới Web client
    videoStreamService.broadcastEvent('ALARM_BREACH', {
      deviceId,
      mode,
      event: 'BREACH',
      imageUrl,
      timestamp: new Date().toISOString()
    });

    res.json({
      success: true,
      message: 'Lưu ảnh vi phạm thành công',
      imageUrl,
      alarmId: alarm.id
    });
  } catch (err) {
    console.error('Lỗi khi lưu ảnh vi phạm:', err);
    res.status(500).json({ success: false, message: err.message });
  }
};
