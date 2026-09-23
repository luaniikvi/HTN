const { DataTypes } = require('sequelize');
const sequelize = require('../config/db');

const AccessLog = sequelize.define('AccessLog', {
  id: {
    type: DataTypes.INTEGER,
    autoIncrement: true,
    primaryKey: true
  },
  device_id: {
    type: DataTypes.STRING(64),
    allowNull: false
  },
  event_type: {
    type: DataTypes.ENUM('FACE_AUTH_SUCCESS', 'DOOR_OPENED', 'DOOR_CLOSED'),
    allowNull: false
  },
  face_id: {
    type: DataTypes.INTEGER,
    allowNull: true
  },
  details: {
    type: DataTypes.TEXT,
    allowNull: true
  },
  timestamp: {
    type: DataTypes.DATE,
    defaultValue: DataTypes.NOW
  }
}, {
  tableName: 'access_logs',
  timestamps: false
});

module.exports = AccessLog;
