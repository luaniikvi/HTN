const express = require('express');
const router = express.Router();
const logController = require('../controllers/log.controller');
const { verifyToken } = require('../middlewares/auth.middleware');

router.get('/access', verifyToken, logController.getAccessLogs);
router.get('/alarms', verifyToken, logController.getAlarmLogs);

// Endpoint nhận ảnh chụp vi phạm từ ESP32 (Hỗ trợ raw binary buffer JPEG)
router.post(
  '/breach-capture',
  express.raw({ type: ['image/jpeg', 'image/*', 'application/octet-stream'], limit: '10mb' }),
  logController.captureBreachImage
);

module.exports = router;
