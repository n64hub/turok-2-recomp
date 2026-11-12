**What's new:**

- In widescreen, the underwater tint covers the whole wide picture instead of stopping short of its sides.
- The renderer finds the picture where the game's own high-resolution video modes point, so the game's video mode is left as it set it, no longer moved a row to make it findable.
- The renderer no longer copies each frame back into the console's memory, which the game never reads, so each frame takes less work; Copy Frames to RAM on the Graphics tab, Off by default, turns the copy back on if something ever looks wrong without it.
