// The Graphics tab's Field of View option, from us.toml's hooks: how wide the view is, 100% the game's own.
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
//
// The weapon in Turok's hands keeps the game's own view. It is drawn (func_00249898) over the world after a depth
// clear, with the projection the view loaded at its start (func_0027E000): the camera's matrix for the display list
// (+0x368 of the camera's struct, one of two by the frame's parity at 0x800B6D1C). Its models are queued
// (func_0021D39C) and drawn when the queue is flushed (func_0020B624), and on some frames they load that matrix again
// themselves. With it scaled, a wider view drew the weapon smaller and further from the screen's corner (at 150% the
// bow floated mid-screen, Turok's hand ending at the wrist), and a narrower one larger, mostly off the screen. So after
// the depth clear a copy of the matrix with the game's own field of view goes into the display list, branched over, and
// is loaded; once the queue is flushed, the weapon's own loads of the camera's matrix are pointed at the copy, and the
// camera's own is loaded again for what the view draws after. The camera's matrix is
// the view, then the perspective, then a scale on the axes, and a perspective's field of view scales only its x and y
// columns, by the cotangent of half its fovy: the copy is the camera's with those two columns scaled by
// tan(fovy / 2) / tan(own fovy / 2).
//
// The fovy isn't the eased angle itself: the camera takes the angle's tangent times its own factor (the aspect ratio
// and the view's size in it) and hands guPerspective the arctangent of that. The factor is the fovy's tangent over the
// scaled angle's, so the game's own fovy is the arctangent of the game's own angle's tangent times it.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <string>

#include "recomp.h"
#include "recompui/config.h"

#include "turok2.h"

namespace {
    const std::string field_of_view_option = "field_of_view";
    // The setting, as a factor: read from the config at each change, so the camera reads a float a frame.
    std::atomic<float> field_of_view{ 1.0f };

    constexpr float kMaxAngle = 85.0f;

    // The camera's eased angle this call, the game's own and scaled, from t2_field_of_view to
    // t2_field_of_view_perspective, which take turns on the game's thread. A cinematic's call never sets it.
    bool angle_pending = false;
    float own_angle = 0.0f;
    float scaled_angle = 0.0f;

    // How much wider each camera's view is than the game's own, as its matrix's x and y columns are scaled: a level
    // has a few cameras, and each one's last perspective is the one its matrix was made with.
    struct CameraScale {
        uint32_t camera;
        double scale;
    };
    std::array<CameraScale, 8> camera_scales{};
    size_t next_camera_slot = 0;

    // While the weapon is drawn: the camera's matrix, and the copy with the game's own field of view and where the
    // weapon's commands start. 0 when the weapon keeps the camera's matrix.
    uint32_t weapon_camera_matrix = 0;
    uint32_t weapon_matrix = 0;

    constexpr uint32_t kDisplayListHead = 0x800D8E24;
    constexpr uint32_t kFrameParity = 0x800B6D1C;
    constexpr uint32_t kCameraMatrix = 0x368;
    constexpr uint32_t kMtxSize = 0x40;
    constexpr uint32_t kLoadProjection = 0xDA380007; // G_MTX: G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
    constexpr uint32_t kBranch = 0xDE010000;         // G_DL, G_DL_NOPUSH

    double radians(double degrees) {
        return degrees * (3.14159265358979323846 / 180.0);
    }

    uint32_t read_word(uint8_t* rdram, uint32_t address) {
        return (uint32_t)MEM_W(0, (gpr)(int32_t)address);
    }

    void write_word(uint8_t* rdram, uint32_t address, uint32_t value) {
        MEM_W(0, (gpr)(int32_t)address) = (int32_t)value;
    }

    // An Mtx's element n (row * 4 + column), s15.16: the integer halves in its first 32 bytes, the fractions after.
    int32_t read_element(uint8_t* rdram, uint32_t mtx, int n) {
        uint32_t whole = read_word(rdram, mtx + 4 * (n / 2));
        uint32_t fraction = read_word(rdram, mtx + 32 + 4 * (n / 2));
        int shift = (n & 1) ? 0 : 16;
        return (int32_t)((((whole >> shift) & 0xFFFF) << 16) | ((fraction >> shift) & 0xFFFF));
    }

    double camera_scale(uint32_t camera) {
        for (const CameraScale& entry : camera_scales) {
            if (entry.camera == camera) {
                return entry.scale;
            }
        }
        return 1.0;
    }

    void set_camera_scale(uint32_t camera, double scale) {
        for (CameraScale& entry : camera_scales) {
            if (entry.camera == camera) {
                entry.scale = scale;
                return;
            }
        }
        camera_scales[next_camera_slot] = { camera, scale };
        next_camera_slot = (next_camera_slot + 1) % camera_scales.size();
    }
}

void t2::add_field_of_view_option(recomp::config::Config& graphics) {
    graphics.add_number_option(
        field_of_view_option,
        "Field of View",
        "How much of the world the view takes in: 100% is the game's own, more shows more at the sides and above and "
        "below, less zooms in. The game sets its own view per area of a level, and this scales whatever that is. "
        "Cinematics keep the game's own view, and the weapon in Turok's hands its own look.",
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
    float scaled = (scale == 1.0f) ? degrees : std::min(degrees * scale, std::max(degrees, kMaxAngle));
    angle_pending = true;
    own_angle = degrees;
    scaled_angle = scaled;
    return scaled;
}

// The camera (its struct's address) about to make its perspective with fovy, in degrees.
extern "C" void t2_field_of_view_perspective(uint32_t camera, float fovy) {
    double scale = 1.0;
    if (angle_pending && scaled_angle != own_angle) {
        double fovy_per_angle = std::tan(radians(fovy)) / std::tan(radians(scaled_angle));
        double own_fovy = std::atan(fovy_per_angle * std::tan(radians(own_angle)));
        double own_half = std::tan(own_fovy / 2.0);
        if (own_half > 0.0) {
            scale = std::tan(radians(fovy) / 2.0) / own_half;
        }
    }
    angle_pending = false;
    set_camera_scale(camera, scale);
}

// After the weapon's depth clear, in a view of the camera: the copy of the camera's matrix with the game's own field
// of view, written into the display list and branched over.
extern "C" void t2_weapon_view_begin(uint8_t* rdram, uint32_t camera) {
    weapon_camera_matrix = 0;
    double scale = camera_scale(camera);
    if (scale == 1.0) {
        return;
    }
    uint32_t camera_matrix = camera + kCameraMatrix + (read_word(rdram, kFrameParity) << 6);
    uint32_t list = read_word(rdram, kDisplayListHead);
    uint32_t mtx = list + 16;
    write_word(rdram, list, kLoadProjection);
    write_word(rdram, list + 4, mtx);
    write_word(rdram, list + 8, kBranch);
    write_word(rdram, list + 12, mtx + kMtxSize);
    int32_t elements[16];
    for (int n = 0; n < 16; n++) {
        int32_t value = read_element(rdram, camera_matrix, n);
        if ((n & 3) < 2) {
            double scaled = std::round((double)value * scale);
            value = (int32_t)std::clamp(scaled, (double)INT32_MIN, (double)INT32_MAX);
        }
        elements[n] = value;
    }
    for (int n = 0; n < 16; n += 2) {
        uint32_t a = (uint32_t)elements[n];
        uint32_t b = (uint32_t)elements[n + 1];
        write_word(rdram, mtx + 4 * (n / 2), (a & 0xFFFF0000) | (b >> 16));
        write_word(rdram, mtx + 32 + 4 * (n / 2), (a << 16) | (b & 0xFFFF));
    }
    write_word(rdram, kDisplayListHead, mtx + kMtxSize);
    weapon_camera_matrix = camera_matrix;
    weapon_matrix = mtx;
}

// After the weapon is drawn: its loads of the camera's matrix take the copy, and the camera's own is loaded again.
extern "C" void t2_weapon_view_end(uint8_t* rdram) {
    if (weapon_camera_matrix == 0) {
        return;
    }
    uint32_t list = read_word(rdram, kDisplayListHead);
    for (uint32_t command = weapon_matrix + kMtxSize; command < list; command += 8) {
        if (read_word(rdram, command) == kLoadProjection && read_word(rdram, command + 4) == weapon_camera_matrix) {
            write_word(rdram, command + 4, weapon_matrix);
        }
    }
    write_word(rdram, list, kLoadProjection);
    write_word(rdram, list + 4, weapon_camera_matrix);
    write_word(rdram, kDisplayListHead, list + 8);
    weapon_camera_matrix = 0;
}
