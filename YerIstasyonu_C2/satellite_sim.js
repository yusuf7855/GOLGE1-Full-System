// GOLGE-1 Uydu Telemetri Simulatoru
// STM32 OBC firmware JSON formatiyla birebir uyumlu

const STATES = {
    BOOT:          0x00,
    DETUMBLE:      0x01,
    IDLE_CHARGE:   0x02,
    OPTICAL_SCAN:  0x10,
    RF_SCAN:       0x11,
    COMMS_RELAY:   0x20,
    STORE_FWD:     0x21,
    SAFE_MODE:     0xFF
};

const STATE_NAMES = {
    0x00: 'BOOT', 0x01: 'DETUMBLE', 0x02: 'IDLE_CHARGE',
    0x10: 'OPTICAL_SCAN', 0x11: 'RF_SCAN',
    0x20: 'COMMS_RELAY', 0x21: 'STORE_FWD', 0xFF: 'SAFE_MODE'
};

// hedef senaryolari (STM32 payload moduluyle ayni)
const TARGET_SCENARIOS = [
    { id: 'TGT-OPT-001', typ: 'OPTICAL', cls: 'TANK', lat: 39.9334, lon: 32.8597, alt: 850, hdg: 45, spd: 35, cnf: 0.89, sen: 0 },
    { id: 'TGT-OPT-002', typ: 'IR_ANOMALY', cls: 'APC', lat: 39.9256, lon: 32.8701, alt: 830, hdg: 120, spd: 50, cnf: 0.78, sen: 1 },
    { id: 'TGT-OPT-003', typ: 'OPTICAL', cls: 'CONVOY', lat: 39.9412, lon: 32.8489, alt: 870, hdg: 270, spd: 40, cnf: 0.92, sen: 0 },
    { id: 'TGT-RF-001', typ: 'RF_SHADOW', cls: 'STEALTH_AC', lat: 41.5012, lon: 36.1120, alt: 12000, hdg: 180, spd: 850, cnf: 0.87, sen: 2 },
    { id: 'TGT-RF-002', typ: 'MICRO_DOPPLER', cls: 'AIRCRAFT', lat: 41.4923, lon: 36.0987, alt: 8500, hdg: 225, spd: 720, cnf: 0.93, sen: 3 },
    { id: 'TGT-RF-003', typ: 'RF_SHADOW', cls: 'UAV', lat: 41.5134, lon: 36.1345, alt: 3000, hdg: 90, spd: 180, cnf: 0.71, sen: 2 },
];

class SatelliteSimulator {
    constructor() {
        this.state = STATES.BOOT;
        this.sequence = 0;
        this.bootTime = Date.now();
        this.stateEntryTime = Date.now();
        this.battery = 50.0;
        this.orbitAngle = 0;
        this.targets = [];
        this.scanCycle = 0;
        this.packetsent = 0;
        this.targetCount = 0;
        this.tmrCorrections = 0;
        this.tickCount = 0;

        // yorunge
        this.satLat = 39.5;
        this.satLon = 32.0;
        this.satAlt = 500;
        this.inclination = 97.4;

        // ADCS
        this.angularRate = [0.15, 0.08, -0.12];
        this.quaternion = [1, 0, 0, 0];
        this.isStable = false;

        // termal
        this.tempMcu = 38;
        this.tempAi = 25;
        this.tempSdr = 20;
    }

    getUptime() {
        return Math.floor((Date.now() - this.bootTime) / 1000);
    }

    getStateSec() {
        return Math.floor((Date.now() - this.stateEntryTime) / 1000);
    }

    changeState(newState) {
        this.state = newState;
        this.stateEntryTime = Date.now();
    }

    // durum makinesi guncelle (STM32 freertos.c ile ayni mantik)
    tick() {
        this.tickCount++;
        const stateSec = this.getStateSec();

        // yorunge guncelle
        this.orbitAngle += 0.006;
        if (this.orbitAngle > 360) this.orbitAngle -= 360;
        this.satLon += 0.006;
        if (this.satLon > 45) this.satLon = 25;
        this.satLat = 39.5 + 3 * Math.sin(this.orbitAngle * Math.PI / 180);

        // tutulma: 240-360 arasi
        const inEclipse = this.orbitAngle >= 240;
        const solarPower = inEclipse ? 0 : 25000 * Math.sin((this.orbitAngle / 240) * Math.PI);

        switch (this.state) {
            case STATES.BOOT:
                if (stateSec > 5) this.changeState(STATES.DETUMBLE);
                break;

            case STATES.DETUMBLE:
                // B-dot: acisal hiz azalir
                this.angularRate = this.angularRate.map(r => r * 0.95);
                const rate = Math.sqrt(this.angularRate.reduce((s, r) => s + r * r, 0));
                if (rate < 0.01 || stateSec > 30) {
                    this.isStable = true;
                    this.changeState(STATES.IDLE_CHARGE);
                }
                break;

            case STATES.IDLE_CHARGE:
                this.battery += inEclipse ? -0.1 : 0.8;
                this.battery = Math.min(100, Math.max(0, this.battery));
                if (this.battery >= 80) {
                    if (this.scanCycle % 2 === 0)
                        this.changeState(STATES.OPTICAL_SCAN);
                    else
                        this.changeState(STATES.RF_SCAN);
                    this.scanCycle++;
                }
                break;

            case STATES.OPTICAL_SCAN:
                this.battery -= 0.3;
                this.tempAi = Math.min(75, this.tempAi + 2);
                // her 3. tick'te hedef bul (~6s)
                if (this.tickCount % 3 === 0 && this.targets.length === 0) {
                    const idx = Math.floor(Math.random() * 3);
                    const t = { ...TARGET_SCENARIOS[idx] };
                    t.lat += (Math.random() - 0.5) * 0.01;
                    t.lon += (Math.random() - 0.5) * 0.01;
                    this.targets.push(t);
                    this.targetCount++;
                }
                if (this.targets.length > 0) this.changeState(STATES.COMMS_RELAY);
                if (this.battery < 40) this.changeState(STATES.IDLE_CHARGE);
                if (stateSec > 45) this.changeState(STATES.IDLE_CHARGE);
                break;

            case STATES.RF_SCAN:
                this.battery -= 0.25;
                this.tempSdr = Math.min(65, this.tempSdr + 1);
                // her 4. tick'te RF golge bul (~8s)
                if (this.tickCount % 4 === 0 && this.targets.length === 0) {
                    const idx = 3 + Math.floor(Math.random() * 3);
                    const t = { ...TARGET_SCENARIOS[idx] };
                    t.lat += (Math.random() - 0.5) * 0.02;
                    t.lon += (Math.random() - 0.5) * 0.02;
                    this.targets.push(t);
                    this.targetCount++;
                }
                if (this.targets.length > 0) this.changeState(STATES.COMMS_RELAY);
                if (this.battery < 40) this.changeState(STATES.IDLE_CHARGE);
                if (stateSec > 60) this.changeState(STATES.IDLE_CHARGE);
                break;

            case STATES.COMMS_RELAY:
                this.battery -= 0.2;
                // 5 saniye sonra iletim tamam
                if (stateSec > 5) {
                    this.targets = [];
                    this.changeState(STATES.IDLE_CHARGE);
                }
                break;

            case STATES.SAFE_MODE:
                this.battery += inEclipse ? -0.05 : 0.3;
                this.battery = Math.min(100, Math.max(0, this.battery));
                if (this.battery > 50 && stateSec > 60) {
                    this.changeState(STATES.IDLE_CHARGE);
                }
                break;
        }

        // termal soguma
        if (this.state !== STATES.OPTICAL_SCAN)
            this.tempAi = Math.max(20, this.tempAi - 1);
        if (this.state !== STATES.RF_SCAN)
            this.tempSdr = Math.max(15, this.tempSdr - 1);

        // batarya kritik -> safe mode
        if (this.battery < 15 && this.state !== STATES.SAFE_MODE) {
            this.changeState(STATES.SAFE_MODE);
        }
    }

    // STM32 COMMS_TelemetryToJSON ile birebir ayni format
    generateTelemetry() {
        this.sequence++;
        this.packetsent++;
        const uptime = this.getUptime();
        const batVoltage = 6.0 + (this.battery / 100) * 2.4;
        const solarPwr = this.orbitAngle >= 240 ? 0 : 25000 * Math.sin((this.orbitAngle / 240) * Math.PI);

        const tlm = {
            h: {
                sat: 'GLG1',
                ver: 2,
                st: this.state,
                seq: this.sequence,
                ts: uptime,
                typ: this.targets.length > 0 ? 1 : 0
            },
            eps: {
                bat: Math.round(this.battery * 10) / 10,
                bv: Math.round(batVoltage * 100) / 100,
                bi: Math.round((solarPwr / batVoltage - 500) * 10) / 10,
                sv: Math.round(9 * (solarPwr / 25000) * 100) / 100,
                sp: Math.round(solarPwr),
                pd: Math.round(this._getPowerDraw()),
                pm: Math.round(solarPwr - this._getPowerDraw()),
                ecl: this.orbitAngle >= 240 ? 1 : 0
            },
            thm: {
                mcu: this.tempMcu,
                ai: this.tempAi,
                sdr: this.tempSdr,
                bat: Math.round(20 + Math.random() * 5),
                ext: Math.round(-20 + Math.random() * 10)
            },
            adcs: {
                q: this.quaternion.map(v => Math.round(v * 1000) / 1000),
                w: this.angularRate.map(v => Math.round(v * 1000) / 1000),
                pm: this._getPointingMode(),
                stb: this.isStable ? 1 : 0,
                err: Math.round(Math.random() * 2 * 100) / 100
            },
            sub: {
                jet: this._subsysState('jetson'),
                sdr: this._subsysState('sdr'),
                cam: this._subsysState('camera'),
                ir: this._subsysState('ir'),
                adcs: this.isStable ? 2 : 1
            },
            stat: {
                up: uptime,
                boot: 1,
                err: 0,
                pkt: this.packetsent,
                tgt: this.targetCount,
                tmr: this.tmrCorrections
            },
            intel: this.targets.map(t => ({
                id: t.id,
                typ: t.typ,
                cls: t.cls,
                lat: t.lat,
                lon: t.lon,
                alt: t.alt,
                hdg: t.hdg,
                spd: t.spd,
                cnf: t.cnf,
                pri: t.cnf >= 0.85 ? 3 : 2,
                sen: t.sen,
                sig: t.sen >= 2 ? -42.5 : 0
            }))
        };

        return tlm;
    }

    _getPowerDraw() {
        let total = 500; // OBC
        switch (this.state) {
            case STATES.OPTICAL_SCAN: total += 5000 + 2000 + 1000 + 800; break;
            case STATES.RF_SCAN: total += 5000 + 1500 + 800; break;
            case STATES.COMMS_RELAY: total += 3000 + 800; break;
            default: total += 200; break;
        }
        return total;
    }

    _getPointingMode() {
        switch (this.state) {
            case STATES.DETUMBLE: return 0;
            case STATES.OPTICAL_SCAN:
            case STATES.RF_SCAN: return 2;
            case STATES.COMMS_RELAY: return 4;
            case STATES.IDLE_CHARGE: return 3;
            default: return 1;
        }
    }

    _subsysState(subsys) {
        switch (subsys) {
            case 'jetson':
                return (this.state === STATES.OPTICAL_SCAN || this.state === STATES.RF_SCAN) ? 2 : 0;
            case 'sdr':
                if (this.state === STATES.COMMS_RELAY) return 2;
                if (this.state === STATES.RF_SCAN) return 1;
                return 0;
            case 'camera':
                return this.state === STATES.OPTICAL_SCAN ? 2 : 0;
            case 'ir':
                return this.state === STATES.OPTICAL_SCAN ? 2 : 0;
            default: return 0;
        }
    }

    getStateName() {
        return STATE_NAMES[this.state] || 'UNKNOWN';
    }
}

module.exports = { SatelliteSimulator, STATES, STATE_NAMES };
