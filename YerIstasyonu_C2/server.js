const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');
const { SatelliteSimulator, STATE_NAMES } = require('./satellite_sim');

const app = express();
const server = http.createServer(app);
const io = new Server(server, { cors: { origin: '*' } });

app.use(express.static(path.join(__dirname, 'public')));

app.get('/', (req, res) => res.sendFile(path.join(__dirname, 'public', 'index.html')));
app.get('/mobile', (req, res) => res.sendFile(path.join(__dirname, 'public', 'mobile.html')));

// uydu simulatoru
const sat = new SatelliteSimulator();
let simInterval = null;

// baglanti takibi
let stations = new Set();
let mobiles = new Set();

// rate limiting
const rateLimits = new Map();
function checkRate(socketId, event, maxPerSec) {
    const key = `${socketId}:${event}`;
    const now = Date.now();
    const last = rateLimits.get(key) || 0;
    if (now - last < (1000 / maxPerSec)) return false;
    rateLimits.set(key, now);
    return true;
}

// input validasyon
function validateCoord(val, min, max) {
    return typeof val === 'number' && !isNaN(val) && val >= min && val <= max;
}

function validateMissile(data) {
    if (!data || typeof data !== 'object') return false;
    if (typeof data.type !== 'string' || data.type.length > 50) return false;
    if (!validateCoord(data.originLat, -90, 90)) return false;
    if (!validateCoord(data.originLon, -180, 180)) return false;
    if (!validateCoord(data.targetLat, -90, 90)) return false;
    if (!validateCoord(data.targetLon, -180, 180)) return false;
    return true;
}

function validateAircraft(data) {
    if (!data || typeof data !== 'object') return false;
    if (typeof data.type !== 'string' || data.type.length > 50) return false;
    return true;
}

// uydu sim baslatma
function startSatSim() {
    if (simInterval) return;

    simInterval = setInterval(() => {
        sat.tick();
        const tlm = sat.generateTelemetry();

        // tum station'lara telemetri gonder
        io.emit('satellite_telemetry', tlm);

        // hedef tespit edildiyse ayri event
        if (tlm.intel.length > 0) {
            io.emit('satellite_intel', {
                state: STATE_NAMES[tlm.h.st],
                targets: tlm.intel,
                timestamp: tlm.h.ts
            });
            io.emit('server_log', {
                msg: `UYDU TESPİT: ${tlm.intel.length} hedef — ${tlm.intel.map(t => t.cls).join(', ')}`,
                level: 'warning'
            });
        }

        // durum degisikligi logu
        const stateName = sat.getStateName();
        if (sat.getStateSec() === 0) {
            io.emit('server_log', {
                msg: `UYDU DURUM: ${stateName}`,
                level: stateName === 'SAFE_MODE' ? 'critical' : 'info'
            });
        }

    }, 2000); // 2 saniyede bir (STM32 telemetri periyodu)

    console.log('\x1b[32m[SIM]\x1b[0m Uydu simulasyonu baslatildi (2s periyot)');
}

function stopSatSim() {
    if (simInterval) {
        clearInterval(simInterval);
        simInterval = null;
    }
}

io.on('connection', (socket) => {
    const clientType = socket.handshake.query.type || 'unknown';

    if (clientType === 'station') {
        stations.add(socket.id);
        console.log(`\x1b[36m[YER İSTASYONU]\x1b[0m Bağlandı (${stations.size} aktif)`);
    } else if (clientType === 'mobile') {
        mobiles.add(socket.id);
        console.log(`\x1b[33m[MOBİL KONTROL]\x1b[0m Bağlandı (${mobiles.size} aktif)`);
    }

    io.emit('conn_status', { stations: stations.size, mobiles: mobiles.size });

    // ilk baglananda sim baslat
    if (stations.size > 0 && !simInterval) {
        startSatSim();
    }

    // --- MOBILE -> SERVER -> STATION ---

    socket.on('launch_missile', (data) => {
        if (!checkRate(socket.id, 'missile', 1)) return; // 1/saniye
        if (!validateMissile(data)) {
            console.log('\x1b[31m[HATA]\x1b[0m Gecersiz fuze verisi reddedildi');
            return;
        }
        console.log(`\x1b[31m[FÜZE]\x1b[0m ${data.type} fırlatıldı!`);
        io.emit('missile_incoming', data);
        io.emit('server_log', { msg: `FÜZE FIRLATILDI: ${data.type}`, level: 'critical' });
    });

    socket.on('spawn_aircraft', (data) => {
        if (!checkRate(socket.id, 'aircraft', 2)) return; // 2/saniye
        if (!validateAircraft(data)) return;
        console.log(`\x1b[33m[HAVA]\x1b[0m ${data.type} tespit`);
        io.emit('aircraft_detected', data);
        io.emit('server_log', { msg: `HAVA HEDEFİ: ${data.type}`, level: 'warning' });
    });

    socket.on('siren_on', () => {
        if (!checkRate(socket.id, 'siren', 1)) return;
        console.log('\x1b[31m[SİREN]\x1b[0m Aktif');
        io.emit('siren_start');
        io.emit('server_log', { msg: 'SİREN AKTİF', level: 'critical' });
    });

    socket.on('siren_off', () => {
        console.log('\x1b[32m[SİREN]\x1b[0m Kapatıldı');
        io.emit('siren_stop');
        io.emit('server_log', { msg: 'SİREN KAPATILDI', level: 'ok' });
    });

    socket.on('adsb_refresh', () => {
        if (!checkRate(socket.id, 'adsb', 0.1)) return; // 10 saniyede 1
        console.log('\x1b[36m[ADS-B]\x1b[0m Güncelleniyor');
        io.emit('adsb_update');
        io.emit('server_log', { msg: 'ADS-B VERİ GÜNCELLEMESİ', level: 'info' });
    });

    socket.on('change_mode', (data) => {
        if (!data || typeof data.mode === 'undefined') return;
        const mode = parseInt(data.mode);
        if (isNaN(mode) || mode < 0 || mode > 2) return;
        console.log(`\x1b[36m[MOD]\x1b[0m Mod: ${mode}`);
        io.emit('mode_change', { mode });
    });

    socket.on('station_status', (data) => {
        socket.broadcast.emit('station_status', data);
    });

    // CubeSat animasyon modu (IDLE / AI_SCAN)
    socket.on('cubesat_mode', (data) => {
        if (!data || typeof data.mode !== 'string') return;
        console.log(`\x1b[35m[CUBESAT]\x1b[0m Mod: ${data.mode}`);
        io.emit('cubesat_mode_change', data);
        io.emit('server_log', { msg: `CUBESAT MOD: ${data.mode}`, level: 'info' });
    });

    // CubeSat panel kontrolu
    socket.on('cubesat_panel', (data) => {
        if (!data) return;
        io.emit('cubesat_panel_update', data);
    });

    // CubeSat anten kontrolu
    socket.on('cubesat_antenna', (data) => {
        if (!data) return;
        io.emit('cubesat_antenna_update', data);
    });

    // uydu sim kontrolu (mobil veya station'dan)
    socket.on('sat_command', (data) => {
        if (!data || typeof data.cmd !== 'string') return;
        const cmd = data.cmd;

        if (cmd === 'FORCE_OPTICAL') {
            sat.changeState(0x10);
            console.log('\x1b[35m[SAT CMD]\x1b[0m Optik tarama zorlanıyor');
        } else if (cmd === 'FORCE_RF') {
            sat.changeState(0x11);
            console.log('\x1b[35m[SAT CMD]\x1b[0m RF tarama zorlanıyor');
        } else if (cmd === 'FORCE_SAFE') {
            sat.changeState(0xFF);
            console.log('\x1b[35m[SAT CMD]\x1b[0m Safe mode zorlanıyor');
        } else if (cmd === 'FORCE_CHARGE') {
            sat.changeState(0x02);
            console.log('\x1b[35m[SAT CMD]\x1b[0m Sarj moduna geciliyor');
        }

        io.emit('server_log', { msg: `UYDU KOMUT: ${cmd}`, level: 'info' });
    });

    socket.on('disconnect', () => {
        stations.delete(socket.id);
        mobiles.delete(socket.id);
        io.emit('conn_status', { stations: stations.size, mobiles: mobiles.size });
        console.log(`\x1b[90m[BAĞLANTI KESİLDİ]\x1b[0m ${stations.size} istasyon, ${mobiles.size} mobil`);

        // hicbir station yoksa sim durdur
        if (stations.size === 0) {
            stopSatSim();
        }
    });
});

// REST API: son telemetri verisi
app.get('/api/telemetry', (req, res) => {
    res.json(sat.generateTelemetry());
});

app.get('/api/status', (req, res) => {
    res.json({
        state: sat.getStateName(),
        battery: sat.battery,
        uptime: sat.getUptime(),
        targets: sat.targets.length,
        sequence: sat.sequence,
        connections: { stations: stations.size, mobiles: mobiles.size }
    });
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, '0.0.0.0', () => {
    console.log('');
    console.log('\x1b[36m╔════════════════════════════════════════════════════╗\x1b[0m');
    console.log('\x1b[36m║\x1b[0m  \x1b[1m\x1b[36mGÖLGE-1 TAKTİK C2 SUNUCUSU v2.1\x1b[0m                  \x1b[36m║\x1b[0m');
    console.log('\x1b[36m╠════════════════════════════════════════════════════╣\x1b[0m');
    console.log(`\x1b[36m║\x1b[0m  Yer İstasyonu : \x1b[32mhttp://localhost:${PORT}/\x1b[0m            \x1b[36m║\x1b[0m`);
    console.log(`\x1b[36m║\x1b[0m  Mobil Kontrol : \x1b[33mhttp://localhost:${PORT}/mobile\x1b[0m      \x1b[36m║\x1b[0m`);
    console.log(`\x1b[36m║\x1b[0m  Telemetri API : \x1b[32mhttp://localhost:${PORT}/api/telemetry\x1b[0m\x1b[36m║\x1b[0m`);
    console.log(`\x1b[36m║\x1b[0m  Durum API     : \x1b[32mhttp://localhost:${PORT}/api/status\x1b[0m   \x1b[36m║\x1b[0m`);
    console.log('\x1b[36m║\x1b[0m  Uydu Sim      : \x1b[35mSTM32 format, 2s periyot\x1b[0m          \x1b[36m║\x1b[0m');
    console.log('\x1b[36m╚════════════════════════════════════════════════════╝\x1b[0m');
    console.log('');
});
