const sqlite3 = require('sqlite3');
const { open } = require('sqlite');
const path = require('path');

async function checkRevenue() {
  const db = await open({
    filename: path.join(__dirname, 'parking.db'),
    driver: sqlite3.Database,
  });

  const todayStr = new Date().toISOString().substring(0, 10);
  const logs = await db.all(`SELECT * FROM parking_logs WHERE status = 'completed' AND exitTime LIKE ?`, [`${todayStr}%`]);
  const sum = logs.reduce((acc, l) => acc + l.fee, 0);
  
  console.log('Today logs:', logs);
  console.log('Calculated Sum:', sum);
}

checkRevenue();
