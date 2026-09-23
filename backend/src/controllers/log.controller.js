const { AccessLog, AlarmLog, Face } = require('../models');

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
