import sqlite3 from 'sqlite3';
import { open, Database } from 'sqlite';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const dbPath = path.join(__dirname, 'parking.db');

export interface User {
  uid: string;
  name: string;
  studentId: string;
  plateNumber: string;
  vehicleType: 'Car' | 'Motorbike';
  balance: number;
  status: 'active' | 'locked';
}

export interface ParkingLog {
  id: string;
  uid: string;
  name?: string; // Joined from users
  plateNumber: string;
  vehicleType: 'Car' | 'Motorbike';
  entryTime: string;
  exitTime: string | null;
  fee: number;
  status: 'parked' | 'completed';
}

export interface Transaction {
  id: string;
  uid: string;
  name?: string; // Joined from users
  plateNumber?: string; // Joined from users
  type: 'topup' | 'subtraction';
  amount: number;
  timestamp: string;
  balanceAfter: number;
}

let dbInstance: Database<sqlite3.Database, sqlite3.Statement> | null = null;

export async function getDB() {
  if (dbInstance) return dbInstance;

  dbInstance = await open({
    filename: dbPath,
    driver: sqlite3.Database,
  });

  await initSchema(dbInstance);
  return dbInstance;
}

async function initSchema(db: Database) {
  // Create tables
  await db.exec(`
    CREATE TABLE IF NOT EXISTS users (
      uid TEXT PRIMARY KEY,
      name TEXT NOT NULL,
      studentId TEXT NOT NULL,
      plateNumber TEXT NOT NULL,
      vehicleType TEXT NOT NULL,
      balance INTEGER DEFAULT 0,
      status TEXT DEFAULT 'active'
    );
  `);

  await db.exec(`
    CREATE TABLE IF NOT EXISTS parking_logs (
      id TEXT PRIMARY KEY,
      uid TEXT NOT NULL,
      plateNumber TEXT NOT NULL,
      vehicleType TEXT NOT NULL,
      entryTime TEXT NOT NULL,
      exitTime TEXT,
      fee INTEGER DEFAULT 0,
      status TEXT DEFAULT 'parked'
    );
  `);

  await db.exec(`
    CREATE TABLE IF NOT EXISTS transactions (
      id TEXT PRIMARY KEY,
      uid TEXT NOT NULL,
      type TEXT NOT NULL,
      amount INTEGER NOT NULL,
      timestamp TEXT NOT NULL,
      balanceAfter INTEGER NOT NULL
    );
  `);
}

// --- DB Helper Functions ---

export async function getUsers(): Promise<User[]> {
  const db = await getDB();
  return db.all<User[]>('SELECT * FROM users ORDER BY name ASC');
}

export async function getUserByUid(uid: string): Promise<User | undefined> {
  const db = await getDB();
  return db.get<User>('SELECT * FROM users WHERE uid = ?', [uid]);
}

export async function createUser(user: User): Promise<void> {
  const db = await getDB();
  await db.run(
    'INSERT INTO users (uid, name, studentId, plateNumber, vehicleType, balance, status) VALUES (?, ?, ?, ?, ?, ?, ?)',
    [user.uid, user.name, user.studentId, user.plateNumber, user.vehicleType, user.balance, user.status]
  );
}

export async function updateUser(uid: string, updates: Partial<User>): Promise<User | undefined> {
  const db = await getDB();
  const existing = await getUserByUid(uid);
  if (!existing) return undefined;

  const merged = { ...existing, ...updates };
  await db.run(
    'UPDATE users SET name = ?, studentId = ?, plateNumber = ?, vehicleType = ?, balance = ?, status = ? WHERE uid = ?',
    [merged.name, merged.studentId, merged.plateNumber, merged.vehicleType, merged.balance, merged.status, uid]
  );
  return merged;
}

export async function deleteUser(uid: string): Promise<boolean> {
  const db = await getDB();
  const res = await db.run('DELETE FROM users WHERE uid = ?', [uid]);
  return (res.changes || 0) > 0;
}

export async function topupUser(uid: string, amount: number): Promise<number | undefined> {
  const db = await getDB();
  const user = await getUserByUid(uid);
  if (!user) return undefined;

  const newBalance = user.balance + amount;
  await db.run('UPDATE users SET balance = ? WHERE uid = ?', [newBalance, uid]);

  // Log transaction
  const txId = 'tx_' + Math.random().toString(36).substring(2, 11);
  await db.run(
    'INSERT INTO transactions (id, uid, type, amount, timestamp, balanceAfter) VALUES (?, ?, ?, ?, ?, ?)',
    [txId, uid, 'topup', amount, new Date().toISOString(), newBalance]
  );

  return newBalance;
}

export async function getParkingLogs(): Promise<ParkingLog[]> {
  const db = await getDB();
  // Join users to get name if available
  return db.all<ParkingLog[]>(`
    SELECT pl.*, u.name 
    FROM parking_logs pl
    LEFT JOIN users u ON pl.uid = u.uid
    ORDER BY pl.entryTime DESC
  `);
}

export async function getTransactionLogs(): Promise<Transaction[]> {
  const db = await getDB();
  return db.all<Transaction[]>(`
    SELECT t.*, u.name, u.plateNumber 
    FROM transactions t
    LEFT JOIN users u ON t.uid = u.uid
    ORDER BY t.timestamp DESC
  `);
}

export async function getParkedVehicles(): Promise<ParkingLog[]> {
  const db = await getDB();
  return db.all<ParkingLog[]>(`
    SELECT pl.*, u.name 
    FROM parking_logs pl
    LEFT JOIN users u ON pl.uid = u.uid
    WHERE pl.status = 'parked'
    ORDER BY pl.entryTime DESC
  `);
}

export async function getLastParkingLog(uid: string): Promise<ParkingLog | undefined> {
  const db = await getDB();
  return db.get<ParkingLog>(
    'SELECT * FROM parking_logs WHERE uid = ? ORDER BY entryTime DESC LIMIT 1',
    [uid]
  );
}

export async function logParkingEntry(uid: string, plateNumber: string, vehicleType: 'Car' | 'Motorbike', entryTime?: string): Promise<ParkingLog> {
  const db = await getDB();
  const id = 'log_' + Math.random().toString(36).substring(2, 11);
  const time = entryTime || new Date().toISOString();

  await db.run(
    'INSERT INTO parking_logs (id, uid, plateNumber, vehicleType, entryTime, exitTime, fee, status) VALUES (?, ?, ?, ?, ?, NULL, 0, "parked")',
    [id, uid, plateNumber, vehicleType, time]
  );

  const newLog = await db.get<ParkingLog>(`
    SELECT pl.*, u.name 
    FROM parking_logs pl
    LEFT JOIN users u ON pl.uid = u.uid
    WHERE pl.id = ?
  `, [id]);
  return newLog!;
}

export async function logParkingExit(logId: string, fee: number, exitTime?: string): Promise<ParkingLog | undefined> {
  const db = await getDB();
  const time = exitTime || new Date().toISOString();
  
  await db.run(
    'UPDATE parking_logs SET exitTime = ?, fee = ?, status = "completed" WHERE id = ?',
    [time, fee, logId]
  );

  const log = await db.get<ParkingLog>(`
    SELECT pl.*, u.name 
    FROM parking_logs pl
    LEFT JOIN users u ON pl.uid = u.uid
    WHERE pl.id = ?
  `, [logId]);
  
  return log;
}
