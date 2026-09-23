const express = require('express');
const router = express.Router();
const faceController = require('../controllers/face.controller');
const { verifyToken } = require('../middlewares/auth.middleware');

router.get('/', verifyToken, faceController.getFaces);
router.post('/enroll', verifyToken, faceController.startEnroll);
router.post('/cancel-enroll', verifyToken, faceController.cancelEnroll);
router.post('/bind', verifyToken, faceController.bindFace);
router.delete('/:id', verifyToken, faceController.deleteFace);

module.exports = router;
