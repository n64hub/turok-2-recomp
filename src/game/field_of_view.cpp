// The Graphics tab's Field of View option, from us.toml's hook: how wide the view is, 100% the game's own.
//
// The camera (func_0027D160) has a view angle in degrees, which each region of a level sets in its record and the
// camera eases towards as the player moves between regions (+0x518 of its struct, eased towards the region's at
// 0x0027D8E0), or which a cinematic sets (func_00284390). From it the camera makes both its perspective (guPerspective's
// fovy and aspect, func_0029815C) and the view pyramid the level is culled to, so scaling the angle there widens what
// is drawn and what is culled alike. Only the angle out of the regions is scaled: a cinematic keeps the framing it was
// made with.
//
// The angle goes through a tangent on its way to the perspective, so it is kept under kMaxAngle, where the view would
// fold over.

#include <algorithm>
#include <atomic>
#include <string>

#include "recompui/config.h"

#include "turok2.h"

namespace {
    const std::string field_of_view_option = "field_of_view";
    // The setting, as a factor: read from the config at each change, so the camera reads a float a frame.
    std::atomic<float> field_of_view{ 1.0f };

    constexpr float kMaxAngle = 85.0f;
}

void t2::add_field_of_view_option(recomp::config::Config& graphics) {
    graphics.add_number_option(
        field_of_view_option,
        "Field of View",
        "How much of the world the view takes in: 100% is the game's own, more shows more at the sides and above and "
        "below, less zooms in. The game sets its own view per area of a level, and this scales whatever that is. "
        "Cinematics keep the game's own view.",
        70, 150, 5, 0, true, 100);
    // Called on load too, with the saved value, and at each change.
    graphics.add_option_change_callback(field_of_view_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant,
           recomp::config::OptionChangeContext) {
            if (const double* percent = std::get_if<double>(&cur_value)) {
                field_of_view.store((float)(*percent / 100.0));
            }
        });
}

// The camera's view angle in degrees, as the game eased it, scaled by the setting.
extern "C" float t2_field_of_view(float degrees) {
    float scale = field_of_view.load(std::memory_order_relaxed);
    if (scale == 1.0f) {
        return degrees;
    }
    return std::min(degrees * scale, std::max(degrees, kMaxAngle));
}
