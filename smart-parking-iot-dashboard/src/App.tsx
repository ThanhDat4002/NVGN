import React, { useState, useEffect, useRef } from 'react';
import { 
  LayoutDashboard, 
  Users, 
  History, 
  Wallet, 
  Car, 
  LogOut, 
  Bell, 
  Search, 
  Plus, 
  Lock, 
  Unlock, 
  ArrowUpRight, 
  ArrowDownLeft,
  CircleDollarSign,
  Activity,
  Wifi,
  Server,
  HardDrive,
  Cpu,
  RefreshCw,
  Trash2,
  Edit3,
  Terminal,
  ShieldAlert,
  Play,
  Square,
  Clock,
  Settings
} from 'lucide-react';
import { motion, AnimatePresence } from 'motion/react';
import io from 'socket.io-client';
import { cn } from './lib/utils';

// --- Types ---
interface User {
  uid: string;
  name: string;
  studentId: string;
  plateNumber: string;
  vehicleType: 'Car' | 'Motorbike';
  balance: number;
  status: 'active' | 'locked';
}

interface ParkingLog {
  id: string;
  uid: string;
  name?: string;
  plateNumber: string;
  vehicleType: 'Car' | 'Motorbike';
  entryTime: string;
  exitTime: string | null;
  fee: number;
  status: 'parked' | 'completed';
}

interface Transaction {
  id: string;
  uid: string;
  name?: string;
  plateNumber?: string;
  type: 'topup' | 'subtraction';
  amount: number;
  timestamp: string;
  balanceAfter: number;
}

interface DeviceStatus {
  esp32: 'online' | 'offline';
  mqtt: 'connected' | 'disconnected';
  wifi: 'connected' | 'disconnected';
  pn532: 'connected' | 'disconnected';
  barrier_in: 'open' | 'closed';
  barrier_out: 'open' | 'closed';
}

interface Stats {
  activeVehicles: number;
  dailyRevenue: number;
  totalUsers: number;
  dailyEntries: number;
  totalLocked: number;
}

interface MqttLog {
  topic: string;
  payload: string;
  direction: 'in' | 'out';
  time: string;
}

const socket = io();

// --- Live Ticking Parked Duration Component ---
function ParkedDuration({ entryTime }: { entryTime: string }) {
  const [duration, setDuration] = useState('');

  useEffect(() => {
    const calculateDuration = () => {
      const diff = Date.now() - new Date(entryTime).getTime();
      const totalMinutes = Math.floor(diff / 60000);
      const hours = Math.floor(totalMinutes / 60);
      const minutes = totalMinutes % 60;
      const days = Math.floor(hours / 24);

      if (days > 0) {
        setDuration(`${days}d ${hours % 24}h ${minutes}m`);
      } else if (hours > 0) {
        setDuration(`${hours}h ${minutes}m`);
      } else {
        setDuration(`${minutes}m`);
      }
    };

    calculateDuration();
    const timer = setInterval(calculateDuration, 30000); // Update every 30s
    return () => clearInterval(timer);
  }, [entryTime]);

  return <span className="font-mono text-slate-300">{duration}</span>;
}

export default function App() {
  const [activeTab, setActiveTab] = useState<'dashboard' | 'users' | 'parked' | 'parking-logs' | 'transactions'>('dashboard');
  const [stats, setStats] = useState<Stats>({ activeVehicles: 0, dailyRevenue: 0, totalUsers: 0, dailyEntries: 0, totalLocked: 0 });
  const [users, setUsers] = useState<User[]>([]);
  const [parkingLogs, setParkingLogs] = useState<ParkingLog[]>([]);
  const [transactions, setTransactions] = useState<Transaction[]>([]);
  const [parkedVehicles, setParkedVehicles] = useState<ParkingLog[]>([]);
  const [deviceStatus, setDeviceStatus] = useState<DeviceStatus>({
    esp32: 'offline',
    mqtt: 'disconnected',
    wifi: 'disconnected',
    pn532: 'disconnected',
    barrier_in: 'closed',
    barrier_out: 'closed'
  });
  const [mqttLogs, setMqttLogs] = useState<MqttLog[]>([]);
  
  const [searchQuery, setSearchQuery] = useState('');
  const [isTopUpModalOpen, setIsTopUpModalOpen] = useState(false);
  const [isUserModalOpen, setIsUserModalOpen] = useState(false); // Used for both Add and Edit
  const [selectedUser, setSelectedUser] = useState<User | null>(null);
  const [topUpAmount, setTopUpAmount] = useState<number>(0);
  
  const [userModalMode, setUserModalMode] = useState<'add' | 'edit'>('add');
  const [userFormData, setUserFormData] = useState({
    uid: '',
    name: '',
    studentId: '',
    plateNumber: '',
    vehicleType: 'Motorbike' as 'Car' | 'Motorbike',
    balance: 0
  });

  const terminalEndRef = useRef<HTMLDivElement>(null);

  // Fetch all APIs
  const fetchAllData = () => {
    fetchStats();
    fetchUsers();
    fetchParkingLogs();
    fetchTransactionLogs();
    fetchParkedVehicles();
    fetchDeviceStatus();
  };

  useEffect(() => {
    fetchAllData();

    // Socket listeners
    socket.on('connect', () => {
      console.log('Socket Connected');
    });

    socket.on('new-parking-log', (log: ParkingLog) => {
      setParkingLogs(prev => {
        const exists = prev.findIndex(p => p.id === log.id);
        if (exists !== -1) {
          const updated = [...prev];
          updated[exists] = log;
          return updated;
        }
        return [log, ...prev];
      });
      fetchParkedVehicles();
      fetchStats();
    });

    socket.on('new-transaction-log', (tx: Transaction) => {
      setTransactions(prev => [tx, ...prev]);
      fetchStats();
      fetchUsers();
    });

    socket.on('device-status-update', (status: DeviceStatus) => {
      setDeviceStatus(status);
    });

    socket.on('users-update', () => {
      fetchUsers();
      fetchStats();
    });

    socket.on('stats-update', fetchStats);

    socket.on('unknown-scan', (uid: string) => {
      setIsUserModalOpen(isOpen => {
        if (isOpen) {
          setUserFormData(prev => ({ ...prev, uid }));
        }
        return isOpen;
      });
    });

    socket.on('mqtt-log', (log: MqttLog) => {
      setMqttLogs(prev => [log, ...prev].slice(0, 50)); // Limit to 50 logs
    });

    return () => {
      socket.off('new-parking-log');
      socket.off('new-transaction-log');
      socket.off('device-status-update');
      socket.off('users-update');
      socket.off('stats-update');
      socket.off('unknown-scan');
      socket.off('mqtt-log');
    };
  }, []);

  // Scroll terminal logs to bottom on update
  useEffect(() => {
    if (terminalEndRef.current) {
      terminalEndRef.current.scrollIntoView({ behavior: 'smooth' });
    }
  }, [mqttLogs, activeTab]);

  const fetchStats = async () => {
    const res = await fetch('/api/stats');
    if (res.ok) setStats(await res.json());
  };

  const fetchUsers = async () => {
    const res = await fetch('/api/users');
    if (res.ok) setUsers(await res.json());
  };

  const fetchParkingLogs = async () => {
    const res = await fetch('/api/logs/parking');
    if (res.ok) setParkingLogs(await res.json());
  };

  const fetchTransactionLogs = async () => {
    const res = await fetch('/api/logs/transactions');
    if (res.ok) setTransactions(await res.json());
  };

  const fetchParkedVehicles = async () => {
    const res = await fetch('/api/logs/parked');
    if (res.ok) setParkedVehicles(await res.json());
  };

  const fetchDeviceStatus = async () => {
    const res = await fetch('/api/status/device');
    if (res.ok) setDeviceStatus(await res.json());
  };

  // Toggle user active status
  const toggleUserStatus = async (uid: string, currentStatus: string) => {
    const newStatus = currentStatus === 'active' ? 'locked' : 'active';
    const res = await fetch(`/api/users/${uid}`, {
      method: 'PATCH',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ status: newStatus })
    });
    if (res.ok) fetchUsers();
  };

  // Delete User
  const handleDeleteUser = async (uid: string) => {
    if (!window.confirm(`Bạn có chắc chắn muốn xóa tài khoản thẻ ${uid}?`)) return;
    const res = await fetch(`/api/users/${uid}`, {
      method: 'DELETE'
    });
    if (res.ok) fetchUsers();
  };

  // Top Up ví
  const handleTopUp = async () => {
    if (!selectedUser || topUpAmount <= 0) return;
    const res = await fetch('/api/topup', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ uid: selectedUser.uid, amount: topUpAmount })
    });
    if (res.ok) {
      setIsTopUpModalOpen(false);
      setSelectedUser(null);
      setTopUpAmount(0);
      fetchUsers();
    }
  };

  // Save Add/Edit user form
  const handleSaveUser = async () => {
    if (!userFormData.uid || !userFormData.name || !userFormData.studentId || !userFormData.plateNumber) {
      alert('Vui lòng nhập đầy đủ thông tin bắt buộc.');
      return;
    }

    if (userModalMode === 'add') {
      const res = await fetch('/api/users', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ...userFormData, status: 'active' })
      });
      if (res.ok) {
        setIsUserModalOpen(false);
        fetchUsers();
      } else {
        const error = await res.json();
        alert('Lỗi: ' + error.error);
      }
    } else {
      // Edit mode
      const res = await fetch(`/api/users/${userFormData.uid}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          name: userFormData.name,
          studentId: userFormData.studentId,
          plateNumber: userFormData.plateNumber,
          vehicleType: userFormData.vehicleType,
          balance: userFormData.balance
        })
      });
      if (res.ok) {
        setIsUserModalOpen(false);
        fetchUsers();
      } else {
        alert('Lỗi khi cập nhật thông tin.');
      }
    }
  };

  // Open barrier gate manually
  const triggerBarrierControl = async (gate: 'in' | 'out', action: 'open' | 'close') => {
    await fetch('/api/control/barrier', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ gate, action })
    });
  };

  // Filter users based on query
  const filteredUsers = users.filter(u => 
    u.name.toLowerCase().includes(searchQuery.toLowerCase()) || 
    u.plateNumber.toLowerCase().includes(searchQuery.toLowerCase()) ||
    u.uid.toLowerCase().includes(searchQuery.toLowerCase()) ||
    u.studentId.toLowerCase().includes(searchQuery.toLowerCase())
  );

  // --- SVG Charts Calculations ---
  
  // Weekly revenue simulation/mock based on transactions
  const getWeeklyRevenueData = () => {
    const weekdays = ['T.Hai', 'T.Ba', 'T.Tư', 'T.Năm', 'T.Sáu', 'T.Bảy', 'C.Nhật'];
    const sums = [45000, 110000, 75000, 180000, 210000, 310000, stats.dailyRevenue || 140000];
    return { labels: weekdays, data: sums };
  };

  // Hourly Traffic simulation (check-in events by range of hours)
  const getHourlyTrafficData = () => {
    const ranges = ['07-09h', '09-12h', '12-15h', '15-18h', '18-21h', '21-24h'];
    const entries = [24, 18, 12, 35, 42, 15];
    const exits = [5, 12, 19, 22, 38, 29];
    return { labels: ranges, entries, exits };
  };

  // Vehicle Distribution (Car vs Motorbike counts in the parking lot)
  const getVehicleDistribution = () => {
    const parkedCars = parkedVehicles.filter(v => v.vehicleType === 'Car').length;
    const parkedBikes = parkedVehicles.filter(v => v.vehicleType === 'Motorbike').length;
    const total = parkedCars + parkedBikes || 1; // avoid divide by zero
    return {
      carCount: parkedCars,
      bikeCount: parkedBikes,
      carPercent: Math.round((parkedCars / total) * 100),
      bikePercent: Math.round((parkedBikes / total) * 100)
    };
  };

  const weeklyRev = getWeeklyRevenueData();
  const hourlyTraffic = getHourlyTrafficData();
  const vehicleDist = getVehicleDistribution();

  return (
    <div className="h-screen w-screen flex bg-gradient-to-br from-slate-950 via-slate-900 to-indigo-950 text-slate-100 overflow-hidden font-sans">
      
      {/* Sidebar */}
      <aside className="w-64 bg-slate-900/40 backdrop-blur-xl border-r border-slate-800/60 flex flex-col justify-between shrink-0">
        <div>
          {/* Logo */}
          <div className="p-6 flex items-center gap-3 border-b border-slate-800/60">
            <div className="w-10 h-10 bg-gradient-to-tr from-indigo-600 to-violet-500 rounded-xl flex items-center justify-center text-white shadow-lg shadow-indigo-500/20">
              <Car size={22} className="animate-pulse" />
            </div>
            <div>
              <h1 className="font-extrabold text-lg tracking-tight bg-gradient-to-r from-indigo-200 to-white bg-clip-text text-transparent">SmartPark</h1>
              <p className="text-[10px] text-slate-400 font-semibold tracking-widest uppercase">Dashboard v2.0</p>
            </div>
          </div>

          {/* Navigation */}
          <nav className="p-4 space-y-1.5">
            <NavItem 
              icon={<LayoutDashboard size={18} />} 
              label="Tổng quan" 
              active={activeTab === 'dashboard'} 
              onClick={() => setActiveTab('dashboard')} 
            />
            <NavItem 
              icon={<Users size={18} />} 
              label="Thành viên & Thẻ" 
              active={activeTab === 'users'} 
              onClick={() => setActiveTab('users')} 
            />
            <NavItem 
              icon={<Car size={18} />} 
              label="Xe đang đỗ" 
              active={activeTab === 'parked'} 
              badge={stats.activeVehicles}
              onClick={() => setActiveTab('parked')} 
            />
            <NavItem 
              icon={<History size={18} />} 
              label="Lịch sử vào ra" 
              active={activeTab === 'parking-logs'} 
              onClick={() => setActiveTab('parking-logs')} 
            />
            <NavItem 
              icon={<Wallet size={18} />} 
              label="Lịch sử ví" 
              active={activeTab === 'transactions'} 
              onClick={() => setActiveTab('transactions')} 
            />

          </nav>
        </div>

        {/* System Connections and Logout */}
        <div className="p-4 border-t border-slate-800/60 bg-slate-950/20 space-y-4">
          <div className="space-y-2">
            <p className="text-[9px] font-bold text-slate-500 uppercase tracking-widest">Trạng thái kết nối</p>
            <div className="grid grid-cols-2 gap-2 text-[10px]">
              <div className="flex items-center gap-1.5 bg-slate-900/40 p-1.5 rounded-lg border border-slate-800/40">
                <Server size={10} className={cn(deviceStatus.mqtt === 'connected' ? "text-emerald-400" : "text-slate-500")} />
                <span className="truncate">MQTT</span>
              </div>
              <div className="flex items-center gap-1.5 bg-slate-900/40 p-1.5 rounded-lg border border-slate-800/40">
                <Cpu size={10} className={cn(deviceStatus.esp32 === 'online' ? "text-emerald-400" : "text-red-400 animate-pulse")} />
                <span className="truncate">ESP32</span>
              </div>
            </div>
          </div>

          <button className="flex items-center gap-3 px-4 py-2.5 w-full text-slate-400 hover:text-red-400 hover:bg-red-950/20 rounded-xl transition-all duration-200 border border-transparent hover:border-red-900/30">
            <LogOut size={18} />
            <span className="font-semibold text-sm">Đăng xuất</span>
          </button>
        </div>
      </aside>

      {/* Main Content Area */}
      <main className="flex-1 flex flex-col overflow-hidden">
        
        {/* Header */}
        <header className="h-16 bg-slate-900/20 backdrop-blur-md border-b border-slate-800/60 px-8 flex items-center justify-between shrink-0">
          <div className="flex items-center gap-4 bg-slate-900/50 border border-slate-800/80 px-4 py-2 rounded-xl w-96 focus-within:border-indigo-500 transition-colors">
            <Search size={16} className="text-slate-400" />
            <input 
              type="text" 
              placeholder="Tìm kiếm: Tên, biển số, RFID, Mã SV..." 
              className="bg-transparent border-none outline-none text-xs w-full text-slate-100 placeholder-slate-500"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
            />
          </div>

          <div className="flex items-center gap-4">
            {/* Quick stats indicator */}
            <div className="hidden lg:flex items-center gap-2 px-3 py-1.5 rounded-lg bg-indigo-950/20 border border-indigo-900/30 text-indigo-300 text-xs font-semibold">
              <span className="w-1.5 h-1.5 bg-indigo-400 rounded-full animate-ping"></span>
              Cổng vào: {deviceStatus.barrier_in === 'open' ? 'Mở' : 'Đóng'} | Cổng ra: {deviceStatus.barrier_out === 'open' ? 'Mở' : 'Đóng'}
            </div>

            <button 
              onClick={fetchAllData}
              className="p-2 text-slate-400 hover:text-indigo-400 hover:bg-slate-800/50 rounded-xl transition-all border border-slate-800"
              title="Làm mới dữ liệu"
            >
              <RefreshCw size={16} />
            </button>
            <div className="w-[1px] h-6 bg-slate-800"></div>
            <div className="flex items-center gap-3">
              <div className="w-8 h-8 rounded-full bg-gradient-to-tr from-indigo-500 to-violet-500 flex items-center justify-center text-white font-bold text-xs shadow-md shadow-indigo-500/20">
                AD
              </div>
              <div className="hidden sm:block text-left">
                <p className="text-xs font-bold leading-tight">Administrator</p>
                <p className="text-[10px] text-slate-400">Hệ thống bãi xe</p>
              </div>
            </div>
          </div>
        </header>

        {/* Scrollable Workspace */}
        <div className="flex-1 overflow-y-auto p-8 space-y-8">
          <AnimatePresence mode="wait">
            
            {/* TAB: DASHBOARD */}
            {activeTab === 'dashboard' && (
              <motion.div 
                key="dashboard"
                initial={{ opacity: 0, y: 15 }}
                animate={{ opacity: 1, y: 0 }}
                exit={{ opacity: 0, y: -15 }}
                className="space-y-8"
              >
                {/* Stats Cards */}
                <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-5 gap-5">
                  <StatCard 
                    title="Tổng số tài khoản" 
                    value={stats.totalUsers} 
                    icon={<Users className="text-violet-400" />} 
                    glowColor="rgba(139, 92, 246, 0.15)"
                    label="Thẻ đã đăng ký"
                  />
                  <StatCard 
                    title="Xe đang trong bãi" 
                    value={stats.activeVehicles} 
                    icon={<Car className="text-indigo-400" />} 
                    glowColor="rgba(99, 102, 241, 0.15)"
                    label="Giám sát trực tiếp"
                  />
                  <StatCard 
                    title="Lượt gửi trong ngày" 
                    value={stats.dailyEntries} 
                    icon={<Activity className="text-emerald-400" />} 
                    glowColor="rgba(16, 185, 129, 0.15)"
                    label="Lượt quét vào bãi"
                  />
                  <StatCard 
                    title="Doanh thu hôm nay" 
                    value={`${(stats.dailyRevenue).toLocaleString()}đ`} 
                    icon={<CircleDollarSign className="text-sky-400" />} 
                    glowColor="rgba(14, 165, 233, 0.15)"
                    label="Tổng trừ ví RFID"
                  />
                  <StatCard 
                    title="Thẻ đang khóa" 
                    value={stats.totalLocked} 
                    icon={<Lock className="text-red-400" />} 
                    glowColor="rgba(239, 68, 68, 0.15)"
                    label="Tài khoản vi phạm"
                    warning={stats.totalLocked > 0}
                  />
                </div>

                {/* Dashboard Charts */}
                <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
                  {/* Revenue Line Chart */}
                  <div className="bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl p-6 shadow-2xl">
                    <div className="flex items-center justify-between mb-6">
                      <h3 className="font-bold text-sm text-slate-300 flex items-center gap-2">
                        <CircleDollarSign size={16} className="text-sky-400" />
                        Biểu đồ Doanh thu (Tuần này)
                      </h3>
                      <span className="text-[10px] bg-slate-800 text-sky-400 px-2 py-0.5 rounded-full font-bold uppercase">VNĐ</span>
                    </div>
                    {/* SVG Line Chart */}
                    <div className="w-full h-48 flex flex-col justify-between">
                      <svg viewBox="0 0 500 200" className="w-full h-40 overflow-visible">
                        <defs>
                          <linearGradient id="chartGlow" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="0%" stopColor="#0ea5e9" stopOpacity="0.4" />
                            <stop offset="100%" stopColor="#0ea5e9" stopOpacity="0.0" />
                          </linearGradient>
                        </defs>
                        {/* Grid lines */}
                        <line x1="0" y1="40" x2="500" y2="40" stroke="#1e293b" strokeDasharray="3,3" />
                        <line x1="0" y1="90" x2="500" y2="90" stroke="#1e293b" strokeDasharray="3,3" />
                        <line x1="0" y1="140" x2="500" y2="140" stroke="#1e293b" strokeDasharray="3,3" />
                        
                        {/* Area Gradient */}
                        <path 
                          d={`M 15 170 
                             L 85 ${170 - (weeklyRev.data[0] / 350000) * 130} 
                             L 155 ${170 - (weeklyRev.data[1] / 350000) * 130} 
                             L 225 ${170 - (weeklyRev.data[2] / 350000) * 130} 
                             L 295 ${170 - (weeklyRev.data[3] / 350000) * 130} 
                             L 365 ${170 - (weeklyRev.data[4] / 350000) * 130} 
                             L 435 ${170 - (weeklyRev.data[5] / 350000) * 130} 
                             L 485 ${170 - (weeklyRev.data[6] / 350000) * 130} 
                             L 485 170 Z`} 
                          fill="url(#chartGlow)" 
                        />
                        
                        {/* Line Path */}
                        <path 
                          d={`M 15 170 
                             L 85 ${170 - (weeklyRev.data[0] / 350000) * 130} 
                             L 155 ${170 - (weeklyRev.data[1] / 350000) * 130} 
                             L 225 ${170 - (weeklyRev.data[2] / 350000) * 130} 
                             L 295 ${170 - (weeklyRev.data[3] / 350000) * 130} 
                             L 365 ${170 - (weeklyRev.data[4] / 350000) * 130} 
                             L 435 ${170 - (weeklyRev.data[5] / 350000) * 130} 
                             L 485 ${170 - (weeklyRev.data[6] / 350000) * 130}`} 
                          fill="none" 
                          stroke="#0ea5e9" 
                          strokeWidth="3" 
                          strokeLinecap="round"
                          className="drop-shadow-[0_0_8px_rgba(14,165,233,0.5)]"
                        />

                        {/* Interactive Data Nodes */}
                        {weeklyRev.data.map((val, idx) => {
                          const x = idx === 0 ? 15 : idx === 6 ? 485 : 15 + idx * 70;
                          const y = 170 - (val / 350000) * 130;
                          return (
                            <g key={idx} className="group cursor-pointer">
                              <circle 
                                cx={x} 
                                cy={y} 
                                r="4" 
                                fill="#ffffff" 
                                stroke="#0ea5e9" 
                                strokeWidth="2" 
                                className="transition-all duration-200 group-hover:r-6" 
                              />
                              <foreignObject x={x - 30} y={y - 28} width="60" height="24" className="opacity-0 group-hover:opacity-100 transition-opacity pointer-events-none">
                                <div className="bg-slate-950 border border-slate-800 text-[9px] font-bold text-center py-0.5 rounded text-sky-400">
                                  {Math.round(val / 1000)}k
                                </div>
                              </foreignObject>
                            </g>
                          );
                        })}
                      </svg>
                      {/* X axis labels */}
                      <div className="flex justify-between text-[10px] text-slate-500 font-bold px-1 font-mono">
                        {weeklyRev.labels.map((lbl, idx) => <span key={idx}>{lbl}</span>)}
                      </div>
                    </div>
                  </div>

                  {/* Hourly Traffic Bar Chart */}
                  <div className="bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl p-6 shadow-2xl">
                    <div className="flex items-center justify-between mb-6">
                      <h3 className="font-bold text-sm text-slate-300 flex items-center gap-2">
                        <Activity size={16} className="text-emerald-400" />
                        Lượt vào / ra (Theo giờ)
                      </h3>
                      <div className="flex items-center gap-3 text-[10px]">
                        <span className="flex items-center gap-1"><span className="w-2 h-2 rounded bg-indigo-500"></span>Vào</span>
                        <span className="flex items-center gap-1"><span className="w-2 h-2 rounded bg-emerald-500"></span>Ra</span>
                      </div>
                    </div>
                    {/* SVG Bar Chart */}
                    <div className="w-full h-48 flex flex-col justify-between">
                      <svg viewBox="0 0 500 200" className="w-full h-40 overflow-visible">
                        {/* Grid lines */}
                        <line x1="0" y1="40" x2="500" y2="40" stroke="#1e293b" strokeDasharray="3,3" />
                        <line x1="0" y1="90" x2="500" y2="90" stroke="#1e293b" strokeDasharray="3,3" />
                        <line x1="0" y1="140" x2="500" y2="140" stroke="#1e293b" strokeDasharray="3,3" />

                        {/* Bars rendering */}
                        {hourlyTraffic.labels.map((lbl, idx) => {
                          const groupX = 25 + idx * 80;
                          
                          // heights scaled max 50 units -> 140px max
                          const entryH = (hourlyTraffic.entries[idx] / 50) * 130;
                          const exitH = (hourlyTraffic.exits[idx] / 50) * 130;
                          
                          const entryY = 160 - entryH;
                          const exitY = 160 - exitH;

                          return (
                            <g key={idx} className="group">
                              {/* Entry Bar */}
                              <rect 
                                x={groupX} 
                                y={entryY} 
                                width="14" 
                                height={entryH} 
                                fill="#6366f1" 
                                rx="3"
                                className="transition-all duration-200 hover:fill-indigo-400 cursor-pointer"
                              />
                              {/* Exit Bar */}
                              <rect 
                                x={groupX + 18} 
                                y={exitY} 
                                width="14" 
                                height={exitH} 
                                fill="#10b981" 
                                rx="3"
                                className="transition-all duration-200 hover:fill-emerald-400 cursor-pointer"
                              />
                              
                              {/* Tooltip */}
                              <foreignObject x={groupX - 12} y={Math.min(entryY, exitY) - 30} width="60" height="30" className="opacity-0 group-hover:opacity-100 transition-opacity pointer-events-none">
                                <div className="bg-slate-950 border border-slate-800 text-[8px] font-bold py-0.5 rounded px-1 flex flex-col items-center">
                                  <span className="text-indigo-400">Vào: {hourlyTraffic.entries[idx]}</span>
                                  <span className="text-emerald-400">Ra: {hourlyTraffic.exits[idx]}</span>
                                </div>
                              </foreignObject>
                            </g>
                          );
                        })}
                        <line x1="0" y1="160" x2="500" y2="160" stroke="#334155" strokeWidth="2" />
                      </svg>
                      {/* X axis labels */}
                      <div className="flex justify-between text-[10px] text-slate-500 font-bold font-mono px-3">
                        {hourlyTraffic.labels.map((lbl, idx) => <span key={idx}>{lbl}</span>)}
                      </div>
                    </div>
                  </div>

                  {/* Donut Chart Vehicle Type Ratio */}
                  <div className="bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl p-6 shadow-2xl">
                    <div className="flex items-center justify-between mb-4">
                      <h3 className="font-bold text-sm text-slate-300 flex items-center gap-2">
                        <Car size={16} className="text-indigo-400" />
                        Tỷ lệ phương tiện trong bãi
                      </h3>
                    </div>
                    
                    <div className="flex items-center justify-around h-48">
                      {/* SVG Donut */}
                      <div className="relative w-32 h-32">
                        <svg viewBox="0 0 100 100" className="w-full h-full transform -rotate-90">
                          {/* Background Circle */}
                          <circle cx="50" cy="50" r="38" fill="none" stroke="#1e293b" strokeWidth="10" />
                          
                          {/* Bike circle (emerald) */}
                          <circle 
                            cx="50" 
                            cy="50" 
                            r="38" 
                            fill="none" 
                            stroke="#10b981" 
                            strokeWidth="10" 
                            strokeDasharray="238.76"
                            strokeDashoffset={238.76 - (238.76 * (vehicleDist.bikeCount / (stats.activeVehicles || 1)))} 
                            className="transition-all duration-500 ease-out"
                          />
                          
                          {/* Car circle (violet - stacked) */}
                          <circle 
                            cx="50" 
                            cy="50" 
                            r="38" 
                            fill="none" 
                            stroke="#8b5cf6" 
                            strokeWidth="10" 
                            strokeDasharray="238.76"
                            strokeDashoffset={238.76 - (238.76 * (vehicleDist.carCount / (stats.activeVehicles || 1)))}
                            className="transition-all duration-500 ease-out"
                            style={{
                              transform: `rotate(${(vehicleDist.bikeCount / (stats.activeVehicles || 1)) * 360}deg)`,
                              transformOrigin: '50% 50%'
                            }}
                          />
                        </svg>
                        <div className="absolute inset-0 flex flex-col items-center justify-center">
                          <span className="text-2xl font-black font-mono text-white">{stats.activeVehicles}</span>
                          <span className="text-[9px] text-slate-400 font-bold uppercase tracking-widest">Xe đỗ</span>
                        </div>
                      </div>

                      {/* Legends */}
                      <div className="space-y-3">
                        <div className="flex items-center gap-3">
                          <div className="w-3.5 h-3.5 rounded bg-violet-500 shadow-md shadow-violet-500/20"></div>
                          <div className="text-left">
                            <p className="text-[10px] text-slate-400 font-semibold uppercase">Ô tô</p>
                            <p className="text-sm font-bold text-slate-100">{vehicleDist.carCount} xe ({vehicleDist.carPercent}%)</p>
                          </div>
                        </div>
                        <div className="flex items-center gap-3">
                          <div className="w-3.5 h-3.5 rounded bg-emerald-500 shadow-md shadow-emerald-500/20"></div>
                          <div className="text-left">
                            <p className="text-[10px] text-slate-400 font-semibold uppercase">Xe máy</p>
                            <p className="text-sm font-bold text-slate-100">{vehicleDist.bikeCount} xe ({vehicleDist.bikePercent}%)</p>
                          </div>
                        </div>
                      </div>
                    </div>
                  </div>
                </div>

                {/* Live Activity & Remote Control Gate */}
                <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
                  
                  {/* Live Activity Feed */}
                  <div className="lg:col-span-2 bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl shadow-2xl overflow-hidden flex flex-col">
                    <div className="p-5 border-b border-slate-800/60 flex items-center justify-between">
                      <div className="flex items-center gap-2">
                        <Activity size={18} className="text-indigo-400" />
                        <h2 className="font-bold text-sm text-slate-300">Hoạt động bãi xe trực tiếp</h2>
                      </div>
                      <span className="text-[10px] font-bold text-emerald-400 bg-emerald-950/40 px-2 py-0.5 rounded-full border border-emerald-900/30 flex items-center gap-1.5">
                        <span className="w-1.5 h-1.5 bg-emerald-400 rounded-full animate-pulse"></span>
                        THỜI GIAN THỰC
                      </span>
                    </div>
                    <div className="divide-y divide-slate-800/40 overflow-y-auto max-h-[360px] flex-1">
                      {parkingLogs.length === 0 ? (
                        <div className="p-16 text-center text-slate-500 text-xs">
                          <HardDrive size={32} className="mx-auto mb-2 text-slate-600" />
                          Chưa có ghi chép xe ra vào nào hôm nay.
                        </div>
                      ) : (
                        parkingLogs.slice(0, 10).map((log) => (
                          <div key={log.id} className="p-4 hover:bg-slate-900/30 transition-colors flex items-center justify-between">
                            <div className="flex items-center gap-4">
                              <div className={cn(
                                "w-9 h-9 rounded-xl flex items-center justify-center text-xs font-bold shadow-md",
                                log.status === 'parked' 
                                  ? "bg-indigo-950/60 text-indigo-400 border border-indigo-900/40" 
                                  : "bg-emerald-950/60 text-emerald-400 border border-emerald-900/40"
                              )}>
                                {log.status === 'parked' ? <ArrowDownLeft size={16} /> : <ArrowUpRight size={16} />}
                              </div>
                              <div className="text-left">
                                <p className="font-bold text-xs text-slate-200">{log.name || 'Người dùng vãng lai'}</p>
                                <p className="text-[10px] text-slate-400 font-medium">
                                  {log.plateNumber} • {log.vehicleType === 'Car' ? 'Ô tô' : 'Xe máy'} • UID: <span className="font-mono text-slate-300">{log.uid}</span>
                                </p>
                              </div>
                            </div>
                            <div className="text-right">
                              <p className={cn(
                                "text-xs font-bold",
                                log.status === 'parked' ? "text-indigo-400" : "text-emerald-400"
                              )}>
                                {log.status === 'parked' ? 'Đã Vào' : `Đã Ra (Thu: ${log.fee.toLocaleString()}đ)`}
                              </p>
                              <p className="text-[9px] text-slate-500 font-mono mt-0.5">
                                {new Date(log.entryTime).toLocaleTimeString()} {log.exitTime ? ` -> ${new Date(log.exitTime).toLocaleTimeString()}` : ''}
                              </p>
                            </div>
                          </div>
                        ))
                      )}
                    </div>
                  </div>

                  {/* Manual Gate Control & Device Stats */}
                  <div className="space-y-6">
                    {/* Remote Gate Controls */}
                    <div className="bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl p-5 shadow-2xl">
                      <div className="flex items-center gap-2 mb-4 border-b border-slate-800/60 pb-3">
                        <Settings size={16} className="text-indigo-400" />
                        <h3 className="font-bold text-sm text-slate-300">Điều khiển Barrier Từ xa</h3>
                      </div>
                      
                      <div className="space-y-4">
                        {/* Gate IN */}
                        <div className="p-3 bg-slate-950/40 rounded-xl border border-slate-800 flex items-center justify-between">
                          <div>
                            <p className="text-xs font-bold">Cổng VÀO (Barrier IN)</p>
                            <span className={cn(
                              "text-[9px] font-bold px-1.5 py-0.5 rounded",
                              deviceStatus.barrier_in === 'open' ? "bg-emerald-950/60 text-emerald-400 border border-emerald-900/40" : "bg-slate-800 text-slate-400"
                            )}>
                              {deviceStatus.barrier_in === 'open' ? 'ĐANG MỞ' : 'ĐANG ĐÓNG'}
                            </span>
                          </div>
                          <div className="flex gap-2">
                            <button 
                              onClick={() => triggerBarrierControl('in', 'open')}
                              className="px-2.5 py-1.5 text-[10px] font-bold bg-indigo-600 hover:bg-indigo-700 text-white rounded-lg transition-colors flex items-center gap-1 shadow-lg shadow-indigo-500/10"
                            >
                              <Play size={10} /> Mở
                            </button>
                            <button 
                              onClick={() => triggerBarrierControl('in', 'close')}
                              className="px-2.5 py-1.5 text-[10px] font-bold bg-slate-800 hover:bg-slate-700 text-slate-300 rounded-lg transition-colors flex items-center gap-1 border border-slate-700"
                            >
                              <Square size={10} /> Đóng
                            </button>
                          </div>
                        </div>

                        {/* Gate OUT */}
                        <div className="p-3 bg-slate-950/40 rounded-xl border border-slate-800 flex items-center justify-between">
                          <div>
                            <p className="text-xs font-bold">Cổng RA (Barrier OUT)</p>
                            <span className={cn(
                              "text-[9px] font-bold px-1.5 py-0.5 rounded",
                              deviceStatus.barrier_out === 'open' ? "bg-emerald-950/60 text-emerald-400 border border-emerald-900/40" : "bg-slate-800 text-slate-400"
                            )}>
                              {deviceStatus.barrier_out === 'open' ? 'ĐANG MỞ' : 'ĐANG ĐÓNG'}
                            </span>
                          </div>
                          <div className="flex gap-2">
                            <button 
                              onClick={() => triggerBarrierControl('out', 'open')}
                              className="px-2.5 py-1.5 text-[10px] font-bold bg-indigo-600 hover:bg-indigo-700 text-white rounded-lg transition-colors flex items-center gap-1 shadow-lg shadow-indigo-500/10"
                            >
                              <Play size={10} /> Mở
                            </button>
                            <button 
                              onClick={() => triggerBarrierControl('out', 'close')}
                              className="px-2.5 py-1.5 text-[10px] font-bold bg-slate-800 hover:bg-slate-700 text-slate-300 rounded-lg transition-colors flex items-center gap-1 border border-slate-700"
                            >
                              <Square size={10} /> Đóng
                            </button>
                          </div>
                        </div>
                      </div>
                    </div>

                    {/* Detailed Device Status */}
                    <div className="bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl p-5 shadow-2xl space-y-4">
                      <div className="flex items-center gap-2 border-b border-slate-800/60 pb-3">
                        <Cpu size={16} className="text-indigo-400" />
                        <h3 className="font-bold text-sm text-slate-300">Trạng thái Phần cứng IoT</h3>
                      </div>
                      <div className="space-y-3 text-xs">
                        <DeviceStatusRow label="Vi điều khiển ESP32" status={deviceStatus.esp32 === 'online' ? 'Trực tuyến' : 'Ngoại tuyến'} online={deviceStatus.esp32 === 'online'} />
                        <DeviceStatusRow label="MQTT Broker Server" status={deviceStatus.mqtt === 'connected' ? 'Đã kết nối' : 'Mất kết nối'} online={deviceStatus.mqtt === 'connected'} />
                        <DeviceStatusRow label="Mạng không dây WiFi" status={deviceStatus.wifi === 'connected' ? 'Đã liên kết' : 'Chưa cấu hình'} online={deviceStatus.wifi === 'connected'} />
                        <DeviceStatusRow label="Đầu đọc RFID PN532" status={deviceStatus.pn532 === 'connected' ? 'Sẵn sàng' : 'Không nhận diện'} online={deviceStatus.pn532 === 'connected'} />
                      </div>
                    </div>
                  </div>

                </div>
              </motion.div>
            )}

            {/* TAB: MEMBERS / CARDS */}
            {activeTab === 'users' && (
              <motion.div 
                key="users"
                initial={{ opacity: 0, x: 15 }}
                animate={{ opacity: 1, x: 0 }}
                exit={{ opacity: 0, x: -15 }}
                className="bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl shadow-2xl overflow-hidden"
              >
                <div className="p-6 border-b border-slate-800/60 flex items-center justify-between">
                  <div>
                    <h2 className="font-bold text-base text-slate-300">Danh sách thành viên đăng ký thẻ</h2>
                    <p className="text-xs text-slate-500">Quản lý định danh NFC RFID và số dư tài khoản</p>
                  </div>
                  <button 
                    onClick={() => {
                      setUserFormData({
                        uid: '',
                        name: '',
                        studentId: '',
                        plateNumber: '',
                        vehicleType: 'Motorbike',
                        balance: 0
                      });
                      setUserModalMode('add');
                      setIsUserModalOpen(true);
                    }}
                    className="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-xl text-xs font-bold flex items-center gap-2 shadow-lg shadow-indigo-600/20 transition-all border border-indigo-500/20"
                  >
                    <Plus size={16} />
                    Đăng ký thẻ mới
                  </button>
                </div>
                <div className="overflow-x-auto">
                  <table className="w-full text-left border-collapse">
                    <thead className="bg-slate-950/40 text-slate-400 text-[10px] uppercase font-bold tracking-widest border-b border-slate-800">
                      <tr>
                        <th className="px-6 py-4">Họ và Tên / ID thẻ</th>
                        <th className="px-6 py-4">Mã số SV/NV</th>
                        <th className="px-6 py-4">Biển số & Phương tiện</th>
                        <th className="px-6 py-4">Số dư ví RFID</th>
                        <th className="px-6 py-4">Trạng thái thẻ</th>
                        <th className="px-6 py-4 text-right">Thao tác</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-slate-800/50">
                      {filteredUsers.length === 0 ? (
                        <tr>
                          <td colSpan={6} className="px-6 py-12 text-center text-slate-500 text-xs">
                            Không tìm thấy tài khoản người dùng nào khớp với truy vấn.
                          </td>
                        </tr>
                      ) : (
                        filteredUsers.map((user) => (
                          <tr key={user.uid} className="hover:bg-slate-900/10 transition-colors">
                            <td className="px-6 py-4">
                              <div className="flex items-center gap-3">
                                <div className="w-10 h-10 rounded-xl bg-indigo-950/60 text-indigo-400 border border-indigo-900/40 flex items-center justify-center font-bold text-sm shadow-md">
                                  {user.name.charAt(0)}
                                </div>
                                <div>
                                  <p className="font-bold text-sm text-slate-200">{user.name}</p>
                                  <p className="text-[10px] text-slate-500 font-mono">UID: {user.uid}</p>
                                </div>
                              </div>
                            </td>
                            <td className="px-6 py-4">
                              <span className="text-sm font-semibold text-slate-300 font-mono">{user.studentId}</span>
                            </td>
                            <td className="px-6 py-4">
                              <p className="text-sm font-bold text-slate-200">{user.plateNumber}</p>
                              <p className="text-[10px] text-slate-400 font-semibold">{user.vehicleType === 'Car' ? '🚗 Ô tô' : '🏍️ Xe máy'}</p>
                            </td>
                            <td className="px-6 py-4">
                              <p className="text-sm font-black text-indigo-400 font-mono">{user.balance.toLocaleString()}đ</p>
                            </td>
                            <td className="px-6 py-4">
                              <span className={cn(
                                "px-2.5 py-0.5 rounded text-[10px] font-bold uppercase tracking-wider",
                                user.status === 'active' 
                                  ? "bg-emerald-950/60 text-emerald-400 border border-emerald-900/40" 
                                  : "bg-red-950/60 text-red-400 border border-red-900/40"
                              )}>
                                {user.status === 'active' ? 'Hoạt động' : 'Đã khóa'}
                              </span>
                            </td>
                            <td className="px-6 py-4 text-right">
                              <div className="flex items-center justify-end gap-2">
                                <button 
                                  onClick={() => {
                                    setSelectedUser(user);
                                    setTopUpAmount(0);
                                    setIsTopUpModalOpen(true);
                                  }}
                                  className="p-1.5 text-sky-400 hover:bg-sky-950/40 rounded-lg transition-colors border border-transparent hover:border-sky-900/30"
                                  title="Nạp tiền vào tài khoản"
                                >
                                  <Wallet size={16} />
                                </button>
                                <button 
                                  onClick={() => {
                                    setUserFormData({
                                      uid: user.uid,
                                      name: user.name,
                                      studentId: user.studentId,
                                      plateNumber: user.plateNumber,
                                      vehicleType: user.vehicleType,
                                      balance: user.balance
                                    });
                                    setUserModalMode('edit');
                                    setIsUserModalOpen(true);
                                  }}
                                  className="p-1.5 text-amber-400 hover:bg-amber-950/40 rounded-lg transition-colors border border-transparent hover:border-amber-900/30"
                                  title="Chỉnh sửa thông tin"
                                >
                                  <Edit3 size={16} />
                                </button>
                                <button 
                                  onClick={() => toggleUserStatus(user.uid, user.status)}
                                  className={cn(
                                    "p-1.5 rounded-lg transition-colors border border-transparent",
                                    user.status === 'active' 
                                      ? "text-red-400 hover:bg-red-950/40 hover:border-red-900/30" 
                                      : "text-emerald-400 hover:bg-emerald-950/40 hover:border-emerald-900/30"
                                  )}
                                  title={user.status === 'active' ? "Khóa thẻ này" : "Mở khóa thẻ này"}
                                >
                                  {user.status === 'active' ? <Lock size={16} /> : <Unlock size={16} />}
                                </button>
                                <button 
                                  onClick={() => handleDeleteUser(user.uid)}
                                  className="p-1.5 text-rose-500 hover:bg-rose-950/40 rounded-lg transition-colors border border-transparent hover:border-rose-900/30"
                                  title="Xóa tài khoản"
                                >
                                  <Trash2 size={16} />
                                </button>
                              </div>
                            </td>
                          </tr>
                        )))
                      }
                    </tbody>
                  </table>
                </div>
              </motion.div>
            )}

            {/* TAB: VEHICLES IN PARK */}
            {activeTab === 'parked' && (
              <motion.div 
                key="parked"
                initial={{ opacity: 0, x: 15 }}
                animate={{ opacity: 1, x: 0 }}
                exit={{ opacity: 0, x: -15 }}
                className="bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl shadow-2xl overflow-hidden"
              >
                <div className="p-6 border-b border-slate-800/60 flex items-center justify-between">
                  <div>
                    <h2 className="font-bold text-base text-slate-300">Xe đang gửi trong bãi đỗ</h2>
                    <p className="text-xs text-slate-500">Giám sát xe chưa checkout và thời gian gửi thực tế</p>
                  </div>
                  <span className="text-xs font-bold text-indigo-400 bg-indigo-950/40 border border-indigo-900/30 px-3 py-1 rounded-full">
                    Tổng cộng: {parkedVehicles.length} xe đang đỗ
                  </span>
                </div>
                <div className="overflow-x-auto">
                  <table className="w-full text-left border-collapse">
                    <thead className="bg-slate-950/40 text-slate-400 text-[10px] uppercase font-bold tracking-widest border-b border-slate-800">
                      <tr>
                        <th className="px-6 py-4">Chủ xe / UID Thẻ</th>
                        <th className="px-6 py-4">Biển kiểm soát</th>
                        <th className="px-6 py-4">Loại xe</th>
                        <th className="px-6 py-4">Thời gian vào bãi</th>
                        <th className="px-6 py-4">Thời gian đã đỗ</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-slate-800/50">
                      {parkedVehicles.length === 0 ? (
                        <tr>
                          <td colSpan={5} className="px-6 py-12 text-center text-slate-500 text-xs">
                            <Car size={32} className="mx-auto mb-2 text-slate-700" />
                            Không có xe nào đang đỗ trong bãi.
                          </td>
                        </tr>
                      ) : (
                        parkedVehicles.map((log) => (
                          <tr key={log.id} className="hover:bg-slate-900/10 transition-colors">
                            <td className="px-6 py-4">
                              <p className="font-bold text-sm text-slate-200">{log.name || 'Người dùng vãng lai'}</p>
                              <p className="text-[10px] text-slate-500 font-mono">UID: {log.uid}</p>
                            </td>
                            <td className="px-6 py-4">
                              <span className="px-2.5 py-1 rounded bg-slate-950 text-slate-200 font-black border border-slate-800 font-mono text-xs">{log.plateNumber}</span>
                            </td>
                            <td className="px-6 py-4">
                              <span className="text-xs font-semibold text-slate-300">{log.vehicleType === 'Car' ? '🚗 Ô tô' : '🏍️ Xe máy'}</span>
                            </td>
                            <td className="px-6 py-4 text-sm font-semibold text-slate-300">
                              {new Date(log.entryTime).toLocaleString()}
                            </td>
                            <td className="px-6 py-4 flex items-center gap-2">
                              <Clock size={14} className="text-indigo-400" />
                              <ParkedDuration entryTime={log.entryTime} />
                            </td>
                          </tr>
                        ))
                      )}
                    </tbody>
                  </table>
                </div>
              </motion.div>
            )}

            {/* TAB: PARKING LOGS */}
            {activeTab === 'parking-logs' && (
              <motion.div 
                key="parking-logs"
                initial={{ opacity: 0, x: -15 }}
                animate={{ opacity: 1, x: 0 }}
                exit={{ opacity: 0, x: 15 }}
                className="bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl shadow-2xl overflow-hidden"
              >
                <div className="p-6 border-b border-slate-800/60">
                  <h2 className="font-bold text-base text-slate-300">Nhật ký ra vào bãi đỗ xe</h2>
                  <p className="text-xs text-slate-500">Lịch sử check-in, check-out và chi tiết phí gửi xe</p>
                </div>
                <div className="overflow-x-auto">
                  <table className="w-full text-left border-collapse">
                    <thead className="bg-slate-950/40 text-slate-400 text-[10px] uppercase font-bold tracking-widest border-b border-slate-800">
                      <tr>
                        <th className="px-6 py-4">Thời gian vào</th>
                        <th className="px-6 py-4">Thời gian ra</th>
                        <th className="px-6 py-4">Thông tin thẻ</th>
                        <th className="px-6 py-4">Biển kiểm soát</th>
                        <th className="px-6 py-4">Loại xe</th>
                        <th className="px-6 py-4">Phí thu</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-slate-800/50">
                      {parkingLogs.length === 0 ? (
                        <tr>
                          <td colSpan={6} className="px-6 py-12 text-center text-slate-500 text-xs">
                            Chưa có dữ liệu nhật ký nào.
                          </td>
                        </tr>
                      ) : (
                        parkingLogs.map((log) => (
                          <tr key={log.id} className="hover:bg-slate-900/10 transition-colors">
                            <td className="px-6 py-4 text-xs font-semibold text-slate-300 font-mono">
                              {new Date(log.entryTime).toLocaleString()}
                            </td>
                            <td className="px-6 py-4 text-xs font-semibold text-slate-400 font-mono">
                              {log.exitTime ? new Date(log.exitTime).toLocaleString() : (
                                <span className="text-indigo-400 font-bold px-1.5 py-0.5 rounded bg-indigo-950/40 border border-indigo-900/30">Đang đỗ</span>
                              )}
                            </td>
                            <td className="px-6 py-4">
                              <p className="font-bold text-xs text-slate-200">{log.name || 'Người dùng vãng lai'}</p>
                              <p className="text-[10px] text-slate-500 font-mono">UID: {log.uid}</p>
                            </td>
                            <td className="px-6 py-4 font-mono text-sm text-slate-300">{log.plateNumber}</td>
                            <td className="px-6 py-4 text-xs font-semibold text-slate-400">
                              {log.vehicleType === 'Car' ? 'Ô tô' : 'Xe máy'}
                            </td>
                            <td className="px-6 py-4 font-black text-sm text-rose-400 font-mono">
                              {log.exitTime ? `${log.fee.toLocaleString()}đ` : '-'}
                            </td>
                          </tr>
                        ))
                      )}
                    </tbody>
                  </table>
                </div>
              </motion.div>
            )}

            {/* TAB: WALLET TRANSACTIONS */}
            {activeTab === 'transactions' && (
              <motion.div 
                key="transactions"
                initial={{ opacity: 0, x: -15 }}
                animate={{ opacity: 1, x: 0 }}
                exit={{ opacity: 0, x: 15 }}
                className="bg-slate-900/40 backdrop-blur-md border border-slate-800/60 rounded-2xl shadow-2xl overflow-hidden"
              >
                <div className="p-6 border-b border-slate-800/60">
                  <h2 className="font-bold text-base text-slate-300">Nhật ký giao dịch ví tài khoản</h2>
                  <p className="text-xs text-slate-500">Chi tiết lịch sử nạp tiền và trừ phí gửi xe</p>
                </div>
                <div className="overflow-x-auto">
                  <table className="w-full text-left border-collapse">
                    <thead className="bg-slate-950/40 text-slate-400 text-[10px] uppercase font-bold tracking-widest border-b border-slate-800">
                      <tr>
                        <th className="px-6 py-4">Thời gian</th>
                        <th className="px-6 py-4">Người dùng / Thẻ RFID</th>
                        <th className="px-6 py-4">Biển kiểm soát</th>
                        <th className="px-6 py-4">Loại giao dịch</th>
                        <th className="px-6 py-4">Số tiền thay đổi</th>
                        <th className="px-6 py-4">Số dư sau GD</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-slate-800/50">
                      {transactions.length === 0 ? (
                        <tr>
                          <td colSpan={6} className="px-6 py-12 text-center text-slate-500 text-xs">
                            Chưa có dữ liệu giao dịch ví.
                          </td>
                        </tr>
                      ) : (
                        transactions.map((tx) => (
                          <tr key={tx.id} className="hover:bg-slate-900/10 transition-colors">
                            <td className="px-6 py-4 text-xs font-semibold text-slate-300 font-mono">
                              {new Date(tx.timestamp).toLocaleString()}
                            </td>
                            <td className="px-6 py-4">
                              <p className="font-bold text-xs text-slate-200">{tx.name || 'Người dùng vãng lai'}</p>
                              <p className="text-[10px] text-slate-500 font-mono">UID: {tx.uid}</p>
                            </td>
                            <td className="px-6 py-4 text-xs font-mono text-slate-300">
                              {tx.plateNumber || '-'}
                            </td>
                            <td className="px-6 py-4">
                              <span className={cn(
                                "px-2 py-0.5 rounded text-[10px] font-bold uppercase tracking-wider",
                                tx.type === 'topup' 
                                  ? "bg-emerald-950/60 text-emerald-400 border border-emerald-900/40" 
                                  : "bg-rose-950/60 text-rose-400 border border-rose-900/40"
                              )}>
                                {tx.type === 'topup' ? 'Nạp tiền' : 'Trừ phí xe ra'}
                              </span>
                            </td>
                            <td className={cn(
                              "px-6 py-4 font-black text-sm font-mono",
                              tx.type === 'topup' ? "text-emerald-400" : "text-rose-400"
                            )}>
                              {tx.type === 'topup' ? '+' : '-'}{tx.amount.toLocaleString()}đ
                            </td>
                            <td className="px-6 py-4 text-sm font-bold text-slate-300 font-mono">
                              {tx.balanceAfter.toLocaleString()}đ
                            </td>
                          </tr>
                        ))
                      )}
                    </tbody>
                  </table>
                </div>
              </motion.div>
            )}



          </AnimatePresence>
        </div>
      </main>

      {/* MODAL: TOP-UP WALLET */}
      <AnimatePresence>
        {isTopUpModalOpen && selectedUser && (
          <div className="fixed inset-0 bg-slate-950/80 backdrop-blur-md flex items-center justify-center z-50 p-4">
            <motion.div 
              initial={{ scale: 0.95, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              exit={{ scale: 0.95, opacity: 0 }}
              className="bg-slate-900 border border-slate-800 rounded-2xl w-full max-w-md shadow-2xl overflow-hidden"
            >
              <div className="p-6 border-b border-slate-800 flex items-center justify-between">
                <div>
                  <h3 className="font-bold text-lg text-slate-100">Nạp tiền vào ví RFID</h3>
                  <p className="text-slate-400 text-xs mt-0.5">Nạp tiền cho thành viên {selectedUser.name}</p>
                </div>
              </div>
              <div className="p-6 space-y-4 text-left">
                <div>
                  <label className="block text-[10px] font-bold text-slate-400 uppercase tracking-wider mb-2">Số tiền cần nạp (VNĐ)</label>
                  <input 
                    type="number" 
                    className="w-full px-4 py-3 rounded-xl bg-slate-950 border border-slate-800 focus:border-indigo-500 focus:ring-1 focus:ring-indigo-500 outline-none font-bold text-xl text-indigo-400 placeholder-slate-700"
                    placeholder="Nhập số tiền..."
                    value={topUpAmount || ''}
                    onChange={(e) => setTopUpAmount(Number(e.target.value))}
                  />
                </div>
                <div className="grid grid-cols-4 gap-2">
                  {[20000, 50000, 100000, 200000].map(amt => (
                    <button 
                      key={amt}
                      onClick={() => setTopUpAmount(amt)}
                      className="py-2 bg-slate-950 hover:bg-slate-800 border border-slate-800 hover:border-slate-700 rounded-xl text-xs font-bold text-slate-300 transition-colors"
                    >
                      +{amt.toLocaleString()}
                    </button>
                  ))}
                </div>
                <div className="grid grid-cols-2 gap-2">
                  {[500000, 1000000].map(amt => (
                    <button 
                      key={amt}
                      onClick={() => setTopUpAmount(amt)}
                      className="py-2 bg-slate-950 hover:bg-slate-800 border border-slate-800 hover:border-slate-700 rounded-xl text-xs font-bold text-slate-300 transition-colors"
                    >
                      +{amt.toLocaleString()}
                    </button>
                  ))}
                </div>
              </div>
              <div className="p-6 bg-slate-950/40 border-t border-slate-800/60 flex gap-3">
                <button 
                  onClick={() => {
                    setIsTopUpModalOpen(false);
                    setSelectedUser(null);
                  }}
                  className="flex-1 py-2.5 rounded-xl font-bold text-slate-400 bg-slate-800 hover:bg-slate-700 transition-colors text-xs"
                >
                  Hủy bỏ
                </button>
                <button 
                  onClick={handleTopUp}
                  className="flex-1 py-2.5 rounded-xl font-bold bg-indigo-600 hover:bg-indigo-700 text-white shadow-lg shadow-indigo-600/10 transition-colors text-xs border border-indigo-500/20"
                >
                  Xác nhận Nạp tiền
                </button>
              </div>
            </motion.div>
          </div>
        )}
      </AnimatePresence>

      {/* MODAL: ADD / EDIT USER */}
      <AnimatePresence>
        {isUserModalOpen && (
          <div className="fixed inset-0 bg-slate-950/80 backdrop-blur-md flex items-center justify-center z-50 p-4">
            <motion.div 
              initial={{ scale: 0.95, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              exit={{ scale: 0.95, opacity: 0 }}
              className="bg-slate-900 border border-slate-800 rounded-2xl w-full max-w-md shadow-2xl overflow-hidden"
            >
              <div className="p-6 border-b border-slate-800">
                <h3 className="font-bold text-lg text-slate-100">
                  {userModalMode === 'add' ? 'Đăng ký thành viên RFID mới' : 'Cập nhật thông tin thành viên'}
                </h3>
                <p className="text-slate-400 text-xs mt-0.5">
                  {userModalMode === 'add' ? 'Nhập thông tin để liên kết ID thẻ RFID NFC' : 'Thay đổi thông tin và số dư tài khoản'}
                </p>
              </div>
              
              <div className="p-6 space-y-4 text-left">
                {/* NFC RFID UID */}
                <div>
                  <label className="block text-[10px] font-bold text-slate-400 uppercase tracking-wider mb-1.5">Mã số thẻ RFID (NFC UID)</label>
                  <input 
                    type="text" 
                    disabled={userModalMode === 'edit'}
                    className={cn(
                      "w-full px-3 py-2 rounded-xl bg-slate-950 border text-xs outline-none transition-all font-mono",
                      userModalMode === 'edit' 
                        ? "border-slate-800 text-slate-500 cursor-not-allowed" 
                        : "border-slate-800 focus:border-indigo-500 text-indigo-400 placeholder-slate-700"
                    )}
                    placeholder="Quẹt thẻ RFID hoặc tự nhập UID..."
                    value={userFormData.uid}
                    onChange={(e) => setUserFormData(prev => ({ ...prev, uid: e.target.value }))}
                  />
                </div>

                {/* Name */}
                <div>
                  <label className="block text-[10px] font-bold text-slate-400 uppercase tracking-wider mb-1.5">Họ và tên thành viên *</label>
                  <input 
                    type="text" 
                    className="w-full px-3 py-2 rounded-xl bg-slate-950 border border-slate-800 focus:border-indigo-500 text-xs text-slate-200 placeholder-slate-700 outline-none"
                    placeholder="Nguyễn Văn A..."
                    value={userFormData.name}
                    onChange={(e) => setUserFormData(prev => ({ ...prev, name: e.target.value }))}
                  />
                </div>

                {/* Student / Employee ID */}
                <div>
                  <label className="block text-[10px] font-bold text-slate-400 uppercase tracking-wider mb-1.5">Mã sinh viên / Mã nhân viên *</label>
                  <input 
                    type="text" 
                    className="w-full px-3 py-2 rounded-xl bg-slate-950 border border-slate-800 focus:border-indigo-500 text-xs text-slate-200 placeholder-slate-700 font-mono outline-none"
                    placeholder="SV12345 / NV098..."
                    value={userFormData.studentId}
                    onChange={(e) => setUserFormData(prev => ({ ...prev, studentId: e.target.value }))}
                  />
                </div>

                {/* Vehicle details */}
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <label className="block text-[10px] font-bold text-slate-400 uppercase tracking-wider mb-1.5">Loại xe *</label>
                    <select 
                      className="w-full px-3 py-2 rounded-xl bg-slate-950 border border-slate-800 focus:border-indigo-500 text-xs text-slate-200 outline-none"
                      value={userFormData.vehicleType}
                      onChange={(e) => setUserFormData(prev => ({ ...prev, vehicleType: e.target.value as 'Car' | 'Motorbike' }))}
                    >
                      <option value="Motorbike">Xe máy</option>
                      <option value="Car">Ô tô</option>
                    </select>
                  </div>
                  <div>
                    <label className="block text-[10px] font-bold text-slate-400 uppercase tracking-wider mb-1.5">Biển số xe *</label>
                    <input 
                      type="text" 
                      className="w-full px-3 py-2 rounded-xl bg-slate-950 border border-slate-800 focus:border-indigo-500 text-xs text-slate-200 text-center uppercase font-mono outline-none placeholder-slate-700"
                      placeholder="29A-12345..."
                      value={userFormData.plateNumber}
                      onChange={(e) => setUserFormData(prev => ({ ...prev, plateNumber: e.target.value }))}
                    />
                  </div>
                </div>

                {/* Initial Balance */}
                <div>
                  <label className="block text-[10px] font-bold text-slate-400 uppercase tracking-wider mb-1.5">
                    {userModalMode === 'add' ? 'Số dư ví khởi tạo (VNĐ)' : 'Cấu hình lại số dư ví (VNĐ)'}
                  </label>
                  <input 
                    type="number" 
                    className="w-full px-3 py-2 rounded-xl bg-slate-950 border border-slate-800 focus:border-indigo-500 text-xs text-slate-200 font-mono outline-none"
                    value={userFormData.balance || 0}
                    onChange={(e) => setUserFormData(prev => ({ ...prev, balance: Number(e.target.value) }))}
                  />
                </div>
              </div>

              <div className="p-6 bg-slate-950/40 border-t border-slate-800/60 flex gap-3">
                <button 
                  onClick={() => setIsUserModalOpen(false)}
                  className="flex-1 py-2.5 rounded-xl font-bold text-slate-400 bg-slate-800 hover:bg-slate-700 transition-colors text-xs"
                >
                  Hủy bỏ
                </button>
                <button 
                  onClick={handleSaveUser}
                  className="flex-1 py-2.5 rounded-xl font-bold bg-indigo-600 hover:bg-indigo-700 text-white shadow-lg shadow-indigo-600/10 transition-colors text-xs border border-indigo-500/20"
                >
                  {userModalMode === 'add' ? 'Lưu & Đăng ký' : 'Lưu cập nhật'}
                </button>
              </div>
            </motion.div>
          </div>
        )}
      </AnimatePresence>

    </div>
  );
}

// --- Internal Helper NavItem Component ---
interface NavItemProps {
  icon: React.ReactNode;
  label: string;
  active: boolean;
  badge?: number;
  badgeColor?: 'indigo' | 'amber';
  onClick: () => void;
}

function NavItem({ icon, label, active, badge, badgeColor = 'indigo', onClick }: NavItemProps) {
  return (
    <button 
      onClick={onClick}
      className={cn(
        "flex items-center justify-between px-4 py-3 w-full rounded-xl transition-all duration-200 text-left border border-transparent font-medium text-xs",
        active 
          ? "bg-indigo-600 text-white shadow-lg shadow-indigo-600/25 border-indigo-500/10 font-bold" 
          : "text-slate-400 hover:bg-slate-900/40 hover:text-slate-200 hover:border-slate-800/50"
      )}
    >
      <div className="flex items-center gap-3">
        {icon}
        <span>{label}</span>
      </div>
      {badge !== undefined && badge > 0 && (
        <span className={cn(
          "text-[9px] font-black font-mono px-2 py-0.5 rounded-full",
          badgeColor === 'indigo' 
            ? "bg-indigo-900/60 text-indigo-300 border border-indigo-800/40" 
            : "bg-amber-950/60 text-amber-400 border border-amber-900/40"
        )}>
          {badge}
        </span>
      )}
    </button>
  );
}

// --- Internal Helper StatCard Component ---
interface StatCardProps {
  title: string;
  value: string | number;
  icon: React.ReactNode;
  glowColor: string;
  label: string;
  warning?: boolean;
}

function StatCard({ title, value, icon, glowColor, label, warning = false }: StatCardProps) {
  return (
    <div 
      className={cn(
        "bg-slate-900/40 backdrop-blur-md p-5 rounded-2xl border border-slate-800/60 shadow-xl flex flex-col justify-between hover:border-slate-700/60 transition-all duration-300 relative group overflow-hidden",
        warning && "border-red-900/40 shadow-red-950/5"
      )}
      style={{
        boxShadow: `inset 0 0 16px rgba(255,255,255,0.01), 0 10px 30px -10px rgba(0,0,0,0.4)`
      }}
    >
      {/* Decorative radial blur backing */}
      <div 
        className="absolute -right-4 -bottom-4 w-24 h-24 rounded-full blur-2xl opacity-10 group-hover:opacity-25 transition-opacity duration-300 pointer-events-none"
        style={{ backgroundColor: glowColor }}
      />
      
      <div className="flex items-center justify-between mb-4 z-10">
        <span className="text-slate-400 text-[10px] font-bold uppercase tracking-widest">{title}</span>
        <div className="w-8 h-8 rounded-lg bg-slate-950/60 border border-slate-850 flex items-center justify-center text-indigo-400 shadow-inner">
          {icon}
        </div>
      </div>
      
      <div className="text-left z-10">
        <h4 className="text-xl font-black text-slate-100 font-mono tracking-tight leading-none mb-1">{value}</h4>
        <span className="text-[9px] font-bold text-slate-500 tracking-wider uppercase">{label}</span>
      </div>
    </div>
  );
}

// --- Internal Helper DeviceStatusRow Component ---
interface DeviceStatusRowProps {
  label: string;
  status: string;
  online: boolean;
}

function DeviceStatusRow({ label, status, online }: DeviceStatusRowProps) {
  return (
    <div className="flex items-center justify-between p-2.5 bg-slate-950/40 border border-slate-850 rounded-xl">
      <span className="text-slate-400 font-medium">{label}</span>
      <div className="flex items-center gap-2">
        <span className={cn(
          "w-1.5 h-1.5 rounded-full shadow-md",
          online ? "bg-emerald-400 shadow-emerald-500/20" : "bg-red-400 shadow-red-500/20 animate-pulse"
        )}></span>
        <span className={cn("font-bold text-[11px]", online ? "text-slate-200" : "text-red-400 font-bold")}>{status}</span>
      </div>
    </div>
  );
}
