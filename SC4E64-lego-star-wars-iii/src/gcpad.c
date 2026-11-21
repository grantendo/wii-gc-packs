/* GameCube pads in LEGO Star Wars III: The Clone Wars (SC4E64, revision 1), as a Wii Remote + Nunchuk
 *. Built the Ranger Cross way (my Super Sentai Battle: Ranger Cross pack):
 *
 * The game links no PAD library, so the pack brings a PADRead of its own (padread.S, in main.dol in place of
 * an unused function at 0x8079CC70; the rest of the pack is a blob at 0x809D2520): it polls the
 * console's GameCube ports through the SI registers (si_read) and marks the others "no controller" in the
 * SDK's own way, which is what RiftWii's PAD search recognises ("Packs that add GameCube controllers" in
 * RiftWii's docs/ARCHITECTURE.md). RiftWii hooks it and fills the missing ports from the WUP-028 adapter.
 *
 * The game reads its controllers through four KPADRead(chan, buf, 1) calls in its controller update
 * (0x80293A3C-0x80293A7C, one 240-byte KPADStatus per channel at 0x80911A48 + chan * 240). kpad_hook wraps
 * them: for a channel whose GameCube port has a pad, it returns one Wii Remote + Nunchuk sample built from
 * the pad. probe_hook wraps the game's four WPADProbe calls (0x80292B88 in its connect callback, 0x80292ED4,
 * 0x802937D0, 0x80293CB8): a channel with a pad answers "connected, Nunchuk". The game also keeps its own
 * "connected" list, filled by its KPAD connect callback (0x80292B5C, registered through
 * KPADSetConnectCallback): the first time a pad shows on a channel, kpad_hook calls that callback with
 * "connected", as WPAD would for a Wii Remote, and with "gone" when the pad leaves.
 *
 * KPADStatus offsets (0xF0 bytes, the same SDK generation as Ranger Cross and Kamen Rider: Super Climax Heroes):
 *   0x00 hold  0x04 trig  0x08 release  0x0C acc (3 f32)  0x18 acc_value  0x1C acc_speed
 *   0x5C dev_type (1 = Nunchuk)  0x5D wpad_err  0x5E dpd_valid  0x5F data_format
 *   0x60 Nunchuk stick (2 f32)  0x68 Nunchuk acc (3 f32)  0x74 acc_value  0x78 acc_speed
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

#define PADRead ((u32 (*)(PADStatus*))0x8079CC70) /* padread.S, in main.dol */
void* pad_memset(void* p, int c, u32 n);

#define KPADRead ((s32 (*)(s32, u8*, u32))0x807BBAB0)
#define WPADProbe ((s32 (*)(s32, u32*))0x807C3A10)
#define CONNECT_CB ((void (*)(s32, s32))0x80292B5C) /* the game's KPAD connect callback: (chan, 0 connected / -1 gone) */
#define KPADSetConnectCallback ((void* (*)(s32, void (*)(s32, s32)))0x807BC950)
#define OSSetArenaLo ((void (*)(void*))0x80765740)
#define VIResetRFIdle ((s32 (*)(void))0x8080B700) /* clears the RF idle flag 0x808D143C (found by searching the game's code) */

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
#define WR_2 0x0100
#define WR_1 0x0200
#define WR_B 0x0400
#define WR_A 0x0800
#define WR_MINUS 0x1000
#define WR_Z 0x2000
#define WR_C 0x4000
#define WR_HOME 0x8000

#define WPAD_DEV_CORE 0      /* a bare Wii Remote */
#define WPAD_DEV_FREESTYLE 1 /* a Wii Remote with a Nunchuk */
#define WPAD_DEV_CLASSIC 2   /* a Wii Remote with a Classic Controller */
#define CHANNELS 2           /* two-player co-op: ports 1-2 -> players 1-2 */
#define TRIGGER_ON 100       /* L/R analog that counts as a press */
#define STICK_FULL 72.0f     /* main stick deflection taken as 1.0 */
#define STICK_DEAD 8         /* main stick dead zone */
#define GRACE 30             /* frames of neutral input across a dropout */

/* ---- SI: the console's own GameCube ports (Ranger Cross's si_read) ------------------------------------ */

#define SI ((volatile u32*)0xCD006400)
#define SI_POLL 12        /* 0x30 / 4 */
#define SI_SR 14          /* 0x38 / 4 */
#define SI_POLL_VALUE ((0xF7u << 16) | (2u << 8) | 0xF0u) /* every 247 lines, twice a frame, all 4 ports */
#define SI_PAD_COMMAND 0x00400300u                         /* read the pad, analog mode 3, motor off */

static u8 si_started;

/* Fills status[i] for every port answering like a standard pad and returns a mask of the others. */
u32 si_read(PADStatus* status) {
    u32 i, missing = 0;
    if (!si_started) {
        for (i = 0; i < 4; ++i) SI[i * 3] = SI_PAD_COMMAND;
        SI[SI_POLL] = SI_POLL_VALUE;
        SI[SI_SR] = 0x80000000u;
        si_started = 1;
    }
    for (i = 0; i < 4; ++i) {
        const u32 hi = SI[i * 3 + 1], lo = SI[i * 3 + 2];
        PADStatus* s = &status[i];
        if ((hi & 0x80000000u) || !(hi & 0x00800000u)) {
            missing |= 1u << i;
            continue;
        }
        s->button = (u16)((((hi >> 24) & 0x1F) << 8) | ((hi >> 16) & 0x7F));
        s->stick_x = (s8)(((hi >> 8) & 0xFF) - 128);
        s->stick_y = (s8)((hi & 0xFF) - 128);
        s->substick_x = (s8)(((lo >> 24) & 0xFF) - 128);
        s->substick_y = (s8)(((lo >> 16) & 0xFF) - 128);
        s->trigger_l = (u8)(lo >> 8);
        s->trigger_r = (u8)lo;
        s->analog_a = s->analog_b = 0;
        s->err = 0;
    }
    return missing;
}

/* ---- the game's heap ---------------------------------------------------------------------------------- */

extern char __image_end[];

/* OSInit's OSSetArenaLo(lo): keep the heap above this section. */
void arena_fix(void* lo) {
    const u32 end = ((u32)__image_end + 31) & ~31u;
    OSSetArenaLo((u32)lo < end ? (void*)end : lo);
}

/* ---- the pads ----------------------------------------------------------------------------------------- */

static PADStatus pads[4];
static u8 live[CHANNELS];      /* a pad has been on this channel and hasn't gone for good */
static u8 grace[CHANNELS];
static u8 announced[CHANNELS]; /* the game's connect callback was told "connected" */
static u32 prev_hold[CHANNELS];

static void put_f32(u8* k, u32 off, f32 v) { *(f32*)(k + off) = v; }

static f32 stick_axis(s8 v) {
    s32 x = v;
    f32 f;
    if (x > -STICK_DEAD && x < STICK_DEAD) return 0.0f;
    f = (f32)x / STICK_FULL;
    return f > 1.0f ? 1.0f : f < -1.0f ? -1.0f : f;
}

/* The layout (Grant, 11 Nov, from LEGO Star Wars II on GameCube; The Complete Saga's sheet):
 * stick and D-pad = Nunchuk stick, A = A (jump), B = B (attack), X = Z (special), Y = C (tag),
 * L = 1 and R = 2 (Free Play), Start = + (pause). Z and the C-stick are left free. */
static u32 map_buttons(const PADStatus* p) {
    u32 b = p->button, h = 0;
    if (p->trigger_l >= TRIGGER_ON) b |= GC_L;
    if (p->trigger_r >= TRIGGER_ON) b |= GC_R;
    if (b & GC_A) h |= WR_A;
    if (b & GC_B) h |= WR_B;
    if (b & GC_X) h |= WR_Z;
    if (b & GC_Y) h |= WR_C;
    if (b & GC_L) h |= WR_1;
    if (b & GC_R) h |= WR_2;
    if (b & GC_START) h |= WR_PLUS;
    return h;
}

/* The Wii dims the picture after minutes with no controller activity; the adapter's pads never reach the
 * SDK's own reset, so clear the Wii Remote idle flag while a pad is in use. */
#define IDLE_STICK 20
#define IDLE_TRIGGER 80
static int pad_active(const PADStatus* p) {
    return p->button != 0 || p->stick_x > IDLE_STICK || p->stick_x < -IDLE_STICK || p->stick_y > IDLE_STICK ||
           p->stick_y < -IDLE_STICK || p->substick_x > IDLE_STICK || p->substick_x < -IDLE_STICK ||
           p->substick_y > IDLE_STICK || p->substick_y < -IDLE_STICK || p->trigger_l >= IDLE_TRIGGER ||
           p->trigger_r >= IDLE_TRIGGER;
}

/* The buttons (with trig and release against last frame's) and the Nunchuk from the pad: hold is the
 * pad's layout plus extra_hold (a real Wii Remote's own buttons when the slot is shared). */
static void put_pad(u32 chan, const PADStatus* p, u32 extra_hold, u8* k) {
    const u32 hold = map_buttons(p) | extra_hold, prev = prev_hold[chan];
    f32 sx = stick_axis(p->stick_x), sy = stick_axis(p->stick_y);
    if (p->button & GC_LEFT) sx = -1.0f;
    if (p->button & GC_RIGHT) sx = 1.0f;
    if (p->button & GC_DOWN) sy = -1.0f;
    if (p->button & GC_UP) sy = 1.0f;
    *(u32*)(k + 0x00) = hold;
    *(u32*)(k + 0x04) = hold & ~prev;
    *(u32*)(k + 0x08) = prev & ~hold;
    prev_hold[chan] = hold;
    k[0x5C] = WPAD_DEV_FREESTYLE;
    put_f32(k, 0x60, sx);
    put_f32(k, 0x64, sy);
    put_f32(k, 0x68, 0.0f); /* the Nunchuk held still */
    put_f32(k, 0x6C, -1.0f);
    put_f32(k, 0x70, 0.0f);
    put_f32(k, 0x74, 1.0f);
    put_f32(k, 0x78, 0.0f);
}

/* No real Wii Remote on this player: the whole sample from the pad, a Wii Remote + Nunchuk at rest. */
static void write_sample(u32 chan, const PADStatus* p, u8* k) {
    pad_memset(k, 0, 0xF0);
    put_f32(k, 0x0C, 0.0f); /* a Wii Remote held still */
    put_f32(k, 0x10, -1.0f);
    put_f32(k, 0x14, 0.0f);
    put_f32(k, 0x18, 1.0f);
    /* The rest as Dolphin's emulated Wii Remote + Nunchuk at rest gives it (a Dolphin reference run),
     * 16 Nov), on which the title screen takes +: pointer y -0.15, horizon (1, 0), distance 1.95,
     * vertical acceleration (1, 0), data format 4. */
    put_f32(k, 0x24, -0.15f);
    put_f32(k, 0x34, 1.0f);
    put_f32(k, 0x48, 1.947f);
    put_f32(k, 0x54, 1.0f);
    k[0x5D] = 0; /* WPAD_ERR_NONE */
    k[0x5E] = 0; /* no pointer */
    k[0x5F] = 4; /* WPAD_FMT_FREESTYLE_ACC: what the game sets (Ranger Cross needed 5) */
    put_pad(chan, p, 0, k);
}

/* Once a frame, before channel 0: read the pads and keep each channel's live state. */
static void read_pads(void) {
    u32 c;
    PADRead(pads);
    for (c = 0; c < CHANNELS; ++c) {
        if (pads[c].err == 0) {
            live[c] = 1;
            grace[c] = GRACE;
            if (!announced[c]) {
                announced[c] = 1;
                CONNECT_CB((s32)c, 0);
            }
        } else if (live[c] && grace[c]) {
            --grace[c];
        } else if (live[c]) {
            live[c] = 0;
            prev_hold[c] = 0;
            if (announced[c]) {
                u32 type;
                announced[c] = 0;
                if (WPADProbe((s32)c, &type) != 0) CONNECT_CB((s32)c, -1); /* no real Wii Remote holds it */
            }
        }
    }
}

/* A real Wii Remote's extension on chan, or -1 when no real Wii Remote is there. */
static s32 real_remote(s32 chan) {
    u32 type = 0xFF;
    return WPADProbe(chan, &type) == 0 ? (s32)type : -1;
}

/* The controller update's KPADRead(chan, buffer, 1). A pad on a player's port shares the slot with a real
 * Wii Remote on that player (the RiftWii proposal's rule): with a bare Wii Remote there, the pad is its
 * Nunchuk and a press on either counts, while the Wii Remote keeps its own pointer, motion and HOME; with
 * a real Nunchuk or Classic Controller plugged into it, the pad stands aside. With no real Wii Remote, the
 * whole sample comes from the pad. */
s32 kpad_hook(s32 chan, u8* buf, u32 len) {
    static const PADStatus neutral;
    const PADStatus* p;
    s32 n, ext;
    if (chan == 0) read_pads();
    n = KPADRead(chan, buf, len);
    if (chan < 0 || chan >= CHANNELS || len == 0) return n;
    if (!live[chan]) {
        /* No pad: the real Wii Remote's own sample. Remember its buttons, so a pad plugged in while one is
         * held doesn't turn that held button into a second press. */
        if (n > 0) prev_hold[chan] = *(u32*)buf;
        return n;
    }
    p = pads[chan].err == 0 ? &pads[chan] : &neutral;
    /* Who owns the slot is decided by what's connected, not by whether KPAD had a sample this frame. */
    ext = real_remote(chan);
    if (ext == WPAD_DEV_FREESTYLE || ext == WPAD_DEV_CLASSIC) {
        if (n > 0) {
            /* Its own attachment wins, sample or not. The press and release edges are against what the game
             * last saw, so a button the pad was holding when the attachment took over is let go. */
            const u32 hold = *(u32*)buf, prev = prev_hold[chan];
            *(u32*)(buf + 0x04) = hold & ~prev;
            *(u32*)(buf + 0x08) = prev & ~hold;
            prev_hold[chan] = hold;
        }
        return n;
    }
    if (ext >= 0) {
        /* A bare Wii Remote: the pad is its Nunchuk. A frame with no sample from it passes through as it is
         * (the game keeps its last sample, pointer and buttons included). */
        if (n <= 0) return n;
        put_pad((u32)chan, p, *(u32*)buf, buf);
    } else {
        write_sample((u32)chan, p, buf); /* no real Wii Remote: the whole sample from the pad */
    }
    if (pads[chan].err == 0 && pad_active(&pads[chan])) VIResetRFIdle();
    return ext >= 0 ? n : 1;
}

/* The game's WPADProbe(chan, &type): a channel with a pad is a Wii Remote with a Nunchuk. */
s32 probe_hook(s32 chan, u32* type) {
    if (chan >= 0 && chan < CHANNELS && live[chan]) {
        const s32 ext = real_remote(chan);
        if (ext == WPAD_DEV_FREESTYLE || ext == WPAD_DEV_CLASSIC) return WPADProbe(chan, type);
        if (type) *type = WPAD_DEV_FREESTYLE;
        return 0; /* WPAD_ERR_NONE */
    }
    return WPADProbe(chan, type);
}

/* The game's connect callback, as KPAD calls it: on a player whose pad is in, a real Wii Remote going off never
 * reaches the game as "gone" (the pad carries on alone), and coming back isn't a second "connected". */
static void connect_filter(s32 chan, s32 reason) {
    if (chan >= 0 && chan < CHANNELS && live[chan]) {
        if (reason != 0) return;              /* the Wii Remote went off: the pad carries on, no "gone" */
        if (announced[chan]) return;          /* it came back: the game already has this player connected */
    }
    CONNECT_CB(chan, reason);
}

/* The game's KPADSetConnectCallback(chan, its callback) at start-up (0x80292CCC, once per channel):
 * registers connect_filter in front of it. */
void* setcb_hook(s32 chan, void (*cb)(s32, s32)) {
    (void)cb;
    return KPADSetConnectCallback(chan, connect_filter);
}
