const { DataTypes } = require('sequelize');
const sequelize = require('../config/db');

const Face = sequelize.define('Face', {
  id: {
    type: DataTypes.INTEGER,
    autoIncrement: true,
    primaryKey: true
  },
  face_id: {
    type: DataTypes.INTEGER,
    allowNull: false,
    unique: true
  },
  name: {
    type: DataTypes.STRING(128),
    allowNull: false
  },
  role_type: {
    type: DataTypes.ENUM('PERMANENT', 'TEMPORARY'),
    allowNull: false,
    defaultValue: 'PERMANENT'
  },
  valid_until: {
    type: DataTypes.DATE,
    allowNull: true
  },
  is_active: {
    type: DataTypes.BOOLEAN,
    allowNull: false,
    defaultValue: true
  }
}, {
  tableName: 'faces',
  timestamps: true,
  createdAt: 'created_at',
  updatedAt: 'updated_at'
});

module.exports = Face;
