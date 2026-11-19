#ifndef __TUROK2_H__
#define __TUROK2_H__

#include <cstdint>
#include <string_view>

namespace recomp::config {
    class Config;
}

namespace t2 {
    inline constexpr std::u8string_view program_id = u8"Turok2Recompiled";
    inline constexpr std::string_view program_name = "Turok 2: Recompiled";

    // The game's code sections (register_overlays.cpp) and the patches over it (register_patches.cpp).
    void register_overlays();
    void register_patches();

    // The settings (config.cpp): RecompFrontend's tabs.
    void init_config();

    // The General tab's Skip Intro (skips.cpp, which also has Start skip the cinematics), before the config is finalized.
    void add_skip_intro_option(recomp::config::Config& general);

    // The Graphics tab's Fog (fog.cpp, with us.toml's hooks in the camera and the frame), before the config is finalized.
    void add_fog_option(recomp::config::Config& graphics);

    // Maps the code at the TLB addresses it runs at, through the engine (register_overlays.cpp), before the game runs.
    void map_code_sections(uint8_t* rdram);
}

#endif
