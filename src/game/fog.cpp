// The Graphics tab's Fog option, from us.toml's two hooks: Off takes the fog away and draws the world further.
//
// The game draws only as far as its fog reaches. Each region of a level has a record (func_0027CAB0 looks it up) with
// its fog colour, the depth its fog starts at in thousandths of the depth range, and its far distance in world units,
// two of each (+0x8, +0x10 and +0xC, +0x18), which the camera's struct eases between as the player moves from one region
// to the next: the far distance at +0x510, the fog's start at +0x524. The far distance is the far plane of the camera's
// perspective (func_0027D160's guPerspectiveF, whose near plane is 16) and the reach of the view pyramid the level is
// culled to, and the frame (func_0027E000) puts the fog's end at it (gSPFogPosition's max, 1000): the world ends
// where the fog is thickest, in the fog's colour, which the frame also fills the screen with before it draws.
//
// Off does two things:
// - The frame's fog command (G_MW_FOG) gets a fog factor of 0 for every depth: with the fog's start and end left as
//   they were, the fog's thickness at a distance d is about 1 - start/d, whatever the far plane, so the fog has to go
//   itself; pushing the far plane out leaves it where it was.
// - The region's far distance, as the camera reads it from the record, is multiplied by kFarScale, so the world goes
//   on that much further before it ends. It is the record's value that is scaled and not the camera's: the camera
//   copies its own far distance back as the start of each move into a new region, and scaling that again would
//   compound.
// On is the game as it was made.

#include <string>
#include <vector>

#include "recompui/config.h"

#include "turok2.h"

namespace {
    const std::string fog_option = "fog";
    enum class Fog { Off, On };
    const std::vector<recomp::config::ConfigOptionEnumOption> fog_options = {
        {Fog::On, "On", "On"},
        {Fog::Off, "Off", "Off"},
    };
    bool fog_on = true;

    constexpr float kFarScale = 4.0f;
}

void t2::add_fog_option(recomp::config::Config& graphics) {
    graphics.add_enum_option(
        fog_option,
        "Fog",
        "Whether the world fades into fog in the distance, as the game was made."
        "<br/>"
        "<b>Off:</b> no fog, and the world is drawn four times as far before it ends. Drawing more of the world may cost "
        "speed.",
        fog_options,
        Fog::On
    );
    graphics.add_option_change_callback(fog_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant,
           recomp::config::OptionChangeContext) {
            if (const uint32_t* value = std::get_if<uint32_t>(&cur_value)) {
                fog_on = static_cast<Fog>(*value) == Fog::On;
            }
        });
}

extern "C" int t2_fog_on(void) {
    return fog_on ? 1 : 0;
}

extern "C" float t2_far_scale(void) {
    return fog_on ? 1.0f : kFarScale;
}
