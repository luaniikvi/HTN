const sequelize = require('../config/db');
const User = require('./User');
const Face = require('./Face');
const AccessLog = require('./AccessLog');
const AlarmLog = require('./AlarmLog');
const SystemState = require('./SystemState');

module.exports = {
  sequelize,
  User,
  Face,
  AccessLog,
  AlarmLog,
  SystemState
};
