#!/usr/bin/env bun
// WP15: run a C++ parity fixture (.tower) in the web port's YootTower mode, headless.
//   bun web_yoot_sim.mjs [--web DIR] [--yoot DIR] [--days N] [--seed S] [--out FILE.json] FIXTURE.tower...
// Same code path as the UI (src/main.js adoptYootFiles + onLoad):
//   buildYootCatalog(all original .t2p) -> game.yootCatalog = catalog -> game.decodeXML(xml)
//   decodeXML calls game.yootActivator = activateYootWorld (setRuleset("yoottower", world),
//   applyYootPlugins, peopleSystem.setPoolDef(world plugin)) before decoding the items.
// Time step: advance(0.2) at speed 3 == 1/960 game day == the C++ SimHarness tick (dt 1/60, speed 3).
// Math.random is replaced by a seeded PRNG (mulberry32) so a run is repeatable.
import { readFileSync, readdirSync, writeFileSync } from "fs";
import { join, basename, resolve } from "path";
import { pathToFileURL } from "url";

const argv = process.argv.slice(2);
const opt = { web: "/home/tim/Projects/OpenSkyWeb-art-v2", yoot: "/home/tim/Downloads/Yoot-Tower_Win_EN/YOOT", days: 3, seed: 1, dt: 0.2, out: null, injectCycle: false };
const fixtures = [];
for (let i = 0; i < argv.length; i++) {
  const a = argv[i];
  if (a === "--web") opt.web = argv[++i];
  else if (a === "--yoot") opt.yoot = argv[++i];
  else if (a === "--days") opt.days = +argv[++i];
  else if (a === "--seed") opt.seed = +argv[++i];
  else if (a === "--dt") opt.dt = +argv[++i];
  else if (a === "--out") opt.out = argv[++i];
  else if (a === "--inject-cycle") opt.injectCycle = true; // add accountingCycle="0" like a C++-written save
  else fixtures.push(a);
}
const imp = (p) => import(pathToFileURL(join(opt.web, p)).href);

function mulberry32(a) {
  return () => { a |= 0; a = (a + 0x6d2b79f5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
}

const { setLinearClock } = await imp("src/core/time.js");
const { createGame } = await imp("src/game/create.js");
const { buildYootCatalog } = await imp("src/game/yoot-setup.js");

function loadCatalog() {
  const dir = join(opt.yoot, "Plugins");
  const files = readdirSync(dir).filter((f) => /\.t2p$/i.test(f)).map((f) => ({ name: f, bytes: new Uint8Array(readFileSync(join(dir, f))) }));
  return buildYootCatalog(files);
}

const roomIds = new Set(["hotel_single", "hotel_double", "hotel_suite", "yoot_hotel_single", "yoot_hotel_double", "yoot_hotel_suite"]);

function snapshot(g, extra) {
  const items = {};
  for (const it of g.items) { const id = it.prototype.id; items[id] = (items[id] ?? 0) + 1; }
  const ps = g.peopleSystem;
  return {
    t: +g.time.absolute.toFixed(5), day: g.time.day,
    funds: g.funds, rating: g.rating, grade: g.rating + 1,
    population: g.population, people: g.people.length,
    visitorsCalled: ps.calledToday, visitorsEntered: ps.enteredToday,
    hotelCheckins: extra.checkins, hotelStaying: ps.stays.size,
    blackout: !!g.powerSystem.isBlackOut(), power: { supply: g.powerSystem.supply(), consumption: g.powerSystem.model.consumption() }, blackoutsStarted: extra.blackoutsStarted,
    money: { ...g.money },
    items,
  };
}

// C++ save ids the web factory does not know (see REPORT.md): web uses yoot_-prefixed ids for the
// trash facility and doctor and has no school prototype at all.
const ALIAS = { trashfacility: "yoot_trashfacility", doctor: "yoot_doctor" };
const DROP = new Set(["school"]);
function aliasXml(xml, rec) {
  rec.dropped = {}; rec.aliased = {};
  return xml.split("\n").filter((ln) => {
    const m = /<item type="([^"]+)"/.exec(ln);
    if (m && DROP.has(m[1])) { rec.dropped[m[1]] = (rec.dropped[m[1]] ?? 0) + 1; return false; }
    return true;
  }).map((ln) => ln.replace(/<item type="([^"]+)"/, (all, t) => { if (!ALIAS[t]) return all; rec.aliased[t] = (rec.aliased[t] ?? 0) + 1; return `<item type="${ALIAS[t]}"`; })).join("\n");
}

async function runOne(path, aliased = false) {
  const rec = { fixture: basename(path), seed: opt.seed, variant: aliased ? "aliased" : "raw", days: [], messages: [], error: null };
  const t0 = performance.now();
  try {
    Math.random = mulberry32(opt.seed);
    setLinearClock(false);
    const catalog = loadCatalog();
    rec.catalog = { loaded: catalog.loaded.length, skipped: catalog.skipped };
    const g = createGame({ window: { width: 1280, height: 768 } }); // the boot path
    const msgs = [];
    g.ui = { ...g.ui, showMessage: (m) => msgs.push([+g.time.absolute.toFixed(4), String(m)]), showLevelUp: () => {}, reloadToolbox: () => {} };
    g.yootCatalog = catalog.catalog; // main.js adoptYootFiles
    let xml = readFileSync(path, "utf8");
    if (aliased) xml = aliasXml(xml, rec);
    if (opt.injectCycle) xml = xml.replace(/<tower /, '<tower accountingCycle="0" ');
    g.decodeXML(xml); // main.js onLoad
    rec.ruleset = g.buildRules.ruleset; rec.world = g.buildRules.profile().worldId;
    rec.poolDef = !!g.peopleSystem.def?.valid;
    g.time.speed = 3; // speed_animated ramps toward 3 exactly like Game::setSpeedMode in the C++ harness
    // counters
    const extra = { checkins: 0, blackoutsStarted: 0 };
    const stays = g.peopleSystem.stays; const origSet = stays.set.bind(stays);
    stays.set = (k, v) => { if (!stays.has(k)) extra.checkins++; return origSet(k, v); };
    let wasBlack = !!g.powerSystem.isBlackOut();
    const start = g.time.absolute;
    rec.start = snapshot(g, extra);
    const ticksPerDay = Math.ceil(64 / opt.dt);
    const total = Math.ceil(opt.days * ticksPerDay * 1.5) + 1000;
    rec.midnights = []; let lastFloor = Math.floor(start);
    let nextDay = 1, ticks = 0, pk = g.population, mn = g.population;
    for (; ticks < total; ticks++) {
      if (g.peopleSystem.stays !== stays) { /* world reset replaced the map */ }
      g.advance(opt.dt);
      if (Math.floor(g.time.absolute) > lastFloor) { lastFloor = Math.floor(g.time.absolute); rec.midnights.push({ at: lastFloor, called: g.peopleSystem.calledToday, entered: g.peopleSystem.enteredToday, checkins: extra.checkins }); }
      const b = !!g.powerSystem.isBlackOut();
      if (b && !wasBlack) extra.blackoutsStarted++;
      wasBlack = b;
      // Sampling mirrors the C++ run (--sim-dump-every 40): peaks/day rows come from every 40th tick,
      // a day row is the first sample at or after start+N days (the last day: the first tick at/after it).
      const sample = (ticks + 1) % 40 === 0;
      if (sample) { pk = Math.max(pk, g.population); mn = Math.min(mn, g.population); }
      while (nextDay <= opt.days && g.time.absolute - start >= nextDay && (sample || nextDay === opt.days)) {
        rec.days.push({ dayEndsAt: nextDay, popMax: pk, popMin: mn, ...snapshot(g, extra) });
        pk = mn = g.population;
        nextDay++;
      }
      if (nextDay > opt.days) break;
    }
    rec.ticks = ticks + 1;
    rec.dt = opt.dt;
    rec.messages = msgs;
  } catch (e) {
    rec.error = { message: String(e?.message ?? e), stack: String(e?.stack ?? "") };
  }
  rec.seconds = +((performance.now() - t0) / 1000).toFixed(2);
  return rec;
}

const results = [];
for (const f of fixtures) {
  let r = await runOne(resolve(f));
  results.push(r);
  if (r.error) {
    console.log(`${r.fixture}: RAW LOAD FAILED ${r.error.message}\n${r.error.stack}\n  -> retrying with id aliases`);
    r = await runOne(resolve(f), true);
    results.push(r);
  }
  if (r.error) console.log(`${r.fixture}: FAILED ${r.error.message}\n${r.error.stack}`);
  else {
    console.log(`${r.fixture} [${r.variant}${r.variant === "aliased" ? " aliased=" + JSON.stringify(r.aliased) + " dropped=" + JSON.stringify(r.dropped) : ""}] (${r.ruleset}/${r.world}, pool=${r.poolDef}, seed ${r.seed}, ${r.seconds}s)`);
    console.log(" day  funds       pop  grade  called entered checkins stay blackout items");
    for (const d of [r.start, ...r.days]) {
      const it = Object.entries(d.items).map(([k, v]) => `${k}:${v}`).join(" ");
      console.log(` ${String(d.dayEndsAt ?? "0").padStart(3)} ${String(d.funds).padStart(10)} ${String(d.population).padStart(5)}(${d.popMax ?? "-"}) ${String(d.grade).padStart(5)} ${String(d.visitorsCalled).padStart(7)} ${String(d.visitorsEntered).padStart(7)} ${String(d.hotelCheckins).padStart(8)} ${String(d.hotelStaying).padStart(4)} ${d.blackout ? "BLACK" : "-"}${d.blackoutsStarted ? "(" + d.blackoutsStarted + ")" : ""}  ${it}`);
    }
  }
}
if (opt.out) writeFileSync(opt.out, JSON.stringify(results, null, 1));
