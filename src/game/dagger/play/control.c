#include "game/dagger/play/control.h"
#include "engine/core/mem.h"

/* A personal setting belongs beside bindings, not in MAIN.DAT's game rules.
 * Zero is View and one is Cursor; an older file with no CTLS keeps classic.
 * Record merging is load time only. Never search this table during a frame. */
typedef struct ControlOption { df_u16 id; df_u8 classic; } ControlOption;
static const RecField g_fields[] = {
    REC_FIELD("id", REC_UINT, ControlOption, id),
    REC_FIELD("classic", REC_UINT, ControlOption, classic)
};
static const ControlOption g_default = { 0, 1 };
static const RecSchema g_schema = { "CTLS", "controls", g_fields, 2, sizeof(ControlOption), &g_default };
static int g_classic = 1;

int control_classic(void) { return g_classic; }
void control_set_classic(int on) { g_classic = on != 0; }

void control_read_settings(const df_u8 *bytes, df_u32 size)
{
    static RecTable table;
    RecChunk chunk;
    df_u32 pos;
    const ControlOption *row;

    g_classic = 1;
    if (!rec_is_file(bytes, size) || (!table.schema && !rec_table_init(&table, &g_schema, 1))) {
        return;
    }
    rec_table_clear(&table);
    pos = 0;
    while (rec_next(bytes, size, &pos, &chunk)) {
        if (df_eq(chunk.tag, "CTLS")) {
            rec_merge(&table, &chunk, 0);
        }
    }
    row = (const ControlOption *)rec_row(&table, 0);
    if (row && row->classic <= 1) {
        g_classic = row->classic;
    }
}

int control_put_settings(RecPut put, void *ctx)
{
    ControlOption row;

    mem_zero(&row, sizeof(row));
    row.classic = (df_u8)g_classic;
    return rec_put_rows(put, ctx, &g_schema, &row, 1, 0, 0);
}

void control_block(ControlState *s)
{
    s->left_route = 0;
    s->left_gate = s->attack_gate = 1;
}

void control_reset(ControlState *s)
{
    mem_zero(s, sizeof(*s));
    control_block(s);
}

/* Three by three regions; the center third is X, the surrounding eight
 * are directional arrows. The HUD is outside the movement surface entirely.
 * Half-open integer edges make a click belong to exactly one region. */
int control_region(int x, int y, int view_height)
{
    static const df_u8 region[9] = {
        CONTROL_NW, CONTROL_N, CONTROL_NE,
        CONTROL_W, CONTROL_TARGET, CONTROL_E,
        CONTROL_SW, CONTROL_S, CONTROL_SE
    };
    int col, row;

    if (x < 0 || x >= 320 || y < 0 || y >= view_height || view_height < 3) {
        return -1;
    }
    col = x < 106 ? 0 : (x < 214 ? 1 : 2);
    row = y < view_height / 3 ? 0 : (y < view_height * 2 / 3 ? 1 : 2);
    return region[row * 3 + col];
}

static float clamp_unit(float v)
{
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

void control_step(ControlState *s, const Input *in, int classic, int pointer,
                  int x, int y, int view_height, int sheathed, float dt, ControlFrame *out)
{
    unsigned attack;
    int held, pressed, released;
    int left, left_down;
    int alt;
    int region;

    mem_zero(out, sizeof(*out));
    if (in->reset) {
        /* A focus/capture loss is not a physical release. Keep the Alt
         * offset, but discard the old click/gesture and its synthetic edges.
         * game.c also cancels an already animated/drawn weapon here. */
        control_block(s);
        return;
    }
    /* Caller passes measured seconds. Defensively refuse negative/stall
     * deltas, as the rest of play does; no large turns on a disk stall. */
    if (dt < 0.0f || dt > 0.1f) {
        dt = 0.0f;
    }
    out->forward = (float)in->move_f;
    out->right = (float)in->move_r;
    out->yaw = ((in->held & INPUT_BIT(INPUT_TURN_RIGHT)) != 0)
                 - ((in->held & INPUT_BIT(INPUT_TURN_LEFT)) != 0);
    out->pitch = ((in->held & INPUT_BIT(INPUT_LOOK_UP)) != 0)
                   - ((in->held & INPUT_BIT(INPUT_LOOK_DOWN)) != 0);
    out->yaw *= CONTROL_TURN_RATE * dt;
    out->pitch *= CONTROL_PITCH_RATE * dt;
    out->center = (in->pressed & INPUT_BIT(INPUT_LOOK_CENTER)) != 0;
    attack = INPUT_BIT(classic ? INPUT_SWING : INPUT_ATTACK);
    held = (in->held & attack) != 0;
    pressed = (in->pressed & attack) != 0;
    released = (in->released & attack) != 0;
    left = (in->mouse_held & INPUT_MOUSE_LEFT) != 0;
    left_down = (in->mouse_pressed & INPUT_MOUSE_LEFT) != 0;
    alt = classic && (in->held & INPUT_BIT(INPUT_FREELOOK));
    region = pointer ? control_region(x, y, view_height) : -1;
    out->cursor = region;

    /* Quarantine a click held across a menu, console or Tab transition.
     * Its release clears the gate without becoming a new use or swing. */
    if (!left) {
        s->left_gate = 0;
        s->left_route = 0;
    }
    if (!held && s->attack_gate) {
        s->attack_gate = 0;
        released = 0;
    }
    if (pointer && pressed && region < 0) {
        s->attack_gate = 1;
    }
    if (pointer && alt) {
        /* Alt can look sideways while walking on the body's old heading.
         * Keep offsets finite and bounded; final camera pitch is clamped too. */
        s->view_yaw = df_wrap_tau(s->view_yaw + (float)in->mouse_dx * CONTROL_LOOK_SENS);
        s->view_pitch += (float)-in->mouse_dy * CONTROL_LOOK_SENS;
        s->view_pitch = clamp_unit(s->view_pitch / 1.5f) * 1.5f;
        if (left_down) {
            out->center = 1;
        }
        s->left_gate = 1;
        s->attack_gate = 1;
    } else if (pointer) {
        if (left_down && !s->left_gate) {
            /* A target/HUD press never turns into movement by dragging it
             * elsewhere. Arrow presses may steer across regions while held. */
            s->left_route = classic && region > CONTROL_TARGET ? 1 : 2;
            if (region >= 0 && (!classic || region == CONTROL_TARGET)) {
                out->use = 1;
            }
        }
        if (classic && left && !s->left_gate && s->left_route == 1 && region >= 0 && !held) {
            float f, turn;
            int top, bottom;

            top = view_height / 3;
            bottom = view_height * 2 / 3;
            f = y < top ? (float)(top - y) / (float)top
                       : (y >= bottom ? -(float)(y - bottom + 1) / (float)(view_height - bottom) : 0.0f);
            turn = x < 106 ? (float)(x - 106) * (1.0f / 106.0f)
                          : (x >= 214 ? (float)(x - 213) * (1.0f / 106.0f) : 0.0f);
            out->forward = clamp_unit(out->forward + f);
            out->yaw += turn * CONTROL_TURN_RATE * dt;
        }
        /* Sheathed right-click, or Shift-right-click while armed, activates
         * at the pointer even in an arrow region (original manual, p.36). */
        if (classic && !s->attack_gate && pressed && region >= 0
            && (sheathed || (in->held & INPUT_BIT(INPUT_RUN)))) {
            out->use = 1;
            s->attack_gate = 1;
        }
    } else {
        /* The old viewer-style look contains a fixed arrow-key increment.
         * Use physical mouse deltas here and the dt-scaled action rates above,
         * so keyboard turning neither doubles nor changes with frame rate. */
        out->yaw += (float)in->mouse_dx * CONTROL_LOOK_SENS;
        out->pitch -= (float)in->mouse_dy * CONTROL_LOOK_SENS;
    }
    if (out->center) {
        s->view_yaw = s->view_pitch = 0.0f;
        out->pitch = 0.0f;
    }
    /* A modern free HUD pointer uses LMB for selection, never for attacking.
     * Classic gestures use the separate, rebindable right-button action. */
    if (!s->attack_gate && !alt && !sheathed && (!pointer || (classic && (region >= 0 || held || released)))) {
        out->attack_pressed = pressed;
        out->attack_held = held;
        out->attack_released = released;
        out->gesture_dx = in->mouse_dx;
        out->gesture_dy = in->mouse_dy;
    }
}

void control_view(const ControlState *s, Camera *cam)
{
    camera_set(cam, cam->x, cam->y, cam->z, cam->yaw + s->view_yaw, cam->pitch + s->view_pitch);
}

void control_point(const Camera *view, int px, int py, Camera *aim)
{
    Basis b;
    float vx, vy, forward;
    float dx, dy, dz;

    camera_basis(view, &b);
    vx = ((float)px - (float)DF_VIEW_W * 0.5f) / camera_focal();
    vy = ((float)py - (float)DF_VIEW_H * 0.5f) / camera_focal();
    forward = b.cp - b.sp * vy;
    dx = b.rx * vx + b.fx * forward;
    dy = b.cp * vy + b.sp;
    dz = b.rz * vx + b.fz * forward;
    /* No normalized-vector copy is needed: atan2 uses ratios. This happens
     * only on activation, not as the passive cursor moves over the world. */
    camera_set(aim, view->x, view->y, view->z, df_atan2(dx, dz), 0.0f);
    /* An aim ray may be exactly vertical although the drawn camera stops
     * one degree short. Do not clamp this derived ray to the view limit. */
    aim->pitch = df_atan2(dy, df_sqrt(dx * dx + dz * dz));
}

/* Small raster helpers used ONCE at menu_setup. An entire cursor sheet is
 * one cutout; choosing a region changes UVs, not texture memory or uploads. */
static void cursor_dot(df_u8 *pixels, int shape, int x, int y, int color)
{
    if (x >= 0 && x < 16 && y >= 0 && y < 16) {
        pixels[((shape / 4) * 16 + y) * CONTROL_CURSOR_W + (shape % 4) * 16 + x] = (df_u8)color;
    }
}

static void cursor_line(df_u8 *pixels, int shape, int x, int y, int x1, int y1, int wide)
{
    int dx, dy, sx, sy, error;

    dx = x1 > x ? x1 - x : x - x1;
    dy = y1 > y ? y1 - y : y - y1;
    sx = x < x1 ? 1 : -1;
    sy = y < y1 ? 1 : -1;
    error = dx - dy;
    for (;;) {
        int twice, a, b;

        for (a = -wide; a <= wide; a++) {
            for (b = -wide; b <= wide; b++) {
                cursor_dot(pixels, shape, x + a, y + b, wide ? 156 : 146);
            }
        }
        if (x == x1 && y == y1) {
            break;
        }
        twice = error * 2;
        if (twice > -dy) { error -= dy; x += sx; }
        if (twice < dx) { error += dx; y += sy; }
    }
}

void control_cursor_sheet(df_u8 *pixels)
{
    static const int dx[9] = { 0, 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int dy[9] = { 0, -1, -1, 0, 1, 1, 1, 0, -1 };
    int shape, wide;

    mem_zero(pixels, CONTROL_CURSOR_W * CONTROL_CURSOR_H);
    for (shape = 0; shape < 9; shape++) {
        for (wide = 1; wide >= 0; wide--) {
            if (shape == CONTROL_TARGET) {
                cursor_line(pixels, shape, 4, 4, 12, 12, wide);
                cursor_line(pixels, shape, 4, 12, 12, 4, wide);
            } else {
                int x, y;

                x = 8 + dx[shape] * 5;
                y = 8 + dy[shape] * 5;
                cursor_line(pixels, shape, 8 - dx[shape] * 4, 8 - dy[shape] * 4, x, y, wide);
                cursor_line(pixels, shape, x, y, x - dx[shape] * 4 - dy[shape] * 3, y - dy[shape] * 4 + dx[shape] * 3, wide);
                cursor_line(pixels, shape, x, y, x - dx[shape] * 4 + dy[shape] * 3, y - dy[shape] * 4 - dx[shape] * 3, wide);
            }
        }
    }
}
