const fs = require('fs');
const path = require('path');

function getSslCredentials() {
  const keyPath = path.resolve(__dirname, '../../ssl/server.key');
  const certPath = path.resolve(__dirname, '../../ssl/server.crt');

  if (fs.existsSync(keyPath) && fs.existsSync(certPath)) {
    return {
      key: fs.readFileSync(keyPath),
      cert: fs.readFileSync(certPath)
    };
  }
  console.warn('⚠️ SSL certificate files not found, fallback to null');
  return null;
}

module.exports = { getSslCredentials };
