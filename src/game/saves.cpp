// More than one save on the Controller Pak, from us.toml's hooks in the save screen.
//
// The game keeps one save, a note named "TUROK 2", on each pak: its Save Game button (func_0041FEC4) looks through the
// pak's 16 notes for one, and if it finds it asks to overwrite it ("would you like to overwrite this note?"), whose YES
// deletes the note (func_00426064) and saves a new one (func_0041F0C8). Only with no TUROK 2 note does Save Game call
// func_0041F0C8 itself, which checks the pak has room, takes the next note letter the pak's notes leave free
// (func_00426A94: A, B, ...) and writes the save. On a console a second save meant a second pak, in another
// controller. The runtime's pak in port 1 is 12 banks (main.cpp), 1498 free pages, room for 16 of the game's 90-page
// saves, so t2_save_new_note makes Save Game take the new note's way while the pak has room for one: each save is a
// note of its own, and only a full pak asks to overwrite one. The load screen lists the pak's notes, and a note chosen
// there loads that note (func_0041F32C), so each save loads on its own.
//
// The save screen's "free pages :xxx - needed :xxx" (the string at 0x800B48B4, func_0041FCF4) is written with "%3d" and
// only its first three characters copied, so the pak's 1498 showed as 149: t2_free_pages_text writes a count of four
// digits in full, as "free pages:1498".

#include <cstdint>

#include "recomp.h"

extern "C" {
    void func_0042670C(uint8_t* rdram, recomp_context* ctx);
    void func_004267A0(uint8_t* rdram, recomp_context* ctx);
    void func_00426E6C(uint8_t* rdram, recomp_context* ctx);
}

namespace {
    // The size of the game's save, from which func_00426E6C counts its pages: the word at 0x800F70A8 plus 0x688, as
    // func_0041F0C8 has it.
    constexpr uint32_t kSaveSizeBase = 0x800F70A8;
    constexpr uint32_t kSaveSizeExtra = 0x688;

    gpr address(uint32_t a) {
        return (gpr)(int32_t)a;
    }
}

// Before Save Game's branch to the overwrite prompt (0x0042001C): v0 is set when the pak (the port in s1) has a TUROK 2
// note. With room on the pak for another (free pages for a save and an empty note, func_004267A0 and func_0042670C as
// func_0041F0C8 asks them), v0 is cleared, and Save Game saves to a new note. The game's own calls are made with its
// registers as they were and restored after; their results go to the button's stack frame at +0x18 and +0x2C, which
// it is done with by then (the name it compared, and the note it read).
extern "C" void t2_save_new_note(uint8_t* rdram, recomp_context* ctx) {
    if (ctx->r2 == 0) {
        return;
    }
    const recomp_context saved = *ctx;
    const gpr sp = ctx->r29;
    bool room = false;

    ctx->r4 = saved.r17;
    ctx->r5 = sp + 0x2C;
    func_004267A0(rdram, ctx);
    if ((int32_t)ctx->r2 == 0) {
        const int32_t free_pages = MEM_W(0x2C, sp);
        ctx->r4 = saved.r17;
        ctx->r5 = sp + 0x18;
        func_0042670C(rdram, ctx);
        if ((int32_t)ctx->r2 == 0 && MEM_W(0x18, sp) != 0) {
            ctx->r4 = address((uint32_t)MEM_W(0, address(kSaveSizeBase)) + kSaveSizeExtra);
            func_00426E6C(rdram, ctx);
            room = free_pages >= (int32_t)ctx->r2;
        }
    }

    *ctx = saved;
    if (room) {
        ctx->r2 = 0;
    }
}

// In func_0041FCF4, once the free pages' three characters are copied (before 0x0041FE74): s1 is the string's +0xE, the
// free pages are the word at +0x18 of its stack frame. "free pages :" is the string's first 12 characters; a count of
// four digits takes the space before the colon, and any other count puts the string back as it was.
extern "C" void t2_free_pages_text(uint8_t* rdram, recomp_context* ctx) {
    const gpr text = ctx->r17 - 0xE;
    const int32_t free_pages = MEM_W(0x18, ctx->r29);
    if (free_pages >= 1000 && free_pages <= 9999) {
        MEM_B(10, text) = ':';
        int32_t n = free_pages;
        for (int i = 3; i >= 0; i--) {
            MEM_B(11 + i, text) = (int8_t)('0' + n % 10);
            n /= 10;
        }
    } else {
        MEM_B(10, text) = ' ';
        MEM_B(11, text) = ':';
    }
}
