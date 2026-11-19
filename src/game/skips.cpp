// Skipping the boot's screens and the cinematics, from us.toml's hook at the start of each pad's reading
// (func_00284EB8, the pad's struct in a0, its stick in a1 and a2, its buttons in a3, which it keeps at +0x18 of the
// struct, the last reading's at +0x14, and makes the presses of). Both work through the buttons the game is about to
// read, so the game moves on its own way.
//
// The word at 0x8011AC88 is the game's mode (tools/survey.js): 0 while it reads the Controller Pak, 1 the legal screen,
// 3 "Expansion Pak found", 4 and 5 the Acclaim logo and on until the title, 7 the title, 8 the menus, 11 a new game's
// first cinematic, 12 the Port of Adia's, 13 the game. Left alone, the title comes about 72 s after the start (65 s of
// it after mode 0).
//
// Skip Intro (General, on by default): from mode 1 until the title, Start is pressed for the player at every other
// reading, as a player pressing through the screens would. Not in mode 0: Start held as the game leaves it opens its
// Controller Pak menu, as on the console, and the menu takes no Start to leave.
//
// The cinematics: Start, which they otherwise ignore, skips one. A cinematic is a run of camera shots, the object at
// *0x800C1BB0 playing while its +0x38 is set, each shot's time (+0x1C, a float) running from its start (+0x30) to its
// end (+0x34) before the game moves on to the next: once Start is pressed in one, each shot's time is set to its end as
// it starts, so the game plays through the rest of the cinematic's shots, one a frame, and whatever comes at its end
// comes as it would (the level after the first, the player's control after the second). The press itself is taken from
// the game, which would pause with it once the player has control.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "recompui/config.h"

#include "turok2.h"

namespace {
    constexpr uint32_t kMode = 0x8011AC88;
    constexpr uint32_t kCinematic = 0x800C1BB0;
    constexpr uint32_t kStart = 0x1000;

    const std::string skip_intro_option = "skip_intro";
    enum class SkipIntro { Off, On };
    const std::vector<recomp::config::ConfigOptionEnumOption> skip_intro_options = {
        {SkipIntro::Off, "Off", "Off"},
        {SkipIntro::On, "On", "On"},
    };
    bool skip_intro_on = true;

    // A cinematic being skipped: from Start's press until the player has control again (mode 13 with no cinematic
    // playing) or the game is somewhere else (the title, the menus).
    bool skipping = false;

    float read_float(uint8_t* rdram, uint32_t address) {
        uint32_t bits = (uint32_t)MEM_W(0, (gpr)(int32_t)address);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    void write_float(uint8_t* rdram, uint32_t address, float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        MEM_W(0, (gpr)(int32_t)address) = (int32_t)bits;
    }

    // The cinematic object while one plays, as the game's own test has it (func_00284188, which its camera and HUD
    // ask): its +0x38 set and its time within its shot; 0 otherwise.
    uint32_t playing_cinematic(uint8_t* rdram) {
        uint32_t object = (uint32_t)MEM_W(0, (gpr)(int32_t)kCinematic);
        if (object < 0x80000000u || object >= 0x80800000u || MEM_W(0x38, (gpr)(int32_t)object) == 0) {
            return 0;
        }
        float time = read_float(rdram, object + 0x1C);
        if (time < read_float(rdram, object + 0x30) || time > read_float(rdram, object + 0x34)) {
            return 0;
        }
        return object;
    }
}

void t2::add_skip_intro_option(recomp::config::Config& general) {
    general.add_enum_option(
        skip_intro_option,
        "Skip Intro",
        "Goes from the start straight to the title, past the legal screen, the Expansion Pak's screen and the logos. "
        "Start skips a cinematic either way.",
        skip_intro_options,
        SkipIntro::On
    );
    general.add_option_change_callback(skip_intro_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant,
           recomp::config::OptionChangeContext) {
            if (const uint32_t* value = std::get_if<uint32_t>(&cur_value)) {
                skip_intro_on = static_cast<SkipIntro>(*value) == SkipIntro::On;
            }
        });
}

extern "C" void t2_pad_read(uint8_t* rdram, recomp_context* ctx) {
    gpr pad = ctx->r4;
    uint32_t buttons = (uint32_t)ctx->r7 & 0xFFFF;
    bool held_before = ((uint32_t)MEM_W(0x18, pad) & kStart) != 0;
    int32_t mode = MEM_W(0, (gpr)(int32_t)kMode);

    if (skip_intro_on && mode >= 1 && mode <= 6) {
        // Pressed at one reading and let go at the next, so each press is a new one.
        if (!held_before) {
            ctx->r7 = (gpr)(int32_t)(buttons | kStart);
        }
        return;
    }

    uint32_t cinematic = playing_cinematic(rdram);
    if (cinematic != 0 && (mode == 11 || mode == 12 || mode == 13)) {
        if ((buttons & kStart) != 0) {
            if (!held_before) {
                skipping = true;
            }
            ctx->r7 = (gpr)(int32_t)(buttons & ~kStart);
        }
    }
    if (!skipping) {
        return;
    }
    if (mode != 11 && mode != 12 && mode != 13 && mode != 17) {
        skipping = false;
        return;
    }
    if (cinematic == 0) {
        // Between the cinematic's end and what comes after it (a level's load): over once the player has control.
        if (mode == 13) {
            skipping = false;
        }
        return;
    }
    write_float(rdram, cinematic + 0x1C, read_float(rdram, cinematic + 0x34));
}
