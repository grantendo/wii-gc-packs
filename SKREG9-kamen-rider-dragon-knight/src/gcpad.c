/* GameCube pads (WUP-028 through RiftWii) as Classic Controllers in Kamen
 * Rider: Dragon Knight (SKREG9). A port of the Kamen Rider: Super Climax
 * Heroes pack (my Kamen Rider: Super Climax Heroes pack), same
 * developer (Eighting) and the same controller code a few months older.
 *
 * The game reads its controllers only through KPADRead (two call sites in
 * its per-frame input update, 0x8001CD9C and 0x8001CE4C) and asks
 * WPADProbe to build the HOME Menu's controller list (0x800F74F0). We
 * wrap those three calls: once a frame we call the game's own PADRead, which
 * RiftWii's adapter hook fills, and for every port with a pad we write a
 * Classic Controller status into the game's KPAD buffer. The game's own
 * Classic path (0x8001DF84) does the rest. Its Classic button table
 * (0x80289F04) and action table (0x80289E0C) are word for word Super Climax
 * Heroes' (0x802F1784, 0x802F168C), so the layouts carry over.
 *
 * Offsets in KPADStatus (176 bytes in this older SDK; Super Climax Heroes'
 * is 240, but every field we write sits at the same offset), read from the
 * disassembly (input update 0x8001CD3C, Classic path 0x8001E038-0x8001E110):
 *   0x5C dev_type (2 = Classic)  0x5D wpad_err  0x5E dpd_valid
 *   0x60 cl hold  0x64 cl trig  0x68 cl release
 *   0x6C/0x70 left stick  0x74/0x78 right stick  0x7C/0x80 L/R analog
 *
 * Stick origin is RiftWii's job (set when a pad is plugged in, X+Y+Start to
 * reset), so nothing here calls PADReset.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;

typedef struct {
    u16 button;
    s8 stick_x, stick_y, substick_x, substick_y;
    u8 trigger_l, trigger_r, analog_a, analog_b;
    s8 err;
    u8 pad;
} PADStatus;

#define KPADRead ((s32 (*)(s32, void*, u32))0x80248970)
#define PADRead ((u32 (*)(PADStatus*))0x802602B0)
#define WPADProbe ((s32 (*)(s32, u32*))0x80271CB0)
#define OSSetArenaLo ((void (*)(void*))0x80253700)
#define VIResetSIIdle ((s32 (*)(void))0x8026C530) /* PADRead calls it (0x802606C4), as in Super Climax Heroes */

/* GameCube buttons */
#define GC_DL 0x0001
#define GC_DR 0x0002
#define GC_DD 0x0004
#define GC_DU 0x0008
#define GC_Z 0x0010
#define GC_R 0x0020
#define GC_L 0x0040
#define GC_A 0x0100
#define GC_B 0x0200
#define GC_X 0x0400
#define GC_Y 0x0800
#define GC_START 0x1000

/* Classic Controller buttons (KPAD/WPAD) */
#define CL_UP 0x0001
#define CL_LEFT 0x0002
#define CL_ZR 0x0004
#define CL_X 0x0008
#define CL_A 0x0010
#define CL_Y 0x0020
#define CL_B 0x0040
#define CL_ZL 0x0080
#define CL_R 0x0200
#define CL_PLUS 0x0400
#define CL_HOME 0x0800
#define CL_MINUS 0x1000
#define CL_L 0x2000
#define CL_DOWN 0x4000
#define CL_RIGHT 0x8000

#define CHANNELS 2      /* the game is two-player; ports 1-2 -> players 1-2 */
#define GRACE 30        /* frames of neutral input across a dropout */
#define TRIGGER_ON 100  /* L/R analog that counts as a press */
#define CSTICK_ON 32    /* C-stick deflection that counts as a direction */

/* Settings, written by the pack's options after the blob (see build.py).
 * Each group starts with its own magic; a group that holds anything else
 * (option left off, leftover memory) uses the defaults below. */
/* Menus vs. battle (found 19 Oct 2026 by memory snapshots in Dolphin across
 * Arcade, Training and Super Heroes): the battle system's
 * start-up sets the flag to 1 and its shutdown clears it. Dragon Knight's
 * copies of that code (matched instruction for instruction,):
 * start-up 0x80020F70, shutdown 0x80020FE8, flag 0x803BC11C (r13-0x7684). Outside a battle GameCube A confirms, B goes back and the
 * D-pad moves (Classic a / b / D-pad), whatever the layout puts on them in
 * battle. A paused battle (bit 0 of 0x802CD950, set at 0x80043F8C) takes the same menu inputs, but
 * keeps the battle layout's icons, so the Command List shows the buttons
 * that do the moves; the pause screens' footers then name the battle
 * buttons (the user's choice: inputs that feel right over right footers). */
#define IN_BATTLE (*(volatile u32*)0x803BC11C)
#define PAUSED (*(volatile u32*)0x802CD950)
static int in_menu(void) { return IN_BATTLE == 0; }

#define CFG_BASE 0x803D2000
#define ARENA_END 0x803D2200u /* the settings: six groups of 0x40 */
enum { SRC_A, SRC_B, SRC_X, SRC_Y, SRC_Z, SRC_R, SRC_L, SRC_START,
       SRC_DU, SRC_DD, SRC_DL, SRC_DR, SRC_CU, SRC_CD, SRC_CL, SRC_CR, SRC_COUNT };

struct group {
    u32 magic;
    u32 first, count;
    u32 map[8];
};
#define GROUP(n) ((volatile struct group*)(CFG_BASE + (n) * 0x40))
#define HOME_GROUP ((volatile u32*)(CFG_BASE + 4 * 0x40))
#define ICON_GROUP ((volatile u32*)(CFG_BASE + 5 * 0x40)) /* 1 with a pad, 2 always, 0 off */
#define MAGIC(n) (0x47434B30u + (n)) /* 'GCK0'.. */

/* The options' recommended choices: Layout A (B light, A heavy), L jumps
 * (R dashes, Y cancel, Z Rider Art; the user's default, 19 Oct), style
 * actions on the C-stick (Finale + Charge on D-Up), Start+Z for HOME. */
static const u16 default_map[SRC_COUNT] = {
    [SRC_A] = CL_B, [SRC_B] = CL_Y, [SRC_X] = CL_A,
    [SRC_Y] = CL_ZR, [SRC_Z] = CL_ZL, [SRC_R] = CL_L,
    [SRC_L] = CL_R, [SRC_START] = CL_PLUS,
    [SRC_DU] = CL_X | CL_MINUS, [SRC_DD] = CL_MINUS, [SRC_DL] = 0, [SRC_DR] = 0,
    [SRC_CU] = CL_UP, [SRC_CD] = CL_DOWN, [SRC_CL] = CL_LEFT, [SRC_CR] = CL_RIGHT,
};
#define DEFAULT_HOME (GC_START | GC_Z)

static u16 map[SRC_COUNT];      /* in battle */
static u16 menu_map[SRC_COUNT]; /* outside a battle */
static u16 home_combo;
static u8 icon_mode;
static u8 configured;

static PADStatus pads[4];
static u8 grace[CHANNELS];
static u8 live[CHANNELS];
static u32 prev_hold[CHANNELS];
/* this frame's status per channel, so the retry read reuses it */
static u32 cur_hold[CHANNELS], cur_trig[CHANNELS], cur_release[CHANNELS];
static float cur_axes[CHANNELS][6];
/* counts input updates, so the HOME Menu can tell whether they still run */
static u32 update_seq, probe_seen;
/* frames left in which the HOME Menu counts as up: only its controller list
 * calls WPADProbe (0x800F74F0), and while it is up the pads use the menu
 * mapping (A selects, B goes back, the D-pad moves), even mid-battle */
static u8 home_frames;
#define HOME_HOLD 30
#define KPAD_BUF(c) ((u8*)0x802C55F8 + (c) * 2816) /* the game's KPADStatus[16] per channel */

static void configure(void) {
    int g, i;
    for (i = 0; i < SRC_COUNT; ++i) map[i] = default_map[i];
    for (g = 0; g < 4; ++g) {
        volatile struct group* p = GROUP(g);
        if (p->magic != MAGIC(g) || p->count > 8 || p->first + p->count > SRC_COUNT) continue;
        for (i = 0; i < (int)p->count; ++i) map[p->first + i] = (u16)p->map[i];
    }
    home_combo = HOME_GROUP[0] == MAGIC(4) ? (u16)HOME_GROUP[1] : DEFAULT_HOME;
    icon_mode = ICON_GROUP[0] == MAGIC(5) ? (u8)ICON_GROUP[1] : 1;
    for (i = 0; i < SRC_COUNT; ++i) menu_map[i] = map[i];
    /* menus: buttons by name (character select: Y Random, L/R Prev/Next) */
    menu_map[SRC_A] = CL_A;
    menu_map[SRC_B] = CL_B;
    menu_map[SRC_X] = CL_X;
    menu_map[SRC_Y] = CL_Y;
    menu_map[SRC_L] = CL_L;
    menu_map[SRC_R] = CL_R;
    menu_map[SRC_DU] = CL_UP;
    menu_map[SRC_DD] = CL_DOWN;
    menu_map[SRC_DL] = CL_LEFT;
    menu_map[SRC_DR] = CL_RIGHT;
    configured = 1;
}

/* The Wii dims the picture (Screen Burn-In Reduction) after minutes with no
 * Wii Remote, GameCube-pad or device activity. RiftWii's adapter fills the
 * pads after the game's PADRead has looked for real ones, so the SDK's own
 * "a pad moved" reset never runs for adapter pads, and the game's Classic path never sees remote input either. While a pad is
 * in use (a button, a stick or C-stick past a small dead zone, or a
 * trigger half down), clear the game's idle flag the way the SDK does
 * (found by searching the game's code). */
#define IDLE_STICK 20
#define IDLE_TRIGGER 80
static int pad_active(const PADStatus* p) {
    return p->button != 0 || p->stick_x > IDLE_STICK || p->stick_x < -IDLE_STICK || p->stick_y > IDLE_STICK ||
           p->stick_y < -IDLE_STICK || p->substick_x > IDLE_STICK || p->substick_x < -IDLE_STICK ||
           p->substick_y > IDLE_STICK || p->substick_y < -IDLE_STICK || p->trigger_l >= IDLE_TRIGGER ||
           p->trigger_r >= IDLE_TRIGGER;
}

static float axis(int v, float full) {
    float f = (float)v / full;
    return f > 1.0f ? 1.0f : f < -1.0f ? -1.0f : f;
}

/* A stick with a dead zone: a GameCube stick at rest reads a few units off
 * centre, which fights never notice (they act past 0.4) but the HOME Menu's
 * pointer drifts on (seen on hardware, 19 Oct). Inside `dead` it reads 0;
 * past it, the rest of the travel is rescaled so full tilt is still 1. */
#define STICK_DEAD 10
static float stick(int v, float full) {
    if (v > -STICK_DEAD && v < STICK_DEAD) return 0.0f;
    return axis(v > 0 ? v - STICK_DEAD : v + STICK_DEAD, full - STICK_DEAD);
}

/* Compute channel `c`'s Classic status from its pad; 0 if it has none. */
static int update(int c) {
    static const PADStatus neutral;
    const PADStatus* p = &pads[c];
    u32 hold = 0, b;
    if (p->err == 0) {
        live[c] = 1;
        grace[c] = GRACE;
    } else if (live[c] && grace[c]) {
        /* not ready, a transfer error or a dropout: neutral, not garbage */
        --grace[c];
        p = &neutral;
    } else {
        /* gone for good: let the game see what KPAD says (its own
         * disconnect handling, or a Wii Remote on this channel) */
        live[c] = 0;
        prev_hold[c] = 0;
        return 0;
    }
    b = p->button;
    if (home_combo && (b & home_combo) == home_combo) {
        hold = CL_HOME;
    } else {
        if (p->trigger_l >= TRIGGER_ON) b |= GC_L;
        if (p->trigger_r >= TRIGGER_ON) b |= GC_R;
        const u16* m = in_menu() || PAUSED || home_frames ? menu_map : map;
        if (b & GC_A) hold |= m[SRC_A];
        if (b & GC_B) hold |= m[SRC_B];
        if (b & GC_X) hold |= m[SRC_X];
        if (b & GC_Y) hold |= m[SRC_Y];
        if (b & GC_Z) hold |= m[SRC_Z];
        if (b & GC_R) hold |= m[SRC_R];
        if (b & GC_L) hold |= m[SRC_L];
        if (b & GC_START) hold |= m[SRC_START];
        if (b & GC_DU) hold |= m[SRC_DU];
        if (b & GC_DD) hold |= m[SRC_DD];
        if (b & GC_DL) hold |= m[SRC_DL];
        if (b & GC_DR) hold |= m[SRC_DR];
        if (p->substick_y >= CSTICK_ON) hold |= m[SRC_CU];
        if (p->substick_y <= -CSTICK_ON) hold |= m[SRC_CD];
        if (p->substick_x <= -CSTICK_ON) hold |= m[SRC_CL];
        if (p->substick_x >= CSTICK_ON) hold |= m[SRC_CR];
    }
    if (pad_active(p)) VIResetSIIdle();
    cur_hold[c] = hold;
    cur_trig[c] = hold & ~prev_hold[c];
    cur_release[c] = prev_hold[c] & ~hold;
    prev_hold[c] = hold;
    cur_axes[c][0] = stick(p->stick_x, 72.0f);
    cur_axes[c][1] = stick(p->stick_y, 72.0f);
    cur_axes[c][2] = stick(p->substick_x, 59.0f);
    cur_axes[c][3] = stick(p->substick_y, 59.0f);
    /* analog L/R follow the buttons when the shoulders are swapped */
    if ((map[SRC_L] & CL_R) && (map[SRC_R] & CL_L)) {
        cur_axes[c][4] = axis(p->trigger_r, 180.0f);
        cur_axes[c][5] = axis(p->trigger_l, 180.0f);
    } else {
        cur_axes[c][4] = axis(p->trigger_l, 180.0f);
        cur_axes[c][5] = axis(p->trigger_r, 180.0f);
    }
    return 1;
}

/* Super Climax Heroes' pack also redraws the game's Classic button icons as GameCube ones. That
 * code works on that game's own textures (font page and button atlas) and isn't ported yet. */
static void icons_update(void) {}

static void write_status(int c, u8* k) {
    int i;
    *(u32*)(k + 0x00) = 0; /* Wii Remote buttons */
    *(u32*)(k + 0x04) = 0;
    *(u32*)(k + 0x08) = 0;
    k[0x5C] = 2;           /* WPAD_DEV_CLASSIC */
    k[0x5D] = 0;           /* WPAD_ERR_NONE */
    k[0x5E] = 0;           /* no pointer */
    *(u32*)(k + 0x60) = cur_hold[c];
    *(u32*)(k + 0x64) = cur_trig[c];
    *(u32*)(k + 0x68) = cur_release[c];
    for (i = 0; i < 6; ++i) *(float*)(k + 0x6C + 4 * i) = cur_axes[c][i];
}

void arena_fix(void* lo) {
    extern char __image_end[];
    const u32 end = ARENA_END; /* blob, then the settings */
    (void)__image_end;
    OSSetArenaLo((u32)lo < end ? (void*)end : lo);
}

s32 kpad_read_main(s32 chan, void* buf, u32 len) {
    s32 n = KPADRead(chan, buf, len);
    if (!configured) configure();
    if (chan == 0) {               /* once a frame, before channel 0 */
        PADRead(pads);
        ++update_seq;
        if (home_frames) --home_frames;
        icons_update();
    }
    if (chan < 0 || chan >= CHANNELS || !update(chan)) return n;
    write_status(chan, (u8*)buf);
    return 1;
}

s32 kpad_read_retry(s32 chan, void* buf, u32 len) {
    s32 n = KPADRead(chan, buf, len);
    if (chan < 0 || chan >= CHANNELS || !live[chan]) return n;
    write_status(chan, (u8*)buf);
    return 1;
}

s32 wpad_probe(s32 chan, u32* type) {
    home_frames = HOME_HOLD;
    /* While the HOME Menu is up the game may stop its input update; then
     * read the pads here so the menu doesn't see one press forever. */
    if (chan == 0) {
        if (update_seq == probe_seen) {
            int c;
            if (!configured) configure();
            PADRead(pads);
            for (c = 0; c < CHANNELS; ++c)
                if (update(c)) write_status(c, KPAD_BUF(c));
        }
        probe_seen = update_seq;
    }
    if (chan >= 0 && chan < CHANNELS && live[chan]) {
        if (type) *type = 2; /* WPAD_DEV_CLASSIC */
        return 0;
    }
    return WPADProbe(chan, type);
}
