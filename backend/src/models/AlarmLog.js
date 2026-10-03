const { DataTypes } = require('sequelize');
const sequelize = require('../config/db');

const AlarmLog = sequelize.define('AlarmLog', {
  id: {
    type: DataTypes.INTEGER,
    autoIncrement: true,
    primaryKey: true
  },
  device_id: {
    type: DataTypes.STRING(64),
    allowNull: false
  },
  mode: {
    type: DataTypes.ENUM('ARMED', 'STAY', 'DISARMED'),
    allowNull: false
  },
  event: {
    type: DataTypes.STRING(64),
    allowNull: false
  },
  details: {
    type: DataTypes.TEXT,
    allowNull: true
  },
  resolved_at: {
    type: DataTypes.DATE,
    allowNull: true
  },
  image_url: {
    type: DataTypes.STRING(255),
    allowNull: true
  },
  timestamp: {
    type: DataTypes.DATE,
    defaultValue: DataTypes.NOW
  }
}, {
  tableName: 'alarm_logs',
  timestamps: false
});

module.exports = AlarmLog;
