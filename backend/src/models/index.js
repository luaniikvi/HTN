const sequelize = require('../config/db');
const User = require('./User');
const Face = require('./Face');
const AccessLog = require('./AccessLog');
const AlarmLog = require('./AlarmLog');
const SystemState = require('./SystemState');

AccessLog.belongsTo(Face, { foreignKey: 'face_id', targetKey: 'face_id', as: 'face', constraints: false });
Face.hasMany(AccessLog, { foreignKey: 'face_id', sourceKey: 'face_id', as: 'access_logs', constraints: false });

module.exports = {
  sequelize,
  User,
  Face,
  AccessLog,
  AlarmLog,
  SystemState
};
