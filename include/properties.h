/* NetHack 3.7	prop.h	$NHDT-Date: 1702274027 2023/12/11 05:53:47 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.24 $ */
/* Copyright (c) 1989 Mike Threepoint                             */
/* NetHack may be freely redistributed.  See license for details. */

/* Modified by This Could Be Better, 2024. */

#ifndef PROP_H
#define PROP_H

/*** What the properties are ***
 *
 * note:  propertynames[] array in timeout.c has string values for these.
 *        Property #0 is not used.
 */
/* Resistances to troubles */
enum prop_types {
    FIRE_RESISTANCE          =  1,
    COLD_RESISTANCE          =  2,
    SLEEP_RESISTANCE         =  3,
    DISINTEGRATION_RESISTANCE        =  4,
    SHOCK_RESISTANCE         =  5,
    POISON_RESISTANCE        =  6,
    ACID_RESISTANCE          =  7,
    STONE_RESISTANCE         =  8,
    /* note: for the first eight properties, MR_xxx == (1 << (xxx_RES - 1)) */
    DRAIN_RESISTANCE         =  9,
    SICK_RESISTANCE          = 10,
    INVULNERABLE      = 11,
    ANTIMAGIC         = 12,
    /* Troubles */
    STUNNED           = 13,
    CONFUSION         = 14,
    BLINDED           = 15,
    DEAF              = 16,
    SICK              = 17,
    STONED            = 18,
    STRANGLED         = 19,
    VOMITING          = 20,
    GLIB              = 21,
    SLIMED            = 22,
    HALLUCINATING     = 23,
    HALLUCINATION_RESISTANCE        = 24,
    FUMBLING          = 25,
    WOUNDED_LEGS      = 26,
    SLEEPY            = 27,
    HUNGER            = 28,
    /* Vision and senses */
    SEE_INVISIBLE       = 29,
    TELEPATHIC          = 30,
    WARNING             = 31,
    WARNED_OF_MONSTERS  = 32,
    WARNED_OF_UNDEAD    = 33,
    SEARCHING           = 34,
    CLAIRVOYANT         = 35,
    INFRAVISION         = 36,
    DETECT_MONSTERS     = 37,
    /* Appearance and behavior */
    ADORNED           = 38,
    INVISIBLE         = 39,
    DISPLACED         = 40,
    STEALTH           = 41,
    AGGRAVATE_MONSTER = 42,
    CONFLICT          = 43,
    /* Transportation */
    JUMPING           = 44,
    TELEPORT          = 45,
    TELEPORT_CONTROL  = 46,
    LEVITATION        = 47,
    FLYING            = 48,
    WWALKING          = 49,
    SWIMMING          = 50,
    MAGICAL_BREATHING = 51,
    PASSES_WALLS      = 52,
    /* Physical attributes */
    SLOW_DIGESTION          = 53,
    HALF_DAMAGE_SPELLS      = 54,
    HALF_DAMAGE_PHYSICAL    = 55, // ?
    REGENERATION            = 56,
    ENERGY_REGENERATION     = 57,
    PROTECTION              = 58, // Divine?
    PROTECTION_FROM_SHAPE_CHANGERS = 59,
    POLYMORPH               = 60,
    POLYMORPH_CONTROL       = 61,
    UNCHANGING              = 62,
    FAST                    = 63,
    REFLECTING              = 64,
    FREE_ACTION             = 65,
    FIXED_ABILITY           = 66,
    LIFESAVED               = 67
};
#define LAST_PROP (LIFESAVED)

/*** Where the properties come from ***/
/* Definitions were moved here from obj.h and you.h */
struct prop {
    /*** Properties conveyed by objects ***/
    long extrinsic;
/* Armor */
#define WEARING_ARMOR_BODY 0x00000001L  /* Body armor */
#define WEARING_ARMOR_CLOAK 0x00000002L /* Cloak */
#define WEARING_ARMOR_HELMET 0x00000004L /* Helmet/hat */
#define WEARING_ARMOR_SHIELD 0x00000008L /* Shield */
#define WEARING_ARMOR_GLOVES 0x00000010L /* Gloves/gauntlets */
#define WEARING_ARMOR_FOOTWEAR 0x00000020L /* Footwear */
#define WEARING_ARMOR_UNDERSHIRT 0x00000040L /* Undershirt */
#define WEARING_ARMOR (WEARING_ARMOR_BODY | WEARING_ARMOR_CLOAK | WEARING_ARMOR_HELMET | WEARING_ARMOR_SHIELD | WEARING_ARMOR_GLOVES | WEARING_ARMOR_FOOTWEAR | WEARING_ARMOR_UNDERSHIRT)
/* Weapons and artifacts */
#define WEARING_WEAPON 0x00000100L     /* Wielded weapon */
#define WEARING_QUIVER 0x00000200L  /* Quiver for (f)iring ammo */
#define WEARING_SECONDARY_WEAPON 0x00000400L /* Secondary weapon */
#define WEARING_WEAPONS (WEARING_WEAPON | WEARING_SECONDARY_WEAPON | WEARING_QUIVER)
#define WEARING_ARTIFACT 0x00001000L     /* Carrying artifact (not really worn) */
#define WEARING_ARTIFACT_INVOKED 0x00002000L    /* Invoked artifact  (not really worn) */
/* Amulets, rings, tools, and other items */
#define WEARING_AMULET 0x00010000L    /* Amulet */
#define WEARING_RING_LEFT 0x00020000L   /* Left ring */
#define WEARING_RING_RIGHT 0x00040000L   /* Right ring */
#define WEARING_RING (WEARING_RING_LEFT | WEARING_RING_RIGHT)
#define WEARING_TOOL 0x00080000L   /* Eyewear */
#define WEARING_ACCESSORY (WEARING_RING | WEARING_AMULET | WEARING_TOOL)
    /* historical note: originally in slash'em, 'worn' saddle stayed in
       hero's inventory; in nethack, it's kept in the steed's inventory */
#define WEARING_SADDLE 0x00100000L /* KMH -- For riding monsters */
#define WEARING_BALL 0x00200000L   /* Punishment ball */
#define WEARING_CHAIN 0x00400000L  /* Punishment chain */

    /*** Property is blocked by an object ***/
    long blocked; /* Same assignments as extrinsic */

    /*** Timeouts, permanent properties, and other flags ***/
    long intrinsic;
/* Timed properties */
#define TIMEOUT 0x00ffffffL     /* Up to 16 million turns */
/* Permanent properties */
#define FROMEXPER   0x01000000L /* Gain/lose with experience, for role */
#define FROMRACE    0x02000000L /* Gain/lose with experience, for race */
#define FROMOUTSIDE 0x04000000L /* By corpses, prayer, thrones, etc. */
#define INTRINSIC   (FROMOUTSIDE | FROMRACE | FROMEXPER)
/* Control flags */
#define FROMFORM    0x10000000L /* Polyd; conferred by monster form */
#define I_SPECIAL   0x20000000L /* Property is controllable */
};

/*** Definitions for backwards compatibility ***/
#define LEFT_RING WEARING_RING_LEFT
#define RIGHT_RING WEARING_RING_RIGHT
#define LEFT_SIDE LEFT_RING
#define RIGHT_SIDE RIGHT_RING
#define BOTH_SIDES (LEFT_SIDE | RIGHT_SIDE)
#define WORN_ARMOR WEARING_ARMOR
#define WORN_CLOAK WEARING_ARMOR_CLOAK
#define WORN_HELMET WEARING_ARMOR_HELMET
#define WORN_SHIELD WEARING_ARMOR_SHIELD
#define WORN_GLOVES WEARING_ARMOR_GLOVES
#define WORN_BOOTS WEARING_ARMOR_FOOTWEAR
#define WORN_AMUL WEARING_AMULET
#define WORN_BLINDF WEARING_TOOL
#define WORN_SHIRT WEARING_ARMOR_UNDERSHIRT

#endif /* PROP_H */
