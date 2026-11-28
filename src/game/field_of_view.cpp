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
// is loaded; once the queue is flushed, the weapon's own loads of the camera's matrix are pointed at the copy. The
// camera's matrix is the view, then the perspective, then a scale on the axes, and a perspective's field of view
// scales only its x and y columns, by the cotangent of half its fovy: the copy is the camera's with those two columns
// scaled by tan(fovy / 2) / tan(own fovy / 2).
//
// The world draws with a second copy beside it, the camera's matrix as it is: every load of the camera's matrix in the
// frame's list, before the weapon and after it, is pointed at it, and it is loaded again after the weapon. With the
// weapon's copy alone (v0.2.26) the world and the weapon were drawn with cameras a frame apart, and the high frame
// rate's interpolation, which pairs each frame's scenes with the frame before's by how alike their views are, paired
// the world with the weapon's view of the frame before: the frames drawn between two of the game's went back two of the
// game's frames and forward again, a jump up and down while walking and back and forth while turning, a few times a
// second. With both copies, the list holds one camera for the world and the weapon.
//
// The copies go into the list at the weapon, but they are filled just before the frame's task is sent
// (t2_field_of_view_frame_send). The game writes the frame's camera's matrix after the weapon is drawn and before the
// task goes, so at the weapon the camera's slot still holds the matrix written into it two frames before. Copies filled
// there (v0.2.27 to v0.2.30) drew the world and the weapon with the camera of two of the game's frames before, about
// 67 ms late, at any Field of View but 100%, which makes no copies: turning, the view trailed the stick as if it had
// inertia. Filled at the send, they hold the frame's own camera.
//
// The view loads the camera's matrix more than once: an object drawn with a projection of its own (func_0021ED04, for
// an object whose +0x98 is set: in a town of level 0x37 one is drawn on every frame) loads its projection, draws, and
// loads the camera's matrix again, and what comes after the weapon can load it too. Pointing only the last load before
// the weapon at the copy (v0.2.27) left the world before that object's load with the camera's matrix, read at times a
// frame late, and the rest with the copy: while moving, the parts of the level drawn before it were drawn a frame's
// turn or step off to the side of the rest. So the frame's list is searched from its start (where the game begins it,
// t2_field_of_view_frame_begin) to the weapon, and once the frame's list is done, as its task is sent, from the weapon
// to its end.
//
// The armoured animal Turok rides isn't in his hands, and widens with the world. Riding, the player's weapon (+0x996 of
// its struct) is the mount's, 0x13: its spiked back across the bottom of the view and a gun at each side, drawn by
// func_00249898 as any weapon is. With the game's own field of view they stayed the size they are at 100% while the
// world around them widened, so the back filled the bottom of the screen and the guns loomed large and close at the
// sides. The mount draws with the camera's matrix, as the world does.
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

    // The player's weapon while riding the armoured animal: the mount's back and guns.
    constexpr int32_t kMountWeapon = 0x13;

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

    // While the weapon is drawn: the camera's matrix, the copy with the game's own field of view and its scale, the
    // matrix the view draws with after the weapon (the world's copy, or the camera's own where the view's load wasn't
    // found), and where the weapon's commands start. 0 when the weapon keeps the camera's matrix.
    uint32_t weapon_camera_matrix = 0;
    double weapon_scale = 1.0;
    uint32_t weapon_matrix = 0;
    uint32_t world_matrix = 0;
    uint32_t weapon_commands = 0;

    // The frame's list from where the game begins it, 0 outside a frame, and the views whose weapon was drawn in it with
    // its copy: each one's camera's matrix, the weapon's copy and its scale, the matrix the view draws with after the
    // weapon, and where the list goes on after the weapon. Whether the copies have been filled for the frame's send.
    uint32_t frame_start = 0;
    struct FrameView {
        uint32_t camera_matrix;
        uint32_t weapon_matrix;
        double scale;
        uint32_t world_matrix;
        uint32_t after;
    };
    std::array<FrameView, 4> frame_views{};
    size_t frame_view_count = 0;
    bool frame_sent = false;

    constexpr uint32_t kDisplayListHead = 0x800D8E24;
    constexpr uint32_t kFrameParity = 0x800B6D1C;
    constexpr uint32_t kCameraMatrix = 0x368;
    constexpr uint32_t kMtxSize = 0x40;
    constexpr uint32_t kLoadProjection = 0xDA380007; // G_MTX: G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
    constexpr uint32_t kBranch = 0xDE010000;         // G_DL, G_DL_NOPUSH
    // How long a frame's list is at most: a head further from the frame's start, or before it, isn't in its list (the
    // map is drawn in a list of its own, after the frame's).
    constexpr uint32_t kMaxFrameBytes = 0x40000;

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

    bool in_frame(uint32_t address) {
        return frame_start != 0 && address >= frame_start && address - frame_start <= kMaxFrameBytes &&
            (address & 7) == 0;
    }

    // Every load of the camera's matrix from start up to end pointed at matrix; how many there were.
    int redirect_loads(uint8_t* rdram, uint32_t start, uint32_t end, uint32_t camera_matrix, uint32_t matrix) {
        int count = 0;
        for (uint32_t command = start; command < end; command += 8) {
            if (read_word(rdram, command) == kLoadProjection && read_word(rdram, command + 4) == camera_matrix) {
                write_word(rdram, command + 4, matrix);
                count++;
            }
        }
        return count;
    }

    // A copy of the camera's matrix at mtx, its x and y columns scaled by scale.
    void write_matrix(uint8_t* rdram, uint32_t mtx, uint32_t camera_matrix, double scale) {
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
    }
}

void t2::add_field_of_view_option(recomp::config::Config& graphics) {
    graphics.add_number_option(
        field_of_view_option,
        "Field of View",
        "How much of the world the view takes in: 100% is the game's own, more shows more at the sides and above and "
        "below, less zooms in. The game sets its own view per area of a level, and this scales whatever that is. "
        "Cinematics keep the game's own view, and the weapon in Turok's hands its own look; an animal he rides widens "
        "with the world.",
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

// After the weapon's depth clear, in a view of the camera with the player's weapon: the copies of the camera's matrix,
// the weapon's with the game's own field of view and the world's as it is, written into the display list and branched
// over, the weapon's loaded, and the view's load of the camera's matrix pointed at the world's. What they are filled with
// here, the slot's matrix of two frames before, is replaced at the send with the frame's. The mount keeps the camera's
// matrix.
extern "C" void t2_weapon_view_begin(uint8_t* rdram, uint32_t camera, int32_t weapon) {
    weapon_camera_matrix = 0;
    double scale = camera_scale(camera);
    if (scale == 1.0 || weapon == kMountWeapon) {
        return;
    }
    uint32_t camera_matrix = camera + kCameraMatrix + (read_word(rdram, kFrameParity) << 6);
    uint32_t list = read_word(rdram, kDisplayListHead);
    uint32_t mtx = list + 16;
    uint32_t end = mtx + kMtxSize;
    write_matrix(rdram, mtx, camera_matrix, scale);
    world_matrix = camera_matrix;
    if (in_frame(list) && redirect_loads(rdram, frame_start, list, camera_matrix, end) != 0) {
        world_matrix = end;
        write_matrix(rdram, world_matrix, camera_matrix, 1.0);
        end += kMtxSize;
    }
    write_word(rdram, list, kLoadProjection);
    write_word(rdram, list + 4, mtx);
    write_word(rdram, list + 8, kBranch);
    write_word(rdram, list + 12, end);
    write_word(rdram, kDisplayListHead, end);
    weapon_camera_matrix = camera_matrix;
    weapon_scale = scale;
    weapon_matrix = mtx;
    weapon_commands = end;
}

// After the weapon is drawn: its loads of the camera's matrix take its copy, and the world's is loaded again.
extern "C" void t2_weapon_view_end(uint8_t* rdram) {
    if (weapon_camera_matrix == 0) {
        return;
    }
    uint32_t list = read_word(rdram, kDisplayListHead);
    for (uint32_t command = weapon_commands; command < list; command += 8) {
        if (read_word(rdram, command) == kLoadProjection && read_word(rdram, command + 4) == weapon_camera_matrix) {
            write_word(rdram, command + 4, weapon_matrix);
        }
    }
    write_word(rdram, list, kLoadProjection);
    write_word(rdram, list + 4, world_matrix);
    write_word(rdram, kDisplayListHead, list + 8);
    if (frame_view_count < frame_views.size()) {
        frame_views[frame_view_count++] = { weapon_camera_matrix, weapon_matrix, weapon_scale, world_matrix, list + 8 };
    }
    weapon_camera_matrix = 0;
}

// Where the game begins the frame's display list.
extern "C" void t2_field_of_view_frame_begin(uint8_t* rdram) {
    frame_start = read_word(rdram, kDisplayListHead);
    frame_view_count = 0;
    frame_sent = false;
}

// Just before the frame's (first) task is sent, its list done: each view's copies are filled from the camera's matrix
// the game wrote for the frame, and what the view drew after its weapon loads the world's copy too. With the head in
// another list (the map's), where the frame's ends isn't known, the loads after the weapon are left.
extern "C" void t2_field_of_view_frame_send(uint8_t* rdram) {
    if (frame_sent) {
        return;
    }
    frame_sent = true;
    uint32_t head = read_word(rdram, kDisplayListHead);
    for (size_t i = 0; i < frame_view_count; i++) {
        const FrameView& view = frame_views[i];
        write_matrix(rdram, view.weapon_matrix, view.camera_matrix, view.scale);
        if (view.world_matrix != view.camera_matrix) {
            write_matrix(rdram, view.world_matrix, view.camera_matrix, 1.0);
            if (in_frame(head)) {
                redirect_loads(rdram, view.after, head, view.camera_matrix, view.world_matrix);
            }
        }
    }
}

// The frame done: one whose task wasn't sent has its copies filled here.
extern "C" void t2_field_of_view_frame_end(uint8_t* rdram) {
    t2_field_of_view_frame_send(rdram);
    frame_view_count = 0;
    frame_start = 0;
}
