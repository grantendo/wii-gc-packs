/* GameCube controllers in Star Wars: The Force Unleashed II (SFUE64)'s
 * Multiplayer mode, as a Wii Remote + Nunchuk per port.
 *
 * The game links the SDK's PADRead (0x80449210) and calls it every frame,
 * so RiftWii's adapter hook already fills its ports; this section calls it
 * too, once a frame, for the pads. It reads its Wii Remotes in one
 * per-frame routine (0x802640B0):
 *   - KPADRead(chan, buf, 16) at 0x802641DC: the routine keeps sample 0 and
 *     never looks at the count. kpad_hook wraps that call: for a channel
 *     whose GameCube port has a pad, sample 0 is built from the pad.
 *   - WPADProbe(chan, &type) at 0x8026431C and 0x802645B0: who is connected,
 *     and with which extension (Multiplayer takes only a Nunchuk, type 1).
 *     probe_hook answers "connected, Nunchuk" for a port with a pad.
 *   - the WPAD sync call at 0x8026453C (the game's reconnect attempt, with
 *     a connect callback): sync_hook calls the callback with 0 for a port with
 *     a pad, so the match doesn't say "Communications with the Wii Remote have
 *     been interrupted for Player N".
 * The HOME Menu's own feed (0x80260BD0, 0x80260BF8) is left alone, so
 * RiftWii's HOME Menu by the GameCube combo stays in charge of it. Ports
 * with no GameCube pad pass the real Wii Remote through.
 *
 * Story Mode (Grant 20 Nov): the pack steps aside only on a port where the real WPADProbe
 * says a Wii Remote is connected; a GameCube-only port keeps the menu layout there (stick, A, B, Start = +), so
 * a GameCube-only player can always get back to the game's menus.
 *
 * Two layouts (my notes):
 *   menus: stick = cursor (Nunchuk stick), A = A, B = B (back), Start = +,
 *          D-pad = D-pad
 *   a match: stick = move (tap jump if that port's option is on), A = attack,
 *          B and Z = Force power (Nunchuk C, held), X/Y = jump (Remote B),
 *          L/R = block (Nunchuk Z, held), C-stick left/right = the
 *          character's swing move (a sideways Remote swing), Start = +
 * A match is live while the match scene is up (0x80705F64 == 1), its
 * loading and intros are over (engine object +0x6988 == 0) and at least two
 * fighters are in (0x80605ABC >= 2: 1 on the results screen); the layout
 * changes after it has held for MODE_FRAMES (LEAVE_FRAMES to leave), with every button let go for
 * one frame so nothing held carries over.
 *
 *
 * KPADStatus offsets (0xF0 bytes):
 *   0x00 hold  0x04 trig  0x08 release  0x0C acc (3 f32)  0x18 acc_value
 *   0x1C acc_speed  0x5C dev_type (1 = Nunchuk)  0x5D wpad_err
 *   0x5E dpd_valid  0x5F data_format (4: Nunchuk, no pointer)
 *   0x60 Nunchuk stick (2 f32)  0x68 Nunchuk acc (3 f32)  0x74 acc_value
 *   0x78 acc_speed
 */
typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef float f32;

typedef struct {
    u16 button;
    s8 stick_x, stick_y, substick_x, substick_y;
    u8 trigger_l, trigger_r, analog_a, analog_b;
    s8 err;
    u8 pad;
} PADStatus;

extern u8 tap_jump[4];

#define KPADRead ((s32 (*)(s32, u8*, u32))0x804312E0)
#define WPADProbe ((s32 (*)(s32, u32*))0x804574D0)
#define WPADSync ((s32 (*)(s32, void*, void*))0x80457850)
#define PADRead ((u32 (*)(PADStatus*))0x80449210)
#define OSSetArenaLo ((void (*)(void*))0x8043C100)

/* The game's state words (Dolphin, 3 Nov; my notes) */
#define MATCH_SCENE ((volatile u32*)0x80705F64)   /* 1 from a match's first intro on */
#define ENGINE ((volatile u32*)0x807066FC)        /* the engine object; +0x6988: 1 while loading or intros */
#define FIGHTERS_IN ((volatile u32*)0x80605ABC)   /* 0 in menus, 2-4 in a match, 1 on results */
#define STORY_SCENE 4  /* 0x80705F64 is 4 for all of Story Mode (crawl, cutscenes, play, pause); 0 menus, 1-3 Multiplayer */

/* GameCube buttons (PADStatus.button) */
#define GC_LEFT 0x0001
#define GC_RIGHT 0x0002
#define GC_DOWN 0x0004
#define GC_UP 0x0008
#define GC_Z 0x0010
#define GC_R 0x0020
#define GC_L 0x0040
#define GC_A 0x0100
#define GC_B 0x0200
#define GC_X 0x0400
#define GC_Y 0x0800
#define GC_START 0x1000

/* Wii Remote + Nunchuk buttons (KPADStatus.hold) */
#define WR_LEFT 0x0001
#define WR_RIGHT 0x0002
#define WR_DOWN 0x0004
#define WR_UP 0x0008
#define WR_PLUS 0x0010
#define WR_B 0x0400
#define WR_A 0x0800
#define WR_Z 0x2000
#define WR_C 0x4000

#define CHANNELS 4
#define TRIGGER_ON 100   /* L/R analog that counts as a press */
#define STICK_FULL 72.0f /* main stick deflection taken as 1.0 */
#define STICK_DEAD 8     /* main stick dead zone */
#define MODE_FRAMES 30   /* the match flags must hold this long (half a second) to switch to the match layout */
/* ... and this long (4 s) to leave it: a fighter's fall and respawn takes 1 to 3 s with the fighter count at 1
 * (Dolphin, 3 Nov), the results screen holds it for good. On the results screen the stick and A work in either layout. */
#define LEAVE_FRAMES 240

/* The swing: a C-stick flick past SWING_ON (from inside SWING_OFF) makes
 * SWING_FRAMES of a sideways spike. In Dolphin 2.5 g for 1 to 2 frames was
 * enough for the game's MeleeLeft/Right, 3.0 g adds the Strong events;
 * 3.4 g is about the sensor's limit. */
#define SWING_ON 50
#define SWING_OFF 25
#define SWING_FRAMES 2
#define SWING_G 3.4f

/* Tap jump: the stick pushed up past TAP_ON within TAP_FRAMES of being
 * below TAP_LOW is a jump (Remote B for TAP_HOLD frames). */
#define TAP_ON 56
#define TAP_LOW 20
#define TAP_FRAMES 3
#define TAP_HOLD 3

/* ---- the game's heap ------------------------------------------------------ */

extern char __image_end[];

/* OSInit's OSSetArenaLo(lo): keep the heap above this section. */
void arena_fix(void* lo) {
    const u32 end = ((u32)__image_end + 31) & ~31u;
    OSSetArenaLo((u32)lo < end ? (void*)end : lo);
}

/* ---- state ---------------------------------------------------------------- */

/* err -1 until the first PADRead: no port counts as a pad before then. */
static PADStatus pads[4] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
                            {0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0}};
static u32 prev_hold[CHANNELS];
static s32 swing_left[CHANNELS];   /* frames of spike still to send */
static s32 swing_dir[CHANNELS];    /* +1 right, -1 left */
static u8 swing_armed[CHANNELS] = {1, 1, 1, 1};
static s8 stick_y_hist[CHANNELS][TAP_FRAMES + 1];
static u8 tap_left[CHANNELS];      /* frames of jump still to hold */
static u8 tap_armed[CHANNELS] = {1, 1, 1, 1};
static u8 in_match;                /* the layout in use */
static u8 in_story;                /* Story Mode is running: a port with a real Wii Remote is left alone */
static u8 real_remote[CHANNELS];   /* the real WPADProbe saw a Wii Remote on this channel this frame */
static u16 story_count;            /* frames the scene word has disagreed with in_story */
static u16 mode_count;             /* frames the flags have disagreed with it */
static u8 let_go;                  /* one frame with nothing held, after a change */

static void put_f32(u8* k, u32 off, f32 v) { *(f32*)(k + off) = v; }

static void clear(u8* p, u32 n) {
    while (n--) *p++ = 0;
}

static f32 stick_axis(s8 v) {
    s32 x = v;
    if (x > -STICK_DEAD && x < STICK_DEAD) return 0.0f;
    {
        f32 f = (f32)x / STICK_FULL;
        if (f > 1.0f) f = 1.0f;
        if (f < -1.0f) f = -1.0f;
        return f;
    }
}

static int heap_pointer(u32 p) { return (p >= 0x80000000u && p < 0x81800000u) || (p >= 0x90000000u && p < 0x94000000u); }

static int match_flags(void) {
    const u32 engine = *ENGINE;
    if (*MATCH_SCENE != 1 || *FIGHTERS_IN < 2 || !heap_pointer(engine)) return 0;
    return *(volatile u32*)(engine + 0x6988) == 0;
}

/* Once a frame: the layout follows the match flags after MODE_FRAMES (LEAVE_FRAMES to leave a match). */
static void update_mode(void) {
    const u8 story = *MATCH_SCENE == STORY_SCENE;
    if (story == in_story) {
        story_count = 0;
    } else if (++story_count >= MODE_FRAMES) {
        in_story = story;
        story_count = 0;
        in_match = 0;
        mode_count = 0;
        let_go = 1;
    }
    if (in_story) return;
    const u8 now = (u8)match_flags();
    if (now == in_match) {
        mode_count = 0;
        return;
    }
    if (++mode_count < (now ? MODE_FRAMES : LEAVE_FRAMES)) return;
    in_match = now;
    mode_count = 0;
    let_go = 1;
}

static u32 menu_buttons(const PADStatus* p) {
    const u32 b = p->button;
    u32 h = 0;
    if (b & GC_A) h |= WR_A;
    if (b & GC_B) h |= WR_B;
    if (b & GC_START) h |= WR_PLUS;
    if (b & GC_UP) h |= WR_UP;
    if (b & GC_DOWN) h |= WR_DOWN;
    if (b & GC_LEFT) h |= WR_LEFT;
    if (b & GC_RIGHT) h |= WR_RIGHT;
    return h;
}

static u32 match_buttons(const PADStatus* p, u32 chan) {
    u32 b = p->button, h = 0;
    u32 i;
    if (p->trigger_l >= TRIGGER_ON) b |= GC_L;
    if (p->trigger_r >= TRIGGER_ON) b |= GC_R;
    if (b & GC_A) h |= WR_A;                   /* attack */
    if (b & (GC_B | GC_Z)) h |= WR_C;          /* Force power, held */
    if (b & (GC_X | GC_Y)) h |= WR_B;          /* jump */
    if (b & (GC_L | GC_R)) h |= WR_Z;          /* block, held */
    if (b & GC_START) h |= WR_PLUS;
    if (b & GC_UP) h |= WR_UP;
    if (b & GC_DOWN) h |= WR_DOWN;
    if (b & GC_LEFT) h |= WR_LEFT;
    if (b & GC_RIGHT) h |= WR_RIGHT;

    /* The swing move: a C-stick flick left or right. */
    {
        const s32 x = p->substick_x;
        if (swing_armed[chan] && (x >= SWING_ON || x <= -SWING_ON)) {
            swing_left[chan] = SWING_FRAMES;
            swing_dir[chan] = x > 0 ? 1 : -1;
            swing_armed[chan] = 0;
        } else if (x < SWING_OFF && x > -SWING_OFF) {
            swing_armed[chan] = 1;
        }
    }

    /* Tap jump, if this port's option is on. */
    for (i = TAP_FRAMES; i > 0; --i) stick_y_hist[chan][i] = stick_y_hist[chan][i - 1];
    stick_y_hist[chan][0] = p->stick_y;
    if (tap_jump[chan]) {
        const s32 y = p->stick_y;
        if (tap_armed[chan] && y >= TAP_ON) {
            for (i = 1; i <= TAP_FRAMES; ++i) {
                if (stick_y_hist[chan][i] < TAP_LOW) {
                    tap_left[chan] = TAP_HOLD;
                    break;
                }
            }
            tap_armed[chan] = 0;
        } else if (y < TAP_LOW) {
            tap_armed[chan] = 1;
        }
        if (tap_left[chan]) {
            h |= WR_B;
            --tap_left[chan];
        }
    }
    return h;
}

static void reset_channel(u32 chan) {
    prev_hold[chan] = 0;
    swing_left[chan] = 0;
    swing_armed[chan] = 1;
    tap_left[chan] = 0;
    tap_armed[chan] = 1;
}

static void write_sample(u32 chan, const PADStatus* p, u8* k) {
    u32 hold = 0;
    const u32 prev = prev_hold[chan];
    f32 ax = 0.0f, value = 1.0f, speed = 0.0f;
    if (let_go) {
        reset_channel(chan);
    } else if (in_match) {
        hold = match_buttons(p, chan);
    } else {
        hold = menu_buttons(p);
        swing_left[chan] = 0;
    }
    clear(k, 0xF0);
    *(u32*)(k + 0x00) = hold;
    *(u32*)(k + 0x04) = hold & ~prev;
    *(u32*)(k + 0x08) = prev & ~hold;
    prev_hold[chan] = hold;
    if (swing_left[chan] > 0) {
        ax = swing_dir[chan] * SWING_G;
        value = SWING_G;
        speed = SWING_G;
        --swing_left[chan];
    }
    /* The Wii Remote at rest reads (0, -1, 0) in this game (Dolphin). */
    put_f32(k, 0x0C, ax);
    put_f32(k, 0x10, -1.0f);
    put_f32(k, 0x14, 0.0f);
    put_f32(k, 0x18, value);
    put_f32(k, 0x1C, speed);
    /* A real Remote's status has 1.0 here too (Dolphin dump, the game's table at 0x806821C8): the pointer's
     * horizontal vector x (0x34), its distance (0x48) and 0x54. */
    put_f32(k, 0x34, 1.0f);
    put_f32(k, 0x48, 1.0f);
    put_f32(k, 0x54, 1.0f);
    k[0x5C] = 1;  /* WPAD_DEV_FREESTYLE: a Nunchuk */
    k[0x5D] = 0;  /* WPAD_ERR_NONE */
    k[0x5E] = 0;  /* no pointer */
    k[0x5F] = 4;  /* WPAD_FMT_FREESTYLE: Remote + Nunchuk, no pointer, as the game asks */
    put_f32(k, 0x60, stick_axis(p->stick_x));
    put_f32(k, 0x64, stick_axis(p->stick_y));
    put_f32(k, 0x68, 0.0f);
    put_f32(k, 0x6C, -1.0f);
    put_f32(k, 0x70, 0.0f);
    put_f32(k, 0x74, 1.0f);
    put_f32(k, 0x78, 0.0f);
}

/* In Story Mode, the pack stands aside on this channel: a real Wii Remote is connected there. */
static int stand_aside(s32 chan) { return in_story && real_remote[chan]; }

/* The per-frame routine's KPADRead(chan, buffers, 16). */
s32 kpad_hook(s32 chan, u8* buf, u32 len) {
    const s32 n = KPADRead(chan, buf, len);
    if (chan < 0 || chan >= CHANNELS || len == 0) return n;
    if (chan == 0) {  /* once a frame: the routine goes 0 to 3 */
        u32 c, type;
        PADRead(pads);
        update_mode();
        for (c = 0; c < CHANNELS; ++c) real_remote[c] = WPADProbe((s32)c, &type) == 0; /* WPAD_ERR_NONE */
    }
    if (stand_aside(chan) || pads[chan].err != 0) {
        reset_channel((u32)chan);
        if (chan == CHANNELS - 1) let_go = 0;
        return n;
    }
    write_sample((u32)chan, &pads[chan], buf);
    if (chan == CHANNELS - 1) let_go = 0;
    return n > 0 ? n : 1;
}

/* WPADProbe(chan, &type) in the same routine: a port with a GameCube pad is
 * a connected Wii Remote with a Nunchuk. */
s32 probe_hook(s32 chan, u32* type) {
    if (chan >= 0 && chan < CHANNELS && !stand_aside(chan) && pads[chan].err == 0) {
        if (type) *type = 1;  /* WPAD_DEV_FREESTYLE */
        return 0;             /* WPAD_ERR_NONE */
    }
    return WPADProbe(chan, type);
}

/* The per-frame routine starts a WPAD sync (the game's reconnect attempt, WPADStartFastSimpleSync-like) for a
 * channel it counts as lost, every 750 ms, with a callback (0x80264040) that the library calls with 0 when a
 * Remote connects. The callback keeps its result in the game's connection table (0x8059EB38) and the time in
 * 0x80682608[chan]; a channel that has never had it, or whose attempt failed (-1: the library busy with another
 * channel's attempt), is "interrupted" when a match starts ("Communications with the Wii Remote have been
 * interrupted for Player N"). A port with a GameCube pad is a connected Remote: call the callback with 0 for it,
 * as the library does for a Remote. The attempt itself still goes to the library. */
s32 sync_hook(s32 chan, void* a, void* cb) {
    const s32 r = WPADSync(chan, a, cb);
    if (chan >= 0 && chan < CHANNELS && !stand_aside(chan) && pads[chan].err == 0) {
        if (cb) ((void (*)(s32, s32))cb)(chan, 0);
        return 0;
    }
    return r;
}
