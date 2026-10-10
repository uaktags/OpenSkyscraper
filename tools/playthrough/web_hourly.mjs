import fs from "node:fs";
import { createGame } from "/home/tim/Projects/OpenSkyWeb-art-v2/src/game/create.js";
import { parseTdt } from "/home/tim/Projects/OpenSkyWeb-art-v2/src/core/tdt-parser.js";

const tdtPath = process.argv[2];
const outPath = process.argv[3];
const days = Number(process.argv[4] ?? 3);

if (!tdtPath || !outPath) {
  console.error("Usage: bun web_hourly.mjs <tdtPath> <outPath> [days=3]");
  process.exit(1);
}

const g = createGame({ window: { width: 1280, height: 768 } });
parseTdt(fs.readFileSync(tdtPath), g);
g.drainRoutes(Infinity);

const startAbs = g.time.absolute;
const totalHours = days * 24;
const rows = [];

// Record initial state as hour 0
rows.push({
  hour_idx: 0,
  absolute: g.time.absolute,
  hour: g.time.hour,
  day: g.time.day,
  quarter: g.time.quarter,
  year: g.time.year,
  funds: g.funds,
  population: g.population,
  countT: [...g.countT]
});

const dt = 0.05;
for (let h = 1; h <= totalHours; h++) {
  const targetAbs = startAbs + h / 24.0;
  while (g.time.absolute < targetAbs) {
    g.advance(dt);
  }
  rows.push({
    hour_idx: h,
    absolute: g.time.absolute,
    hour: g.time.hour,
    day: g.time.day,
    quarter: g.time.quarter,
    year: g.time.year,
    funds: g.funds,
    population: g.population,
    countT: [...g.countT]
  });
}

fs.writeFileSync(outPath, JSON.stringify(rows, null, 2));
console.log(`Wrote ${rows.length} hourly records to ${outPath}`);
