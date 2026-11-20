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
// Skip Intro (General, on by default) shortens the boot's screens: the title comes about 4 s after the start, not 13.
// - Mode 0, "Accessing the Controller Pak": the game reads the pak and then holds the screen on a countdown of about
//   4 s, which t2_boot_wait ends. Start held as the game leaves mode 0 opens its Controller Pak menu, as on the
//   console, and the menu takes no Start to leave (A, EXIT). With the countdown ended and Start seen in mode 0, the
//   save check's plan, which presses Enter every 1.5 s from the start, never reached a new game in 4 of 7 runs (none of
//   5 with the countdown left alone), most likely stopped in that menu; with Start hidden from the game in mode 0 it
//   reached it in all of 4. So with Skip Intro on the game sees no Start in mode 0, and with it Off, Start held at the
//   boot opens the menu as on the console.
// - Mode 1, the legal screen, takes no Start: it leaves once its time (the float at 0x8011AC80, each mode's time,
//   counting at 15 a second from 0) passes about 75, 5 s. Skip Intro sets it there, and the screen fades out at once.
// - From mode 1 until the title, Start is pressed for the player at every other reading, as a player pressing through
//   the screens would.
//
// The cinematics: Start, which they otherwise ignore, skips one. A cinematic is a run of camera shots, the object at
// *0x800C1BB0 playing while its +0x38 is set, each shot's time (+0x1C, a float) running from its start (+0x30) to its
// end (+0x34) before the game moves on to the next: once Start is pressed in one, each shot's time is set to its end as
// it starts, so the game plays through the rest of the cinematic's shots, one a frame, and whatever comes at its end
// comes as it would (the level after the first, the player's control after the second). The press itself is taken from
// the game, which would pause with it once the player has control.
//
// A save portal is no cinematic to skip, though it plays as one: walking into one sets the word at 0x801198A4 until the
// player leaves it, and its shots (Adon, held while her menu is up, and the turns to the save's screens) play all the
// while. Its menus take Start to choose, as A, so while that word is set, Start is the game's.

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
    constexpr uint32_t kBootWait = 0x8011AD40;
    constexpr uint32_t kModeTime = 0x8011AC80;
    constexpr float kLegalTime = 75.0f;
    constexpr uint32_t kStart = 0x1000;
    constexpr uint32_t kSavePortal = 0x801198A4;

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
        "Goes from the start straight to the title, past the Controller Pak's wait, the legal screen, the Expansion "
        "Pak's screen and the logos. Off, holding Start as the game starts opens the Controller Pak's menu, as on "
        "the console. "
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

// The boot's "Accessing the Controller Pak" screen: the game reads the pak and then holds the screen for about 4 s
// more, a countdown (the float at kBootWait, about 120 at 30 a second) that func_0028917C takes each frame's time from, moving the boot
// on once it is 0. Skip Intro ends it there, in mode 0 only; the pak has been read by then.
extern "C" void t2_boot_wait(uint8_t* rdram) {
    if (skip_intro_on && MEM_W(0, (gpr)(int32_t)kMode) == 0 && read_float(rdram, kBootWait) > 0.0f) {
        write_float(rdram, kBootWait, 0.0f);
    }
}

extern "C" void t2_pad_read(uint8_t* rdram, recomp_context* ctx) {
    gpr pad = ctx->r4;
    uint32_t buttons = (uint32_t)ctx->r7 & 0xFFFF;
    bool held_before = ((uint32_t)MEM_W(0x18, pad) & kStart) != 0;
    int32_t mode = MEM_W(0, (gpr)(int32_t)kMode);

    if (skip_intro_on && mode == 0) {
        ctx->r7 = (gpr)(int32_t)(buttons & ~kStart);
        return;
    }
    if (skip_intro_on && mode >= 1 && mode <= 6) {
        // The legal screen takes no Start: it leaves once its time is up.
        if (mode == 1 && read_float(rdram, kModeTime) < kLegalTime) {
            write_float(rdram, kModeTime, kLegalTime);
        }
        // Pressed at one reading and let go at the next, so each press is a new one.
        if (!held_before) {
            ctx->r7 = (gpr)(int32_t)(buttons | kStart);
        }
        return;
    }

    if (MEM_W(0, (gpr)(int32_t)kSavePortal) != 0) {
        skipping = false;
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
