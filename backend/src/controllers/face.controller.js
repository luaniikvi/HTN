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

// Gửi lệnh xóa khuôn mặt khỏi Flash NVS trên MCU
exports.deleteFace = async (req, res) => {
  try {
    const faceId = parseInt(req.params.id);
    if (isNaN(faceId)) {
      return res.status(400).json({ success: false, message: 'Invalid face_id' });
    }

    const face = await Face.findOne({ where: { face_id: faceId } });
    if (!face) {
      return res.status(404).json({ success: false, message: 'Face not found in database' });
    }

    // Phát lệnh xóa xuống ESP32
    console.log(`Sending MQTT command to delete face ${faceId} on ESP32...`);
    mqttGateway.sendDeleteCommand(faceId);

    // Trả về thông báo đang chờ xử lý ACK từ ESP32
    res.json({
      success: true,
      message: `Delete request for face ${faceId} dispatched to MCU. Database will be updated upon ACK.`
    });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};
