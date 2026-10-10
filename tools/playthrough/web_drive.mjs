#!/usr/bin/env bun
import { chromium } from "playwright";
import fs from "fs";
import path from "path";

function parseArgs() {
  const args = process.argv.slice(2);
  const opts = {
    url: "http://localhost:3000/?edition=opensky",
    tower: "",
    days: 1,
    out: "./out/web",
    parkingShim: true,
  };
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--url" && args[i + 1]) opts.url = args[++i];
    else if (args[i] === "--tower" && args[i + 1]) opts.tower = args[++i];
    else if (args[i] === "--days" && args[i + 1]) opts.days = parseInt(args[++i], 10);
    else if (args[i] === "--no-parking-shim") opts.parkingShim = false;
    else if (args[i] === "--out" && args[i + 1]) opts.out = args[++i];
  }
  return opts;
}

async function main() {
  const opts = parseArgs();
  console.log(`[web_drive] Target URL: ${opts.url}`);
  console.log(`[web_drive] Tower: ${opts.tower || "(default/new)"}`);
  console.log(`[web_drive] Days: ${opts.days}`);
  console.log(`[web_drive] Out dir: ${opts.out}`);

  fs.mkdirSync(opts.out, { recursive: true });

  const browser = await chromium.launch({
    executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE_PATH || "/usr/bin/chromium",
    args: ["--no-sandbox", "--disable-gpu", "--disable-dev-shm-usage"],
  });

  const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });

  try {
    // PERF SHIM (see README "Known findings"): the web port's Parking.advance() and every
    // car arrival call recomputeAllChains(), an O(parking^2 x items) scan costing ~95 s per
    // call on this tower (264 parking items, ~6000 items). The harness never edits the tower,
    // so the harness serves parking.js with that function memoised per (item count, game hour).
    // The result is identical for an unedited tower. Pass --no-parking-shim to measure the raw port.
    if (opts.parkingShim) {
      await page.route(/\/src\/game\/items\/parking\.js/, async (route) => {
        const resp = await route.fetch();
        let body = await resp.text();
        body = body.replace("export function recomputeAllChains(game) {", "function recomputeAllChainsRaw(game) {");
        body += `
export function recomputeAllChains(game) {
  const key = game.items.size + ":" + Math.floor(game.time.absolute * 24);
  if (game.__chainKey === key) return;
  game.__chainKey = key;
  recomputeAllChainsRaw(game);
}
`;
        await route.fulfill({ response: resp, body });
      });
    }

    console.log("[web_drive] Navigating to page...");
    await page.goto(opts.url, { waitUntil: "domcontentloaded", timeout: 30000 });

    // Wait for __game to exist
    await page.waitForFunction(() => typeof window.__game !== "undefined", { timeout: 15000 });
    console.log("[web_drive] window.__game initialized.");

    // Select OpenSky edition button if present
    try {
      const openskyBtn = page.locator('button[data-mode="opensky"]');
      if (await openskyBtn.isVisible({ timeout: 2000 })) {
        await openskyBtn.click();
        await page.waitForTimeout(500);
      }
    } catch (_) {}

    // Load tower file or start new game. .TDT saves are fed to the web port's own
    // TDT importer (src/core/tdt-parser.js, served by server.js), so the run uses the
    // fixed importer instead of a stale pre-converted .tower.
    let towerContent = null;
    let tdtB64 = null;
    if (opts.tower && fs.existsSync(opts.tower)) {
      console.log(`[web_drive] Loading tower file from ${opts.tower}`);
      if (/\.tdt$/i.test(opts.tower)) tdtB64 = fs.readFileSync(opts.tower).toString("base64");
      else towerContent = fs.readFileSync(opts.tower, "utf8");
    }

    await page.evaluate(async ({ xml, b64 }) => {
      const game = window.__game;
      if (b64) {
        const bin = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));
        const mod = await import("/src/core/tdt-parser.js");
        mod.parseTdt(bin.buffer, game);
      } else if (xml && xml.trim().startsWith("<")) {
        game.decodeXML(xml);
      } else {
        game.seedNewTower();
      }
      game.setSpeedMode(3);
      // Close menu if present
      const mm = document.getElementById("mainmenu");
      if (mm) mm.style.display = "none";
    }, { xml: towerContent, b64: tdtB64 });

    // Initial render and screenshot
    await page.waitForTimeout(500);
    const initialShot = path.join(opts.out, "shot_initial.png");
    await page.screenshot({ path: initialShot });
    console.log(`[web_drive] Initial screenshot saved to ${initialShot}`);

    // Simulation loop: advance hour by hour, capture screenshots and metrics
    const dailyMetrics = [];
    const hourly = [];
    const targetDays = opts.days;
    console.log(`[web_drive] Running simulation for ${targetDays} day(s)...`);

    for (let day = 1; day <= targetDays; day++) {
      console.log(`[web_drive] --- Simulating Day ${day}/${targetDays} ---`);

      for (let hour = 0; hour < 24; hour++) {
        // Advance exactly one in-game hour (1/24 of a game day on game.time.absolute).
        // Hour labels count hours elapsed since load (same convention as the oracle and
        // C++ shot names); the real in-game clock is logged in hourly.json.
        const hourIdx = (day - 1) * 24 + hour;
        const snap = await page.evaluate(({ hourIdx }) => {
          const game = window.__game;
          if (window.__abs0 === undefined) window.__abs0 = game.time.absolute;
          const target = window.__abs0 + hourIdx / 24;
          let guard = 0;
          while (game.time.absolute < target && guard++ < 200000) game.advance(0.05);
          if (window.__renderer) window.__renderer.frame(game);
          const t = game.time;
          // HUD element boxes + the text the DOM shows, so the OCR crops follow the reflowing
          // layout and have an independent per-shot truth (what the HUD displays, not game state).
          const box = (el) => { if (!el) return null; const r = el.getBoundingClientRect(); return [Math.floor(r.left), Math.floor(r.top), Math.ceil(r.right), Math.ceil(r.bottom)]; };
          const vals = document.querySelectorAll("#timewindow .tw-value");
          const q = (sel) => document.querySelector("#timewindow " + sel);
          const hud = {
            fund: { box: box(vals[0]), text: vals[0] && vals[0].textContent },
            pop: { box: box(vals[1]), text: vals[1] && vals[1].textContent },
            date: { box: box(q(".tw-date")), text: q(".tw-date") && q(".tw-date").textContent },
            clock: { box: box(q(".tw-clock")), text: q(".tw-clock") && q(".tw-clock").textContent },
            stars: { box: box(q(".tw-stars")), text: q(".tw-stars") && q(".tw-stars").textContent },
          };
          return { hud, hourIdx, absolute: t.absolute, hour: t.hour, day: t.day, quarter: t.quarter, year: t.year,
                   funds: game.funds, population: game.population, rating: game.rating + 1 };
        }, { hourIdx });
        hourly.push(snap);
        await page.waitForTimeout(150);

        // Screenshot each hour
        const compatShotPath = path.join(opts.out, `shot_d${day}_h${String(hour).padStart(2, "0")}.png`);
        await page.screenshot({ path: compatShotPath });
      }

      // Collect end-of-day metrics
      const metrics = await page.evaluate((dayNum) => {
        const game = window.__game;
        const items = {};
        for (const it of game.items) {
          const type = it.type || (it.prototype && it.prototype.name) || "item";
          items[type] = (items[type] || 0) + 1;
        }

        let stressed = 0;
        if (game.people) {
          for (const p of game.people) {
            if (p.stress > 0) stressed++;
          }
        }

        let totalWait = 0;
        let waiterCount = 0;
        for (const it of game.items) {
          if (it.isElevator && it.isElevator() && it.queues) {
            for (const q of it.queues) {
              if (q.people && q.people.length > 0) {
                for (const p of q.people) {
                  waiterCount++;
                  const jf = q.joinFrames?.get(p);
                  if (jf !== undefined) totalWait += Math.max(0, q.frameNow() - jf);
                }
              }
            }
          }
        }
        const avgWait = waiterCount > 0 ? (totalWait / waiterCount) : 0;

        return {
          day: dayNum,
          funds: game.funds ?? 0,
          population: game.population ?? 0,
          rating: (game.rating ?? 0) + 1,
          stressed_people: stressed,
          elevator_avg_wait: parseFloat(avgWait.toFixed(2)),
          items: items,
        };
      }, day);

      console.log(`[web_drive] Day ${day} metrics: funds=${metrics.funds} pop=${metrics.population} rating=${metrics.rating} stressed=${metrics.stressed_people} avg_wait=${metrics.elevator_avg_wait}`);
      dailyMetrics.push(metrics);
    }

    fs.writeFileSync(path.join(opts.out, "hourly.json"), JSON.stringify(hourly, null, 1), "utf8");

    // Save metrics.json and metrics.csv
    const jsonPath = path.join(opts.out, "metrics.json");
    fs.writeFileSync(jsonPath, JSON.stringify(dailyMetrics, null, 2), "utf8");
    console.log(`[web_drive] Metrics saved to ${jsonPath}`);

    const csvPath = path.join(opts.out, "metrics.csv");
    const csvRows = ["day,funds,population,rating,stressed_people,elevator_avg_wait"];
    for (const m of dailyMetrics) {
      csvRows.push(`${m.day},${m.funds},${m.population},${m.rating},${m.stressed_people},${m.elevator_avg_wait}`);
    }
    fs.writeFileSync(csvPath, csvRows.join("\n") + "\n", "utf8");
    console.log(`[web_drive] CSV saved to ${csvPath}`);

    console.log("[web_drive] Playthrough completed successfully.");
  } finally {
    await browser.close();
  }
}

main().catch((err) => {
  console.error("[web_drive] Error:", err);
  process.exit(1);
});
