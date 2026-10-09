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
  };
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--url" && args[i + 1]) opts.url = args[++i];
    else if (args[i] === "--tower" && args[i + 1]) opts.tower = args[++i];
    else if (args[i] === "--days" && args[i + 1]) opts.days = parseInt(args[++i], 10);
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

    // Load tower file or start new game
    let towerContent = null;
    if (opts.tower && fs.existsSync(opts.tower)) {
      console.log(`[web_drive] Loading tower file from ${opts.tower}`);
      towerContent = fs.readFileSync(opts.tower, "utf8");
    }

    await page.evaluate((xml) => {
      const game = window.__game;
      if (xml && xml.trim().startsWith("<")) {
        game.decodeXML(xml);
      } else {
        game.seedNewTower();
      }
      game.setSpeedMode(3);
      // Close menu if present
      const mm = document.getElementById("mainmenu");
      if (mm) mm.style.display = "none";
    }, towerContent);

    // Initial render and screenshot
    await page.waitForTimeout(500);
    const initialShot = path.join(opts.out, "shot_initial.png");
    await page.screenshot({ path: initialShot });
    console.log(`[web_drive] Initial screenshot saved to ${initialShot}`);

    // Simulation loop: advance hour by hour, capture screenshots and metrics
    const dailyMetrics = [];
    const targetDays = opts.days;
    console.log(`[web_drive] Running simulation for ${targetDays} day(s)...`);

    for (let day = 1; day <= targetDays; day++) {
      console.log(`[web_drive] --- Simulating Day ${day}/${targetDays} ---`);

      for (let hour = 0; hour < 24; hour++) {
        // Advance simulation ~1 in-game hour
        // Speed 3 is ~40 ticks/hour, advance in discrete steps
        await page.evaluate(() => {
          const game = window.__game;
          // Step game clock 1 hour (dt in seconds)
          // 1 game hour is ~0.5s at speed 3, run ~35 advance ticks
          for (let i = 0; i < 35; i++) {
            game.advance(0.016);
          }
          if (window.__renderer) {
            window.__renderer.frame(game);
          }
        });

        // Screenshot each hour
        const shotPath = path.join(opts.out, `shot_day${day}_hr${String(hour).padStart(2, "0")}.png`);
        await page.screenshot({ path: shotPath });
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
