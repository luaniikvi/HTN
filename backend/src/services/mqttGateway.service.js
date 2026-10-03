const mqtt = require('mqtt');
const EventEmitter = require('events');
const { SystemState, AccessLog, AlarmLog, Face } = require('../models');
require('dotenv').config();

class MqttGateway extends EventEmitter {
  constructor() {
    super();
    this.client = null;
    this.deviceId = process.env.DEVICE_ID || 'dev_01';
  }

  connect() {
    const brokerUrl = process.env.MQTT_BROKER_URL || 'mqtt://mosquitto:1883';
    const options = {
      username: process.env.MQTT_USER || 'esp32_client',
      password: process.env.MQTT_PASS || 'esp32_pass_secure',
      clientId: `backend_service_${Math.random().toString(16).slice(2, 8)}`,
      clean: true,
      reconnectPeriod: 3000
    };

    console.log(`Connecting to MQTT Broker at ${brokerUrl}...`);
    this.client = mqtt.connect(brokerUrl, options);

    this.client.on('connect', () => {
      console.log('✅ Connected to MQTT Broker');
      this.subscribeTopics();
    });

    this.client.on('message', (topic, message) => {
      this.handleIncomingMessage(topic, message);
    });

    this.client.on('error', (err) => {
      console.error('❌ MQTT Error:', err.message);
    });

    this.client.on('offline', () => {
      console.warn('⚠️ MQTT Connection Offline');
    });
  }

  subscribeTopics() {
    const topics = [
      'device/+/status',
      'device/+/events/door',
      'device/+/events/auth',
      'device/+/events/alarm',
      'device/+/events/enroll_step',
      'device/+/events/enroll_done',
      'device/+/events/deleted_done',
      'device/+/events/config',
      'device/+/events/faces'
    ];

    this.client.subscribe(topics, { qos: 1 }, (err) => {
      if (err) console.error('Subscription error:', err);
      else console.log('Subscribed to device MQTT topics');
    });
  }

  async handleIncomingMessage(topic, message) {
    try {
      const payload = JSON.parse(message.toString());
      const parts = topic.split('/');
      const deviceId = parts[1];
      const subType = parts[2];
      const eventType = parts[3];

      // 1. device/{id}/status
      if (subType === 'status') {
        const status = payload.status || 'OFFLINE';
        const doorState = payload.door || 'CLOSED';
        const securityMode = payload.mode || 'DISARMED';

        const gracePeriod = payload.grace_period ? parseInt(payload.grace_period) : null;

        const updateData = {
          device_id: deviceId,
          status: status,
          door_state: doorState,
          security_mode: securityMode,
          last_heartbeat: new Date()
        };

        if (gracePeriod && gracePeriod >= 3 && gracePeriod <= 60) {
          updateData.grace_period = gracePeriod;
        }

        if (payload.alarm !== undefined) {
          updateData.forced_alarm = Boolean(payload.alarm);
        } else if (securityMode === 'DISARMED') {
          updateData.armed_latched = false;
        }

        await SystemState.upsert(updateData);

        if (status === 'ONLINE') {
          Face.findAll().then(faces => {
            for (const f of faces) {
              if (!f.is_active) {
                this.sendToggleFaceCommand(f.face_id, false, deviceId);
              }
              this.sendUpdateFaceMetaCommand({
                face_id: f.face_id,
                name: f.name || `Person #${f.face_id}`,
                role_type: f.role_type || 'PERMANENT',
                valid_until: f.valid_until ? f.valid_until.toISOString() : '',
                is_active: f.is_active
              }, deviceId);
            }
          }).catch(err => console.warn('Could not sync faces to device:', err.message));
        }

        this.emit('device_status', {
          deviceId,
          status,
          doorState,
          securityMode,
          gracePeriod: updateData.grace_period,
          alarm: payload.alarm !== undefined ? Boolean(payload.alarm) : updateData.forced_alarm
        });
      }

      // 2. device/{id}/events/door
      else if (subType === 'events' && eventType === 'door') {
        const state = payload.state; // OPEN or CLOSED
        const evtType = state === 'OPEN' ? 'DOOR_OPENED' : 'DOOR_CLOSED';

        await AccessLog.create({
          device_id: deviceId,
          event_type: evtType,
          details: `Door state changed to ${state}`
        });

        await SystemState.update(
          { door_state: state, last_heartbeat: new Date() },
          { where: { device_id: deviceId } }
        );

        this.emit('door_event', { deviceId, state });
      }

      // 2b. device/{id}/events/auth (Xác thực khuôn mặt thành công)
      else if (subType === 'events' && eventType === 'auth') {
        const faceId = payload.face_id;
        const face = await Face.findOne({ where: { face_id: faceId } });
        const faceName = face ? face.name : `Person #${faceId}`;
        const roleType = face ? face.role_type : 'PERMANENT';

        const newLog = await AccessLog.create({
          device_id: deviceId,
          event_type: 'FACE_AUTH_SUCCESS',
          face_id: faceId,
          details: `Xác thực khuôn mặt thành công: ${faceName} (${roleType})`
        });

        this.emit('auth_event', {
          id: newLog.id,
          deviceId,
          faceId,
          faceName,
          roleType,
          face: face ? { name: face.name, role_type: face.role_type, is_active: face.is_active } : null,
          timestamp: newLog.timestamp
        });
      }

      // 3. device/{id}/events/alarm
      else if (subType === 'events' && eventType === 'alarm') {
        const event = payload.event || 'BREACH';
        const mode = payload.mode || 'ARMED';

        // Kiểm tra xem đã có bản ghi cảnh báo nào vừa được tạo trong vòng 8 giây gần đây chưa (tránh duplicate với HTTP upload)
        const recentAlarm = await AlarmLog.findOne({
          where: { device_id: deviceId, event: event },
          order: [['id', 'DESC']]
        });

        let isRecent = false;
        if (recentAlarm && recentAlarm.timestamp) {
          const diffMs = Math.abs(Date.now() - new Date(recentAlarm.timestamp).getTime());
          if (diffMs < 8000 || Math.abs(diffMs - 7 * 3600 * 1000) < 8000) {
            isRecent = true;
          }
        }

        if (!isRecent) {
          await AlarmLog.create({
            device_id: deviceId,
            mode: mode,
            event: event,
            details: `Phát hiện vi phạm đột nhập mở cửa khi chưa xác thực ở chế độ ${mode}.`
          });
        }

        await SystemState.update(
          { armed_latched: mode === 'ARMED', last_heartbeat: new Date() },
          { where: { device_id: deviceId } }
        );

        this.emit('alarm_event', { deviceId, mode, event, timestamp: new Date() });
      }

      // 4. device/{id}/events/enroll_step
      else if (subType === 'events' && eventType === 'enroll_step') {
        this.emit('enroll_step', { deviceId, step: payload.step, status: payload.status, angle: payload.angle });
      }

      // 5. device/{id}/events/enroll_done
      else if (subType === 'events' && eventType === 'enroll_done') {
        const faceId = payload.face_id;
        this.emit('enroll_done', { deviceId, faceId, status: payload.status, reason: payload.reason });
      }

      // 6. device/{id}/events/deleted_done
      else if (subType === 'events' && eventType === 'deleted_done') {
        const faceId = payload.face_id;
        const status = payload.status;

        if (status === 'SUCCESS') {
          if (faceId === -1) {
            await Face.destroy({ where: {}, truncate: true });
          } else {
            await Face.destroy({ where: { face_id: faceId } });
          }
        }

        this.emit('deleted_done', { deviceId, faceId, status });
      }

      // 7. device/{id}/events/config
      else if (subType === 'events' && eventType === 'config') {
        const gracePeriod = parseInt(payload.grace_period);
        if (gracePeriod && gracePeriod >= 3 && gracePeriod <= 60) {
          await SystemState.update(
            { grace_period: gracePeriod, last_heartbeat: new Date() },
            { where: { device_id: deviceId } }
          );
          console.log(`[MQTT] ⏱️ Synced grace_period (${gracePeriod}s) from device ${deviceId} to DB`);
          this.emit('device_status', { deviceId, gracePeriod });
        }
      }

      // 8. device/{id}/events/faces (Đồng bộ danh sách khuôn mặt từ Flash MCU)
      else if (subType === 'events' && eventType === 'faces') {
        const facesList = Array.isArray(payload.faces) ? payload.faces : [];
        console.log(`[MQTT] 👥 Received faces sync from device ${deviceId}: ${facesList.length} faces`);

        let anyChanged = false;
        for (const item of facesList) {
          const fid = parseInt(item.face_id);
          if (!fid || isNaN(fid)) continue;

          const targetName = (item.name && item.name.trim()) ? item.name.trim() : `Person #${fid}`;
          const targetRole = (item.role_type === 'TEMPORARY') ? 'TEMPORARY' : 'PERMANENT';
          const targetValid = (targetRole === 'TEMPORARY' && item.valid_until) ? new Date(item.valid_until) : null;
          const targetActive = item.is_active !== false;

          const existing = await Face.findOne({ where: { face_id: fid } });
          if (!existing) {
            // DB bị reset hoặc thiếu khuôn mặt -> Tự động khôi phục vào CSDL với đầy đủ thông tin!
            await Face.create({
              face_id: fid,
              name: targetName,
              role_type: targetRole,
              valid_until: targetValid,
              is_active: targetActive
            });
            anyChanged = true;
            console.log(`[FACE SYNC] ➕ Auto-restored Face ID ${fid} (${targetName}, ${targetRole}) into database`);
          } else {
            // Cập nhật nếu có trường thay đổi
            const updateFields = {
              is_active: targetActive,
              role_type: existing.role_type || targetRole,
              valid_until: existing.valid_until || targetValid
            };
            if (targetName && targetName !== `Person #${fid}`) {
              updateFields.name = targetName;
            }
            await existing.update(updateFields);
            anyChanged = true;
          }
        }

        this.emit('faces_sync', { deviceId, count: facesList.length, changed: anyChanged });
      }
    } catch (err) {
      console.error('Error handling MQTT message:', err.message);
    }
  }

  // --- HÀM PHÁT LỆNH ĐIỀU KHIỂN XUỐNG ESP32 (QoS 1) ---
  publishCommand(topic, data, options = { qos: 1 }) {
    if (!this.client || !this.client.connected) {
      console.warn(`Cannot publish to ${topic}: MQTT client disconnected`);
      return false;
    }
    this.client.publish(topic, JSON.stringify(data), options);
    return true;
  }

  sendModeCommand(mode, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/mode`, { mode });
  }

  sendEnrollCommand(deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/enroll`, { cmd: 'START_ENROLL' });
  }

  sendCancelEnrollCommand(deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/enroll`, { cmd: 'CANCEL' });
  }

  sendDeleteCommand(faceId, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/delete_face`, { cmd: 'DELETE', face_id: faceId });
  }

  sendDeleteAllCommand(deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/delete_face`, { cmd: 'DELETE', all: true, face_id: -1 });
  }

  sendAlarmCommand(alarmState, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/alarm`, { alarm: alarmState });
  }

  sendStreamCommand(enable, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/stream`, { enable });
  }

  sendConfigCommand(gracePeriod, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/config`, { grace_period: gracePeriod }, { qos: 1, retain: true });
  }

  sendToggleFaceCommand(faceId, active, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/toggle_face`, { face_id: faceId, active });
  }

  sendUpdateFaceMetaCommand(faceData, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/update_face_meta`, faceData, { qos: 1, retain: true });
  }
}

const mqttGateway = new MqttGateway();
module.exports = mqttGateway;
