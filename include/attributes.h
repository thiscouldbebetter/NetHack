/* NetHack 3.7	attributes.h	$NHDT-Date: 1596498527 2020/08/03 23:48:47 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.12 $ */
/* Copyright 1988, Mike Stephenson                                */
/* NetHack may be freely redistributed.  See license for details. */

/* Modified by This Could Be Better, 2024. */

/*      attributes.h - Header file for character class processing. */

#ifndef ATTRIB_H
#define ATTRIB_H

enum attrib_types {
    ATTRIBUTE_STRENGTH = 0,
    ATTRIBUTE_INTELLIGENCE,
    ATTRIBUTE_WISDOM,
    ATTRIBUTE_DEXTERITY,
    ATTRIBUTE_CONSTITUTION,
    ATTRIBUTE_CHARISMA,

    ATTRIBUTE_COUNT /* used in rn2() selection of attrib */
};

#define ATTRIBUTE_BASE(x) (u.attributes_current.a[x])
#define ATTRIBUTE_BONUS(x) (u.attributes_bonus.a[x])
#define ATTRIBUTE_FROM_EXERCISE(x) (u.attributes_from_exercise.a[x])
#define ATTRIBUTE_CURRENT(x) (acurr(x))
#define ATTRIBUTE_CURRENT_STRENGTH (acurrstr())
/* should be: */
/* #define ACURR(x) (ABON(x) + ATEMP(x) + (Upolyd  ? MBASE(x) : ABASE(x)) */
#define MCURR(x) (u.macurr.a[x])
#define ATTRIBUTE_MAX(x) (u.attributes_max.a[x])
#define MMAX(x) (u.mamax.a[x])

#define ATTRIBUTE_TEMPORARY(x) (u.attributes_temporary.a[x])
#define ATTRIBUTE_TEMPORARY_TIME(x) (u.attributes_temporary_countdown.a[x])

/* KMH -- Conveniences when dealing with strength constants */
#define STRENGTH18(x) (18 + (x))  /* 18/xx */
#define STRENGTH19(x) (100 + (x)) /* For 19 and above */

struct attribs {
    schar a[ATTRIBUTE_COUNT];
};

#define ATTRMAX(x) \
    ((x == ATTRIBUTE_STRENGTH && Upolyd) ? uasmon_maxStr() : gu.urace.attrmax[x])
#define ATTRMIN(x) (gu.urace.attrmin[x])

#endif /* ATTRIB_H */
