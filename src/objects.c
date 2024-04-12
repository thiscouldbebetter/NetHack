/* NetHack 3.7	objects.c	$NHDT-Date: 1596498192 2020/08/03 23:43:12 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.66 $ */
/* Copyright (c) Mike Threepoint, 1989.                           */
/* NetHack may be freely redistributed.  See license for details. */

/* Modified by This Could Be Better, 2024. */

#include "config.h"
#include "obj.h"

#include "properties.h"
#include "skills.h"
#include "color.h"
#include "objclass.h"

static struct objdescr obj_descr_init[NUM_OBJECTS + 1] = {
#define OBJECTS_DESCR_INIT
#include "objects.h"
#undef OBJECTS_DESCR_INIT
};

static struct objclass obj_init[NUM_OBJECTS + 1] = {
#define OBJECTS_INIT
#include "objects.h"
#undef OBJECTS_INIT
};

void objects_globals_initialize(void); /* in hack.h but we're using config.h */

struct objdescr obj_descr[SIZE(obj_descr_init)];
struct objclass objects[SIZE(obj_init)];

void
objects_globals_initialize(void)
{
    memcpy(obj_descr, obj_descr_init, sizeof(obj_descr));
    memcpy(objects, obj_init, sizeof(objects));
}

/*objects.c*/
