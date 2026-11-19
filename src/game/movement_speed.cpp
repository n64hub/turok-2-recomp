// The General tab's Movement Speed option, from us.toml's hook: how fast the player moves, 100% the game's own.
//
// Each player's struct holds a speed factor at +0xB28 that the movement reads (func_0024D344 at 0x0024D868 and
// func_0024FE10 at 0x0024FEE8). func_00254640 sets it afresh from the player's character data (+0x14, then +0x1C)
// and, where the game's own debug menu has its "player speed" on (0.5 to 2, the byte at 0x8011AAF1 and the float at
// 0x8011AAF4), multiplies it by that. The hook multiplies the result by this setting at the function's end, which
// every path through it reaches, so it never compounds and stacks with what the game does to it.

#include <atomic>
#include <string>

#include "recompui/config.h"

#include "turok2.h"

namespace {
    const std::string movement_speed_option = "movement_speed";
    // The setting, as a factor: read from the config at each change, so the movement reads a float.
    std::atomic<float> movement_speed{ 1.0f };
}

void t2::add_movement_speed_option(recomp::config::Config& general) {
    general.add_number_option(
        movement_speed_option,
        "Movement Speed",
        "How fast you walk, strafe and back up: 100% is the game's own speed, and 200% twice that, as fast as the "
        "game's own hidden debug setting goes.",
        100, 200, 5, 0, true, 100);
    // Called on load too, with the saved value, and at each change.
    general.add_option_change_callback(movement_speed_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant,
           recomp::config::OptionChangeContext) {
            if (const double* percent = std::get_if<double>(&cur_value)) {
                movement_speed.store((float)(*percent / 100.0));
            }
        });
}

extern "C" float t2_movement_speed(void) {
    return movement_speed.load(std::memory_order_relaxed);
}
