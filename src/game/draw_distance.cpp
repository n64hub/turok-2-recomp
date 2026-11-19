// The Graphics tab's Draw Distance option, from us.toml's hooks in the camera (func_0027CAB0 and func_0027D160):
// Far draws all of the level the game has loaded, Normal what the console draws.
//
// The game draws a level in pieces, and keeps two limits on which it draws:
// - Areas. Each area of a level has a record (the one func_00220A1C finds under the camera, kept at +0x58 of the
//   camera's struct) whose +0x1C is a mask of the other areas' pieces seen from it. The camera takes that mask as its
//   own (+0x64) at the end of each frame's update (func_0027D160), and the level's draw (func_002222E0) and the actors'
//   (func_0021D39C) leave out every piece whose mask (+0x84 of a piece) has none of its bits. The masks were made for
//   the console's fog: what they leave out is past it. Without the fog, whole pieces of the level go missing as the
//   camera moves from one area into the next, the floor under a dock or the walls across the water.
// - Distance. The camera's far distance (+0x510, from the region's record as fog.cpp has it) ends the world, and the
//   fog is thickest there.
//
// Far does two things:
// - The camera's area mask is set to every area's, so every piece of the loaded level is drawn that is in view.
// - With the Fog off, the region's far distance is multiplied by kFarScale, so the world goes on that much further
//   before it ends. With the fog on, the fog ends where the world does, and it is left as the game made it.
// What the game hasn't loaded is still not drawn: a level is loaded in parts (the town of the Port of Adia is not loaded
// on its docks), and with the fog off the end of the part loaded shows as sky.
// Normal is the game as it was made.

#include <string>
#include <vector>

#include "recompui/config.h"

#include "turok2.h"

namespace {
    const std::string draw_distance_option = "draw_distance";
    enum class DrawDistance { Normal, Far };
    const std::vector<recomp::config::ConfigOptionEnumOption> draw_distance_options = {
        {DrawDistance::Normal, "Normal", "Normal"},
        {DrawDistance::Far, "Far", "Far"},
    };
    bool draw_far = true;

    constexpr float kFarScale = 4.0f;
}

void t2::add_draw_distance_option(recomp::config::Config& graphics) {
    graphics.add_enum_option(
        draw_distance_option,
        "Draw Distance",
        "How much of the level is drawn."
        "<br/>"
        "<b>Far:</b> all of the level that is loaded, also what the console leaves to its fog, and with Fog off four "
        "times as far before the world ends. Drawing more may cost speed."
        "<br/>"
        "<b>Normal:</b> what the console draws, with whole pieces of the level left out where its fog would hide them.",
        draw_distance_options,
        DrawDistance::Far
    );
    graphics.add_option_change_callback(draw_distance_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant,
           recomp::config::OptionChangeContext) {
            if (const uint32_t* value = std::get_if<uint32_t>(&cur_value)) {
                draw_far = static_cast<DrawDistance>(*value) == DrawDistance::Far;
            }
        });
}

extern "C" int t2_draw_far(void) {
    return draw_far ? 1 : 0;
}

extern "C" int t2_fog_on(void);

extern "C" float t2_far_scale(void) {
    return draw_far && !t2_fog_on() ? kFarScale : 1.0f;
}
