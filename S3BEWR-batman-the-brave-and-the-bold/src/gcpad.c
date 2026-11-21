/* GameCube pads in Batman: The Brave and the Bold – The Videogame (S3BEWR, revision 0), as a Wii Remote + Nunchuk
 *. Made from LEGO Star Wars III's pack (same Dec 2009 SDK;
 * (SC4E64-lego-star-wars-iii in this repo), with Ranger Cross's way of adding the code
 * (my Super Sentai Battle: Ranger Cross pack): one more main.dol text section at 0x8064F3E0,
 * just above the main thread's stack, holding everything, PADRead included.
 *
 * The game links no PAD library, so the pack brings a PADRead of its own (entry.S): it polls the console's
 * GameCube ports through the SI registers (si_read) and marks the others "no controller" in the SDK's own way,
 * which is what RiftWii's PAD search recognises ("Packs that add GameCube controllers" in RiftWii's
 * docs/ARCHITECTURE.md). RiftWii hooks it and fills the missing ports from the WUP-028 adapter.
 *
 * The game reads each player's controller in one function (0x8026EE60, one object per player, channel at
 * +6288): KPADRead(chan, obj + 328, 16, &err) at 0x8026EED8 (up to 16 KPADStatus samples of 240 bytes),
 * KPADGetUnifiedWpadStatus, then WPADProbe(chan, obj + 6280) at 0x8026EF0C; the player counts as active when
 * the read returned samples and the probe didn't fail. kpad_hook and probe_hook wrap those two calls.
 *
 * KPADStatus offsets (0xF0 bytes, as in LEGO Star Wars III):
 *   0x00 hold  0x04 trig  0x08 release  0x0C acc (3 f32)  0x18 acc_value  0x1C acc_speed
 *   0x20 pointer pos (2 f32)  0x28 pos change  0x30 pos speed  0x34 horizon  0x48 distance  0x54 vertical acc
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

u32 PADRead(PADStatus* status); /* entry.S */
void* pad_memset(void* p, int c, u32 n);

#define KPADRead ((s32 (*)(s32, u8*, u32, s32*))0x80457FC0)
#define WPADProbe ((s32 (*)(s32, u32*))0x804414F0)
#define OSSetArenaLo ((void (*)(void*))0x80405EA0)
#define VIResetRFIdle ((s32 (*)(void))0x804179D0) /* matched from LEGO Star Wars III's 0x8080B700 */

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

#define WPAD_DEV_FREESTYLE 1 /* a Wii Remote with a Nunchuk */
#define CHANNELS 2           /* two-player co-op: ports 1-2 -> players 1-2 */
#define TRIGGER_ON 100       /* L/R analog that counts as a press */
#define STICK_FULL 72.0f     /* main stick deflection taken as 1.0 */
#define STICK_DEAD 8         /* main stick dead zone */
#define CSTICK_DEAD 12       /* C-stick dead zone (the pointer) */
#define POINTER_SPEED 0.035f /* pointer units per frame at full C-stick (the screen is 2.0 wide) */
/* A shake, shaped like Dolphin's emulated one (a Dolphin reference run): all three axes swing together
 * between +SHAKE_G and -SHAKE_G, a flip every SHAKE_FLIP frames. 3.4 g is above the game's own motion threshold
 * (3.2 g for the Wii Remote, 2.0 g for the Nunchuk: 0x806305F0/F4, checked at 0x800264FC). In Dolphin (20 Nov) the
 * training's heavy attack (Y) and power attack (X) both counted with it, once Batman was up close to Wildcat. */
#define SHAKE_G 3.4f
#define SHAKE_VALUE 5.889f   /* |(3.4, 3.4, 3.4)| */
#define SHAKE_FLIP 4
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
static u8 live[CHANNELS]; /* a pad has been on this channel and hasn't gone for good */
static u8 grace[CHANNELS];
static u32 prev_hold[CHANNELS];
static u32 shake_tick[CHANNELS];
static f32 prev_acc_value[CHANNELS], prev_fs_acc_value[CHANNELS];
static f32 ptr_x[CHANNELS], ptr_y[CHANNELS];
static u32 frame_mask; /* channels read this frame */
u8* dbg_buf[CHANNELS];  /* each channel's sample buffer, for Dolphin probes */

static void put_f32(u8* k, u32 off, f32 v) { *(f32*)(k + off) = v; }

static f32 axis(s8 v, s32 dead) {
    s32 x = v;
    f32 f;
    if (x > -dead && x < dead) return 0.0f;
    f = (f32)x / STICK_FULL;
    return f > 1.0f ? 1.0f : f < -1.0f ? -1.0f : f;
}

/* The layout (the 11 Nov assessment, my notes):
 * main stick = Nunchuk stick, A = A (punch), B = B (jump), L = C (block), R = - (grapple), Z = Z (gadget),
 * D-pad left/right = Wii D-pad (change gadget), D-pad up = 1 (Jump-In Hero), Start = + (pause),
 * Y = shake the Wii Remote (heavy attack), X = shake both (power attack), C-stick = the pointer (menus). */
static u32 map_buttons(const PADStatus* p) {
    u32 b = p->button, h = 0;
    if (p->trigger_l >= TRIGGER_ON) b |= GC_L;
    if (p->trigger_r >= TRIGGER_ON) b |= GC_R;
    if (b & GC_A) h |= WR_A;
    if (b & GC_B) h |= WR_B;
    if (b & GC_L) h |= WR_C;
    if (b & GC_R) h |= WR_MINUS;
    if (b & GC_Z) h |= WR_Z;
    if (b & GC_LEFT) h |= WR_LEFT;
    if (b & GC_RIGHT) h |= WR_RIGHT;
    if (b & GC_UP) h |= WR_1;
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

/* The pointer stays a little inside the screen, so the cursor never goes past its edge. */
static f32 clamp1(f32 v) { return v > 0.95f ? 0.95f : v < -0.95f ? -0.95f : v; }

static void write_sample(u32 chan, const PADStatus* p, u8* k) {
    const u32 hold = map_buttons(p), prev = prev_hold[chan];
    const int shake_remote = (p->button & (GC_X | GC_Y)) != 0, shake_nunchuk = (p->button & GC_X) != 0;
    const f32 sx = axis(p->stick_x, STICK_DEAD), sy = axis(p->stick_y, STICK_DEAD);
    const f32 cx = axis(p->substick_x, CSTICK_DEAD), cy = axis(p->substick_y, CSTICK_DEAD);
    const f32 old_x = ptr_x[chan], old_y = ptr_y[chan];
    f32 swing, value;
    int flip;
    if (shake_remote || shake_nunchuk) ++shake_tick[chan];
    else shake_tick[chan] = 0;
    swing = ((shake_tick[chan] / SHAKE_FLIP) & 1) ? SHAKE_G : -SHAKE_G;
    flip = shake_tick[chan] > 1 && (shake_tick[chan] % SHAKE_FLIP) == 0;
    /* The pointer: the C-stick moves it (up is up), and it stays where it was left. */
    ptr_x[chan] = clamp1(old_x + cx * POINTER_SPEED);
    ptr_y[chan] = clamp1(old_y - cy * POINTER_SPEED);

    pad_memset(k, 0, 0xF0);
    *(u32*)(k + 0x00) = hold;
    *(u32*)(k + 0x04) = hold & ~prev;
    *(u32*)(k + 0x08) = prev & ~hold;
    prev_hold[chan] = hold;
    value = shake_remote ? SHAKE_VALUE : 1.0f;
    put_f32(k, 0x0C, shake_remote ? swing : 0.0f); /* else a Wii Remote held still */
    put_f32(k, 0x10, shake_remote ? swing : -1.0f);
    put_f32(k, 0x14, shake_remote ? -swing : 0.0f);
    put_f32(k, 0x18, value);
    put_f32(k, 0x1C, shake_remote && flip ? SHAKE_VALUE : value - prev_acc_value[chan]); /* a flip is a jump in speed */
    prev_acc_value[chan] = value;
    put_f32(k, 0x20, ptr_x[chan]);
    put_f32(k, 0x24, ptr_y[chan]);
    put_f32(k, 0x28, ptr_x[chan] - old_x);
    put_f32(k, 0x2C, ptr_y[chan] - old_y);
    /* The rest as Dolphin's emulated Wii Remote + Nunchuk at rest gives it (LEGO Star Wars III's pack,
     * (a Dolphin reference run): horizon (1, 0), distance 1.95, vertical acceleration (1, 0), data format 4. */
    put_f32(k, 0x34, 1.0f);
    put_f32(k, 0x48, 1.947f);
    put_f32(k, 0x54, 1.0f);
    k[0x5C] = WPAD_DEV_FREESTYLE;
    k[0x5D] = 0; /* WPAD_ERR_NONE */
    k[0x5E] = 2; /* the pointer sees both dots */
    k[0x5F] = 4; /* WPAD_FMT_FREESTYLE_ACC */
    put_f32(k, 0x60, sx);
    put_f32(k, 0x64, sy);
    value = shake_nunchuk ? SHAKE_VALUE : 1.0f;
    put_f32(k, 0x68, shake_nunchuk ? swing : 0.0f); /* else the Nunchuk held still */
    put_f32(k, 0x6C, shake_nunchuk ? swing : -1.0f);
    put_f32(k, 0x70, shake_nunchuk ? -swing : 0.0f);
    put_f32(k, 0x74, value);
    put_f32(k, 0x78, shake_nunchuk && flip ? SHAKE_VALUE : value - prev_fs_acc_value[chan]);
    prev_fs_acc_value[chan] = value;
}

/* Once a frame (the first channel the game reads): read the pads and keep each channel's live state. */
static void read_pads(void) {
    u32 c;
    PADRead(pads);
    for (c = 0; c < CHANNELS; ++c) {
        if (pads[c].err == 0) {
            live[c] = 1;
            grace[c] = GRACE;
        } else if (live[c] && grace[c]) {
            --grace[c];
        } else if (live[c]) {
            live[c] = 0;
            prev_hold[c] = 0;
            shake_tick[c] = 0;
        }
    }
}

/* The controller update's KPADRead(chan, buffer, 16, &err), once per player object a frame. */
s32 kpad_hook(s32 chan, u8* buf, u32 len, s32* err) {
    static const PADStatus neutral;
    s32 n;
    if (chan >= 0 && chan < 4) {
        if (frame_mask & (1u << chan)) frame_mask = 0; /* a channel read twice: a new frame */
        if (!frame_mask) read_pads();
        frame_mask |= 1u << chan;
    }
    n = KPADRead(chan, buf, len, err);
    if (chan < 0 || chan >= CHANNELS || len == 0 || !live[chan]) return n;
    dbg_buf[chan] = buf;
    write_sample((u32)chan, pads[chan].err == 0 ? &pads[chan] : &neutral, buf);
    if (err) *err = 0; /* KPAD_READ_ERR_NONE */
    if (pads[chan].err == 0 && pad_active(&pads[chan])) VIResetRFIdle();
    return 1;
}

/* The controller update's WPADProbe(chan, &type): a channel with a pad is a Wii Remote with a Nunchuk. */
s32 probe_hook(s32 chan, u32* type) {
    if (chan >= 0 && chan < CHANNELS && live[chan]) {
        if (type) *type = WPAD_DEV_FREESTYLE;
        return 0; /* WPAD_ERR_NONE */
    }
    return WPADProbe(chan, type);
}
