const { DataTypes } = require('sequelize');
const sequelize = require('../config/db');

const SystemState = sequelize.define('SystemState', {
  device_id: {
    type: DataTypes.STRING(64),
    primaryKey: true
  },
  status: {
    type: DataTypes.ENUM('ONLINE', 'OFFLINE'),
    defaultValue: 'OFFLINE'
  },
  door_state: {
    type: DataTypes.ENUM('OPEN', 'CLOSED'),
    defaultValue: 'CLOSED'
  },
  security_mode: {
    type: DataTypes.ENUM('DISARMED', 'ARMED', 'STAY'),
    defaultValue: 'DISARMED'
  },
  forced_alarm: {
    type: DataTypes.BOOLEAN,
    defaultValue: false
  },
  armed_latched: {
    type: DataTypes.BOOLEAN,
    defaultValue: false
  },
  grace_period: {
    type: DataTypes.INTEGER,
    defaultValue: 10
  },
  last_heartbeat: {
    type: DataTypes.DATE,
    defaultValue: DataTypes.NOW
  }
}, {
  tableName: 'system_state',
  timestamps: false
});

module.exports = SystemState;
