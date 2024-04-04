/* NetHack 3.7	attrib.h	$NHDT-Date: 1596498527 2020/08/03 23:48:47 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.12 $ */
/* Copyright 1988, Mike Stephenson                                */
/* NetHack may be freely redistributed.  See license for details. */

/* Modified by This Could Be Better, 2024. */

/*      attrib.h - Header file for character class processing. */

#ifndef ATTRIB_H
#define ATTRIB_H

enum attrib_types {
    A_STR = 0,
    A_INT,
    A_WIS,
    A_DEX,
    A_CON,
    A_CHA,

    A_MAX /* used in rn2() selection of attrib */
};

#define ATTRIBUTE_BASE(x) (u.attributes_current.a[x])
#define ABON(x) (u.attributes_bonus.a[x])
#define AEXE(x) (u.attributes_from_exercise.a[x])
#define ATTRIBUTE_CURRENT(x) (acurr(x))
#define ATTRIBUTE_CURRENT_STRENGTH (acurrstr())
/* should be: */
/* #define ACURR(x) (ABON(x) + ATEMP(x) + (Upolyd  ? MBASE(x) : ABASE(x)) */
#define MCURR(x) (u.macurr.a[x])
#define AMAX(x) (u.attributes_max.a[x])
#define MMAX(x) (u.mamax.a[x])

#define ATEMP(x) (u.attributes_temporary.a[x])
#define ATIME(x) (u.attributes_temporary_countdown.a[x])

/* KMH -- Conveniences when dealing with strength constants */
#define STR18(x) (18 + (x))  /* 18/xx */
#define STR19(x) (100 + (x)) /* For 19 and above */

struct attribs {
    schar a[A_MAX];
};

#define ATTRMAX(x) \
    ((x == A_STR && Upolyd) ? uasmon_maxStr() : gu.urace.attrmax[x])
#define ATTRMIN(x) (gu.urace.attrmin[x])

#endif /* ATTRIB_H */
