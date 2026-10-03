const express = require('express');
const router = express.Router();
const systemController = require('../controllers/system.controller');
const { verifyToken } = require('../middlewares/auth.middleware');

router.get('/state', verifyToken, systemController.getSystemState);
router.post('/mode', verifyToken, systemController.changeMode);
router.post('/alarm', verifyToken, systemController.toggleAlarm);
router.post('/config', verifyToken, systemController.updateConfig);
router.post('/stream', verifyToken, systemController.toggleStream);

module.exports = router;
