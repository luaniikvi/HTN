CREATE DATABASE IF NOT EXISTS doorlock_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE doorlock_db;

-- 1. Bảng quản lý người dùng / quản trị viên
CREATE TABLE IF NOT EXISTS users (
  id INT AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(64) NOT NULL UNIQUE,
  password VARCHAR(255) NOT NULL,
  fullname VARCHAR(128) NULL,
  role ENUM('admin', 'operator') DEFAULT 'admin',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- 2. Bảng quản lý danh sách khuôn mặt đồng bộ với Flash NVS trên MCU
CREATE TABLE IF NOT EXISTS faces (
  id INT AUTO_INCREMENT PRIMARY KEY,
  face_id INT NOT NULL UNIQUE,
  name VARCHAR(128) NOT NULL,
  role_type ENUM('PERMANENT', 'TEMPORARY') NOT NULL DEFAULT 'PERMANENT',
  valid_until DATETIME NULL,
  is_active BOOLEAN NOT NULL DEFAULT TRUE,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_valid_until (valid_until),
  INDEX idx_is_active (is_active)
) ENGINE=InnoDB;

-- 3. Bảng nhật ký ra vào (Access Logs)
CREATE TABLE IF NOT EXISTS access_logs (
  id INT AUTO_INCREMENT PRIMARY KEY,
  device_id VARCHAR(64) NOT NULL,
  event_type ENUM('FACE_AUTH_SUCCESS', 'DOOR_OPENED', 'DOOR_CLOSED') NOT NULL,
  face_id INT NULL,
  details TEXT NULL,
  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB;

-- 4. Bảng nhật ký cảnh báo an ninh & vi phạm (Alarm Logs)
CREATE TABLE IF NOT EXISTS alarm_logs (
  id INT AUTO_INCREMENT PRIMARY KEY,
  device_id VARCHAR(64) NOT NULL,
  mode ENUM('ARMED', 'STAY', 'DISARMED') NOT NULL,
  event VARCHAR(64) NOT NULL,
  details TEXT NULL,
  image_url VARCHAR(255) NULL,
  resolved_at DATETIME NULL,
  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB;

-- 5. Bảng lưu trạng thái hệ thống và thiết bị
CREATE TABLE IF NOT EXISTS system_state (
  device_id VARCHAR(64) PRIMARY KEY,
  status ENUM('ONLINE', 'OFFLINE') DEFAULT 'OFFLINE',
  door_state ENUM('OPEN', 'CLOSED') DEFAULT 'CLOSED',
  security_mode ENUM('DISARMED', 'ARMED', 'STAY') DEFAULT 'DISARMED',
  forced_alarm BOOLEAN DEFAULT FALSE,
  armed_latched BOOLEAN DEFAULT FALSE,
  grace_period INT DEFAULT 10,
  last_heartbeat TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- Chèn tài khoản quản trị mặc định (admin / admin123, hash bcrypt chuẩn)
INSERT INTO users (username, password, fullname, role)
VALUES ('admin', '$2a$10$CEoCy7QkV3n3wUnWZtM7ce.emTvK.vUJc3uPWvf9v2jXFkScN1AP2', 'System Administrator', 'admin')
ON DUPLICATE KEY UPDATE password=VALUES(password);

-- Khởi tạo thiết bị dev_01 mặc định
INSERT INTO system_state (device_id, status, door_state, security_mode, grace_period)
VALUES ('dev_01', 'OFFLINE', 'CLOSED', 'DISARMED', 10)
ON DUPLICATE KEY UPDATE device_id=device_id;
