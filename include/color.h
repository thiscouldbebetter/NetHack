/* NetHack 3.7	color.h	$NHDT-Date: 1682205020 2023/04/22 23:10:20 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.18 $ */
/* Copyright (c) Steve Linhart, Eric Raymond, 1989. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef COLOR_H
#define COLOR_H

/*
 * The color scheme used is tailored for an IBM PC.  It consists of the
 * standard 8 colors, followed by their bright counterparts.  There are
 * exceptions, these are listed below.	Bright black doesn't mean very
 * much, so it is used as the "default" foreground color of the screen.
 */
#define COLOR_CODE_BLACK 0
#define COLOR_CODE_RED 1
#define COLOR_CODE_GREEN 2
#define COLOR_CODE_BROWN 3 /* on IBM, low-intensity yellow is brown */
#define COLOR_CODE_BLUE 4
#define COLOR_CODE_MAGENTA 5
#define COLOR_CODE_CYAN 6
#define COLOR_CODE_GRAY 7 /* low-intensity white */
#define COLOR_CODE_NONE 8
#define COLOR_CODE_ORANGE 9
#define COLOR_CODE_BRIGHT_GREEN 10
#define COLOR_CODE_YELLOW 11
#define COLOR_CODE_BRIGHT_BLUE 12
#define COLOR_CODE_BRIGHT_MAGENTA 13
#define COLOR_CODE_BRIGHT_CYAN 14
#define COLOR_CODE_WHITE 15
#define COLOR_CODE_MAX 16

/* The "half-way" point for tty-based color systems.  This is used in */
/* the tty color setup code.  (IMHO, it should be removed - dean).    */
#define BRIGHT 8

/* color aliases used in monsters.h and display.c  */
#define HI_DOMESTIC COLOR_CODE_WHITE /* for player + pets */
#define HI_LORD COLOR_CODE_MAGENTA /* for high-end monsters */
#define HI_OVERLORD COLOR_CODE_BRIGHT_MAGENTA /* for few uniques */

/* these can be configured */
#define HI_OBJ COLOR_CODE_MAGENTA
#define HI_METAL COLOR_CODE_CYAN
#define HI_COPPER COLOR_CODE_YELLOW
#define HI_SILVER COLOR_CODE_GRAY
#define HI_GOLD COLOR_CODE_YELLOW
#define HI_LEATHER COLOR_CODE_BROWN
#define HI_CLOTH COLOR_CODE_BROWN
#define HI_ORGANIC COLOR_CODE_BROWN
#define HI_WOOD COLOR_CODE_BROWN
#define HI_PAPER COLOR_CODE_WHITE
#define HI_GLASS COLOR_CODE_BRIGHT_CYAN
#define HI_MINERAL COLOR_CODE_GRAY
#define DRAGON_SILVER COLOR_CODE_BRIGHT_CYAN
#define HI_ZAP COLOR_CODE_BRIGHT_BLUE

#define NH_BASIC_COLOR  0x1000000
#define COLORVAL(x) ((x) & 0xFFFFFF)

enum nhcolortype { no_color, nh_color, rgb_color };

struct nethack_color {
    enum nhcolortype colortyp;
    int tableindex;
    int rgbindex;
    const char *name;
    const char *hexval;
    long r, g, b;
};

typedef struct color_and_attr {
           int color, attr;
} color_attr;

#endif /* COLOR_H */

/*color.h*/
