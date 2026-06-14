import express from 'express';
import { createServer } from 'http';
import { Server } from 'socket.io';
import mqtt from 'mqtt';
import cors from 'cors';
import path from 'path';
import { fileURLToPath } from 'url';
import { createServer as createViteServer } from 'vite';

import {
  getUsers,
  getUserByUid,
  createUser,
  updateUser,
  deleteUser,
  topupUser,
  getParkingLogs,
  getTransactionLogs,
  getParkedVehicles,
  getLastParkingLog,
  logParkingEntry,
  logParkingExit
} from './db';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// --- In-memory device status state ---
interface DeviceStatus {
  esp32: 'online' | 'offline';
  mqtt: 'connected' | 'disconnected';
  wifi: 'connected' | 'disconnected';
  pn532: 'connected' | 'disconnected';
  barrier_in: 'open' | 'closed';
  barrier_out: 'open' | 'closed';
  lastSeen: number;
}

let deviceStatus: DeviceStatus = {
  esp32: 'offline',
  mqtt: 'disconnected',
  wifi: 'disconnected',
  pn532: 'disconnected',
  barrier_in: 'closed',
  barrier_out: 'closed',
  lastSeen: 0
};

// --- MQTT Setup ---
const mqttBroker = process.env.MQTT_BROKER_URL || 'mqtt://broker.hivemq.com';
const mqttOptions = {
  username: process.env.MQTT_USERNAME,
  password: process.env.MQTT_PASSWORD,
};

const mqttClient = mqtt.connect(mqttBroker, mqttOptions);
const TOPIC_PREFIX = process.env.MQTT_TOPIC_PREFIX || 'parking/system';

mqttClient.on('connect', () => {
  console.log('Connected to MQTT Broker');
  deviceStatus.mqtt = 'connected';
  mqttClient.subscribe(`${TOPIC_PREFIX}/scan`);
  mqttClient.subscribe(`${TOPIC_PREFIX}/event`);
  mqttClient.subscribe(`${TOPIC_PREFIX}/device/status`);
});

mqttClient.on('close', () => {
  console.log('MQTT Connection closed');
  deviceStatus.mqtt = 'disconnected';
});

// --- Express & Socket.io Setup ---
async function startServer() {
  const app = express();
  const httpServer = createServer(app);
  const io = new Server(httpServer, {
    cors: { origin: '*' }
  });

  app.use(cors());
  app.use(express.json());

  // Intercept outgoing publish for socket logs
  const originalPublish = mqttClient.publish.bind(mqttClient);
  mqttClient.publish = (topic: string, message: string | Buffer, ...args: any[]) => {
    io.emit('mqtt-log', {
      topic,
      payload: message.toString(),
      direction: 'out',
      time: new Date().toLocaleTimeString()
    });
    return originalPublish(topic, message, ...args);
  };

  // Heartbeat check for ESP32 connection (runs every 5 seconds)
  setInterval(() => {
    const now = Date.now();
    if (deviceStatus.esp32 === 'online' && now - deviceStatus.lastSeen > 15000) {
      console.log('ESP32 status timeout, setting to offline');
      deviceStatus.esp32 = 'offline';
      deviceStatus.wifi = 'disconnected';
      deviceStatus.pn532 = 'disconnected';
      io.emit('device-status-update', deviceStatus);
    }
  }, 5000);

  mqttClient.on('message', async (topic, message) => {
    try {
      const payloadString = message.toString();
      
      // Emit incoming MQTT log to socket client
      io.emit('mqtt-log', {
        topic,
        payload: payloadString,
        direction: 'in',
        time: new Date().toLocaleTimeString()
      });

      console.log(`MQTT Received: ${topic} -> ${payloadString}`);

      if (topic === `${TOPIC_PREFIX}/device/status`) {
        const data = JSON.parse(payloadString);
        deviceStatus = {
          ...deviceStatus,
          ...data,
          esp32: data.esp32 || 'online',
          lastSeen: Date.now()
        };
        io.emit('device-status-update', deviceStatus);
      } 
      else if (topic === `${TOPIC_PREFIX}/scan` || topic === `${TOPIC_PREFIX}/event`) {
        let uid = '';
        let explicitEvent: 'checkin' | 'checkout' | null = null;
        
        // Try parsing JSON payload
        try {
          const json = JSON.parse(payloadString);
          uid = json.uid;
          if (json.event === 'checkin' || json.event === 'checkout') {
            explicitEvent = json.event;
          }
        } catch {
          // If not JSON, it's just raw UID string
          uid = payloadString.trim();
        }

        if (uid) {
          await handleScan(uid, explicitEvent);
        }
      }
    } catch (err) {
      console.error('Error handling MQTT message:', err);
    }
  });

  // Handle scans (RFID events)
  async function handleScan(uid: string, explicitEvent: 'checkin' | 'checkout' | null) {
    const user = await getUserByUid(uid);
    
    // 1. Unrecognized Card
    if (!user) {
      console.log(`Unknown card scanned: ${uid}`);
      io.emit('unknown-scan', uid);
      mqttClient.publish(`${TOPIC_PREFIX}/response/${uid}`, JSON.stringify({ cmd: "unknown_card", uid }));
      mqttClient.publish(`${TOPIC_PREFIX}/response`, 'UNKNOWN');
      return;
    }

    // 2. Card Locked
    if (user.status === 'locked') {
      console.log(`Locked card scanned: ${uid}`);
      mqttClient.publish(`${TOPIC_PREFIX}/response/${uid}`, JSON.stringify({ cmd: "lock_card", uid }));
      mqttClient.publish(`${TOPIC_PREFIX}/response`, 'REJECTED');
      return;
    }

    // Determine event: check-in vs check-out
    let isEntry = true;
    const lastLog = await getLastParkingLog(uid);
    
    if (explicitEvent) {
      isEntry = (explicitEvent === 'checkin');
    } else {
      isEntry = !lastLog || lastLog.status === 'completed';
    }

    if (isEntry) {
      if (lastLog && lastLog.status === 'parked') {
        console.log(`Error: Vehicle already parked for UID ${uid}`);
        mqttClient.publish(`${TOPIC_PREFIX}/response/${uid}`, JSON.stringify({ cmd: "error", msg: "Vehicle already parked" }));
        mqttClient.publish(`${TOPIC_PREFIX}/response`, 'ERROR_ALREADY_PARKED');
        return;
      }

      // --- Process Check-In ---
      const log = await logParkingEntry(uid, user.plateNumber, user.vehicleType);
      
      console.log(`User Checked In: ${user.name}`);
      io.emit('new-parking-log', log);
      io.emit('stats-update');
      
      mqttClient.publish(`${TOPIC_PREFIX}/response/${uid}`, JSON.stringify({ cmd: "open_gate_in", uid }));
      mqttClient.publish(`${TOPIC_PREFIX}/response`, 'OPEN_ENTRY');
      
      // Update barrier state tentatively
      deviceStatus.barrier_in = 'open';
      io.emit('device-status-update', deviceStatus);
    } else {
      // --- Process Check-Out ---
      const lastLog = await getLastParkingLog(uid);
      
      if (lastLog && lastLog.status === 'parked') {
        let fee = 3000;
        try {
          const entryDate = new Date(lastLog.entryTime);
          const exitDate = new Date();
          
          if (isNaN(entryDate.getTime())) {
            fee = 3000;
          } else {
            const isSameDay = entryDate.getFullYear() === exitDate.getFullYear() && 
                              entryDate.getMonth() === exitDate.getMonth() && 
                              entryDate.getDate() === exitDate.getDate();
            if (!isSameDay) {
              fee = 11000;
            } else {
              const exitHour = exitDate.getHours();
              if (exitHour >= 18) {
                fee = 5000;
              } else {
                fee = 3000;
              }
            }
          }
        } catch (e) {
          fee = 3000;
        }

        // Calculate fee for display only (no balance deduction)
        const log = await logParkingExit(lastLog.id, fee);
        const updatedUser = await getUserByUid(uid);
        
        console.log(`User Checked Out: ${user.name}, Fee Recorded: ${fee} (Not Deducted)`);
        io.emit('new-parking-log', log);
        
        // Emit transaction log update
        const txs = await getTransactionLogs();
        if (txs.length > 0) {
          io.emit('new-transaction-log', txs[0]);
        }

        io.emit('users-update');
        io.emit('stats-update');

        mqttClient.publish(`${TOPIC_PREFIX}/response/${uid}`, JSON.stringify({ 
          cmd: "open_gate_out", 
          uid, 
          balance: updatedUser?.balance || 0 
        }));
        mqttClient.publish(`${TOPIC_PREFIX}/response`, 'OPEN_EXIT');

        // Update barrier state tentatively
        deviceStatus.barrier_out = 'open';
        io.emit('device-status-update', deviceStatus);
        
        // Sync balance to ESP32
        mqttClient.publish(`${TOPIC_PREFIX}/cmd`, JSON.stringify({
          cmd: "update_balance",
          uid: uid,
          balance: updatedUser?.balance || 0
        }));
      } else {
        console.log('Error: Found no active parking entry for checkout of UID ' + uid);
        mqttClient.publish(`${TOPIC_PREFIX}/response/${uid}`, JSON.stringify({ cmd: "error", msg: "No active entry" }));
        mqttClient.publish(`${TOPIC_PREFIX}/response`, 'ERROR_NO_ENTRY');
      }
    }
  }

  // --- API Routes ---

  // Dashboard Stats
  app.get('/api/stats', async (req, res) => {
    try {
      const allUsers = await getUsers();
      const parkedVehicles = await getParkedVehicles();
      const logs = await getParkingLogs();
      const txs = await getTransactionLogs();

      const totalUsers = allUsers.length;
      const activeVehicles = parkedVehicles.length;
      const totalLocked = allUsers.filter(u => u.status === 'locked').length;

      // Revenue today
      const todayStr = new Date().toISOString().substring(0, 10);
      const dailyRevenue = logs
        .filter(l => l.status === 'completed' && l.exitTime && l.exitTime.startsWith(todayStr))
        .reduce((sum, l) => sum + (l.fee || 0), 0);

      // Total entries today
      const dailyEntries = logs.filter(l => l.entryTime.startsWith(todayStr)).length;

      res.json({ activeVehicles, dailyRevenue, totalUsers, dailyEntries, totalLocked });
    } catch (err) {
      res.status(500).json({ error: (err as Error).message });
    }
  });

  // User list
  app.get('/api/users', async (req, res) => {
    try {
      const list = await getUsers();
      res.json(list);
    } catch (err) {
      res.status(500).json({ error: (err as Error).message });
    }
  });

  // Create User
  app.post('/api/users', async (req, res) => {
    try {
      const newUser = req.body;
      await createUser(newUser);
      
      // Notify ESP32 about new user
      mqttClient.publish(`${TOPIC_PREFIX}/cmd`, JSON.stringify({
        cmd: 'add_user',
        uid: newUser.uid,
        name: newUser.name,
        studentId: newUser.studentId,
        plateNumber: newUser.plateNumber,
        balance: newUser.balance
      }));

      io.emit('users-update');
      io.emit('stats-update');
      res.status(201).json(newUser);
    } catch (err) {
      res.status(500).json({ error: (err as Error).message });
    }
  });

  // Update User
  app.patch('/api/users/:uid', async (req, res) => {
    try {
      const { uid } = req.params;
      const updates = req.body;
      const user = await updateUser(uid, updates);

      if (!user) {
        return res.status(404).json({ error: 'User not found' });
      }

      // Sync user edits to ESP32
      if (updates.status) {
        mqttClient.publish(`${TOPIC_PREFIX}/cmd`, JSON.stringify({
          cmd: updates.status === 'active' ? 'unlock_card' : 'lock_card',
          uid
        }));
      }

      if (updates.balance !== undefined) {
        mqttClient.publish(`${TOPIC_PREFIX}/cmd`, JSON.stringify({
          cmd: 'update_balance',
          uid,
          balance: updates.balance
        }));
      }

      // Sync other fields as well if needed
      mqttClient.publish(`${TOPIC_PREFIX}/cmd`, JSON.stringify({
        cmd: 'update_user',
        uid,
        name: user.name,
        studentId: user.studentId,
        plateNumber: user.plateNumber
      }));

      io.emit('users-update');
      io.emit('stats-update');
      res.json(user);
    } catch (err) {
      res.status(500).json({ error: (err as Error).message });
    }
  });

  // Delete User
  app.delete('/api/users/:uid', async (req, res) => {
    try {
      const { uid } = req.params;
      const success = await deleteUser(uid);
      if (success) {
        // Sync delete to ESP32
        mqttClient.publish(`${TOPIC_PREFIX}/cmd`, JSON.stringify({
          cmd: 'delete_user',
          uid
        }));

        io.emit('users-update');
        io.emit('stats-update');
        res.json({ success: true });
      } else {
        res.status(404).json({ error: 'User not found' });
      }
    } catch (err) {
      res.status(500).json({ error: (err as Error).message });
    }
  });

  // Top Up ví
  app.post('/api/topup', async (req, res) => {
    try {
      const { uid, amount } = req.body;
      const newBalance = await topupUser(uid, amount);
      if (newBalance !== undefined) {
        // Sync balance to ESP32
        mqttClient.publish(`${TOPIC_PREFIX}/cmd`, JSON.stringify({
          cmd: 'update_balance',
          uid,
          balance: newBalance
        }));

        io.emit('users-update');
        io.emit('stats-update');
        
        // Emit transaction update
        const txs = await getTransactionLogs();
        if (txs.length > 0) {
          io.emit('new-transaction-log', txs[0]);
        }

        res.json({ success: true, newBalance });
      } else {
        res.status(404).json({ error: 'User not found' });
      }
    } catch (err) {
      res.status(500).json({ error: (err as Error).message });
    }
  });

  // Logs queries
  app.get('/api/logs/parking', async (req, res) => {
    try {
      const list = await getParkingLogs();
      res.json(list);
    } catch (err) {
      res.status(500).json({ error: (err as Error).message });
    }
  });

  app.get('/api/logs/transactions', async (req, res) => {
    try {
      const list = await getTransactionLogs();
      res.json(list);
    } catch (err) {
      res.status(500).json({ error: (err as Error).message });
    }
  });

  app.get('/api/logs/parked', async (req, res) => {
    try {
      const list = await getParkedVehicles();
      res.json(list);
    } catch (err) {
      res.status(500).json({ error: (err as Error).message });
    }
  });

  // Manual Barrier Control
  app.post('/api/control/barrier', (req, res) => {
    const { gate, action } = req.body; // gate: 'in' | 'out', action: 'open' | 'close'
    const cmd = `${action}_gate_${gate}`; // e.g. open_gate_in, close_gate_in, open_gate_out, close_gate_out

    console.log(`Remote barrier command triggered: ${cmd}`);
    
    // Publish control command to MQTT
    mqttClient.publish(`${TOPIC_PREFIX}/cmd`, JSON.stringify({ cmd }));

    // Tentatively update state locally for UI speed
    if (gate === 'in') {
      deviceStatus.barrier_in = action === 'open' ? 'open' : 'closed';
    } else if (gate === 'out') {
      deviceStatus.barrier_out = action === 'open' ? 'open' : 'closed';
    }
    
    io.emit('device-status-update', deviceStatus);
    res.json({ success: true, deviceStatus });
  });

  // Fetch device status
  app.get('/api/status/device', (req, res) => {
    res.json(deviceStatus);
  });

  // --- Vite Integration ---
  if (process.env.NODE_ENV !== 'production') {
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: 'spa',
    });
    app.use(vite.middlewares);
  } else {
    app.use(express.static(path.join(__dirname, 'dist')));
    app.get('*', (req, res) => {
      res.sendFile(path.join(__dirname, 'dist', 'index.html'));
    });
  }

  const PORT = 3000;
  httpServer.listen(PORT, '0.0.0.0', () => {
    console.log(`Server running on http://localhost:${PORT}`);
  });
}

startServer();
