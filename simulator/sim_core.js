// River network simulator: the "real world" side.
//
// This file works out what the water is like everywhere on the river (flow,
// mixing, pollution plumes, rain) and turns it into the voltages each node's
// sensors would output. Those voltages go into the real node code (src/,
// compiled to WebAssembly), which does all the detecting. Nothing in here
// decides whether there's pollution; the node code does that.
//
// Works in the browser and in Node (for testing).

"use strict";

// ---------------------------------------------------------------- Node code (WebAssembly)

class NodeCode {
    constructor(instance) {
        this.x = instance.exports;
        if (this.x._initialize) {
            this.x._initialize();
        }
        this.encoder = new TextEncoder();
        this.decoder = new TextDecoder();
    }

    static async load(wasmBytes) {
        // The C library asks for these, but the node code never prints to a file
        const stubs = {
            fd_close: () => 0, fd_seek: () => 0, fd_write: () => 0,
            proc_exit: () => 0, fd_fdstat_get: () => 0,
        };
        const { instance } = await WebAssembly.instantiate(wasmBytes, { wasi_snapshot_preview1: stubs });
        return new NodeCode(instance);
    }

    slot(i, text) {
        const pointer = this.x.text_slot(i);
        const bytes = this.encoder.encode(String(text ?? "").slice(0, 60));
        const memory = new Uint8Array(this.x.memory.buffer, pointer, 64);
        memory.set(bytes);
        memory[bytes.length] = 0;
    }

    text(pointer) {
        const memory = new Uint8Array(this.x.memory.buffer);
        let end = pointer;
        while (memory[end] !== 0) end++;
        return this.decoder.decode(memory.subarray(pointer, end));
    }

    reset() { this.x.sim_reset(); }
    addBranch(name, lengthM, flowsInto, joinsAtM) {
        this.slot(0, name); this.slot(1, flowsInto || "");
        return this.x.sim_add_branch(lengthM, joinsAtM);
    }
    addNode(id, branch, distanceM, place) {
        this.slot(0, id); this.slot(1, branch); this.slot(2, place);
        return this.x.sim_add_node(Math.round(distanceM)) === 1;
    }
    removeNode(id) { this.slot(0, id); return this.x.sim_remove_node() === 1; }
    moveNode(id, branch, distanceM) {
        this.slot(0, id); this.slot(1, branch);
        return this.x.sim_move_node(Math.round(distanceM)) === 1;
    }
    addSample(id, phV, tdsV, ntuV, tempC) { this.slot(0, id); this.x.sim_add_sample(phV, tdsV, ntuV, tempC); }
    closeMinute(id, uptimeSec, nowMin) { this.slot(0, id); this.x.sim_close_minute(uptimeSec, nowMin); }
    serial(id, nowMin) { this.slot(0, id); return this.text(this.x.sim_serial_output(nowMin)); }
    station(id, nowMin) { this.slot(0, id); return JSON.parse(this.text(this.x.sim_station_json(nowMin))); }
    label(labelId) { return JSON.parse(this.text(this.x.sim_label_json(labelId))); }
    setting(which) { return this.x.sim_setting(which); }
}

// ---------------------------------------------------------------- Sensor voltages
// The inverse of src/compensation.cpp: what voltage each sensor gives for a
// given water quality. (Numbers match src/config.h.)

const SENSOR = {
    PH_V7: 2.50, PH_V4: 3.05,
    TDS_K: 1.0, TDS_TEMP_COEFF: 0.02, TDS_FACTOR: 0.5,
    TURB_V_CLEAR: 4.50, TURB_V_CURVE_CLEAR: 4.20,
    ADC_MAX_V: 3.1,               // the ESP32 ADC can't read above about 3.1 V
    TDS_BOARD_MAX_V: 2.3,         // the STJF TDS board can't output more than this
    PH_DIVIDER: 2.2, TDS_DIVIDER: 1.0, TURB_DIVIDER: 2.2,
};

function phToVolts(ph) {
    return SENSOR.PH_V7 + (ph - 7.0) * (SENSOR.PH_V7 - SENSOR.PH_V4) / 3.0;
}

function tdsToVolts(tds, tempC) {
    // TDS (at 25 C) -> EC25 -> EC at the water's temperature -> voltage (solve the cubic)
    const ec = (tds / SENSOR.TDS_FACTOR) * (1 + SENSOR.TDS_TEMP_COEFF * (tempC - 25)) / SENSOR.TDS_K;
    const curve = (v) => 133.42 * v * v * v - 255.86 * v * v + 857.39 * v;
    let low = 0, high = SENSOR.ADC_MAX_V * SENSOR.TDS_DIVIDER;
    if (ec <= 0) return 0;
    if (curve(high) <= ec) return high;           // sensor maxed out
    for (let i = 0; i < 40; i++) {
        const mid = (low + high) / 2;
        if (curve(mid) < ec) low = mid; else high = mid;
    }
    return (low + high) / 2;
}

function ntuToVolts(ntu) {
    // ntu = -1120.4 v^2 + 5742.3 v - 4352.9 on the clear-water side of the curve
    const n = Math.max(0, Math.min(3000, ntu));
    const disc = 5742.3 * 5742.3 - 4 * 1120.4 * (4352.9 + n);
    const v = (5742.3 + Math.sqrt(Math.max(0, disc))) / (2 * 1120.4);
    return v * SENSOR.TURB_V_CLEAR / SENSOR.TURB_V_CURVE_CLEAR;
}

// ---------------------------------------------------------------- Water chemistry
// pH is worked out from alkalinity and dissolved carbonate (the river's natural
// buffer), so a small amount of acid barely moves the pH but a lot of it
// suddenly drops it, like real rivers.

const K1 = Math.pow(10, -6.35), K2 = Math.pow(10, -10.33), KW = 1e-14;

function alkalinityAt(ph, ct) {
    const h = Math.pow(10, -ph);
    const d = h * h + K1 * h + K1 * K2;
    return ct * (K1 * h + 2 * K1 * K2) / d + KW / h - h;
}

function phFromAlkalinity(alk, ct) {
    let low = 0, high = 14;
    for (let i = 0; i < 50; i++) {
        const mid = (low + high) / 2;
        if (alkalinityAt(mid, ct) < alk) low = mid; else high = mid;
    }
    return (low + high) / 2;
}

// Waste types. strength scales how much acid/alkali is behind the pH (mine
// water carries dissolved iron and aluminium that act like extra acid).
// ct = dissolved carbonate in mmol/L, acid = extra acidity in meq/L (ammonium
// turning into nitrate releases acid), nutrient = how much it feeds algae.
const WASTE_TYPES = {
    mine:     { name: "Mine: acid drainage",       ph: 2.8,  tds: 3000,  ntu: 40,  temp: null, strength: 8,  ct: 0,   nutrient: 0,   flow: 250 },
    sewage:   { name: "Sewage overflow (untreated)", ph: 7.0, tds: 600,  ntu: 150, temp: 21,   strength: 1,  ct: 6,   nutrient: 0.03, flow: 1000 },
    treated:  { name: "Treated sewage effluent",   ph: 6.6,  tds: 800,   ntu: 4,   temp: 20,   strength: 1,  ct: 3,   acid: 0.5, nutrient: 0.05, flow: 600 },
    concrete: { name: "Concrete plant washwater",  ph: 12.0, tds: 2500,  ntu: 150, temp: null, strength: 1,  ct: 0,   nutrient: 0,   flow: 250 },
    chemical: { name: "Chemical plant: acid",      ph: 2.5,  tds: 4000,  ntu: 20,  temp: 35,   strength: 6,  ct: 0,   nutrient: 0,   flow: 400 },
    power:    { name: "Power station cooling",     ph: 7.4,  tds: 260,   ntu: 6,   temp: 38,   strength: 1,  ct: 1.2, nutrient: 0,   flow: 800 },
    quarry:   { name: "Quarry / land clearing",    ph: 7.4,  tds: 240,   ntu: 900, temp: null, strength: 1,  ct: 1.2, nutrient: 0,   flow: 300 },
    brine:    { name: "Desalination brine",        ph: 7.8,  tds: 40000, ntu: 3,   temp: null, strength: 1,  ct: 2,   nutrient: 0,   flow: 250 },
    farm:     { name: "Farm fertiliser runoff",    ph: 6.0,  tds: 700,   ntu: 12,  temp: null, strength: 1,  ct: 0.5, acid: 3, nutrient: 1, flow: 300 },
};

// ---------------------------------------------------------------- The river

const CELL_M = 20;                // the river is split into 20 m long cells
const MINUTE_S = 60;
const SETTLE_S = 2 * 3600;        // suspended mud halves roughly every 1.4 h
const COOL_S = 3 * 3600;          // warm water cools towards the river's temperature

// Default layout: two streams joining into the main river
function defaultLayout() {
    return {
        branches: [
            // main first: the streams flow into it
            { name: "main",     length: 4500, flowsInto: null,   joinsAt: 0, flow: 0,   speed: 0.60,
              water: null },
            { name: "stream_a", length: 3000, flowsInto: "main", joinsAt: 0, flow: 2.0, speed: 0.45,
              water: { ph: 7.2, tds: 180, ntu: 6, temp: 16.0, ct: 1.1 } },
            { name: "stream_b", length: 2600, flowsInto: "main", joinsAt: 0, flow: 3.0, speed: 0.50,
              water: { ph: 7.4, tds: 240, ntu: 9, temp: 17.5, ct: 1.5 } },
        ],
        nodes: [
            { id: "A1", branch: "stream_a", x: 700,  place: "forest creek" },
            { id: "A2", branch: "stream_a", x: 2300, place: "below mine" },
            { id: "B1", branch: "stream_b", x: 1900, place: "below works" },
            { id: "M1", branch: "main",     x: 500,  place: "below the join" },
            { id: "M2", branch: "main",     x: 2500, place: "below plant" },
            { id: "M3", branch: "main",     x: 4000, place: "town intake" },
        ],
        factories: [
            { name: "Factory 1", branch: "stream_a", x: 1500, type: "mine" },
            { name: "Factory 2", branch: "stream_b", x: 900,  type: "sewage" },
            { name: "Factory 3", branch: "main",     x: 1500, type: "concrete" },
        ],
    };
}

class River {
    constructor(layout) {
        this.branches = layout.branches.map((b) => ({ ...b, water: b.water ? { ...b.water } : null }));
        this.byName = {};
        for (const b of this.branches) {
            this.byName[b.name] = b;
            b.cells = Math.ceil(b.length / CELL_M) + 1;
        }
        this.numTracers = 0;
    }

    // One set of tracer arrays per factory: f = fraction of the water that is
    // its waste, s = its mud (settles), h = its heat (cools)
    setTracers(count) {
        this.numTracers = count;
        for (const b of this.branches) {
            b.f = []; b.s = []; b.h = [];
            for (let k = 0; k < count; k++) {
                b.f.push(new Float64Array(b.cells));
                b.s.push(new Float64Array(b.cells));
                b.h.push(new Float64Array(b.cells));
            }
        }
    }

    streamsInto(branch) {
        return this.branches.filter((b) => b.flowsInto === branch.name);
    }

    // Flow (m3/s) and speed (m/s) right now. Rain makes both go up.
    flowOf(branch, rain) {
        if (branch.flowsInto === null || branch.flow === 0) {
            let total = 0;
            for (const s of this.streamsInto(branch)) total += this.flowOf(s, rain);
            return total;
        }
        return branch.flow * (1 + 0.8 * rain);
    }
    speedOf(branch, rain) {
        return branch.speed * (1 + 0.3 * rain);
    }

    // Clean water on a branch (the main river is the streams mixed together)
    background(branch, rain, hour) {
        let w;
        if (branch.water) {
            w = { ...branch.water };
            w.alk = alkalinityAt(w.ph, w.ct / 1000);
            w.ct = w.ct / 1000;
        } else {
            let q = 0;
            w = { tds: 0, ntu: 0, temp: 0, alk: 0, ct: 0 };
            for (const s of this.streamsInto(branch)) {
                const sw = this.background(s, 0, hour);
                const sq = this.flowOf(s, 0);
                q += sq;
                for (const key of ["tds", "ntu", "temp", "alk", "ct"]) w[key] += sw[key] * sq;
            }
            for (const key of ["tds", "ntu", "temp", "alk", "ct"]) w[key] /= q;
            w.ph = phFromAlkalinity(w.alk, w.ct);
        }
        // Water is warmest mid afternoon and coolest before dawn
        w.temp += 0.6 * Math.sin((hour - 9) / 24 * 2 * Math.PI);
        // Rain dilutes the dissolved salts, washes mud in and cools the water
        w.tds *= (1 - 0.25 * rain);
        w.alk *= (1 - 0.25 * rain);
        w.ct *= (1 - 0.25 * rain);
        w.ntu *= (1 + 3.5 * rain);
        w.temp -= 2.0 * rain;
        return w;
    }

    // Moves the water along one minute, then adds whatever the factories put in
    step(factories, rain) {
        const order = [...this.branches].sort((a, b) => this.depth(b) - this.depth(a)); // streams first
        const settle = Math.exp(-MINUTE_S / SETTLE_S);
        const cool = Math.exp(-MINUTE_S / COOL_S);
        for (const b of order) {
            const shift = this.speedOf(b, rain) * MINUTE_S / CELL_M;
            // What flows in at the top: nothing for a stream, the mixed streams for the main river
            const inflow = { f: [], s: [], h: [] };
            const streams = this.streamsInto(b).filter((s) => s.joinsAt <= CELL_M);
            let q = 0;
            for (let k = 0; k < this.numTracers; k++) { inflow.f.push(0); inflow.s.push(0); inflow.h.push(0); }
            for (const s of streams) {
                const sq = this.flowOf(s, rain);
                q += sq;
                for (let k = 0; k < this.numTracers; k++) {
                    inflow.f[k] += s.f[k][s.cells - 1] * sq;
                    inflow.s[k] += s.s[k][s.cells - 1] * sq;
                    inflow.h[k] += s.h[k][s.cells - 1] * sq;
                }
            }
            for (let k = 0; k < this.numTracers; k++) {
                if (q > 0) { inflow.f[k] /= q; inflow.s[k] /= q; inflow.h[k] /= q; }
                for (const [name, keep] of [["f", 1], ["s", settle], ["h", cool]]) {
                    const old = b[name][k];
                    const moved = new Float64Array(b.cells);
                    for (let i = 0; i < b.cells; i++) {
                        const from = i - shift;
                        if (from <= 0) {
                            const t = Math.max(0, from + 1); // part of this cell came from the top
                            moved[i] = inflow[name][k] * (1 - t) + old[0] * t;
                            if (from < -1) moved[i] = inflow[name][k];
                        } else {
                            const j = Math.floor(from);
                            const t = from - j;
                            moved[i] = old[j] * (1 - t) + old[Math.min(j + 1, b.cells - 1)] * t;
                        }
                    }
                    // A little spreading out (turbulent mixing)
                    for (let i = 1; i < b.cells - 1; i++) {
                        old[i] = moved[i] + 0.08 * (moved[i - 1] - 2 * moved[i] + moved[i + 1]);
                    }
                    old[0] = moved[0];
                    old[b.cells - 1] = moved[b.cells - 1];
                    if (keep !== 1) for (let i = 0; i < b.cells; i++) old[i] *= keep;
                }
            }
            // Streams that join part way down this branch
            for (const s of this.streamsInto(b).filter((s) => s.joinsAt > CELL_M)) {
                const i = Math.min(b.cells - 1, Math.round(s.joinsAt / CELL_M));
                const mainQ = this.flowOf(b, rain) - this.flowOf(s, rain);
                const sq = this.flowOf(s, rain);
                for (let k = 0; k < this.numTracers; k++) {
                    for (const name of ["f", "s", "h"]) {
                        b[name][k][i] = (b[name][k][i] * mainQ + s[name][k][s.cells - 1] * sq) / (mainQ + sq);
                    }
                }
            }
            // Factory outfalls: mix the waste into the water that went past this minute
            const riverQ = this.flowOf(b, rain);
            factories.forEach((fac, k) => {
                if (!fac.on || fac.branch !== b.name || fac.flowLps <= 0) return;
                const q = fac.flowLps / 1000;
                const m = q / (riverQ + q);
                const first = Math.floor(fac.x / CELL_M);
                const last = Math.min(b.cells - 1, Math.max(first, Math.floor((fac.x + shift * CELL_M) / CELL_M)));
                for (let i = first; i <= last; i++) {
                    for (let j = 0; j < this.numTracers; j++) {
                        for (const name of ["f", "s", "h"]) {
                            b[name][j][i] = b[name][j][i] * (1 - m) + (j === k ? m : 0);
                        }
                    }
                }
            });
        }
    }

    depth(branch) {
        let d = 0;
        let b = branch;
        while (b.flowsInto && d < 10) { b = this.byName[b.flowsInto]; d++; }
        return d;
    }

    tracerAt(branch, x, name, k) {
        const pos = Math.max(0, Math.min(branch.cells - 1, x / CELL_M));
        const i = Math.floor(pos);
        const t = pos - i;
        const arr = branch[name][k];
        return arr[i] * (1 - t) + arr[Math.min(i + 1, branch.cells - 1)] * t;
    }

    // What the water is like at a spot on the river
    waterAt(branchName, x, factories, rain, hour) {
        const b = this.byName[branchName];
        const bg = this.background(b, rain, hour);
        let wasteFraction = 0;
        const w = { tds: 0, alk: 0, ct: 0, ntu: bg.ntu, temp: bg.temp, nutrient: 0 };
        const mix = [];
        factories.forEach((fac, k) => {
            const f = this.tracerAt(b, x, "f", k);
            const s = this.tracerAt(b, x, "s", k);
            const h = this.tracerAt(b, x, "h", k);
            mix.push(f);
            wasteFraction += f;
            const e = fac.waste;
            const eAlk = alkalinityAt(e.ph, e.ct / 1000) +
                (e.strength - 1) * (KW / Math.pow(10, -e.ph) - Math.pow(10, -e.ph)) -
                (e.acid || 0) / 1000;
            w.tds += f * e.tds;
            w.alk += f * eAlk;
            w.ct += f * e.ct / 1000;
            w.ntu += s * e.ntu - f * bg.ntu;
            if (e.temp !== null && e.temp !== undefined) w.temp += h * (e.temp - fac.riverTemp);
            w.nutrient += f * e.nutrient * fac.algae;   // algae take time to grow
        });
        const clean = Math.max(0, 1 - wasteFraction);
        w.tds += clean * bg.tds;
        w.alk += clean * bg.alk;
        w.ct += clean * bg.ct;
        w.ph = phFromAlkalinity(w.alk, w.ct);
        // Algae fed by nutrients push the pH up by day and down at night (the
        // bloom builds up over about a day after the nutrients start arriving)
        const swing = 0.04 + Math.min(1.0, w.nutrient * 20);
        w.ph += swing * Math.sin((hour - 9) / 24 * 2 * Math.PI);
        w.ntu = Math.max(0, w.ntu);
        w.mix = mix;
        w.waste = wasteFraction;
        return w;
    }

    // Metres the water travels from spot a down to spot b (-1 if b isn't downstream of a)
    riverDistance(a, b) {
        let total = 0;
        let branch = this.byName[a.branch];
        let x = a.x;
        for (let steps = 0; steps < 10; steps++) {
            if (branch.name === b.branch) {
                return b.x >= x ? total + (b.x - x) : -1;
            }
            if (!branch.flowsInto) return -1;
            total += Math.max(0, branch.length - x);
            x = branch.joinsAt;
            branch = this.byName[branch.flowsInto];
        }
        return -1;
    }
}

// ---------------------------------------------------------------- The whole simulation

const SAMPLES_PER_MIN = 5;
const START_HOUR = 6;               // day 1 starts at 06:00
const HISTORY_MIN = 24 * 60;
const FAULTS = {
    none: "Working",
    temp: "Temp sensor unplugged",
    ph_broken: "pH probe broken",
    ph_stuck: "pH probe stuck",
    offline: "Node offline (no radio)",
};

class Simulation {
    constructor(code, layout) {
        this.code = code;
        this.layout = layout || defaultLayout();
        this.random = mulberry32(1234);
        this.reset();
    }

    reset() {
        const layout = this.layout;
        this.minute = 0;
        this.rainStart = null;
        this.rainTimes = [];
        this.log = [];
        this.detections = [];     // label turning on at a node: {label, node, minute}
        this.speedEstimates = [];
        this.river = new River(layout);
        this.factories = layout.factories.map((f) => {
            const waste = { ...WASTE_TYPES[f.type] };
            return { name: f.name, branch: f.branch, x: f.x, type: f.type, waste, flowLps: waste.flow,
                     on: false, startedMin: null, riverTemp: 16, firstSeen: {}, algae: 0 };
        });
        this.river.setTracers(this.factories.length);
        this.code.reset();
        for (const b of layout.branches) {
            this.code.addBranch(b.name, b.flowsInto ? b.length : 0, b.flowsInto, b.joinsAt);
        }
        this.nodes = [];
        this.nextNodeNumber = 1;
        for (const n of layout.nodes) this.addNode(n.id, n.branch, n.x, n.place, true);
        this.labels = [];
        const count = this.code.setting(5);
        for (let i = 0; i < count; i++) this.labels.push(this.code.label(i));
    }

    get hour() { return START_HOUR + this.minute / 60; }
    get rain() {
        if (this.rainStart === null) return 0;
        const t = this.minute - this.rainStart;        // 20 min to build, 60 min heavy, 60 min to ease off
        if (t < 0 || t > 140) return 0;
        if (t < 20) return t / 20;
        if (t < 80) return 1;
        return 1 - (t - 80) / 60;
    }

    clock(minute = this.minute) {
        const total = START_HOUR * 60 + minute;
        const day = Math.floor(total / 1440) + 1;
        const hh = String(Math.floor((total % 1440) / 60)).padStart(2, "0");
        const mm = String(total % 60).padStart(2, "0");
        return `Day ${day} ${hh}:${mm}`;
    }

    addLog(kind, text, extra = {}) {
        this.log.unshift({ minute: this.minute, time: this.clock(), kind, text, ...extra });
        if (this.log.length > 300) this.log.length = 300;
    }

    // ---- nodes

    addNode(id, branch, x, place, quiet = false) {
        if (!id) {
            while (this.nodes.some((n) => n.id === `N${this.nextNodeNumber}`)) this.nextNodeNumber++;
            id = `N${this.nextNodeNumber}`;
        }
        if (!this.code.addNode(id, branch, x, place || "")) return null;
        const node = { id, branch, x: Math.round(x), place: place || "", fault: "none", stuckPh: null,
                       report: null, history: [], lastActiveMin: -1e9 };
        this.nodes.push(node);
        if (!quiet) this.addLog("user", `Added node ${id} on ${branch} at ${Math.round(x)} m (learning normal levels)`);
        return node;
    }

    removeNode(id) {
        if (this.nodes.length <= 1) return false;
        this.code.removeNode(id);
        this.nodes = this.nodes.filter((n) => n.id !== id);
        this.addLog("user", `Removed node ${id}`);
        return true;
    }

    moveNode(id, branch, x) {
        const node = this.nodes.find((n) => n.id === id);
        if (!node) return;
        x = Math.round(x / 10) * 10;
        if (node.branch === branch && node.x === x) return;
        this.code.moveNode(id, branch, x);
        node.branch = branch;
        node.x = x;
        node.history = [];
        node.report = null;
        this.addLog("user", `Moved node ${id} to ${branch} ${x} m (it relearns normal levels there)`);
    }

    setFault(id, fault) {
        const node = this.nodes.find((n) => n.id === id);
        if (!node || node.fault === fault) return;
        node.fault = fault;
        node.stuckPh = null;
        this.addLog("user", fault === "none" ? `${id}: sensors fixed` : `${id}: ${FAULTS[fault]}`);
    }

    // ---- factories

    setFactoryType(k, type) {
        const fac = this.factories[k];
        fac.type = type;
        fac.waste = { ...WASTE_TYPES[type] };
        fac.flowLps = fac.waste.flow;
    }

    toggleFactory(k) {
        const fac = this.factories[k];
        fac.on = !fac.on;
        if (fac.on) {
            fac.startedMin = this.minute;
            fac.firstSeen = {};
            fac.riverTemp = this.river.background(this.river.byName[fac.branch], this.rain, this.hour).temp;
            this.addLog("factory", `${fac.name} started discharging ${fac.waste.name.toLowerCase()} at ${fac.flowLps} L/s`, { factory: k });
        } else {
            this.addLog("factory", `${fac.name} stopped discharging`, { factory: k });
        }
    }

    moveFactory(k, branch, x) {
        const fac = this.factories[k];
        fac.branch = branch;
        fac.x = Math.round(x / 10) * 10;
    }

    startRain() {
        this.rainStart = this.minute;
        this.rainTimes.push(this.minute);
        this.addLog("weather", "Rain storm started (about 2 h 20 min)");
    }

    // ---- one minute of time

    tick() {
        const rain = this.rain;
        const hour = this.hour;
        this.river.step(this.factories, rain);
        // Algae bloom grows while nutrients keep coming (about 12 h to build), dies back after
        for (const fac of this.factories) {
            if (fac.on && fac.waste.nutrient > 0) fac.algae += (1 - fac.algae) * (1 - Math.exp(-1 / 720));
            else fac.algae *= Math.exp(-1 / 720);
        }
        const uptime = (this.minute + 1) * 60;
        const nowMin = this.minute + 1;

        for (const node of this.nodes) {
            if (node.fault === "offline") continue;
            const w = this.river.waterAt(node.branch, node.x, this.factories, rain, hour);
            node.truth = w;
            for (let k = 0; k < SAMPLES_PER_MIN; k++) {
                const ph = w.ph + this.noise(0.01);
                const temp = w.temp + this.noise(0.02);
                const tds = w.tds * (1 + this.noise(0.004));
                const ntu = w.ntu * (1 + this.noise(0.02)) + this.noise(0.2);
                let phV = phToVolts(ph) + this.noise(0.001);
                let tempReading = temp;
                if (node.fault === "temp") tempReading = -127;
                if (node.fault === "ph_broken") phV = 5.6;
                if (node.fault === "ph_stuck") {
                    if (node.stuckPh === null) node.stuckPh = phV;
                    phV = node.stuckPh;
                }
                phV = clamp(phV, 0, SENSOR.ADC_MAX_V * SENSOR.PH_DIVIDER);
                const tdsV = clamp(tdsToVolts(tds, temp) + this.noise(0.0008), 0, SENSOR.TDS_BOARD_MAX_V); // very salty water pins it at the top
                const ntuV = clamp(ntuToVolts(ntu) + this.noise(0.0004), 0, 4.5); // TS-300B maxes out at 4.5 V
                this.code.addSample(node.id, phV, tdsV, ntuV, tempReading);
            }
            this.code.closeMinute(node.id, uptime, nowMin);
        }

        this.minute++;
        for (const node of this.nodes) {
            if (node.fault === "offline") continue;
            const report = this.code.station(node.id, nowMin);
            node.report = report;
            node.history.push({ minute: this.minute, now: report.now, base: report.base, state: report.state });
            if (node.history.length > HISTORY_MIN) node.history.shift();
            for (const ev of report.events) this.handleEvent(node, ev);
            if (this.hasPollutionLabel(report)) node.lastActiveMin = this.minute;
        }
    }

    // true if a pollution or watch label is on at the node
    hasPollutionLabel(report) {
        const ids = [report.label, ...report.also];
        return ids.some((id) => this.labels[id].category === 1 || this.labels[id].category === 2);
    }

    handleEvent(node, ev) {
        const label = this.labels[ev.label];
        if (label.category !== 1 && label.category !== 2) return;   // pollution and watch labels only
        if (ev.on) {
            this.addLog("alert-on", `${node.id} ALERT ON: ${label.short} (conf ${ev.conf.toFixed(2)})`, { node: node.id, label: ev.label });
            this.trackPlume(node, ev.label);
        } else {
            this.addLog("alert-off", `${node.id} alert off: ${label.short}`, { node: node.id, label: ev.label });
        }
    }

    // Works out how fast pollution is moving from when the nodes picked it up.
    // Only a plume arriving counts: the node had no pollution alerts for the
    // hour before. It's paired with the closest node upstream that saw the same
    // kind of pollution arrive in the 6 hours before, as long as no rain storm happened in between
    // (rain pauses the pollution rules, so alerts restart afterwards).
    trackPlume(node, labelId) {
        const here = { branch: node.branch, x: node.x };
        const fresh = this.minute - node.lastActiveMin > 60;
        if (fresh) {
            let best = null;
            for (const d of this.detections) {
                if (!d.fresh || d.label !== labelId || d.node === node.id || this.minute - d.minute > 6 * 60 || d.minute >= this.minute) continue;
                if (this.rainTimes.some((t) => t <= this.minute && t + 140 >= d.minute)) continue;
                const from = this.nodes.find((n) => n.id === d.node);
                if (!from) continue;
                const dist = this.river.riverDistance({ branch: from.branch, x: from.x }, here);
                if (dist <= 0) continue;
                if (!best || dist < best.dist || (dist === best.dist && d.minute > best.d.minute)) best = { d, dist, from };
            }
            if (best) {
                const minutes = this.minute - best.d.minute;
                this.speedEstimates.unshift({
                    from: best.from.id, to: node.id, label: labelId, metres: best.dist, minutes,
                    speed: best.dist / (minutes * 60), time: this.clock(),
                });
                this.speedEstimates.length = Math.min(this.speedEstimates.length, 12);
            }
        }
        this.detections.push({ label: labelId, node: node.id, minute: this.minute, fresh });
        if (this.detections.length > 200) this.detections.shift();
        // Which factory's waste is it? (the simulator knows, the nodes don't)
        const w = node.truth;
        if (!w) return;
        let best = -1, bestF = 0.002;
        w.mix.forEach((f, k) => { if (f > bestF) { bestF = f; best = k; } });
        if (best >= 0) {
            const fac = this.factories[best];
            if (fac.startedMin !== null && fac.firstSeen[node.id] === undefined) {
                const dist = this.river.riverDistance({ branch: fac.branch, x: fac.x }, here);
                fac.firstSeen[node.id] = { minutes: this.minute - fac.startedMin, metres: dist, label: labelId };
            }
        }
    }

    noise(amount) {
        return (this.random() * 2 - 1) * amount;
    }
}

function clamp(v, low, high) {
    return Math.max(low, Math.min(high, v));
}

// Small repeatable random number generator (same run every time)
function mulberry32(seed) {
    return function () {
        seed |= 0; seed = seed + 0x6D2B79F5 | 0;
        let t = Math.imul(seed ^ seed >>> 15, 1 | seed);
        t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
        return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
}

if (typeof module !== "undefined") {
    module.exports = { NodeCode, River, Simulation, WASTE_TYPES, FAULTS, defaultLayout, phToVolts,
                       tdsToVolts, ntuToVolts, alkalinityAt, phFromAlkalinity };
}
