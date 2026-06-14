const sqlite3 = require('sqlite3');
const { open } = require('sqlite');
const path = require('path');

async function fixDB() {
  const db = await open({
    filename: path.join(__dirname, 'parking.db'),
    driver: sqlite3.Database,
  });

  await db.run("UPDATE parking_logs SET status='completed', exitTime=datetime('now'), fee=0 WHERE uid='C95EC805' AND status='parked' AND entryTime < '2026-06-14T17:00:00'");
  console.log('Fixed hanging log.');
}

fixDB();
