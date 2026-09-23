const express = require('express');
const router = express.Router();
const logController = require('../controllers/log.controller');
const { verifyToken } = require('../middlewares/auth.middleware');

router.get('/access', verifyToken, logController.getAccessLogs);
router.get('/alarms', verifyToken, logController.getAlarmLogs);

module.exports = router;
