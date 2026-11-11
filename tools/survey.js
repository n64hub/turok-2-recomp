// Turok 2's survey for the page's plans (N64Recomp-browser's tools/play_page.mjs and record_shaders.mjs load it with an
// evalFile step): where the game is, so a plan presses through the boot and the menus and then waits out a new game's
// opening cinematics until the player has control. There is no decomp: the addresses are the game's own code's, the
// values were read as the page played.
//
// The word at 0x8011AC88 is the game's mode, which its camera and HUD test: 0-3 through the boot, 4-5 the title's
// attract demo, 7 the title, 8 and 13 the menus, 11 a new game's first cinematic (Adon and the Eye of Truth), 12 the
// Port of Adia's opening cinematic, then 13 again with the player in control. Start skips neither cinematic; the two
// take about 2 minutes 10 s together. The game keeps its players in a list at 0x80119890, a pointer to the first, 0
// until one is loaded (the attract demo has one too). A cinematic plays from the object at *0x800C1BB0: func_00284188,
// which the camera and the HUD ask, has one playing while its +0x38 is set and its time (+0x1C, a float) is between
// its start (+0x30) and its end (+0x34).
(() => {
  const f = (a) => { const b = new DataView(new ArrayBuffer(4)); b.setInt32(0, n64r(a)); return b.getFloat32(0); };
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
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
    // Waits up to ms, from a new game's opening, for the player to have control: true once it does.
    control: async (ms = 240000) => {
      for (const t0 = Date.now(); Date.now() - t0 < ms; await sleep(250)) {
        if (t2.mode() === 13 && t2.player() !== 0 && !t2.cinematic()) return true;
      }
      console.log(`[survey] no control after ${ms} ms: ${t2.where()}`);
      return false;
    },
    // For a plan's log.
    where: () => `mode ${t2.mode()} player ${t2.player().toString(16)} cinematic ${t2.cinematic()}`,
  };
  return 'survey: ' + t2.where();
})()
