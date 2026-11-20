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
// Far sets the camera's area mask to every area's, so every piece of the loaded level is drawn that is in view. What
// the game hasn't loaded is still not drawn: a level is loaded in parts (the town of the Port of Adia is not loaded on
// its docks), and with the fog off the end of the part loaded shows as sky. Normal is the game's own masks.
//
// At either, with the Fog off, the region's far distance is multiplied by kFarScale, so the world goes on that much
// further before it ends. With the fog on, the fog ends where the world does, and it is left as the game made it.
//
// At either, the frame's lists of what it draws are made longer (below): the console's run out in places, more so at
// Far, and then leave out the pieces nearest the camera.

#include <string>
#include <vector>

#include "recomp.h"
#include "librecomp/addresses.hpp"
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
        "<b>Far:</b> all of the level that is loaded, also what the console leaves to its fog. Drawing more may cost "
        "speed."
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
    return t2_fog_on() ? 1.0f : kFarScale;
}

// The frame's list of models in use. Each time the level's draw starts a batch of another model (func_0020B6A0, and
// func_0020B7F8 and func_0020B9F4 for the actors'), it takes the model (func_00205110) and adds it to a list of 256
// (the count at 0x800DD170, the models from 0x800DD178), and the end of the draw (func_0020B624) gives each one back
// (func_0020565C, func_002051F4). With the list full, every batch after it that frame is left out: what the game
// draws last, the pieces nearest the camera. The console's masks keep a frame under 256; Far's fill it in places (a
// room past the Port of Adia's town), so the list goes on here: us.toml's hooks before each writer's check move the
// list's last model here when it is full, so the game adds the new one in its place, and give these back after the
// game's own at the end of the draw.
namespace {
    constexpr uint32_t kModelCount = 0x800DD170;
    constexpr uint32_t kModels = 0x800DD178;
    constexpr int32_t kModelsMax = 0x100;
    std::vector<uint32_t> models_past_list;
}

extern "C" void t2_model_list_full(uint8_t* rdram) {
    const gpr count = (gpr)(int32_t)kModelCount;
    if (MEM_W(0, count) == kModelsMax) {
        models_past_list.push_back((uint32_t)MEM_W(0, (gpr)(int32_t)(kModels + 4 * (kModelsMax - 1))));
        MEM_W(0, count) = kModelsMax - 1;
    }
}

// The next model past the list to give back, 0 when there are none left.
extern "C" uint32_t t2_model_past_list(void) {
    if (models_past_list.empty()) {
        return 0;
    }
    uint32_t model = models_past_list.back();
    models_past_list.pop_back();
    return model;
}

// The frame's sort of what it draws. Each batch of a model's piece the level's draw hands func_0020A568 is sorted by its
// material into a tree of nodes (0x20 bytes each, 100 of them from 0x800DC4F0, the count at 0x800D8E2C), and each
// piece's place into a record hung on its node (0x1C bytes each, 500 of them from 0x800D8E40, the count at 0x800D8E20);
// func_0020A994 then draws the tree, material by material. With the nodes all taken, the rest of the batch is left
// out, and with the records all taken, every piece after: the pieces the game sorts last, the ones nearest the camera.
// In the Port of Adia the console's masks already ask for more than 100 nodes in places, and Far's for some 140 nodes
// and over 1000 records: the ground under the player and the walls beside it went missing. Nodes and records are reached only through
// func_0020A568's own base addresses and the links between them, so us.toml's hooks give it arrays eight times the
// size from the runtime's heap, past the console's memory, with their sizes for its checks.
namespace {
    constexpr uint32_t kNodeSize = 0x20;
    constexpr uint32_t kRecordSize = 0x1C;
    constexpr int32_t kSortScale = 8;
    uint32_t sort_nodes = 0;
    uint32_t sort_records = 0;
    bool sort_tried = false;

    // The arrays' addresses, made the first time they are asked for; 0 if the heap had no room, and the game's own
    // are used.
    void make_sort_arrays(uint8_t* rdram) {
        if (sort_tried) {
            return;
        }
        sort_tried = true;
        void* nodes = recomp::alloc(rdram, kNodeSize * 100 * kSortScale);
        void* records = recomp::alloc(rdram, kRecordSize * 500 * kSortScale);
        if (nodes == nullptr || records == nullptr) {
            return;
        }
        sort_nodes = 0x80000000u + (uint32_t)((uint8_t*)nodes - rdram);
        sort_records = 0x80000000u + (uint32_t)((uint8_t*)records - rdram);
    }
}

extern "C" uint32_t t2_sort_nodes(uint8_t* rdram) {
    make_sort_arrays(rdram);
    return sort_nodes;
}

extern "C" uint32_t t2_sort_records(uint8_t* rdram) {
    make_sort_arrays(rdram);
    return sort_records;
}

extern "C" int32_t t2_sort_scale(void) {
    return kSortScale;
}
