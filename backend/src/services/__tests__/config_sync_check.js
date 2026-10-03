const assert = require('assert');
const mqttGateway = require('../mqttGateway.service');

// Mock client to test publishCommand and sendConfigCommand without a live broker
let published = null;
mqttGateway.client = {
  connected: true,
  publish: (topic, message, options) => {
    published = { topic, payload: JSON.parse(message), options };
  }
};

// 1. Check sendConfigCommand sends retain: true and valid payload
const ok = mqttGateway.sendConfigCommand(15, 'dev_01');
assert.strictEqual(ok, true, 'sendConfigCommand should return true when client is connected');
assert.strictEqual(published.topic, 'device/dev_01/cmd/config');
assert.deepStrictEqual(published.payload, { grace_period: 15 });
assert.strictEqual(published.options.retain, true, 'config message MUST be retained');
assert.strictEqual(published.options.qos, 1, 'config message MUST use QoS 1');

// 2. Check subscribeTopics includes events/config and events/faces
const subscribed = [];
mqttGateway.client.subscribe = (topics, opts, cb) => {
  subscribed.push(...topics);
};
mqttGateway.subscribeTopics();
assert.ok(subscribed.includes('device/+/events/config'), 'Must subscribe to device/+/events/config');
assert.ok(subscribed.includes('device/+/events/faces'), 'Must subscribe to device/+/events/faces');

console.log('✅ All config and face sync assertions passed successfully!');
