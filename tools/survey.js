// Turok 2's survey for the page's plans (N64Recomp-browser's tools/play_page.mjs and record_shaders.mjs load it with an
// evalFile step): whether the game is in a level, so a plan presses through the boot, the menus and a new game's
// opening scenes until it is. There is no decomp: the address is the game's own code's (us.toml's comment on the HUD).
//
// The game keeps its players in a list at 0x80119890, a pointer to the first, empty (0) until a level is loaded with
// its player in it. The HUD (func_0027C1E8) finds its player there.
(() => {
  const t2 = window.t2 = {
    player: () => n64r(0x80119890) >>> 0,
    inLevel: () => t2.player() !== 0,
    // For a plan's log.
    where: () => `player ${t2.player().toString(16)}`,
  };
  return 'survey: ' + t2.where();
})()
