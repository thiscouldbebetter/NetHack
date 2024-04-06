/* NetHack 3.7  botl.h  $NHDT-Date: 1694893330 2023/09/16 19:42:10 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.37 $ */
/* Copyright (c) Michael Allison, 2003                            */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef BOTL_H
#define BOTL_H

/* MAXCO must hold longest uncompressed status line, and must be larger
 * than COLNO
 *
 * longest practical second status line at the moment is
Astral Plane \GXXXXNNNN:123456 HP:1234(1234) Pw:1234(1234) AC:-127
 Xp:30/123456789 T:123456  Stone Slime Strngl FoodPois TermIll
 Satiated Overloaded Blind Deaf Stun Conf Hallu Lev Ride
 * -- or about 185 characters.  '$' gets encoded even when it
 * could be used as-is.  The first five status conditions are fatal
 * so it's rare to have more than one at a time.
 *
 * When the full line is wider than the map, the basic status line
 * formatting will move less important fields to the end, so if/when
 * truncation is necessary, it will chop off the least significant
 * information.
 */
#if COLNO <= 160
#define MAXCO 200
#else
#define MAXCO (COLNO + 40)
#endif

struct condmap {
    const char* id;
    unsigned long bitmask;
};

enum statusfields {
    CONDITION_CHARACTERISTICS = -3, /* alias for CONDITION_STR..CONDITION_CH */
    CONDITION_RESET = -2,           /* Force everything to redisplay */
    CONDITION_FLUSH = -1,           /* Finished cycling through bot fields */
    CONDITION_TITLE = 0,
    CONDITION_STR, CONDITION_DX, CONDITION_CO, CONDITION_IN, CONDITION_WI, CONDITION_CH,  /* 1..6 */
    CONDITION_ALIGN, CONDITION_SCORE, CONDITION_CAP, CONDITION_GOLD, CONDITION_ENE, CONDITION_ENEMAX, /* 7..12 */
    CONDITION_XP, CONDITION_AC, CONDITION_HD, CONDITION_TIME, CONDITION_HUNGER, CONDITION_HP, /* 13..18 */
    CONDITION_HPMAX, CONDITION_LEVELDESC, CONDITION_EXP, CONDITION_CONDITION, /* 19..22 */
    CONDITION_VERS, /* 23 */
    MAXBLSTATS, /* [24] */
};

enum relationships {
    NO_LTEQGT = -1,
    EQ_VALUE, LT_VALUE, LE_VALUE,
    GE_VALUE, GT_VALUE, TXT_VALUE
};

enum blconditions {
    bl_bareh,
    bl_blind,
    bl_busy,
    bl_conf,
    bl_deaf,
    bl_elf_iron,
    bl_fly,
    bl_foodpois,
    bl_glowhands,
    bl_grab,
    bl_hallu,
    bl_held,
    bl_icy,
    bl_inlava,
    bl_lev,
    bl_parlyz,
    bl_ride,
    bl_sleeping,
    bl_slime,
    bl_slippery,
    bl_stone,
    bl_strngl,
    bl_stun,
    bl_submerged,
    bl_termill,
    bl_tethered,
    bl_trapped,
    bl_unconsc,
    bl_woundedl,
    bl_holding,

    CONDITION_COUNT
};

/* Boolean condition bits for the condition mask */

/* clang-format off */
#define CONDITION_MASK_BAREHANDED       0x00000001L
#define CONDITION_MASK_BLIND            0x00000002L
#define CONDITION_MASK_BUSY             0x00000004L
#define CONDITION_MASK_CONFUSED         0x00000008L
#define CONDITION_MASK_DEAF             0x00000010L
#define CONDITION_MASK_ELF_IRON         0x00000020L
#define CONDITION_MASK_FLYING           0x00000040L
#define CONDITION_MASK_FOOD_POISONED    0x00000080L
#define CONDITION_MASK_GLOWING_HANDS    0x00000100L
#define CONDITION_MASK_GRAB             0x00000200L
#define CONDITION_MASK_HALLUCINATING    0x00000400L
#define CONDITION_MASK_HELD             0x00000800L
#define CONDITION_MASK_ICY              0x00001000L
#define CONDITION_MASK_INLAVA           0x00002000L
#define CONDITION_MASK_LEVITATING       0x00004000L
#define CONDITION_MASK_PARALYZED        0x00008000L
#define CONDITION_MASK_RIDE             0x00010000L
#define CONDITION_MASK_SLEEPING         0x00020000L
#define CONDITION_MASK_SLIMED           0x00040000L
#define CONDITION_MASK_SLIPPERY         0x00080000L
#define CONDITION_MASK_PETRIFIED        0x00100000L
#define CONDITION_MASK_STRANGLED        0x00200000L
#define CONDITION_MASK_STUNNED          0x00400000L
#define CONDITION_MASK_SUBMERGED        0x00800000L
#define CONDITION_MASK_TERMINALLY_ILL   0x01000000L
#define CONDITION_MASK_TETHERED         0x02000000L
#define CONDITION_MASK_TRAPPED          0x04000000L
#define CONDITION_MASK_UNCONSCIOUS      0x08000000L
#define CONDITION_MASK_WOUNDED_LEG      0x10000000L
#define CONDITION_MASK_HOLDING          0x20000000L
#define CONDITION_MASK_BITS            30 /* number of mask bits that can be set */
/* clang-format on */

struct conditions_t {
    int ranking;
    long mask;
    enum blconditions c;
    const char* text[3];
};
extern const struct conditions_t conditions[CONDITION_COUNT];

struct condtests_t {
    enum blconditions c;
    const char* useroption;
    enum opt_in_or_out opt;
    boolean enabled;
    boolean choice;
    boolean test;
};

extern struct condtests_t condtests[CONDITION_COUNT];
extern int cond_idx[CONDITION_COUNT];

#define BEFORE  0
#define NOW     1

/*
 * Possible additional conditions:
 *  major:
 *      grab   - grabbed by eel so about to be drowned ("wrapd"? damage type
 *               is AD_WRAP but message is "<mon> swings itself around you")
 *      digst  - swallowed and being digested
 *      lava   - trapped sinking into lava
 *  in_between: (potentially severe but don't necessarily lead to death;
 *               explains to player why he isn't getting to take any turns)
 *      unconc - unconscious
 *      parlyz - (multi < 0 && (!strncmp(multi_reason, "paralyzed", 9)
 *                              || !strncmp(multi_reason, "frozen", 6)))
 *      asleep - (multi < 0 && !strncmp(multi_reason, "sleeping", 8))
 *      busy   - other multi < 0
 *  minor:
 *      held   - grabbed by non-eel or by eel but not susceptible to drowning
 *      englf  - engulfed or swallowed but not being digested (usually
 *               obvious but the blank symbol set makes that uncertain)
 *      vomit  - vomiting (causes confusion and stun late in countdown)
 *      trap   - trapped in pit, bear trap, web, or floor (solidified lava)
 *      teth   - tethered to buried iron ball
 *      chain  - punished
 *      slip   - slippery fingers
 *      ice    - standing on ice (movement becomes uncertain)
 *     [underwater - movement uncertain, vision truncated, equipment at risk]
 *  other:
 *     [hold      - poly'd into grabber and holding adjacent monster]
 *      Stormbringer - wielded weapon poses risks
 *      Cleaver   - wielded weapon risks unintended consequences
 *      barehand  - not wielding any weapon nor wearing gloves
 *      no-weapon - not wielding any weapon
 *      bow/xbow/sling - wielding a missile launcher of specified type
 *      pole      - wielding a polearm
 *      pick      - wielding a pickaxe
 *      junk      - wielding non-weapon, non-weptool
 *      naked     - no armor
 *      no-gloves - self-explanatory
 *      no-cloak  - ditto
 *     [no-{other armor slots?} - probably much too verbose]
 *  conduct?
 *      [maybe if third status line is added]
 *
 *  Can't add all of these and probably don't want to.  But maybe we
 *  can add some of them and it's not as many as first appears:
 *  lava/trap/teth are mutually exclusive;
 *  digst/grab/englf/held/hold are also mutually exclusive;
 *  Stormbringer/Cleaver/barehand/no-weapon/bow&c/pole/pick/junk too;
 *  naked/no-{any armor slot} likewise.
 */

#define VIA_WINDOWPORT() \
    ((windowprocs.wincap2 & (WC2_HIGHLIGHT_STATUS | WC2_FLUSH_STATUS)) != 0)

#define REASSESS_ONLY TRUE

/* #ifdef STATUS_HILITES */
/* hilite status field behavior - coloridx values */
#define CONDITION_HIGHLIGHT_NONE    -1    /* no hilite of this field */

#if 0
#define CONDITION_HIGHLIGHT_BOLD       -2    /* bold hilite */
#define CONDITION_HIGHLIGHT_DIM        -3    /* dim hilite */
#define CONDITION_HIGHLIGHT_ITALIC     -4    /* italic hilite */
#define CONDITION_HIGHLIGHT_UNDERLINE  -5    /* underline hilite */
#define CONDITION_HIGHLIGHT_BLINK      -6    /* blink hilite */
#define CONDITION_HIGHLIGHT_INVERSE    -7    /* inverse hilite */
                                /* or any CLR_ index (0 - 15) */
#endif

#define CONDITION_THRESHOLD_NONE 0
#define CONDITION_THRESHOLD_VALUE_PERCENTAGE 100 /* threshold is percentage */
#define CONDITION_THRESHOLD_VALUE_ABSOLUTE 101   /* threshold is particular value */
#define CONDITION_THRESHOLD_UPDOWN 102           /* threshold is up or down change */
#define CONDITION_THRESHOLD_CONDITION 103      /* threshold is bitmask of conditions */
#define CONDITION_THRESHOLD_TEXTMATCH 104      /* threshold text value to match against */
#define CONDITION_THRESHOLD_ALWAYS_HILITE 105  /* highlight regardless of value */
#define CONDITION_THRESHOLD_CRITICALHP 106     /* highlight critically low HP */

#define HIGHLIGHT_ATTRIBUTE_COLOR_NONE    COLOR_CODE_MAX + 1
#define HIGHLIGHT_ATTRIBUTE_COLOR_BOLD    COLOR_CODE_MAX + 2
#define HIGHLIGHT_ATTRIBUTE_COLOR_DIM     COLOR_CODE_MAX + 3
#define HIGHLIGHT_ATTRIBUTE_COLOR_ITALIC  COLOR_CODE_MAX + 4
#define HIGHLIGHT_ATTRIBUTE_COLOR_ULINE   COLOR_CODE_MAX + 5
#define HIGHLIGHT_ATTRIBUTE_COLOR_BLINK   COLOR_CODE_MAX + 6
#define HIGHLIGHT_ATTRIBUTE_COLOR_INVERSE COLOR_CODE_MAX + 7
#define CONDITION_ATTRIBUTE_COLOR_MAX     COLOR_CODE_MAX + 8

enum hlattribs {
    HIGHLIGHT_UNDEFINED = 0x00,
    HIGHLIGHT_NONE      = 0x01,
    HIGHLIGHT_BOLD      = 0x02,
    HIGHLIGHT_DIM       = 0x04,
    HIGHLIGHT_ITALIC    = 0x08,
    HIGHLIGHT_UNDERLINE = 0x10,
    HIGHLIGHT_BLINK     = 0x20,
    HIGHLIGHT_INVERSE   = 0x40
};

#define MAXVALWIDTH 80 /* actually less, but was using 80 to allocate title
                        * and leveldesc then using QBUFSZ everywhere else   */
#ifdef STATUS_HILITES
struct hilite_s {
    enum statusfields fld;
    boolean set;
    unsigned anytype;
    anything value;
    int behavior;
    char textmatch[MAXVALWIDTH];
    enum relationships relationships;
    int coloridx;
    struct hilite_s *next;
};
#endif

/*
 * Note: If you add/change/remove fields in istat_s, you need to
 * update the initialization of the istat_s struct blstats[][]
 * array in instance_globals_b (decl.c).
 */
struct istat_s {
    const char *fldname;
    const char *fldfmt;
    long time;  /* moves when this field hilite times out */
    boolean chg; /* need to recalc time? */
    boolean percent_matters;
    short percent_value;
    unsigned anytype;
    anything a, rawval;
    char* val;
    int valwidth;
    enum statusfields idxmax;
    enum statusfields fld;
#ifdef STATUS_HILITES
    struct hilite_s* hilite_rule; /* the entry, if any, in 'thresholds'
                                   * list that currently applies        */
    struct hilite_s* thresholds;
#endif
};

extern const char* status_fieldnames[]; /* in botl.c */

#endif /* BOTL_H */
