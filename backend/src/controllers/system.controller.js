const { SystemState } = require('../models');
const mqttGateway = require('../services/mqttGateway.service');

// Lấy trạng thái hệ thống hiện tại
exports.getSystemState = async (req, res) => {
  try {
    const deviceId = req.query.device_id || process.env.DEVICE_ID || 'dev_01';
    let state = await SystemState.findByPk(deviceId);
    if (!state) {
      state = await SystemState.create({
        device_id: deviceId,
        status: 'OFFLINE',
        door_state: 'CLOSED',
        security_mode: 'DISARMED',
        forced_alarm: false,
        armed_latched: false,
        grace_period: 10
      });
    } else if (state.security_mode === 'DISARMED' && (state.forced_alarm || state.armed_latched)) {
      await state.update({ forced_alarm: false, armed_latched: false });
    }
    res.json({ success: true, state });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Chuyển đổi 3 trạng thái an ninh (ARMED / STAY / DISARMED)
exports.changeMode = async (req, res) => {
  try {
    const { mode } = req.body;
    if (!mode || !['ARMED', 'STAY', 'DISARMED'].includes(mode)) {
      return res.status(400).json({ success: false, message: 'Valid mode must be ARMED, STAY, or DISARMED' });
    }

    const deviceId = req.body.device_id || process.env.DEVICE_ID || 'dev_01';
    mqttGateway.sendModeCommand(mode, deviceId);

    const updatePayload = { security_mode: mode, armed_latched: false };
    if (mode === 'DISARMED') {
      updatePayload.forced_alarm = false;
    }

    // Cập nhật trạng thái tạm thời trong DB
    await SystemState.update(
      updatePayload,
      { where: { device_id: deviceId } }
    );

    res.json({ success: true, message: `Mode change command sent: ${mode}` });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Bật/tắt còi khẩn cấp Panic Siren hoặc ngắt còi hú bị chốt
exports.toggleAlarm = async (req, res) => {
  try {
    const { alarm } = req.body; // boolean
    const deviceId = req.body.device_id || process.env.DEVICE_ID || 'dev_01';

    const alarmState = Boolean(alarm);
    mqttGateway.sendAlarmCommand(alarmState, deviceId);

    await SystemState.update(
      { forced_alarm: alarmState, armed_latched: alarmState ? true : false },
      { where: { device_id: deviceId } }
    );

    res.json({ success: true, message: `Alarm set to ${alarmState}` });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Cấu hình thời gian ân hạn mở cửa (Grace Timeout)
exports.updateConfig = async (req, res) => {
  try {
    const { grace_period } = req.body;
    const period = parseInt(grace_period);
    if (isNaN(period) || period < 3 || period > 60) {
      return res.status(400).json({ success: false, message: 'grace_period must be between 3 and 60 seconds' });
    }

    const deviceId = req.body.device_id || process.env.DEVICE_ID || 'dev_01';
    mqttGateway.sendConfigCommand(period, deviceId);

    await SystemState.update(
      { grace_period: period },
      { where: { device_id: deviceId } }
    );

    res.json({ success: true, message: `Grace period updated to ${period} seconds` });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};

// Bật/tắt luồng camera stream theo yêu cầu từ Web
exports.toggleStream = async (req, res) => {
  try {
    const { enable } = req.body;
    const deviceId = req.body.device_id || process.env.DEVICE_ID || 'dev_01';
    const streamState = Boolean(enable);
    mqttGateway.sendStreamCommand(streamState, deviceId);
    res.json({ success: true, message: `Stream command sent: ${streamState}`, streamEnabled: streamState });
  } catch (err) {
    res.status(500).json({ success: false, message: err.message });
  }
};
