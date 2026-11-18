// Turok 2's survey for the page's plans (N64Recomp-browser's tools/play_page.mjs and record_shaders.mjs load it with an
// evalFile step): where the game is, so a plan presses through the boot and the menus and then waits out a new game's
// opening cinematics until the player has control. There is no decomp: the addresses are the game's own code's, the
// values were read as the page played.
//
// The word at 0x8011AC88 is the game's mode, which its camera and HUD test: 0-3 through the boot, 4-5 the title's
// attract demo, 7 the title, 8 and 13 the menus, 11 a new game's first cinematic (Adon and the Eye of Truth), 12 the
// Port of Adia's opening cinematic, then 13 again with the player in control; 17 comes for a moment between some of
// them. Start skips neither cinematic; the two take about 2 minutes 10 s together. Mode 0 lasts about 4 s on a desktop
// and 15-25 s where the page is slow (a browser presenting through a virtual display). The game keeps its players in a
// list at 0x80119890, a pointer to the first, 0 until one is loaded (the attract demo has one too). A cinematic plays
// from the object at *0x800C1BB0: func_00284188, which the camera and the HUD ask, has one playing while its +0x38 is
// set and its time (+0x1C, a float) is between its start (+0x30) and its end (+0x34).
//
// A run that never gets control says why in its log: the modes the game went through and when, and every place in the
// page's memory that holds RDRAM's boot globals (what n64r finds RDRAM by) with the mode read there, so a run that read
// the wrong copy is told from a game that stayed where it was.
//
// A plan presses Start only once the boot is past its first stage, mode 0 (booted): a Start held as the game finishes
// that stage opens its Controller Pak menu, as on the console, and the menu stays in mode 0 and takes no Start to
// leave (EXIT is the A button's). The plans pressed Enter for 150 ms every 2.5 s from the boot on, and about one boot in
// thirty or fifty, wherever it played, one of them landed there: the run pressed on for four minutes at the menu.
(() => {
  const f = (a) => { const b = new DataView(new ArrayBuffer(4)); b.setInt32(0, n64r(a)); return b.getFloat32(0); };
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const t0 = performance.now();
  const modes = [];
  const t2 = window.t2 = {
    mode: () => n64r(0x8011AC88),
    player: () => n64r(0x80119890) >>> 0,
    cinematic: () => {
      const o = n64r(0x800C1BB0) >>> 0;
      if (o < 0x80000000 || o >= 0x80800000 || n64r(o + 0x38) === 0) return false;
      const t = f(o + 0x1C);
      return f(o + 0x30) <= t && t <= f(o + 0x34);
    },
    // A new game's opening has started (Start pressed through the menus).
    opening: () => t2.mode() === 11,
    // Waits up to ms for the boot to leave its first stage (mode 0): true once it has. Start is safe to press from then on.
    booted: async (ms = 120000) => {
      for (const t0 = Date.now(); Date.now() - t0 < ms; await sleep(250)) {
        if (t2.mode() !== 0) return true;
      }
      console.log(`[survey] still booting after ${ms} ms: ${t2.where()}`);
      return false;
    },
    // Waits up to ms, from a new game's opening, for the player to have control: true once it does.
    control: async (ms = 240000) => {
      for (const t0 = Date.now(); Date.now() - t0 < ms; await sleep(250)) {
        if (t2.mode() === 13 && t2.player() !== 0 && !t2.cinematic()) return true;
      }
      console.log(`[survey] no control after ${ms} ms: ${t2.where()}`);
      console.log(`[survey] modes since the survey: ${modes.map(([t, m]) => `${m} at ${t} s`).join(', ')}`);
      console.log(`[survey] RDRAM's boot globals in the page's memory: ${t2.rdram()}`);
      return false;
    },
    // For a plan's log.
    where: () => `mode ${t2.mode()} player ${t2.player().toString(16)} cinematic ${t2.cinematic()}`,
    // Every place in the heap n64r's search (N64Recomp-browser's page_driver.mjs, rdramHelpers) takes for RDRAM, with
    // the mode read there: one place, whose mode is the one t2.mode() reads, unless n64r found a wrong copy.
    rdram: () => {
      const h = HEAPU32, found = [];
      for (let i = 2; i < h.length - 8; i++) {
        if (h[i] === 0xB0000000 && h[i - 2] <= 2 && (h[i + 4] === 0x400000 || h[i + 4] === 0x800000)) {
          const base = i * 4 - 0x308;
          found.push(`0x${base.toString(16)} (mode ${h[(base + 0x11AC88) >>> 2]})`);
        }
      }
      return `${found.join(', ') || 'none'}; n64r reads mode ${t2.mode()}`;
    },
  };
  // The modes the game goes through, for control's log.
  const watch = setInterval(() => {
    let m;
    try { m = t2.mode(); } catch { return; }
    if (modes.length === 0 || modes[modes.length - 1][1] !== m) modes.push([((performance.now() - t0) / 1000).toFixed(1), m]);
    if (modes.length > 60) clearInterval(watch);
  }, 250);
  return 'survey: ' + t2.where();
})()
