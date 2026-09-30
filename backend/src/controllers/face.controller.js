const { Face } = require('../models');
const mqttGateway = require('../services/mqttGateway.service');

// Lấy danh sách khuôn mặt
exports.getFaces = async (req, res) => {
  try {
    const faces = await Face.findAll({
      order: [['created_at', 'DESC']]
    });
    res.json({ success: true, faces });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Kích hoạt quy trình nạp khuôn mặt trên ESP32
exports.startEnroll = async (req, res) => {
  try {
    const sent = mqttGateway.sendEnrollCommand();
    if (!sent) {
      return res.status(503).json({ success: false, message: 'MQTT Broker not connected' });
    }
    res.json({ success: true, message: 'Enrollment mode started on ESP32' });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Hủy quy trình nạp khuôn mặt trên ESP32
exports.cancelEnroll = async (req, res) => {
  try {
    const sent = mqttGateway.sendCancelEnrollCommand();
    if (!sent) {
      return res.status(503).json({ success: false, message: 'MQTT Broker not connected' });
    }
    res.json({ success: true, message: 'Enrollment mode cancelled on ESP32' });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Gán thông tin và phân quyền (PERMANENT / TEMPORARY) cho face_id vừa nạp
exports.bindFace = async (req, res) => {
  try {
    const { face_id, name, role_type, valid_until } = req.body;
    if (!face_id || !name) {
      return res.status(400).json({ success: false, message: 'face_id and name are required' });
    }

    const [face, created] = await Face.upsert({
      face_id: parseInt(face_id),
      name,
      role_type: role_type === 'TEMPORARY' ? 'TEMPORARY' : 'PERMANENT',
      valid_until: role_type === 'TEMPORARY' ? valid_until : null,
      is_active: true
    });

    res.status(201).json({
      success: true,
      message: 'Face assigned successfully',
      face
    });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Bật/tắt trạng thái hoạt động (Active / Inactive) của khuôn mặt
exports.toggleFaceActive = async (req, res) => {
  try {
    const faceId = parseInt(req.params.id);
    if (isNaN(faceId)) {
      return res.status(400).json({ success: false, message: 'Invalid face_id' });
    }

    const face = await Face.findOne({ where: { face_id: faceId } });
    if (!face) {
      return res.status(404).json({ success: false, message: 'Face not found in database' });
    }

    face.is_active = typeof req.body.is_active === 'boolean' ? req.body.is_active : !face.is_active;
    await face.save();

    // Đồng bộ ngay lập tức trạng thái Active / Inactive xuống ESP32
    console.log(`Sending MQTT toggle command for face #${faceId}: ${face.is_active ? 'ACTIVE' : 'INACTIVE'}`);
    mqttGateway.sendToggleFaceCommand(faceId, face.is_active);

    res.json({
      success: true,
      message: `Face #${faceId} is now ${face.is_active ? 'ACTIVE' : 'INACTIVE'}`,
      face
    });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Xóa vĩnh viễn khuôn mặt khỏi Database và gửi lệnh xóa tới MCU
exports.deleteFace = async (req, res) => {
  try {
    if (req.params.id === 'all' || req.params.id === '-1') {
      return exports.deleteAllFaces(req, res);
    }

    const faceId = parseInt(req.params.id);
    if (isNaN(faceId)) {
      return res.status(400).json({ success: false, message: 'Invalid face_id' });
    }

    const face = await Face.findOne({ where: { face_id: faceId } });
    if (!face) {
      return res.status(404).json({ success: false, message: 'Face not found in database' });
    }

    // Xóa luôn khỏi database
    await face.destroy();

    // Phát lệnh xóa xuống ESP32
    console.log(`Sending MQTT command to delete face ${faceId} on ESP32...`);
    mqttGateway.sendDeleteCommand(faceId);

    res.json({
      success: true,
      message: `Face #${faceId} permanently deleted from database and MCU.`
    });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Xóa tất cả khuôn mặt trong Database và trên Flash MCU
exports.deleteAllFaces = async (req, res) => {
  try {
    // Xóa toàn bộ trong database
    const deletedCount = await Face.destroy({ where: {}, truncate: true });

    // Phát lệnh xóa tất cả xuống ESP32
    console.log('Sending MQTT command to delete ALL faces on ESP32...');
    mqttGateway.sendDeleteAllCommand();

    res.json({
      success: true,
      message: 'All faces permanently deleted from database and MCU.',
      deletedCount
    });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};
