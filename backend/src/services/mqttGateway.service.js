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
      'device/+/events/alarm',
      'device/+/events/enroll_step',
      'device/+/events/enroll_done',
      'device/+/events/deleted_done'
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

        const updateData = {
          device_id: deviceId,
          status: status,
          door_state: doorState,
          security_mode: securityMode,
          last_heartbeat: new Date()
        };

        if (securityMode === 'DISARMED') {
          updateData.forced_alarm = false;
          updateData.armed_latched = false;
        }

        await SystemState.upsert(updateData);

        this.emit('device_status', { deviceId, status, doorState, securityMode });
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

      // 3. device/{id}/events/alarm
      else if (subType === 'events' && eventType === 'alarm') {
        const event = payload.event || 'BREACH';
        const mode = payload.mode || 'ARMED';

        await AlarmLog.create({
          device_id: deviceId,
          mode: mode,
          event: event,
          details: `Security breach detected! Door opened without valid face auth in ${mode} mode.`
        });

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
        this.emit('enroll_done', { deviceId, faceId, status: payload.status });
      }

      // 6. device/{id}/events/deleted_done
      else if (subType === 'events' && eventType === 'deleted_done') {
        const faceId = payload.face_id;
        const status = payload.status;

        if (status === 'SUCCESS') {
          await Face.update({ is_active: false }, { where: { face_id: faceId } });
        }

        this.emit('deleted_done', { deviceId, faceId, status });
      }
    } catch (err) {
      console.error('Error handling MQTT message:', err.message);
    }
  }

  // --- HÀM PHÁT LỆNH ĐIỀU KHIỂN XUỐNG ESP32 (QoS 1) ---
  publishCommand(topic, data) {
    if (!this.client || !this.client.connected) {
      console.warn(`Cannot publish to ${topic}: MQTT client disconnected`);
      return false;
    }
    this.client.publish(topic, JSON.stringify(data), { qos: 1 });
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

  sendAlarmCommand(alarmState, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/alarm`, { alarm: alarmState });
  }

  sendStreamCommand(enable, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/stream`, { enable });
  }

  sendConfigCommand(gracePeriod, deviceId = this.deviceId) {
    return this.publishCommand(`device/${deviceId}/cmd/config`, { grace_period: gracePeriod });
  }
}

const mqttGateway = new MqttGateway();
module.exports = mqttGateway;
