/* NetHack 3.7	do_wear.c	$NHDT-Date: 1702017586 2023/12/08 06:39:46 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.175 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2012. */
/* NetHack may be freely redistributed.  See license for details. */

/* Modified by This Could Be Better, 2024. */

#include "hack.h"

static NEARDATA const char see_yourself[] = "see yourself";
static NEARDATA const char unknown_type[] = "Unknown type of %s (%d)";
static NEARDATA const char c_armor[] = "armor", c_suit[] = "suit",
                           c_shirt[] = "shirt", c_cloak[] = "cloak",
                           c_gloves[] = "gloves", c_boots[] = "boots",
                           c_helmet[] = "helmet", c_shield[] = "shield",
                           c_weapon[] = "weapon", c_sword[] = "sword",
                           c_axe[] = "axe", c_that_[] = "that";

static NEARDATA const long takeoff_order[] = {
    WORN_BLINDF, WEARING_WEAPON,      WORN_SHIELD, WORN_GLOVES, LEFT_RING,
    RIGHT_RING,  WORN_CLOAK, WORN_HELMET, WORN_AMUL,   WEARING_ARMOR_BODY,
    WORN_SHIRT,  WORN_BOOTS, WEARING_SECONDARY_WEAPON,   WEARING_QUIVER,    0L
};

staticfn void on_msg(struct obj *);
staticfn void toggle_stealth(struct obj *, long, boolean);
staticfn int Armor_on(void);
/* int Boots_on(void); -- moved to extern.h */
staticfn int Cloak_on(void);
staticfn int Helmet_on(void);
staticfn int Gloves_on(void);
staticfn int Shield_on(void);
staticfn int Shirt_on(void);
staticfn void dragon_armor_handling(struct obj *, boolean, boolean);
staticfn void Amulet_on(void);
staticfn void learnring(struct obj *, boolean);
staticfn void adjust_attrib(struct obj *, int, int);
staticfn void Ring_off_or_gone(struct obj *, boolean);
staticfn int select_off(struct obj *);
staticfn struct obj *do_takeoff(void);
staticfn int take_off(void);
staticfn int menu_remove_armor(int);
staticfn void worn_armor_destroyed(struct obj *);
staticfn void count_worn_stuff(struct obj **, boolean);
staticfn int armor_or_accessory_off(struct obj *);
staticfn int accessory_or_armor_on(struct obj *);
staticfn void already_wearing(const char *);
staticfn void already_wearing2(const char *, const char *);
staticfn int equip_ok(struct obj *, boolean, boolean);
staticfn int puton_ok(struct obj *);
staticfn int remove_ok(struct obj *);
staticfn int wear_ok(struct obj *);
staticfn int takeoff_ok(struct obj *);
/* maybe_destroy_armor() may return NULL */
staticfn struct obj *maybe_destroy_armor(struct obj *, struct obj *,
                                       boolean *) NONNULLARG3;

/* plural "fingers" or optionally "gloves" */
const char *
fingers_or_gloves(boolean check_gloves)
{
    return ((check_gloves && player_armor_gloves)
            ? gloves_simple_name(player_armor_gloves) /* "gloves" or "gauntlets" */
            : makeplural(body_part(FINGER))); /* "fingers" */
}

void
off_msg(struct obj *otmp)
{
    if (flags.verbose)
        You("were wearing %s.", doname(otmp));
}

/* for items that involve no delay */
staticfn void
on_msg(struct obj *otmp)
{
    if (flags.verbose) {
        char how[BUFSZ];
        /* call xname() before obj_is_pname(); formatting obj's name
           might set obj->dknown and that affects the pname test */
        const char *otmp_name = xname(otmp);

        how[0] = '\0';
        if (otmp->otyp == TOWEL)
            Sprintf(how, " around your %s", body_part(HEAD));
        You("are now wearing %s%s.",
            obj_is_pname(otmp) ? the(otmp_name) : an(otmp_name), how);
    }
}

/* putting on or taking off an item which confers stealth;
   give feedback and discover it iff stealth state is changing;
   stealth is blocked by riding unless hero+steed fly (handled with
   BStealth by mount and dismount routines) */
staticfn
void
toggle_stealth(
    struct obj *obj,
    long oldprop, /* prop[].extrinsic, with obj->owornmask pre-stripped */
    boolean on)
{
    if (on ? gi.initial_don : gc.context.takeoff.cancelled_don)
        return;

    if (!oldprop /* extrinsic stealth from something else */
        && !HStealth /* intrinsic stealth */
        && !BStealth) { /* stealth blocked by something */
        if (obj->otyp == RIN_STEALTH)
            learnring(obj, TRUE);
        else /* discover elven cloak or elven boots */
            makeknown(obj->otyp);

        if (on) {
            if (!is_boots(obj))
                You("move very quietly.");
            else if (Levitation || Flying)
                You("float imperceptibly.");
            else
                You("walk very quietly.");
        } else {
            boolean riding = (u.monster_being_ridden != NULL);

            You("%s%s are noisy.", riding ? "and " : "sure",
                riding ? x_monnam(u.monster_being_ridden, ARTICLE_YOUR, (char *) NULL,
                                  (SUPPRESS_SADDLE | SUPPRESS_HALLUCINATION),
                                  FALSE)
                       : "");
        }
    }
}

/* putting on or taking off an item which confers displacement, or gaining
   or losing timed displacement after eating a displacer beast corpse or tin;
   give feedback and discover it iff displacement state is changing *and*
   hero is able to see self (or sense monsters); for timed, 'obj' is Null
   and this is only called for the message */
void
toggle_displacement(
    struct obj *obj,
    long oldprop, /* prop[].extrinsic, with obj->owornmask
                     stripped by caller */
    boolean on)
{
    if (on ? gi.initial_don : gc.context.takeoff.cancelled_don)
        return;

    if (!oldprop /* extrinsic displacement from something else */
        && !(u.uprops[DISPLACED].intrinsic) /* timed, from eating */
        && !(u.uprops[DISPLACED].blocked) /* (theoretical) */
        /* we don't use canseeself() here because it augments vision
           with touch, which isn't appropriate for deciding whether
           we'll notice that monsters have trouble spotting the hero */
        && ((!Blind         /* see anything */
             && !u.uswallow /* see surroundings */
             && !Invisible) /* see self */
            /* actively sensing nearby monsters via telepathy or extended
               monster detection overrides vision considerations because
               hero also senses self in this situation */
            || (Unblind_telepat
                || (Blind_telepat && Blind)
                || Detect_monsters))) {
        if (obj)
            makeknown(obj->otyp);

        You_feel("that monsters%s have difficulty pinpointing your location.",
                 on ? "" : " no longer");
    }
}

/*
 * The Type_on() functions should be called *after* setworn().
 * The Type_off() functions call setworn() themselves.
 * [Blindf_on() is an exception and calls setworn() itself.]
 */

int
Boots_on(void)
{
    long oldprop =
        u.uprops[objects[player_armor_footwear->otyp].oc_oprop].extrinsic & ~WORN_BOOTS;

    switch (player_armor_footwear->otyp) {
    case LOW_BOOTS:
    case IRON_SHOES:
    case HIGH_BOOTS:
    case JUMPING_BOOTS:
    case KICKING_BOOTS:
        break;
    case WATER_WALKING_BOOTS:
        if (u.uinwater)
            spoteffects(TRUE);
        /* (we don't need a lava check here since boots can't be
           put on while feet are stuck) */
        break;
    case SPEED_BOOTS:
        /* Speed boots are still better than intrinsic speed, */
        /* though not better than potion speed */
        if (!oldprop && !(HFast & TIMEOUT)) {
            makeknown(player_armor_footwear->otyp);
            You_feel("yourself speed up%s.",
                     (oldprop || HFast) ? " a bit more" : "");
        }
        break;
    case ELVEN_BOOTS:
        toggle_stealth(player_armor_footwear, oldprop, TRUE);
        break;
    case FUMBLE_BOOTS:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            incr_itimeout(&HFumbling, random(20));
        break;
    case LEVITATION_BOOTS:
        if (!oldprop && !HLevitation && !(BLevitation & FROMOUTSIDE)) {
            player_armor_footwear->known = 1; /* might come off if putting on over a sink,
                               * so uarmf could be Null below; status line
                               * gets updated during brief interval they're
                               * worn so hero and player learn enchantment */
            disp.bottom_line = TRUE; /* status hilites might mark AC changed */
            makeknown(player_armor_footwear->otyp);
            float_up();
            if (Levitation)
                spoteffects(FALSE); /* for sink effect */
        } else {
            float_vs_flight(); /* maybe toggle BFlying's I_SPECIAL */
        }
        break;
    default:
        impossible(unknown_type, c_boots, player_armor_footwear->otyp);
    }
    /* uarmf could be Null here (levitation boots put on over a sink) */
    if (player_armor_footwear && !player_armor_footwear->known) {
        player_armor_footwear->known = 1; /* boots' +/- evident because of status line AC */
        update_inventory();
    }
    return 0;
}

int
Boots_off(void)
{
    struct obj *otmp = player_armor_footwear;
    int otyp = otmp->otyp;
    long oldprop = u.uprops[objects[otyp].oc_oprop].extrinsic & ~WORN_BOOTS;

    gc.context.takeoff.mask &= ~WEARING_ARMOR_FOOTWEAR;
    /* For levitation, float_down() returns if Levitation, so we
     * must do a setworn() _before_ the levitation case.
     */
    setworn((struct obj *) 0, WEARING_ARMOR_FOOTWEAR);
    switch (otyp) {
    case SPEED_BOOTS:
        if (!Very_fast && !gc.context.takeoff.cancelled_don) {
            makeknown(otyp);
            You_feel("yourself slow down%s.", Fast ? " a bit" : "");
        }
        break;
    case WATER_WALKING_BOOTS:
        /* check for lava since fireproofed boots make it viable */
        if ((is_pool(u.ux, u.uy) || is_lava(u.ux, u.uy))
            && !Levitation && !Flying
            && !(is_clinger(gy.youmonst.data) && has_ceiling(&u.uz))
            && !gc.context.takeoff.cancelled_don
            /* avoid recursive call to lava_effects() */
            && !iflags.in_lava_effects) {
            /* make boots known in case you survive the drowning */
            makeknown(otyp);
            spoteffects(TRUE);
        }
        break;
    case ELVEN_BOOTS:
        toggle_stealth(otmp, oldprop, FALSE);
        break;
    case FUMBLE_BOOTS:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            HFumbling = EFumbling = 0;
        break;
    case LEVITATION_BOOTS:
        if (!oldprop && !HLevitation && !(BLevitation & FROMOUTSIDE)
            && !gc.context.takeoff.cancelled_don) {
            /* lava_effects() sets in_lava_effects and calls Boots_off()
               so hero is already in midst of floating down */
            if (!iflags.in_lava_effects)
                (void) float_down(0L, 0L);
            makeknown(otyp);
        } else {
            float_vs_flight(); /* maybe toggle (BFlying & I_SPECIAL) */
        }
        break;
    case LOW_BOOTS:
    case IRON_SHOES:
    case HIGH_BOOTS:
    case JUMPING_BOOTS:
    case KICKING_BOOTS:
        break;
    default:
        impossible(unknown_type, c_boots, otyp);
    }
    gc.context.takeoff.cancelled_don = FALSE;
    return 0;
}

staticfn int
Cloak_on(void)
{
    long oldprop =
        u.uprops[objects[player_armor_cloak->otyp].oc_oprop].extrinsic & ~WORN_CLOAK;

    switch (player_armor_cloak->otyp) {
    case ORCISH_CLOAK:
    case DWARVISH_CLOAK:
    case CLOAK_OF_MAGIC_RESISTANCE:
    case ROBE:
    case LEATHER_CLOAK:
        break;
    case CLOAK_OF_PROTECTION:
        makeknown(player_armor_cloak->otyp);
        break;
    case ELVEN_CLOAK:
        toggle_stealth(player_armor_cloak, oldprop, TRUE);
        break;
    case CLOAK_OF_DISPLACEMENT:
        toggle_displacement(player_armor_cloak, oldprop, TRUE);
        break;
    case MUMMY_WRAPPING:
        /* Note: it's already being worn, so we have to cheat here. */
        if ((HInvis || EInvis) && !Blind) {
            newsym(u.ux, u.uy);
            You("can %s!", See_invisible ? "no longer see through yourself"
                                         : see_yourself);
        }
        break;
    case CLOAK_OF_INVISIBILITY:
        /* since cloak of invisibility was worn, we know mummy wrapping
           wasn't, so no need to check `oldprop' against blocked */
        if (!oldprop && !HInvis && !Blind) {
            makeknown(player_armor_cloak->otyp);
            newsym(u.ux, u.uy);
            pline("Suddenly you can%s yourself.",
                  See_invisible ? " see through" : "not see");
        }
        break;
    case OILSKIN_CLOAK:
        pline("%s very tightly.", Tobjnam(player_armor_cloak, "fit"));
        break;
    /* Alchemy smock gives poison _and_ acid resistance */
    case ALCHEMY_SMOCK:
        EAcid_resistance |= WORN_CLOAK;
        break;
    default:
        impossible(unknown_type, c_cloak, player_armor_cloak->otyp);
    }
    if (player_armor_cloak && !player_armor_cloak->known) { /* no known instance of !uarmc here */
        player_armor_cloak->known = 1; /* cloak's +/- evident because of status line AC */
        update_inventory();
    }
    return 0;
}

int
Cloak_off(void)
{
    struct obj *otmp = player_armor_cloak;
    int otyp = otmp->otyp;
    long oldprop = u.uprops[objects[otyp].oc_oprop].extrinsic & ~WORN_CLOAK;

    gc.context.takeoff.mask &= ~WEARING_ARMOR_CLOAK;
    /* For mummy wrapping, taking it off first resets `Invisible'. */
    setworn((struct obj *) 0, WEARING_ARMOR_CLOAK);
    switch (otyp) {
    case ORCISH_CLOAK:
    case DWARVISH_CLOAK:
    case CLOAK_OF_PROTECTION:
    case CLOAK_OF_MAGIC_RESISTANCE:
    case OILSKIN_CLOAK:
    case ROBE:
    case LEATHER_CLOAK:
        break;
    case ELVEN_CLOAK:
        toggle_stealth(otmp, oldprop, FALSE);
        break;
    case CLOAK_OF_DISPLACEMENT:
        toggle_displacement(otmp, oldprop, FALSE);
        break;
    case MUMMY_WRAPPING:
        if (Invis && !Blind) {
            newsym(u.ux, u.uy);
            You("can %s.", See_invisible ? "see through yourself"
                                         : "no longer see yourself");
        }
        break;
    case CLOAK_OF_INVISIBILITY:
        if (!oldprop && !HInvis && !Blind) {
            makeknown(CLOAK_OF_INVISIBILITY);
            newsym(u.ux, u.uy);
            pline("Suddenly you can %s.",
                  See_invisible ? "no longer see through yourself"
                                : see_yourself);
        }
        break;
    /* Alchemy smock gives poison _and_ acid resistance */
    case ALCHEMY_SMOCK:
        EAcid_resistance &= ~WORN_CLOAK;
        break;
    default:
        impossible(unknown_type, c_cloak, otyp);
    }
    return 0;
}

staticfn int
Helmet_on(void)
{
    switch (player_armor_hat->otyp) {
    case FEDORA:
    case HELMET:
    case DENTED_POT:
    case ELVEN_LEATHER_HELM:
    case DWARVISH_IRON_HELM:
    case ORCISH_HELM:
    case HELM_OF_TELEPATHY:
        break;
    case HELM_OF_CAUTION:
        see_monsters();
        break;
    case HELM_OF_BRILLIANCE:
        adj_abon(player_armor_hat, player_armor_hat->spe);
        break;
    case CORNUTHAUM:
        /* people think marked wizards know what they're talking about,
           but it takes trained arrogance to pull it off, and the actual
           enchantment of the hat is irrelevant */
        ATTRIBUTE_BONUS(ATTRIBUTE_CHARISMA) += (Role_if(PM_WIZARD) ? 1 : -1);
        disp.bottom_line = TRUE;
        makeknown(player_armor_hat->otyp);
        break;
    case HELM_OF_OPPOSITE_ALIGNMENT:
        player_armor_hat->known = 1; /* do this here because uarmh could get cleared */
        /* changing alignment can toggle off active artifact properties,
           including levitation; uarmh could get dropped or destroyed here
           by hero falling onto a polymorph trap or into water (emergency
           disrobe) or maybe lava (probably not, helm isn't 'organic') */
        uchangealign((u.alignment.type != A_NEUTRAL)
                         ? -u.alignment.type
                         : (player_armor_hat->o_id % 2) ? A_CHAOTIC : A_LAWFUL,
                     A_CG_HELM_ON);
        /* makeknown(HELM_OF_OPPOSITE_ALIGNMENT); -- below, after Tobjnam() */
    /*FALLTHRU*/
    case DUNCE_CAP:
        if (player_armor_hat && !player_armor_hat->cursed) {
            if (Blind)
                pline("%s for a moment.", Tobjnam(player_armor_hat, "vibrate"));
            else
                pline("%s %s for a moment.", Tobjnam(player_armor_hat, "glow"),
                      hcolor(NH_BLACK));
            curse(player_armor_hat);
            /* curse() doesn't touch bknown so doesn't update persistent
               inventory; do so now [set_bknown() calls update_inventory()] */
            if (Blind)
                set_bknown(player_armor_hat, 0); /* lose bknown if previously set */
            else if (Role_if(PM_CLERIC))
                set_bknown(player_armor_hat, 1); /* (bknown should already be set) */
            else if (player_armor_hat->bknown)
                update_inventory(); /* keep bknown as-is; display the curse */
        }
        disp.bottom_line = TRUE; /* reveal new alignment or INT & WIS */
        if (Hallucination) {
            pline("My brain hurts!"); /* Monty Python's Flying Circus */
        } else if (player_armor_hat && player_armor_hat->otyp == DUNCE_CAP) {
            You_feel("%s.", /* track INT change; ignore WIS */
                     ATTRIBUTE_CURRENT(ATTRIBUTE_INTELLIGENCE)
                             <= (ATTRIBUTE_BASE(ATTRIBUTE_INTELLIGENCE) + ATTRIBUTE_BONUS(ATTRIBUTE_INTELLIGENCE) + ATTRIBUTE_TEMPORARY(ATTRIBUTE_INTELLIGENCE))
                         ? "like sitting in a corner"
                         : "giddy");
        } else {
            /* [message formerly given here moved to uchangealign()] */
            makeknown(HELM_OF_OPPOSITE_ALIGNMENT);
        }
        break;
    default:
        impossible(unknown_type, c_helmet, player_armor_hat->otyp);
    }
    /* uarmh could be Null due to uchangealign() */
    if (player_armor_hat && !player_armor_hat->known) {
        player_armor_hat->known = 1; /* helmet's +/- evident because of status line AC */
        update_inventory();
    }
    return 0;
}

int
Helmet_off(void)
{
    gc.context.takeoff.mask &= ~WEARING_ARMOR_HELMET;

    switch (player_armor_hat->otyp) {
    case FEDORA:
    case HELMET:
    case DENTED_POT:
    case ELVEN_LEATHER_HELM:
    case DWARVISH_IRON_HELM:
    case ORCISH_HELM:
        break;
    case DUNCE_CAP:
        disp.bottom_line = TRUE;
        break;
    case CORNUTHAUM:
        if (!gc.context.takeoff.cancelled_don) {
            ATTRIBUTE_BONUS(ATTRIBUTE_CHARISMA) += (Role_if(PM_WIZARD) ? -1 : 1);
            disp.bottom_line = TRUE;
        }
        break;
    case HELM_OF_TELEPATHY:
    case HELM_OF_CAUTION:
        /* need to update ability before calling see_monsters() */
        setworn((struct obj *) 0, WEARING_ARMOR_HELMET);
        see_monsters();
        return 0;
    case HELM_OF_BRILLIANCE:
        if (!gc.context.takeoff.cancelled_don)
            adj_abon(player_armor_hat, -player_armor_hat->spe);
        break;
    case HELM_OF_OPPOSITE_ALIGNMENT:
        /* changing alignment can toggle off active artifact
           properties, including levitation; uarmh could get
           dropped or destroyed here */
        uchangealign(u.ualignbase[A_CURRENT], A_CG_HELM_OFF);
        break;
    default:
        impossible(unknown_type, c_helmet, player_armor_hat->otyp);
    }
    setworn((struct obj *) 0, WEARING_ARMOR_HELMET);
    gc.context.takeoff.cancelled_don = FALSE;
    return 0;
}

/* hard helms provide better protection against falling rocks */
boolean
hard_helmet(struct obj *obj)
{
    if (!obj || !is_helmet(obj))
        return FALSE;
    return (is_metallic(obj) || is_crackable(obj)) ? TRUE : FALSE;
}

staticfn int
Gloves_on(void)
{
    long oldprop =
        u.uprops[objects[player_armor_gloves->otyp].oc_oprop].extrinsic & ~WORN_GLOVES;

    switch (player_armor_gloves->otyp) {
    case LEATHER_GLOVES:
        break;
    case GAUNTLETS_OF_FUMBLING:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            incr_itimeout(&HFumbling, random(20));
        break;
    case GAUNTLETS_OF_POWER:
        makeknown(player_armor_gloves->otyp);
        disp.bottom_line = TRUE; /* taken care of in attrib.c */
        break;
    case GAUNTLETS_OF_DEXTERITY:
        adj_abon(player_armor_gloves, player_armor_gloves->spe);
        break;
    default:
        impossible(unknown_type, c_gloves, player_armor_gloves->otyp);
    }
    if (!player_armor_gloves->known) {
        player_armor_gloves->known = 1; /* gloves' +/- evident because of status line AC */
        update_inventory();
    }
    return 0;
}

/* check for wielding cockatrice corpse after taking off gloves or yellow
   dragon scales/mail or having temporary stoning resistance time out */
void
wielding_corpse(
    struct obj *obj,   /* uwep, potentially a wielded cockatrice corpse */
    struct obj *how,   /* gloves or dragon armor or Null (resist timeout) */
    boolean voluntary) /* True: taking protective armor off on purpose */
{
    if (!obj || obj->otyp != CORPSE || player_armor_gloves)
        return;
    /* note: can't dual-wield with non-weapons/weapon-tools so u.twoweap
       will always be false if uswapwep happens to be a corpse */
    if (obj != player_weapon && (obj != player_secondary_weapon || !u.using_two_weapons))
        return;

    if (touch_petrifies(&mons[obj->corpsenm]) && !Stone_resistance) {
        char kbuf[BUFSZ], hbuf[BUFSZ];

        You("%s %s in your bare %s.",
            (how && is_gloves(how)) ? "now wield" : "are wielding",
            corpse_xname(obj, (const char *) 0, CXN_ARTICLE),
            makeplural(body_part(HAND)));
        /* "removing" ought to be "taking off" but that makes the
           tombstone text more likely to be truncated */
        if (how)
            Sprintf(hbuf, "%s %s", voluntary ? "removing" : "losing",
                    is_gloves(how) ? gloves_simple_name(how)
                    : strsubst(simpleonames(how), "set of ", ""));
        else
            Strcpy(hbuf, "resistance timing out");
        Snprintf(kbuf, sizeof kbuf, "%s while wielding %s",
                 hbuf, killer_xname(obj));
        instapetrify(kbuf);
        /* life-saved or got poly'd into a stone golem; can't continue
           wielding cockatrice corpse unless have now become resistant */
        if (!Stone_resistance)
            remove_worn_item(obj, FALSE);
    }
}

int
Gloves_off(void)
{
    struct obj *gloves = player_armor_gloves; /* needed after uarmg has been set to Null */
    long oldprop =
        u.uprops[objects[player_armor_gloves->otyp].oc_oprop].extrinsic & ~WORN_GLOVES;
    boolean on_purpose = !gc.context.mon_moving && !player_armor_gloves->in_use;

    gc.context.takeoff.mask &= ~WEARING_ARMOR_GLOVES;

    switch (player_armor_gloves->otyp) {
    case LEATHER_GLOVES:
        break;
    case GAUNTLETS_OF_FUMBLING:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            HFumbling = EFumbling = 0;
        break;
    case GAUNTLETS_OF_POWER:
        makeknown(player_armor_gloves->otyp);
        disp.bottom_line = TRUE; /* taken care of in attrib.c */
        break;
    case GAUNTLETS_OF_DEXTERITY:
        if (!gc.context.takeoff.cancelled_don)
            adj_abon(player_armor_gloves, -player_armor_gloves->spe);
        break;
    default:
        impossible(unknown_type, c_gloves, player_armor_gloves->otyp);
    }
    setworn((struct obj *) 0, WEARING_ARMOR_GLOVES);
    gc.context.takeoff.cancelled_don = FALSE;
    (void) encumbered_message(); /* immediate feedback for GoP */

    /* usually can't remove gloves when they're slippery but it can
       be done by having them fall off (polymorph), stolen, or
       destroyed (scroll, overenchantment, monster spell); if that
       happens, 'cure' slippery fingers so that it doesn't transfer
       from gloves to bare hands */
    if (Glib)
        make_glib(0); /* for update_inventory() */

    /* prevent wielding cockatrice when not wearing gloves */
    if (player_weapon && player_weapon->otyp == CORPSE)
        wielding_corpse(player_weapon, gloves, on_purpose);
    /* KMH -- ...or your secondary weapon when you're wielding it
       [This case can't actually happen; twoweapon mode won't engage
       if a corpse has been set up as either the primary or alternate
       weapon.  If it could happen and /both/ uwep and uswapwep could
       be cockatrice corpses, life-saving for the first would need to
       prevent the second from being fatal since conceptually they'd
       be being touched simultaneously.] */
    if (u.using_two_weapons && player_secondary_weapon && player_secondary_weapon->otyp == CORPSE)
        wielding_corpse(player_secondary_weapon, gloves, on_purpose);

    if (condtests[bl_bareh].enabled)
        disp.bottom_line = TRUE;

    return 0;
}

staticfn int
Shield_on(void)
{
    /* no shield currently requires special handling when put on, but we
       keep this uncommented in case somebody adds a new one which does
       [reflection is handled by setting u.uprops[REFLECTION].extrinsic
       in setworn() called by armor_or_accessory_on() before Shield_on()] */
    switch (player_armor_shield->otyp) {
    case SMALL_SHIELD:
    case ELVEN_SHIELD:
    case URUK_HAI_SHIELD:
    case ORCISH_SHIELD:
    case DWARVISH_ROUNDSHIELD:
    case LARGE_SHIELD:
    case SHIELD_OF_REFLECTION:
        break;
    default:
        impossible(unknown_type, c_shield, player_armor_shield->otyp);
    }
    if (!player_armor_shield->known) {
        player_armor_shield->known = 1; /* shield's +/- evident because of status line AC */
        update_inventory();
    }
    return 0;
}

int
Shield_off(void)
{
    gc.context.takeoff.mask &= ~WEARING_ARMOR_SHIELD;

    /* no shield currently requires special handling when taken off, but we
       keep this uncommented in case somebody adds a new one which does */
    switch (player_armor_shield->otyp) {
    case SMALL_SHIELD:
    case ELVEN_SHIELD:
    case URUK_HAI_SHIELD:
    case ORCISH_SHIELD:
    case DWARVISH_ROUNDSHIELD:
    case LARGE_SHIELD:
    case SHIELD_OF_REFLECTION:
        break;
    default:
        impossible(unknown_type, c_shield, player_armor_shield->otyp);
    }

    setworn((struct obj *) 0, WEARING_ARMOR_SHIELD);
    return 0;
}

staticfn int
Shirt_on(void)
{
    /* no shirt currently requires special handling when put on, but we
       keep this uncommented in case somebody adds a new one which does */
    switch (player_armor_undershirt->otyp) {
    case HAWAIIAN_SHIRT:
    case T_SHIRT:
        break;
    default:
        impossible(unknown_type, c_shirt, player_armor_undershirt->otyp);
    }
    if (!player_armor_undershirt->known) {
        player_armor_undershirt->known = 1; /* shirt's +/- evident because of status line AC */
        update_inventory();
    }
    return 0;
}

int
Shirt_off(void)
{
    gc.context.takeoff.mask &= ~WEARING_ARMOR_UNDERSHIRT;

    /* no shirt currently requires special handling when taken off, but we
       keep this uncommented in case somebody adds a new one which does */
    switch (player_armor_undershirt->otyp) {
    case HAWAIIAN_SHIRT:
    case T_SHIRT:
        break;
    default:
        impossible(unknown_type, c_shirt, player_armor_undershirt->otyp);
    }

    setworn((struct obj *) 0, WEARING_ARMOR_UNDERSHIRT);
    return 0;
}

/* handle extra abilities for hero wearing dragon scale armor */
staticfn void
dragon_armor_handling(
    struct obj *otmp,   /* armor being put on or taken off */
    boolean puton,      /* True: on, False: off */
    boolean on_purpose) /* voluntary removal; not applicable for putting on */
{
    if (!otmp)
        return;

    switch (otmp->otyp) {
        /* grey: no extra effect */
        /* silver: no extra effect */
    case BLACK_DRAGON_SCALES:
    case BLACK_DRAGON_SCALE_MAIL:
        if (puton) {
            EDrain_resistance |= WEARING_ARMOR_BODY;
        } else {
            EDrain_resistance &= ~WEARING_ARMOR_BODY;
        }
        break;
    case BLUE_DRAGON_SCALES:
    case BLUE_DRAGON_SCALE_MAIL:
        if (puton) {
            if (!Very_fast)
                You("speed up%s.", Fast ? " a bit more" : "");
            EFast |= WEARING_ARMOR_BODY;
        } else {
            EFast &= ~WEARING_ARMOR_BODY;
            if (!Very_fast && !gc.context.takeoff.cancelled_don)
                You("slow down.");
        }
        break;
    case GREEN_DRAGON_SCALES:
    case GREEN_DRAGON_SCALE_MAIL:
        if (puton) {
            ESick_resistance |= WEARING_ARMOR_BODY;
        } else {
            ESick_resistance &= ~WEARING_ARMOR_BODY;
        }
        break;
    case RED_DRAGON_SCALES:
    case RED_DRAGON_SCALE_MAIL:
        if (puton) {
            EInfravision |= WEARING_ARMOR_BODY;
        } else {
            EInfravision &= ~WEARING_ARMOR_BODY;
        }
        see_monsters();
        break;
    case GOLD_DRAGON_SCALES:
    case GOLD_DRAGON_SCALE_MAIL:
        (void) make_hallucinated((long) !puton,
                                 gp.program_state.restoring ? FALSE : TRUE,
                                 WEARING_ARMOR_BODY);
        break;
    case ORANGE_DRAGON_SCALES:
    case ORANGE_DRAGON_SCALE_MAIL:
        if (puton) {
            Free_action |= WEARING_ARMOR_BODY;
        } else {
            Free_action &= ~WEARING_ARMOR_BODY;
        }
        break;
    case YELLOW_DRAGON_SCALES:
    case YELLOW_DRAGON_SCALE_MAIL:
        if (puton) {
            EStone_resistance |= WEARING_ARMOR_BODY;
        } else {
            EStone_resistance &= ~WEARING_ARMOR_BODY;

            /* prevent wielding cockatrice after losing stoning resistance
               when not wearing gloves; the uswapwep case is always a no-op */
            wielding_corpse(player_weapon, otmp, on_purpose);
            wielding_corpse(player_secondary_weapon, otmp, on_purpose);
        }
        break;
    case WHITE_DRAGON_SCALES:
    case WHITE_DRAGON_SCALE_MAIL:
        if (puton) {
            ESlow_digestion |= WEARING_ARMOR_BODY;
        } else {
            ESlow_digestion &= ~WEARING_ARMOR_BODY;
        }
        break;
    default:
        break;
    }
}

staticfn int
Armor_on(void)
{
    if (!player_armor) /* no known instances of !uarm here but play it safe */
        return 0;
    if (!player_armor->known) {
        player_armor->known = 1; /* suit's +/- evident because of status line AC */
        update_inventory();
    }
    dragon_armor_handling(player_armor, TRUE, TRUE);
    /* gold DSM requires extra handling since it emits light when worn;
       do that after the special armor handling */
    if (artifact_light(player_armor) && !player_armor->lamplit) {
        begin_burn(player_armor, FALSE);
        if (!Blind)
            pline("%s %s to shine %s!",
                  Yname2(player_armor), otense(player_armor, "begin"),
                  arti_light_description(player_armor));
    }
    return 0;
}

int
Armor_off(void)
{
    struct obj *otmp = player_armor;
    boolean was_arti_light = otmp && otmp->lamplit && artifact_light(otmp);

    gc.context.takeoff.mask &= ~WEARING_ARMOR_BODY;
    setworn((struct obj *) 0, WEARING_ARMOR_BODY);
    gc.context.takeoff.cancelled_don = FALSE;

    /* taking off yellow dragon scales/mail might be fatal; arti_light
       comes from gold dragon scales/mail so they don't overlap, but
       conceptually the non-fatal change should be done before the
       potentially fatal change in case the latter results in bones */
    if (was_arti_light && !artifact_light(otmp)) {
        end_burn(otmp, FALSE);
        if (!Blind)
            pline("%s shining.", Tobjnam(otmp, "stop"));
    }
    dragon_armor_handling(otmp, FALSE, TRUE);

    return 0;
}

/* The gone functions differ from the off functions in that if you die from
 * taking it off and have life saving, you still die.  [Obsolete reference
 * to lack of fire resistance being fatal in hell (nethack 3.0) and life
 * saving putting a removed item back on to prevent that from immediately
 * repeating.]
 */
int
Armor_gone(void)
{
    struct obj *otmp = player_armor;
    boolean was_arti_light = otmp && otmp->lamplit && artifact_light(otmp);

    gc.context.takeoff.mask &= ~WEARING_ARMOR_BODY;
    setnotworn(player_armor);
    gc.context.takeoff.cancelled_don = FALSE;

    /* losing yellow dragon scales/mail might be fatal; arti_light
       comes from gold dragon scales/mail so they don't overlap, but
       conceptually the non-fatal change should be done before the
       potentially fatal change in case the latter results in bones */
    if (was_arti_light && !artifact_light(otmp)) {
        end_burn(otmp, FALSE);
        if (!Blind)
            pline("%s shining.", Tobjnam(otmp, "stop"));
    }
    dragon_armor_handling(otmp, FALSE, FALSE);

    return 0;
}

staticfn void
Amulet_on(void)
{
    /* make sure amulet isn't wielded; can't use remove_worn_item()
       here because it has already been set worn in amulet slot */
    if (player_amulet == player_weapon)
        setuwep((struct obj *) 0);
    else if (player_amulet == player_secondary_weapon)
        setuswapwep((struct obj *) 0);
    else if (player_amulet == player_quiver)
        setuqwep((struct obj *) 0);

    switch (player_amulet->otyp) {
    case AMULET_OF_ESP:
    case AMULET_OF_LIFE_SAVING:
    case AMULET_VERSUS_POISON:
    case AMULET_OF_REFLECTION:
    case AMULET_OF_MAGICAL_BREATHING:
    case FAKE_AMULET_OF_YENDOR:
        break;
    case AMULET_OF_UNCHANGING:
        if (Slimed)
            make_slimed(0L, (char *) 0);
        break;
    case AMULET_OF_CHANGE: {
        int new_sex, orig_sex = poly_gender();

        if (Unchanging)
            break;
        change_sex();
        new_sex = poly_gender();
        /* Don't use same message as polymorph */
        if (new_sex != orig_sex) {
            makeknown(AMULET_OF_CHANGE);
            You("are suddenly very %s!",
                flags.female ? "feminine" : "masculine");
            disp.bottom_line = TRUE;
            newsym(u.ux, u.uy); /* glyphmon flag and tile may have gone
                                 * from male to female or vice versa */
        } else {
            /* already polymorphed into single-gender monster; only
               changed the character's base sex */
            You("don't feel like yourself.");
        }
        livelog_newform(FALSE, orig_sex, new_sex);
        pline_The("amulet disintegrates!");
        if (orig_sex == poly_gender() && player_amulet->dknown)
            trycall(player_amulet);
        useup(player_amulet);
        break;
    }
    case AMULET_OF_STRANGULATION:
        if (can_be_strangled(&gy.youmonst)) {
            makeknown(AMULET_OF_STRANGULATION);
            Strangled = 6L;
            disp.bottom_line = TRUE;
            pline("It constricts your throat!");
        }
        break;
    case AMULET_OF_RESTFUL_SLEEP: {
        long newnap = (long) random(100), oldnap = (HSleepy & TIMEOUT);

        /* avoid clobbering FROMOUTSIDE bit, which might have
           gotten set by previously eating one of these amulets */
        if (newnap < oldnap || oldnap == 0L)
            HSleepy = (HSleepy & ~TIMEOUT) | newnap;
        break;
    }
    case AMULET_OF_FLYING:
        /* setworn() has already set extrinsic flying */
        float_vs_flight(); /* block flying if levitating */
        if (Flying) {
            boolean already_flying;

            /* to determine whether this flight is new we have to muck
               about in the Flying intrinsic (actually extrinsic) */
            EFlying &= ~WEARING_AMULET;
            already_flying = !!Flying;
            EFlying |= WEARING_AMULET;

            if (!already_flying) {
                makeknown(AMULET_OF_FLYING);
                disp.bottom_line = TRUE; /* status: 'Fly' On */
                You("are now in flight.");
            }
        }
        break;
    case AMULET_OF_GUARDING:
        makeknown(AMULET_OF_GUARDING);
        find_ac();
        break;
    case AMULET_OF_YENDOR:
        break;
    }
}

void
Amulet_off(void)
{
    gc.context.takeoff.mask &= ~WEARING_AMULET;

    switch (player_amulet->otyp) {
    case AMULET_OF_ESP:
        /* need to update ability before calling see_monsters() */
        setworn((struct obj *) 0, WEARING_AMULET);
        see_monsters();
        return;
    case AMULET_OF_LIFE_SAVING:
    case AMULET_VERSUS_POISON:
    case AMULET_OF_REFLECTION:
    case AMULET_OF_CHANGE:
    case AMULET_OF_UNCHANGING:
    case FAKE_AMULET_OF_YENDOR:
        break;
    case AMULET_OF_MAGICAL_BREATHING:
        if (Underwater) {
            /* HMagical_breathing must be set off
                before calling drown() */
            setworn((struct obj *) 0, WEARING_AMULET);
            if (!cant_drown(gy.youmonst.data) && !Swimming) {
                You("suddenly inhale an unhealthy amount of %s!",
                    hliquid("water"));
                (void) drown();
            }
            return;
        }
        break;
    case AMULET_OF_STRANGULATION:
        if (Strangled) {
            Strangled = 0L;
            disp.bottom_line = TRUE;
            if (Breathless)
                Your("%s is no longer constricted!", body_part(NECK));
            else
                You("can breathe more easily!");
        }
        break;
    case AMULET_OF_RESTFUL_SLEEP:
        setworn((struct obj *) 0, WEARING_AMULET);
        /* HSleepy = 0L; -- avoid clobbering FROMOUTSIDE bit */
        if (!ESleepy && !(HSleepy & ~TIMEOUT))
            HSleepy &= ~TIMEOUT; /* clear timeout bits */
        return;
    case AMULET_OF_FLYING: {
        boolean was_flying = !!Flying;

        /* remove amulet 'early' to determine whether Flying changes */
        setworn((struct obj *) 0, WEARING_AMULET);
        float_vs_flight(); /* probably not needed here */
        if (was_flying && !Flying) {
            makeknown(AMULET_OF_FLYING);
            disp.bottom_line = TRUE; /* status: 'Fly' Off */
            You("%s.", (is_pool_or_lava(u.ux, u.uy)
                        || Is_waterlevel(&u.uz) || Is_airlevel(&u.uz))
                          ? "stop flying"
                          : "land");
            spoteffects(TRUE);
        }
        break;
    }
    case AMULET_OF_GUARDING:
        find_ac();
        break;
    case AMULET_OF_YENDOR:
        break;
    }
    setworn((struct obj *) 0, WEARING_AMULET);
    return;
}

/* handle ring discovery; comparable to learnwand() */
staticfn void
learnring(struct obj *ring, boolean observed)
{
    int ringtype = ring->otyp;

    /* if effect was observable then we usually discover the type */
    if (observed) {
        /* if we already know the ring type which accomplishes this
           effect (assumes there is at most one type for each effect),
           mark this ring as having been seen (no need for makeknown);
           otherwise if we have seen this ring, discover its type */
        if (objects[ringtype].oc_name_known)
            ring->dknown = 1;
        else if (ring->dknown)
            makeknown(ringtype);
#if 0 /* see learnwand() */
        else
            ring->eknown = 1;
#endif
    }

    /* make enchantment of charged ring known (might be +0) and update
       perm invent window if we've seen this ring and know its type */
    if (ring->dknown && objects[ringtype].oc_name_known) {
        if (objects[ringtype].oc_charged)
            ring->known = 1;
        update_inventory();
    }
}

staticfn void
adjust_attrib(struct obj *obj, int which, int val)
{
    int old_attrib;
    boolean observable;

    old_attrib = ATTRIBUTE_CURRENT(which);
    ATTRIBUTE_BONUS(which) += val;
    observable = (old_attrib != ATTRIBUTE_CURRENT(which));
    /* if didn't change, usually means ring is +0 but might
        be because nonzero couldn't go below min or above max;
        learn +0 enchantment if attribute value is not stuck
        at a limit [and ring has been seen and its type is
        already discovered, both handled by learnring()] */
    if (observable || !extremeattr(which))
        learnring(obj, observable);
    disp.bottom_line = TRUE;
}

void
Ring_on(struct obj *obj)
{
    long oldprop = u.uprops[objects[obj->otyp].oc_oprop].extrinsic;
    boolean observable;

    /* make sure ring isn't wielded; can't use remove_worn_item()
       here because it has already been set worn in a ring slot */
    if (obj == player_weapon)
        setuwep((struct obj *) 0);
    else if (obj == player_secondary_weapon)
        setuswapwep((struct obj *) 0);
    else if (obj == player_quiver)
        setuqwep((struct obj *) 0);

    /* only mask out W_RING when we don't have both
       left and right rings of the same type */
    if ((oldprop & WEARING_RING) != WEARING_RING)
        oldprop &= ~WEARING_RING;

    switch (obj->otyp) {
    case RIN_TELEPORTATION:
    case RIN_REGENERATION:
    case RIN_SEARCHING:
    case RIN_HUNGER:
    case RIN_AGGRAVATE_MONSTER:
    case RIN_POISON_RESISTANCE:
    case RIN_FIRE_RESISTANCE:
    case RIN_COLD_RESISTANCE:
    case RIN_SHOCK_RESISTANCE:
    case RIN_CONFLICT:
    case RIN_TELEPORT_CONTROL:
    case RIN_POLYMORPH:
    case RIN_POLYMORPH_CONTROL:
    case RIN_FREE_ACTION:
    case RIN_SLOW_DIGESTION:
    case RIN_SUSTAIN_ABILITY:
    case MEAT_RING:
        break;
    case RIN_STEALTH:
        toggle_stealth(obj, oldprop, TRUE);
        break;
    case RIN_WARNING:
        see_monsters();
        break;
    case RIN_SEE_INVISIBLE:
        /* can now see invisible monsters */
        set_mimic_blocking(); /* do special mimic handling */
        see_monsters();

        if (Invis && !oldprop && !HSee_invisible && !Blind) {
            newsym(u.ux, u.uy);
            pline("Suddenly you are transparent, but there!");
            learnring(obj, TRUE);
        }
        break;
    case RIN_INVISIBILITY:
        if (!oldprop && !HInvis && !BInvis && !Blind) {
            learnring(obj, TRUE);
            newsym(u.ux, u.uy);
            self_invis_message();
        }
        break;
    case RIN_LEVITATION:
        if (!oldprop && !HLevitation && !(BLevitation & FROMOUTSIDE)) {
            float_up();
            learnring(obj, TRUE);
            if (Levitation)
                spoteffects(FALSE); /* for sinks */
        } else {
            float_vs_flight(); /* maybe toggle (BFlying & I_SPECIAL) */
        }
        break;
    case RIN_GAIN_STRENGTH:
        adjust_attrib(obj, ATTRIBUTE_STRENGTH, obj->spe);
        break;
    case RIN_GAIN_CONSTITUTION:
        adjust_attrib(obj, ATTRIBUTE_CONSTITUTION, obj->spe);
        break;
    case RIN_ADORNMENT:
        adjust_attrib(obj, ATTRIBUTE_CHARISMA, obj->spe);
        break;
    case RIN_INCREASE_ACCURACY: /* KMH */
        u.hit_increment += obj->spe;
        break;
    case RIN_INCREASE_DAMAGE:
        u.damage_increment += obj->spe;
        break;
    case RIN_PROTECTION_FROM_SHAPE_CHAN:
        rescham();
        break;
    case RIN_PROTECTION:
        /* usually learn enchantment and discover type;
           won't happen if ring is unseen or if it's +0
           and the type hasn't been discovered yet */
        observable = (obj->spe != 0);
        learnring(obj, observable);
        if (obj->spe)
            find_ac(); /* updates botl */
        break;
    }
}

staticfn void
Ring_off_or_gone(struct obj *obj, boolean gone)
{
    long mask = (obj->owornmask & WEARING_RING);
    boolean observable;

    gc.context.takeoff.mask &= ~mask;
    if (!(u.uprops[objects[obj->otyp].oc_oprop].extrinsic & mask))
        impossible("Strange... I didn't know you had that ring.");
    if (gone)
        setnotworn(obj);
    else
        setworn((struct obj *) 0, obj->owornmask);

    switch (obj->otyp) {
    case RIN_TELEPORTATION:
    case RIN_REGENERATION:
    case RIN_SEARCHING:
    case RIN_HUNGER:
    case RIN_AGGRAVATE_MONSTER:
    case RIN_POISON_RESISTANCE:
    case RIN_FIRE_RESISTANCE:
    case RIN_COLD_RESISTANCE:
    case RIN_SHOCK_RESISTANCE:
    case RIN_CONFLICT:
    case RIN_TELEPORT_CONTROL:
    case RIN_POLYMORPH:
    case RIN_POLYMORPH_CONTROL:
    case RIN_FREE_ACTION:
    case RIN_SLOW_DIGESTION:
    case RIN_SUSTAIN_ABILITY:
    case MEAT_RING:
        break;
    case RIN_STEALTH:
        toggle_stealth(obj, (EStealth & ~mask), FALSE);
        break;
    case RIN_WARNING:
        see_monsters();
        break;
    case RIN_SEE_INVISIBLE:
        /* Make invisible monsters go away */
        if (!See_invisible) {
            set_mimic_blocking(); /* do special mimic handling */
            see_monsters();
        }

        if (Invisible && !Blind) {
            newsym(u.ux, u.uy);
            pline("Suddenly you cannot see yourself.");
            learnring(obj, TRUE);
        }
        break;
    case RIN_INVISIBILITY:
        if (!Invis && !BInvis && !Blind) {
            newsym(u.ux, u.uy);
            Your("body seems to unfade%s.",
                 See_invisible ? " completely" : "..");
            learnring(obj, TRUE);
        }
        break;
    case RIN_LEVITATION:
        if (!(BLevitation & FROMOUTSIDE)) {
            (void) float_down(0L, 0L);
            if (!Levitation)
                learnring(obj, TRUE);
        } else {
            float_vs_flight(); /* maybe toggle (BFlying & I_SPECIAL) */
        }
        break;
    case RIN_GAIN_STRENGTH:
        adjust_attrib(obj, ATTRIBUTE_STRENGTH, -obj->spe);
        break;
    case RIN_GAIN_CONSTITUTION:
        adjust_attrib(obj, ATTRIBUTE_CONSTITUTION, -obj->spe);
        break;
    case RIN_ADORNMENT:
        adjust_attrib(obj, ATTRIBUTE_CHARISMA, -obj->spe);
        break;
    case RIN_INCREASE_ACCURACY: /* KMH */
        u.hit_increment -= obj->spe;
        break;
    case RIN_INCREASE_DAMAGE:
        u.damage_increment -= obj->spe;
        break;
    case RIN_PROTECTION:
        /* might have been put on while blind and we can now see
           or perhaps been forgotten due to amnesia */
        observable = (obj->spe != 0);
        learnring(obj, observable);
        if (obj->spe)
            find_ac(); /* updates botl */
        break;
    case RIN_PROTECTION_FROM_SHAPE_CHAN:
        /* If you're no longer protected, let the chameleons
         * change shape again -dgk
         */
        restartcham();
        break;
    }
}

void
Ring_off(struct obj *obj)
{
    Ring_off_or_gone(obj, FALSE);
}

void
Ring_gone(struct obj *obj)
{
    Ring_off_or_gone(obj, TRUE);
}

void
Blindf_on(struct obj *otmp)
{
    boolean already_blind = Blind, changed = FALSE;

    /* blindfold might be wielded; release it for wearing */
    if (otmp->owornmask & WEARING_WEAPONS)
        remove_worn_item(otmp, FALSE);
    setworn(otmp, WEARING_TOOL);
    on_msg(otmp);

    if (Blind && !already_blind) {
        changed = TRUE;
        if (flags.verbose)
            You_cant("see any more.");
        /* set ball&chain variables before the hero goes blind */
        if (Punished)
            set_bc(0);
    } else if (already_blind && !Blind) {
        changed = TRUE;
        /* "You are now wearing the Eyes of the Overworld." */
        if (u.uroleplay.blind) {
            /* this can only happen by putting on the Eyes of the Overworld;
               that shouldn't actually produce a permanent cure, but we
               can't let the "blind from birth" conduct remain intact */
            pline("For the first time in your life, you can see!");
            u.uroleplay.blind = FALSE;
        } else
            You("can see!");
    }
    if (changed) {
        toggle_blindness(); /* potion.c */
    }
}

void
Blindf_off(struct obj *otmp)
{
    boolean was_blind = Blind, changed = FALSE,
            nooffmsg = !otmp;

    if (!otmp)
        otmp = player_blindfold;
    if (!otmp) {
        impossible("Blindf_off without eyewear?");
        return;
    }
    gc.context.takeoff.mask &= ~WEARING_TOOL;
    setworn((struct obj *) 0, otmp->owornmask);
    if (!nooffmsg)
        off_msg(otmp);

    if (Blind) {
        if (was_blind) {
            /* "still cannot see" makes no sense when removing lenses
               since they can't have been the cause of your blindness */
            if (otmp->otyp != LENSES)
                You("still cannot see.");
        } else {
            changed = TRUE; /* !was_blind */
            /* "You were wearing the Eyes of the Overworld." */
            You_cant("see anything now!");
            /* set ball&chain variables before the hero goes blind */
            if (Punished)
                set_bc(0);
        }
    } else if (was_blind) {
        if (!gulp_blnd_check()) {
            changed = TRUE; /* !Blind */
            You("can see again.");
        }
    }
    if (changed) {
        toggle_blindness(); /* potion.c */
    }
}

/* called in moveloop()'s prologue to set side-effects of worn start-up items;
   also used by poly_obj() when a worn item gets transformed */
void
set_wear(
    struct obj *obj) /* if Null, do all worn items; otherwise just obj */
{
    gi.initial_don = !obj;

    if (!obj ? player_blindfold != 0 : (obj == player_blindfold))
        (void) Blindf_on(player_blindfold);
    if (!obj ? player_finger_right != 0 : (obj == player_finger_right))
        (void) Ring_on(player_finger_right);
    if (!obj ? player_finger_left != 0 : (obj == player_finger_left))
        (void) Ring_on(player_finger_left);
    if (!obj ? player_amulet != 0 : (obj == player_amulet))
        (void) Amulet_on();

    if (!obj ? player_armor_undershirt != 0 : (obj == player_armor_undershirt))
        (void) Shirt_on();
    if (!obj ? player_armor != 0 : (obj == player_armor))
        (void) Armor_on();
    if (!obj ? player_armor_cloak != 0 : (obj == player_armor_cloak))
        (void) Cloak_on();
    if (!obj ? player_armor_footwear != 0 : (obj == player_armor_footwear))
        (void) Boots_on();
    if (!obj ? player_armor_gloves != 0 : (obj == player_armor_gloves))
        (void) Gloves_on();
    if (!obj ? player_armor_hat != 0 : (obj == player_armor_hat))
        (void) Helmet_on();
    if (!obj ? player_armor_shield != 0 : (obj == player_armor_shield))
        (void) Shield_on();

    gi.initial_don = FALSE;
}

/* check whether the target object is currently being put on (or taken off--
   also checks for doffing--[why?]) */
boolean
donning(struct obj *otmp)
{
    boolean result = FALSE;

    /* 'W' (or 'P' used for armor) sets ga.afternmv */
    if (doffing(otmp))
        result = TRUE;
    else if (otmp == player_armor)
        result = (ga.afternmv == Armor_on);
    else if (otmp == player_armor_undershirt)
        result = (ga.afternmv == Shirt_on);
    else if (otmp == player_armor_cloak)
        result = (ga.afternmv == Cloak_on);
    else if (otmp == player_armor_footwear)
        result = (ga.afternmv == Boots_on);
    else if (otmp == player_armor_hat)
        result = (ga.afternmv == Helmet_on);
    else if (otmp == player_armor_gloves)
        result = (ga.afternmv == Gloves_on);
    else if (otmp == player_armor_shield)
        result = (ga.afternmv == Shield_on);

    return result;
}

/* check whether the target object is currently being taken off,
   so that stop_donning() and steal() can vary messages and doname()
   can vary "(being worn)" suffix */
boolean
doffing(struct obj *otmp)
{
    long what = gc.context.takeoff.what;
    boolean result = FALSE;

    /* 'T' (or 'R' used for armor) sets ga.afternmv, 'A' sets takeoff.what */
    if (otmp == player_armor)
        result = (ga.afternmv == Armor_off || what == WEARING_ARMOR_BODY);
    else if (otmp == player_armor_undershirt)
        result = (ga.afternmv == Shirt_off || what == WORN_SHIRT);
    else if (otmp == player_armor_cloak)
        result = (ga.afternmv == Cloak_off || what == WORN_CLOAK);
    else if (otmp == player_armor_footwear)
        result = (ga.afternmv == Boots_off || what == WORN_BOOTS);
    else if (otmp == player_armor_hat)
        result = (ga.afternmv == Helmet_off || what == WORN_HELMET);
    else if (otmp == player_armor_gloves)
        result = (ga.afternmv == Gloves_off || what == WORN_GLOVES);
    else if (otmp == player_armor_shield)
        result = (ga.afternmv == Shield_off || what == WORN_SHIELD);
    /* these 1-turn items don't need 'ga.afternmv' checks */
    else if (otmp == player_amulet)
        result = (what == WORN_AMUL);
    else if (otmp == player_finger_left)
        result = (what == LEFT_RING);
    else if (otmp == player_finger_right)
        result = (what == RIGHT_RING);
    else if (otmp == player_blindfold)
        result = (what == WORN_BLINDF);
    else if (otmp == player_weapon)
        result = (what == WEARING_WEAPON);
    else if (otmp == player_secondary_weapon)
        result = (what == WEARING_SECONDARY_WEAPON);
    else if (otmp == player_quiver)
        result = (what == WEARING_QUIVER);

    return result;
}

/* despite their names, cancel_don() and cancel_doff() both apply to both
   donning and doffing... */
void
cancel_doff(struct obj *obj, long slotmask)
{
    /* Called by setworn() for old item in specified slot or by setnotworn()
     * for specified item.  We don't want to call cancel_don() if we got
     * here via <X>_off() -> setworn((struct obj *) 0) -> cancel_doff()
     * because that would stop the 'A' command from continuing with next
     * selected item.  So do_takeoff() sets a flag in takeoff.mask for us.
     * [For taking off an individual item with 'T'/'R'/'w-', it doesn't
     * matter whether cancel_don() gets called here--the item has already
     * been removed by now.]
     */
    if (!(gc.context.takeoff.mask & I_SPECIAL) && donning(obj))
        cancel_don(); /* applies to doffing too */
    gc.context.takeoff.mask &= ~slotmask;
}

/* despite their names, cancel_don() and cancel_doff() both apply to both
   donning and doffing... */
void
cancel_don(void)
{
    /* the piece of armor we were donning/doffing has vanished, so stop
     * wasting time on it (and don't dereference it when donning would
     * otherwise finish); afternmv never has some of these values because
     * every item of the corresponding armor category takes 1 turn to wear,
     * but check all of them anyway
     */
    gc.context.takeoff.cancelled_don = (ga.afternmv == Cloak_on
                                        || ga.afternmv == Armor_on
                                        || ga.afternmv == Shirt_on
                                        || ga.afternmv == Helmet_on
                                        || ga.afternmv == Gloves_on
                                        || ga.afternmv == Boots_on
                                        || ga.afternmv == Shield_on);
    ga.afternmv = (int (*)(void)) 0;
    gn.nomovemsg = (char *) 0;
    gm.multi = 0;
    gc.context.takeoff.delay = 0;
    gc.context.takeoff.what = 0L;
}

/* called by steal() during theft from hero; interrupt donning/doffing */
int
stop_donning(struct obj *stolenobj) /* no message if stolenobj is already
                                       being doffing */
{
    char buf[BUFSZ];
    struct obj *otmp;
    boolean putting_on;
    int result = 0;

    for (otmp = gi.invent; otmp; otmp = otmp->nobj)
        if ((otmp->owornmask & WEARING_ARMOR) && donning(otmp))
            break;
    /* at most one item will pass donning() test at any given time */
    if (!otmp)
        return 0;

    /* donning() returns True when doffing too; doffing() is more specific */
    putting_on = !doffing(otmp);
    /* cancel_don() looks at afternmv; it can also cancel doffing */
    cancel_don();
    /* don't want <armor>_on() or <armor>_off() being called
       by unmul() since the on or off action isn't completing */
    ga.afternmv = (int (*)(void)) 0;
    if (putting_on || otmp != stolenobj) {
        Sprintf(buf, "You stop %s %s.",
                putting_on ? "putting on" : "taking off",
                thesimpleoname(otmp));
    } else {
        buf[0] = '\0';   /* silently stop doffing stolenobj */
        result = (int) -gm.multi; /* remember this before calling unmul() */
    }
    unmul(buf);
    /* while putting on, item becomes worn immediately but side-effects are
       deferred until the delay expires; when interrupted, make it unworn
       (while taking off, item stays worn until the delay expires; when
       interrupted, leave it worn) */
    if (putting_on)
        remove_worn_item(otmp, FALSE);

    return result;
}

static NEARDATA int Narmorpieces, Naccessories;

/* assign values to Narmorpieces and Naccessories */
staticfn void
count_worn_stuff(struct obj **which, /* caller wants this when count is 1 */
                 boolean accessorizing)
{
    struct obj *otmp;

    Narmorpieces = Naccessories = 0;

#define MOREWORN(x,wtyp) do { if (x) { wtyp++; otmp = x; } } while (0)
    otmp = 0;
    MOREWORN(player_armor_hat, Narmorpieces);
    MOREWORN(player_armor_shield, Narmorpieces);
    MOREWORN(player_armor_gloves, Narmorpieces);
    MOREWORN(player_armor_footwear, Narmorpieces);
    /* for cloak/suit/shirt, we only count the outermost item so that it
       can be taken off without confirmation if final count ends up as 1 */
    if (player_armor_cloak)
        MOREWORN(player_armor_cloak, Narmorpieces);
    else if (player_armor)
        MOREWORN(player_armor, Narmorpieces);
    else if (player_armor_undershirt)
        MOREWORN(player_armor_undershirt, Narmorpieces);
    if (!accessorizing)
        *which = otmp; /* default item iff Narmorpieces is 1 */

    otmp = 0;
    MOREWORN(player_finger_left, Naccessories);
    MOREWORN(player_finger_right, Naccessories);
    MOREWORN(player_amulet, Naccessories);
    MOREWORN(player_blindfold, Naccessories);
    if (accessorizing)
        *which = otmp; /* default item iff Naccessories is 1 */
#undef MOREWORN
}

/* take off one piece or armor or one accessory;
   shared by dotakeoff('T') and doremring('R') */
staticfn int
armor_or_accessory_off(struct obj *obj)
{
    if (!(obj->owornmask & (WEARING_ARMOR | WEARING_ACCESSORY))) {
        You("are not wearing that.");
        return ECMD_OK;
    }
    if (obj == player_skin_if_dragon
        || ((obj == player_armor) && player_armor_cloak)
        || ((obj == player_armor_undershirt) && (player_armor_cloak || player_armor))) {
        char why[QBUFSZ], what[QBUFSZ];

        why[0] = what[0] = '\0';
        if (obj != player_skin_if_dragon) {
            if (player_armor_cloak)
                Strcat(what, cloak_simple_name(player_armor_cloak));
            if ((obj == player_armor_undershirt) && player_armor) {
                if (player_armor_cloak)
                    Strcat(what, " and ");
                Strcat(what, suit_simple_name(player_armor));
            }
            Snprintf(why, sizeof(why), " without taking off your %s first",
                     what);
        } else {
            Strcpy(why, "; it's embedded");
        }
        You_cant("take that off%s.", why);
        return ECMD_OK;
    }

    reset_remove_all_armor(); /* clear context.takeoff.mask and context.takeoff.what */
    (void) select_off(obj);
    if (!gc.context.takeoff.mask)
        return ECMD_OK;
    /* none of armoroff()/Ring_/Amulet/Blindf_off() use context.takeoff.mask */
    reset_remove_all_armor();

    if (obj->owornmask & WEARING_ARMOR) {
        (void) armoroff(obj);
    } else if (obj == player_finger_right || obj == player_finger_left) {
        /* Sometimes we want to give the off_msg before removing and
         * sometimes after; for instance, "you were wearing a moonstone
         * ring (on right hand)" is desired but "you were wearing a
         * square amulet (being worn)" is not because of the redundant
         * "being worn".
         */
        off_msg(obj);
        Ring_off(obj);
    } else if (obj == player_amulet) {
        Amulet_off();
        off_msg(obj);
    } else if (obj == player_blindfold) {
        Blindf_off(obj); /* does its own off_msg */
    } else {
        impossible("removing strange accessory?");
        if (obj->owornmask)
            remove_worn_item(obj, FALSE);
    }
    return ECMD_TIME;
}

/* the #takeoff command - remove worn armor */
int
dotakeoff(void)
{
    struct obj *otmp = (struct obj *) 0;

    count_worn_stuff(&otmp, FALSE);
    if (!Narmorpieces && !Naccessories) {
        /* assert( GRAY_DRAGON_SCALES > YELLOW_DRAGON_SCALE_MAIL ); */
        if (player_skin_if_dragon)
            pline_The("%s merged with your skin!",
                      player_skin_if_dragon->otyp >= GRAY_DRAGON_SCALES
                          ? "dragon scales are"
                          : "dragon scale mail is");
        else
            pline("Not wearing any armor or accessories.");
        return ECMD_OK;
    }
    if (Narmorpieces != 1 || ParanoidRemove || cmdq_peek(CQ_CANNED))
        otmp = getobj("take off", takeoff_ok, GETOBJ_NOFLAGS);
    if (!otmp)
        return ECMD_CANCEL;

    return armor_or_accessory_off(otmp);
}

/* the #remove command - take off ring or other accessory */
int
doremring(void)
{
    struct obj *otmp = 0;

    count_worn_stuff(&otmp, TRUE);
    if (!Naccessories && !Narmorpieces) {
        pline("Not wearing any accessories or armor.");
        return ECMD_OK;
    }
    if (Naccessories != 1 || ParanoidRemove || cmdq_peek(CQ_CANNED))
        otmp = getobj("remove", remove_ok, GETOBJ_NOFLAGS);
    if (!otmp)
        return ECMD_CANCEL;

    return armor_or_accessory_off(otmp);
}

/* Check if something worn is cursed _and_ unremovable. */
int
cursed(struct obj *otmp)
{
    if (!otmp) {
        impossible("cursed without otmp");
        return 0;
    }
    /* Curses, like chickens, come home to roost. */
    if ((otmp == player_weapon) ? welded(otmp) : (int) otmp->cursed) {
        boolean use_plural = (is_boots(otmp) || is_gloves(otmp)
                              || otmp->otyp == LENSES || otmp->quan > 1L);

        /* might be trying again after applying grease to hands */
        if (Glib && otmp->bknown
            /* for weapon, we'll only get here via 'A )' */
            && (player_armor_gloves ? (otmp == player_weapon)
                      : ((otmp->owornmask & (WEARING_WEAPON | WEARING_RING)) != 0)))
            pline("Despite your slippery %s, you can't.",
                  fingers_or_gloves(TRUE));
        else
            You("can't.  %s cursed.", use_plural ? "They are" : "It is");
        set_bknown(otmp, 1);
        return 1;
    }
    return 0;
}

int
armoroff(struct obj *otmp)
{
    static char offdelaybuf[60];
    int delay = -objects[otmp->otyp].oc_delay;
    const char *what = 0;

    if (cursed(otmp))
        return 0;
    /* this used to make assumptions about which types of armor had
       delays and which didn't; now both are handled for all types */
    if (delay) {
        nomul(delay);
        gm.multi_reason = "disrobing";
        switch (objects[otmp->otyp].oc_armcat) {
        case ARM_SUIT:
            what = suit_simple_name(otmp);
            ga.afternmv = Armor_off;
            break;
        case ARM_SHIELD:
            what = shield_simple_name(otmp);
            ga.afternmv = Shield_off;
            break;
        case ARM_HELM:
            what = helm_simple_name(otmp);
            ga.afternmv = Helmet_off;
            break;
        case ARM_GLOVES:
            what = gloves_simple_name(otmp);
            ga.afternmv = Gloves_off;
            break;
        case ARM_BOOTS:
            what = boots_simple_name(otmp);
            ga.afternmv = Boots_off;
            break;
        case ARM_CLOAK:
            what = cloak_simple_name(otmp);
            ga.afternmv = Cloak_off;
            break;
        case ARM_SHIRT:
            what = shirt_simple_name(otmp);
            ga.afternmv = Shirt_off;
            break;
        default:
            impossible("Taking off unknown armor (%d: %d), delay %d",
                       otmp->otyp, objects[otmp->otyp].oc_armcat, delay);
            break;
        }
        if (what) {
            /* sizeof offdelaybuf == 60; increase it if this becomes longer */
            Sprintf(offdelaybuf, "You finish taking off your %s.", what);
            gn.nomovemsg = offdelaybuf;
        }
    } else {
        /* no delay so no '(*afternmv)()' or 'nomovemsg' */
        switch (objects[otmp->otyp].oc_armcat) {
        case ARM_SUIT:
            (void) Armor_off();
            break;
        case ARM_SHIELD:
            (void) Shield_off();
            break;
        case ARM_HELM:
            (void) Helmet_off();
            break;
        case ARM_GLOVES:
            (void) Gloves_off();
            break;
        case ARM_BOOTS:
            (void) Boots_off();
            break;
        case ARM_CLOAK:
            (void) Cloak_off();
            break;
        case ARM_SHIRT:
            (void) Shirt_off();
            break;
        default:
            impossible("Taking off unknown armor (%d: %d), no delay",
                       otmp->otyp, objects[otmp->otyp].oc_armcat);
            break;
        }
        /* We want off_msg() after removing the item to
           avoid "You were wearing ____ (being worn)." */
        off_msg(otmp);
    }
    gc.context.takeoff.mask = gc.context.takeoff.what = 0L;
    return 1;
}

staticfn void
already_wearing(const char *cc)
{
    You("are already wearing %s%c", cc, (cc == c_that_) ? '!' : '.');
}

staticfn void
already_wearing2(const char *cc1, const char *cc2)
{
    You_cant("wear %s because you're wearing %s there already.", cc1, cc2);
}

/*
 * canwearobj checks to see whether the player can wear a piece of armor
 *
 * inputs: otmp (the piece of armor)
 *         noisy (if TRUE give error messages, otherwise be quiet about it)
 * output: mask (otmp's armor type)
 */
int
canwearobj(struct obj *otmp, long *mask, boolean noisy)
{
    int err = 0;
    const char *which;

    /* this is the same check as for 'W' (dowear), but different message,
       in case we get here via 'P' (doputon) */
    if (verysmall(gy.youmonst.data) || nohands(gy.youmonst.data)) {
        if (noisy)
            You("can't wear any armor in your current form.");
        return 0;
    }

    which = is_cloak(otmp) ? c_cloak
            : is_shirt(otmp) ? c_shirt
              : is_suit(otmp) ? c_suit
                : 0;
    if (which && cantweararm(gy.youmonst.data)
        /* same exception for cloaks as used in m_dowear() */
        && (which != c_cloak
            || ((otmp->otyp != MUMMY_WRAPPING)
                ? gy.youmonst.data->msize != MZ_SMALL
                : !WrappingAllowed(gy.youmonst.data)))
        && (racial_exception(&gy.youmonst, otmp) < 1)) {
        if (noisy)
            pline_The("%s will not fit on your body.", which);
        return 0;
    } else if (otmp->owornmask & WEARING_ARMOR) {
        if (noisy)
            already_wearing(c_that_);
        return 0;
    }

    if (welded(player_weapon) && bimanual(player_weapon) && (is_suit(otmp) || is_shirt(otmp))) {
        if (noisy)
            You("cannot do that while holding your %s.",
                is_sword(player_weapon) ? c_sword : c_weapon);
        return 0;
    }

    if (is_helmet(otmp)) {
        if (player_armor_hat) {
            if (noisy)
                already_wearing(an(helm_simple_name(player_armor_hat)));
            err++;
        } else if (Upolyd && has_horns(gy.youmonst.data) && !is_flimsy(otmp)) {
            /* (flimsy exception matches polyself handling) */
            if (noisy)
                pline_The("%s won't fit over your horn%s.",
                          helm_simple_name(otmp),
                          plur(num_horns(gy.youmonst.data)));
            err++;
        } else
            *mask = WEARING_ARMOR_HELMET;
    } else if (is_shield(otmp)) {
        if (player_armor_shield) {
            if (noisy)
                already_wearing(an(c_shield));
            err++;
        } else if (player_weapon && bimanual(player_weapon)) {
            if (noisy)
                You("cannot wear a shield while wielding a two-handed %s.",
                    is_sword(player_weapon) ? c_sword : (player_weapon->otyp == BATTLE_AXE)
                                                   ? c_axe
                                                   : c_weapon);
            err++;
        } else if (u.using_two_weapons) {
            if (noisy)
                You("cannot wear a shield while wielding two weapons.");
            err++;
        } else
            *mask = WEARING_ARMOR_SHIELD;
    } else if (is_boots(otmp)) {
        if (player_armor_footwear) {
            if (noisy)
                already_wearing(c_boots);
            err++;
        } else if (Upolyd && slithy(gy.youmonst.data)) {
            if (noisy)
                You("have no feet..."); /* not body_part(FOOT) */
            err++;
        } else if (Upolyd && gy.youmonst.data->mlet == S_CENTAUR) {
            /* break_armor() pushes boots off for centaurs,
               so don't let dowear() put them back on... */
            if (noisy)
                You("have too many hooves to wear %s.",
                      c_boots); /* makeplural(body_part(FOOT)) yields
                                   "rear hooves" which sounds odd */
            err++;
        } else if (u.utrap
                   && (u.utraptype == TT_BEARTRAP || u.utraptype == TT_INFLOOR
                       || u.utraptype == TT_LAVA
                       || u.utraptype == TT_BURIEDBALL)) {
            if (u.utraptype == TT_BEARTRAP) {
                if (noisy)
                    Your("%s is trapped!", body_part(FOOT));
            } else if (u.utraptype == TT_INFLOOR || u.utraptype == TT_LAVA) {
                if (noisy)
                    Your("%s are stuck in the %s!",
                         makeplural(body_part(FOOT)), surface(u.ux, u.uy));
            } else { /*TT_BURIEDBALL*/
                if (noisy)
                    Your("%s is attached to the buried ball!",
                         body_part(LEG));
            }
            err++;
        } else
            *mask = WEARING_ARMOR_FOOTWEAR;
    } else if (is_gloves(otmp)) {
        if (player_armor_gloves) {
            if (noisy)
                already_wearing(c_gloves);
            err++;
        } else if (welded(player_weapon)) {
            if (noisy)
                You("cannot wear gloves over your %s.",
                    is_sword(player_weapon) ? c_sword : c_weapon);
            err++;
        } else if (Glib) {
            /* prevent slippery bare fingers from transferring to
               gloved fingers */
            if (noisy)
                Your("%s are too slippery to pull on %s.",
                     fingers_or_gloves(FALSE), gloves_simple_name(otmp));
            err++;
        } else
            *mask = WEARING_ARMOR_GLOVES;
    } else if (is_shirt(otmp)) {
        if (player_armor || player_armor_cloak || player_armor_undershirt) {
            if (player_armor_undershirt) {
                if (noisy)
                    already_wearing(an(c_shirt));
            } else {
                if (noisy)
                    You_cant("wear that over your %s.",
                             (player_armor && !player_armor_cloak) ? c_armor
                                              : cloak_simple_name(player_armor_cloak));
            }
            err++;
        } else
            *mask = WEARING_ARMOR_UNDERSHIRT;
    } else if (is_cloak(otmp)) {
        if (player_armor_cloak) {
            if (noisy)
                already_wearing(an(cloak_simple_name(player_armor_cloak)));
            err++;
        } else
            *mask = WEARING_ARMOR_CLOAK;
    } else if (is_suit(otmp)) {
        if (player_armor_cloak) {
            if (noisy)
                You("cannot wear armor over a %s.", cloak_simple_name(player_armor_cloak));
            err++;
        } else if (player_armor) {
            if (noisy)
                already_wearing("some armor");
            err++;
        } else
            *mask = WEARING_ARMOR_BODY;
    } else {
        /* getobj can't do this after setting its allow_all flag; that
           happens if you have armor for slots that are covered up or
           extra armor for slots that are filled */
        if (noisy)
            silly_thing("wear", otmp);
        err++;
    }
    /* Unnecessary since now only weapons and special items like pick-axes get
     * welded to your hand, not armor
        if (welded(otmp)) {
            if (!err++) {
                if (noisy) weldmsg(otmp);
            }
        }
     */
    return !err;
}

staticfn int
accessory_or_armor_on(struct obj *obj)
{
    long mask = 0L;
    boolean armor, ring, eyewear;

    if (obj->owornmask & (WEARING_ACCESSORY | WEARING_ARMOR)) {
        already_wearing(c_that_);
        return ECMD_OK;
    }
    armor = (obj->oclass == ARMOR_CLASS);
    ring = (obj->oclass == RING_CLASS || obj->otyp == MEAT_RING);
    eyewear = (obj->otyp == BLINDFOLD || obj->otyp == TOWEL
               || obj->otyp == LENSES);
    /* checks which are performed prior to actually touching the item */
    if (armor) {
        if (!canwearobj(obj, &mask, TRUE))
            return ECMD_OK;

        if (obj->otyp == HELM_OF_OPPOSITE_ALIGNMENT
            && qstart_level.dungeon_number == u.uz.dungeon_number) { /* in quest */
            if (u.ualignbase[A_CURRENT] == u.ualignbase[A_ORIGINAL])
                You("narrowly avoid losing all chance at your goal.");
            else /* converted */
                You("are suddenly overcome with shame and change your mind.");
            u.blessed = 0; /* lose your god's protection */
            makeknown(obj->otyp);
            disp.bottom_line = TRUE; /* for AC after zeroing u.ublessed */
            return ECMD_TIME;
        }
    } else {
        /*
         * FIXME:
         *  except for the rings/nolimbs case, this allows you to put on
         *  accessories without having any hands to manipulate them, and
         *  to put them on when poly'd into a tiny or huge form where
         *  they shouldn't fit.  [If the latter situation changes, make
         *  comparable change to break_armor(polyself.c).]
         */

        /* accessory */
        if (ring) {
            char answer, qbuf[QBUFSZ];
            int res = 0;

            if (nolimbs(gy.youmonst.data)) {
                You("cannot make the ring stick to your body.");
                return ECMD_OK;
            }
            if (player_finger_left && player_finger_right) {
                There("are no more %s%s to fill.",
                      humanoid(gy.youmonst.data) ? "ring-" : "",
                      fingers_or_gloves(FALSE));
                return ECMD_OK;
            }
            if (player_finger_left) {
                mask = RIGHT_RING;
            } else if (player_finger_right) {
                mask = LEFT_RING;
            } else {
                do {
                    Sprintf(qbuf, "Which %s%s, Right or Left?",
                            humanoid(gy.youmonst.data) ? "ring-" : "",
                            body_part(FINGER));
                    answer = yn_function(qbuf, rightleftchars, '\0', TRUE);
                    switch (answer) {
                    case '\0':
                    case '\033':
                        return ECMD_OK;
                    case 'l':
                    case 'L':
                        mask = LEFT_RING;
                        break;
                    case 'r':
                    case 'R':
                        mask = RIGHT_RING;
                        break;
                    }
                } while (!mask);
            }
            if (player_armor_gloves && Glib) {
                Your(
              "%s are too slippery to remove, so you cannot put on the ring.",
                     gloves_simple_name(player_armor_gloves));
                return ECMD_TIME; /* always uses move */
            }
            if (player_armor_gloves && player_armor_gloves->cursed) {
                res = !player_armor_gloves->bknown;
                set_bknown(player_armor_gloves, 1);
                You("cannot remove your %s to put on the ring.", c_gloves);
                /* uses move iff we learned gloves are cursed */
                return res ? ECMD_TIME : ECMD_OK;
            }
            if (player_weapon) {
                res = !player_weapon->bknown; /* check this before calling welded() */
                if (((mask == RIGHT_RING && URIGHTY)
                     || (mask == LEFT_RING  && ULEFTY)
                     || bimanual(player_weapon)) && welded(player_weapon)) {
                    const char *hand = body_part(HAND);

                    /* welded will set bknown */
                    if (bimanual(player_weapon))
                        hand = makeplural(hand);
                    You("cannot free your weapon %s to put on the ring.",
                        hand);
                    /* uses move iff we learned weapon is cursed */
                    return res ? ECMD_TIME : ECMD_OK;
                }
            }
        } else if (obj->oclass == AMULET_CLASS) {
            if (player_amulet) {
                already_wearing("an amulet");
                return ECMD_OK;
            }
        } else if (eyewear) {
            if (!has_head(gy.youmonst.data)) {
                You("have no head to wear %s on.", ansimpleoname(obj));
                return ECMD_OK;
            }

            if (player_blindfold) {
                if (player_blindfold->otyp == TOWEL)
                    Your("%s is already covered by a towel.",
                         body_part(FACE));
                else if (player_blindfold->otyp == BLINDFOLD) {
                    if (obj->otyp == LENSES)
                        already_wearing2("lenses", "a blindfold");
                    else
                        already_wearing("a blindfold");
                } else if (player_blindfold->otyp == LENSES) {
                    if (obj->otyp == BLINDFOLD)
                        already_wearing2("a blindfold", "some lenses");
                    else
                        already_wearing("some lenses");
                } else {
                    already_wearing(something); /* ??? */
                }
                return ECMD_OK;
            }
        } else {
            /* neither armor nor accessory */
            You_cant("wear that!");
            return ECMD_OK;
        }
    }

    if (!retouch_object(&obj, FALSE))
        return ECMD_TIME; /* costs a turn even though it didn't get worn */

    if (armor) {
        int delay;

        /* if the armor is wielded, release it for wearing (won't be
           welded even if cursed; that only happens for weapons/weptools) */
        if (obj->owornmask & WEARING_WEAPONS)
            remove_worn_item(obj, FALSE);
        /*
         * Setting obj->known=1 is done because setworn() causes hero's AC
         * to change so armor's +/- value is evident via the status line.
         * We used to set it here because of that, but then it would stick
         * if a nymph stole the armor before it was fully worn.  Delay it
         * until the afternmv action.  The player may still know this armor's
         * +/- amount if donning gets interrupted, but the hero won't.
         *
        obj->known = 1;
         */
        setworn(obj, mask);
        /* if there's no delay, we'll execute 'afternmv' immediately */
        if (obj == player_armor)
            ga.afternmv = Armor_on;
        else if (obj == player_armor_hat)
            ga.afternmv = Helmet_on;
        else if (obj == player_armor_gloves)
            ga.afternmv = Gloves_on;
        else if (obj == player_armor_footwear)
            ga.afternmv = Boots_on;
        else if (obj == player_armor_shield)
            ga.afternmv = Shield_on;
        else if (obj == player_armor_cloak)
            ga.afternmv = Cloak_on;
        else if (obj == player_armor_undershirt)
            ga.afternmv = Shirt_on;
        else
            panic("wearing armor not worn as armor? [%08lx]", obj->owornmask);

        delay = -objects[obj->otyp].oc_delay;
        if (delay) {
            nomul(delay);
            gm.multi_reason = "dressing up";
            gn.nomovemsg = "You finish your dressing maneuver.";
        } else {
            unmul(""); /* call afternmv, clear it+nomovemsg+multi_reason */
            on_msg(obj);
        }
        gc.context.takeoff.mask = gc.context.takeoff.what = 0L;
    } else { /* not armor */
        boolean give_feedback = FALSE;

        /* [releasing wielded accessory handled in Xxx_on()] */
        if (ring) {
            setworn(obj, mask);
            Ring_on(obj);
            give_feedback = TRUE;
        } else if (obj->oclass == AMULET_CLASS) {
            setworn(obj, WEARING_AMULET);
            Amulet_on();
            /* no feedback here if amulet of change got used up */
            give_feedback = (player_amulet != 0);
        } else if (eyewear) {
            /* setworn() handled by Blindf_on() */
            Blindf_on(obj);
            /* message handled by Blindf_on(); leave give_feedback False */
        }
        /* feedback for ring or for amulet other than 'change' */
        if (give_feedback && is_worn(obj))
            prinv((char *) 0, obj, 0L);
    }
    return ECMD_TIME;
}

/* the #wear command */
int
dowear(void)
{
    struct obj *otmp;

    /* cantweararm() checks for suits of armor, not what we want here;
       verysmall() or nohands() checks for shields, gloves, etc... */
    if (verysmall(gy.youmonst.data) || nohands(gy.youmonst.data)) {
        pline("Don't even bother.");
        return ECMD_OK;
    }
    if (player_armor && player_armor_undershirt && player_armor_cloak && player_armor_hat && player_armor_shield && player_armor_gloves && player_armor_footwear
        && player_finger_left && player_finger_right && player_amulet && player_blindfold) {
        /* 'W' message doesn't mention accessories */
        You("are already wearing a full complement of armor.");
        return ECMD_OK;
    }
    otmp = getobj("wear", wear_ok, GETOBJ_NOFLAGS);
    return otmp ? accessory_or_armor_on(otmp) : ECMD_CANCEL;
}

/* the #puton command */
int
doputon(void)
{
    struct obj *otmp;

    if (player_finger_left && player_finger_right && player_amulet && player_blindfold
        && player_armor && player_armor_undershirt && player_armor_cloak && player_armor_hat && player_armor_shield && player_armor_gloves && player_armor_footwear) {
        /* 'P' message doesn't mention armor */
        Your("%s%s are full, and you're already wearing an amulet and %s.",
             humanoid(gy.youmonst.data) ? "ring-" : "",
             fingers_or_gloves(FALSE),
             (player_blindfold->otyp == LENSES) ? "some lenses" : "a blindfold");
        return ECMD_OK;
    }
    otmp = getobj("put on", puton_ok, GETOBJ_NOFLAGS);
    return otmp ? accessory_or_armor_on(otmp) : ECMD_CANCEL;
}

/* calculate current armor class */
void
find_ac(void)
{
    int uac = mons[u.umonnum].ac; /* base armor class for current form */

    /* armor class from worn gear */
    if (player_armor)
        uac -= ARM_BONUS(player_armor);
    if (player_armor_cloak)
        uac -= ARM_BONUS(player_armor_cloak);
    if (player_armor_hat)
        uac -= ARM_BONUS(player_armor_hat);
    if (player_armor_footwear)
        uac -= ARM_BONUS(player_armor_footwear);
    if (player_armor_shield)
        uac -= ARM_BONUS(player_armor_shield);
    if (player_armor_gloves)
        uac -= ARM_BONUS(player_armor_gloves);
    if (player_armor_undershirt)
        uac -= ARM_BONUS(player_armor_undershirt);
    if (player_finger_left && player_finger_left->otyp == RIN_PROTECTION)
        uac -= player_finger_left->spe;
    if (player_finger_right && player_finger_right->otyp == RIN_PROTECTION)
        uac -= player_finger_right->spe;
    if (player_amulet && player_amulet->otyp == AMULET_OF_GUARDING)
        uac -= 2; /* fixed amount; main benefit is to MC */

    /* armor class from other sources */
    if (HProtection & INTRINSIC)
        uac -= u.blessed;
    uac -= u.spell_protection;

    /* put a cap on armor class [3.7: was +127,-128, now reduced to +/- 99 */
    if (abs(uac) > AC_MAX)
        uac = sgn(uac) * AC_MAX;

    if (uac != u.armor_class) {
        u.armor_class = uac;
        disp.bottom_line = TRUE;
#if 0
        /* these could conceivably be achieved out of order (by being near
           threshold and putting on +N dragon scale mail from bones, for
           instance), but if that happens, that's the order it happened;
           also, testing for these in the usual order would result in more
           record_achievement() attempts and rejects for duplication */
        if (u.uac <= -20)
            record_achievement(ACH_AC_20);
        else if (u.uac <= -10)
            record_achievement(ACH_AC_10);
        else if (u.uac <= 0)
            record_achievement(ACH_AC_00);
#endif
    }
}

void
glibr(void)
{
    struct obj *otmp;
    int xfl = 0;
    boolean leftfall, rightfall, wastwoweap = FALSE;
    const char *otherwep = 0, *thiswep, *which, *hand;

    leftfall = (player_finger_left && !player_finger_left->cursed
                && (!player_weapon || !(welded(player_weapon) && ULEFTY)
                    || !bimanual(player_weapon)));
    rightfall = (player_finger_right && !player_finger_right->cursed
                && (!player_weapon || !(welded(player_weapon) && URIGHTY)
                    || !bimanual(player_weapon)));
/*
    leftfall = (uleft && !uleft->cursed
                && (!uwep || !welded(uwep) || !bimanual(uwep)));
    rightfall = (uright && !uright->cursed && (!welded(uwep)));
*/

    if (!player_armor_gloves && (leftfall || rightfall) && !nolimbs(gy.youmonst.data)) {
        /* changed so cursed rings don't fall off, GAN 10/30/86 */
        Your("%s off your %s.",
             (leftfall && rightfall) ? "rings slip" : "ring slips",
             (leftfall && rightfall) ? fingers_or_gloves(FALSE)
                                     : body_part(FINGER));
        xfl++;
        if (leftfall) {
            otmp = player_finger_left;
            Ring_off(player_finger_left);
            dropx(otmp);
            cmdq_clear(CQ_CANNED);
        }
        if (rightfall) {
            otmp = player_finger_right;
            Ring_off(player_finger_right);
            dropx(otmp);
            cmdq_clear(CQ_CANNED);
        }
    }

    otmp = player_secondary_weapon;
    if (u.using_two_weapons && otmp) {
        /* secondary weapon doesn't need nearly as much handling as
           primary; when in two-weapon mode, we know it's one-handed
           with something else in the other hand and also that it's
           a weapon or weptool rather than something unusual, plus
           we don't need to compare its type with the primary */
        otherwep = is_sword(otmp) ? c_sword : weapon_descr(otmp);
        if (otmp->quan > 1L)
            otherwep = makeplural(otherwep);
        hand = body_part(HAND);
        which = URIGHTY ? "left " : "right ";  /* text for the off hand */
        Your("%s %s%s from your %s%s.", otherwep, xfl ? "also " : "",
             otense(otmp, "slip"), which, hand);
        xfl++;
        wastwoweap = TRUE;
        setuswapwep((struct obj *) 0); /* clears u.twoweap */
        cmdq_clear(CQ_CANNED);
        if (canletgo(otmp, ""))
            dropx(otmp);
    }
    otmp = player_weapon;
    if (otmp && otmp->otyp != AKLYS && !welded(otmp)) {
        long savequan = otmp->quan;

        /* nice wording if both weapons are the same type */
        thiswep = is_sword(otmp) ? c_sword : weapon_descr(otmp);
        if (otherwep && strcmp(thiswep, makesingular(otherwep)))
            otherwep = 0;
        if (otmp->quan > 1L) {
            /* most class names for unconventional wielded items
               are ok, but if wielding multiple apples or rations
               we don't want "your foods slip", so force non-corpse
               food to be singular; skipping makeplural() isn't
               enough--we need to fool otense() too */
            if (!strcmp(thiswep, "food"))
                otmp->quan = 1L;
            else
                thiswep = makeplural(thiswep);
        }
        hand = body_part(HAND);
        which = "";
        if (bimanual(otmp)) {
            hand = makeplural(hand);
        } else if (wastwoweap) {
            /* preceding msg was about non-dominant hand */
            which = URIGHTY ? "right " : "left ";
	}
        pline("%s %s%s %s%s from your %s%s.",
              !strncmp(thiswep, "corpse", 6) ? "The" : "Your",
              otherwep ? "other " : "", thiswep, xfl ? "also " : "",
              otense(otmp, "slip"), which, hand);
        /* xfl++; */
        otmp->quan = savequan;
        setuwep((struct obj *) 0);
        cmdq_clear(CQ_CANNED);
        if (canletgo(otmp, ""))
            dropx(otmp);
    }
}

struct obj *
some_armor(struct monster *victim)
{
    struct obj *otmph, *otmp;

    otmph = (victim == &gy.youmonst) ? player_armor_cloak : which_armor(victim, WEARING_ARMOR_CLOAK);
    if (!otmph)
        otmph = (victim == &gy.youmonst) ? player_armor : which_armor(victim, WEARING_ARMOR_BODY);
    if (!otmph)
        otmph = (victim == &gy.youmonst) ? player_armor_undershirt : which_armor(victim, WEARING_ARMOR_UNDERSHIRT);

    otmp = (victim == &gy.youmonst) ? player_armor_hat : which_armor(victim, WEARING_ARMOR_HELMET);
    if (otmp && (!otmph || !random_integer_between_zero_and(4)))
        otmph = otmp;
    otmp = (victim == &gy.youmonst) ? player_armor_gloves : which_armor(victim, WEARING_ARMOR_GLOVES);
    if (otmp && (!otmph || !random_integer_between_zero_and(4)))
        otmph = otmp;
    otmp = (victim == &gy.youmonst) ? player_armor_footwear : which_armor(victim, WEARING_ARMOR_FOOTWEAR);
    if (otmp && (!otmph || !random_integer_between_zero_and(4)))
        otmph = otmp;
    otmp = (victim == &gy.youmonst) ? player_armor_shield : which_armor(victim, WEARING_ARMOR_SHIELD);
    if (otmp && (!otmph || !random_integer_between_zero_and(4)))
        otmph = otmp;
    return otmph;
}

/* used for praying to check and fix levitation trouble */
struct obj *
stuck_ring(struct obj *ring, int otyp)
{
    if (ring != player_finger_left && ring != player_finger_right) {
        impossible("stuck_ring: neither left nor right?");
        return (struct obj *) 0;
    }

    if (ring && ring->otyp == otyp) {
        /* reasons ring can't be removed match those checked by select_off();
           limbless case has extra checks because ordinarily it's temporary */
        if (nolimbs(gy.youmonst.data) && player_amulet
            && player_amulet->otyp == AMULET_OF_UNCHANGING && player_amulet->cursed)
            return player_amulet;
        if (welded(player_weapon) && ((ring == RING_ON_PRIMARY) || bimanual(player_weapon)))
            return player_weapon;
        if (player_armor_gloves && player_armor_gloves->cursed)
            return player_armor_gloves;
        if (ring->cursed)
            return ring;
        /* normally outermost layer is processed first, but slippery gloves
           wears off quickly so uncurse ring itself before handling those */
        if (player_armor_gloves && Glib)
            return player_armor_gloves;
    }
    /* either no ring or not right type or nothing prevents its removal */
    return (struct obj *) 0;
}

/* also for praying; find worn item that confers "Unchanging" attribute */
struct obj *
unchanger(void)
{
    if (player_amulet && player_amulet->otyp == AMULET_OF_UNCHANGING)
        return player_amulet;
    return 0;
}

staticfn
int
select_off(struct obj *otmp)
{
    struct obj *why;
    char buf[BUFSZ];

    if (!otmp)
        return 0;
    *buf = '\0'; /* lint suppression */

    /* special ring checks */
    if (otmp == player_finger_right || otmp == player_finger_left) {
        struct obj glibdummy;

        if (nolimbs(gy.youmonst.data)) {
            pline_The("ring is stuck.");
            return 0;
        }
        glibdummy = cg.zeroobj;
        why = 0; /* the item which prevents ring removal */
        if (welded(player_weapon) && ((otmp == RING_ON_PRIMARY) || bimanual(player_weapon))) {
            Sprintf(buf, "free a weapon %s", body_part(HAND));
            why = player_weapon;
        } else if (player_armor_gloves && (player_armor_gloves->cursed || Glib)) {
            Sprintf(buf, "take off your %s%s",
                    Glib ? "slippery " : "", gloves_simple_name(player_armor_gloves));
            why = !Glib ? player_armor_gloves : &glibdummy;
        }
        if (why) {
            You("cannot %s to remove the ring.", buf);
            set_bknown(why, 1);
            return 0;
        }
    }
    /* special glove checks */
    if (otmp == player_armor_gloves) {
        if (welded(player_weapon)) {
            You("are unable to take off your %s while wielding that %s.",
                c_gloves, is_sword(player_weapon) ? c_sword : c_weapon);
            set_bknown(player_weapon, 1);
            return 0;
        } else if (Glib) {
            pline("%s %s are too slippery to take off.",
                  player_armor_gloves->unpaid ? "The" : "Your", /* simplified Shk_Your() */
                  gloves_simple_name(player_armor_gloves));
            return 0;
        }
    }
    /* special boot checks */
    if (otmp == player_armor_footwear) {
        if (u.utrap && u.utraptype == TT_BEARTRAP) {
            pline_The("bear trap prevents you from pulling your %s out.",
                      body_part(FOOT));
            return 0;
        } else if (u.utrap && u.utraptype == TT_INFLOOR) {
            You("are stuck in the %s, and cannot pull your %s out.",
                surface(u.ux, u.uy), makeplural(body_part(FOOT)));
            return 0;
        }
    }
    /* special suit and shirt checks */
    if (otmp == player_armor || otmp == player_armor_undershirt) {
        why = 0; /* the item which prevents disrobing */
        if (player_armor_cloak && player_armor_cloak->cursed) {
            Sprintf(buf, "remove your %s", cloak_simple_name(player_armor_cloak));
            why = player_armor_cloak;
        } else if (otmp == player_armor_undershirt && player_armor && player_armor->cursed) {
            Sprintf(buf, "remove your %s", c_suit);
            why = player_armor;
        } else if (welded(player_weapon) && bimanual(player_weapon)) {
            Sprintf(buf, "release your %s",
                    is_sword(player_weapon) ? c_sword : (player_weapon->otyp == BATTLE_AXE)
                                                   ? c_axe
                                                   : c_weapon);
            why = player_weapon;
        }
        if (why) {
            You("cannot %s to take off %s.", buf, the(xname(otmp)));
            set_bknown(why, 1);
            return 0;
        }
    }
    /* basic curse check */
    if (otmp == player_quiver || (otmp == player_secondary_weapon && !u.using_two_weapons)) {
        ; /* some items can be removed even when cursed */
    } else {
        /* otherwise, this is fundamental */
        if (cursed(otmp))
            return 0;
    }

    if (otmp == player_armor)
        gc.context.takeoff.mask |= WEARING_ARMOR_BODY;
    else if (otmp == player_armor_cloak)
        gc.context.takeoff.mask |= WORN_CLOAK;
    else if (otmp == player_armor_footwear)
        gc.context.takeoff.mask |= WORN_BOOTS;
    else if (otmp == player_armor_gloves)
        gc.context.takeoff.mask |= WORN_GLOVES;
    else if (otmp == player_armor_hat)
        gc.context.takeoff.mask |= WORN_HELMET;
    else if (otmp == player_armor_shield)
        gc.context.takeoff.mask |= WORN_SHIELD;
    else if (otmp == player_armor_undershirt)
        gc.context.takeoff.mask |= WORN_SHIRT;
    else if (otmp == player_finger_left)
        gc.context.takeoff.mask |= LEFT_RING;
    else if (otmp == player_finger_right)
        gc.context.takeoff.mask |= RIGHT_RING;
    else if (otmp == player_amulet)
        gc.context.takeoff.mask |= WORN_AMUL;
    else if (otmp == player_blindfold)
        gc.context.takeoff.mask |= WORN_BLINDF;
    else if (otmp == player_weapon)
        gc.context.takeoff.mask |= WEARING_WEAPON;
    else if (otmp == player_secondary_weapon)
        gc.context.takeoff.mask |= WEARING_SECONDARY_WEAPON;
    else if (otmp == player_quiver)
        gc.context.takeoff.mask |= WEARING_QUIVER;

    else
        impossible("select_off: %s???", doname(otmp));

    return 0;
}

staticfn struct obj *
do_takeoff(void)
{
    struct obj *otmp = (struct obj *) 0;
    boolean was_twoweap = u.using_two_weapons;
    struct takeoff_info *doff = &gc.context.takeoff;

    gc.context.takeoff.mask |= I_SPECIAL; /* set flag for cancel_doff() */
    if (doff->what == WEARING_WEAPON) {
        if (!cursed(player_weapon)) {
            setuwep((struct obj *) 0);
            if (was_twoweap)
                You("are no longer wielding either weapon.");
            else
                You("are %s.", empty_handed());
        }
    } else if (doff->what == WEARING_SECONDARY_WEAPON) {
        setuswapwep((struct obj *) 0);
        You("%sno longer %s.", was_twoweap ? "are " : "",
            was_twoweap ? "wielding two weapons at once"
                        : "have a second weapon readied");
    } else if (doff->what == WEARING_QUIVER) {
        setuqwep((struct obj *) 0);
        You("no longer have ammunition readied.");
    } else if (doff->what == WEARING_ARMOR_BODY) {
        otmp = player_armor;
        if (!cursed(otmp))
            (void) Armor_off();
    } else if (doff->what == WORN_CLOAK) {
        otmp = player_armor_cloak;
        if (!cursed(otmp))
            (void) Cloak_off();
    } else if (doff->what == WORN_BOOTS) {
        otmp = player_armor_footwear;
        if (!cursed(otmp))
            (void) Boots_off();
    } else if (doff->what == WORN_GLOVES) {
        otmp = player_armor_gloves;
        if (!cursed(otmp))
            (void) Gloves_off();
    } else if (doff->what == WORN_HELMET) {
        otmp = player_armor_hat;
        if (!cursed(otmp))
            (void) Helmet_off();
    } else if (doff->what == WORN_SHIELD) {
        otmp = player_armor_shield;
        if (!cursed(otmp))
            (void) Shield_off();
    } else if (doff->what == WORN_SHIRT) {
        otmp = player_armor_undershirt;
        if (!cursed(otmp))
            (void) Shirt_off();
    } else if (doff->what == WORN_AMUL) {
        otmp = player_amulet;
        if (!cursed(otmp))
            Amulet_off();
    } else if (doff->what == LEFT_RING) {
        otmp = player_finger_left;
        if (!cursed(otmp))
            Ring_off(player_finger_left);
    } else if (doff->what == RIGHT_RING) {
        otmp = player_finger_right;
        if (!cursed(otmp))
            Ring_off(player_finger_right);
    } else if (doff->what == WORN_BLINDF) {
        if (!cursed(player_blindfold))
            Blindf_off(player_blindfold);
    } else {
        impossible("do_takeoff: taking off %lx", doff->what);
    }
    gc.context.takeoff.mask &= ~I_SPECIAL; /* clear cancel_doff() flag */

    return otmp;
}

/* occupation callback for 'A' */
staticfn int
take_off(void)
{
    int i;
    struct obj *otmp;
    struct takeoff_info *doff = &gc.context.takeoff;

    if (doff->what) {
        if (doff->delay > 0) {
            doff->delay--;
            return 1; /* still busy */
        }
        if ((otmp = do_takeoff()) != 0)
            off_msg(otmp);
        doff->mask &= ~doff->what;
        doff->what = 0L;
    }

    for (i = 0; takeoff_order[i]; i++)
        if (doff->mask & takeoff_order[i]) {
            doff->what = takeoff_order[i];
            break;
        }

    otmp = (struct obj *) 0;
    doff->delay = 0;

    if (doff->what == 0L) {
        You("finish %s.", doff->disrobing);
        return 0;
    } else if (doff->what == WEARING_WEAPON) {
        doff->delay = 1;
    } else if (doff->what == WEARING_SECONDARY_WEAPON) {
        doff->delay = 1;
    } else if (doff->what == WEARING_QUIVER) {
        doff->delay = 1;
    } else if (doff->what == WEARING_ARMOR_BODY) {
        otmp = player_armor;
        /* If a cloak is being worn, add the time to take it off and put
         * it back on again.  Kludge alert! since that time is 0 for all
         * known cloaks, add 1 so that it actually matters...
         */
        if (player_armor_cloak)
            doff->delay += 2 * objects[player_armor_cloak->otyp].oc_delay + 1;
    } else if (doff->what == WORN_CLOAK) {
        otmp = player_armor_cloak;
    } else if (doff->what == WORN_BOOTS) {
        otmp = player_armor_footwear;
    } else if (doff->what == WORN_GLOVES) {
        otmp = player_armor_gloves;
    } else if (doff->what == WORN_HELMET) {
        otmp = player_armor_hat;
    } else if (doff->what == WORN_SHIELD) {
        otmp = player_armor_shield;
    } else if (doff->what == WORN_SHIRT) {
        otmp = player_armor_undershirt;
        /* add the time to take off and put back on armor and/or cloak */
        if (player_armor)
            doff->delay += 2 * objects[player_armor->otyp].oc_delay;
        if (player_armor_cloak)
            doff->delay += 2 * objects[player_armor_cloak->otyp].oc_delay + 1;
    } else if (doff->what == WORN_AMUL) {
        doff->delay = 1;
    } else if (doff->what == LEFT_RING) {
        doff->delay = 1;
    } else if (doff->what == RIGHT_RING) {
        doff->delay = 1;
    } else if (doff->what == WORN_BLINDF) {
        /* [this used to be 2, but 'R' (and 'T') only require 1 turn to
           remove a blindfold, so 'A' shouldn't have been requiring 2] */
        doff->delay = 1;
    } else {
        impossible("take_off: taking off %lx", doff->what);
        return 0; /* force done */
    }

    if (otmp)
        doff->delay += objects[otmp->otyp].oc_delay;

    /* Since setting the occupation now starts the counter next move, that
     * would always produce a delay 1 too big per item unless we subtract
     * 1 here to account for it.
     */
    if (doff->delay > 0)
        doff->delay--;

    set_occupation(take_off, doff->disrobing, 0);
    return 1; /* get busy */
}

/* clear saved context to avoid inappropriate resumption of interrupted 'A' */
void
reset_remove_all_armor(void)
{
    gc.context.takeoff.what = gc.context.takeoff.mask = 0L;
    gc.context.takeoff.disrobing[0] = '\0';
}

/* the #takeoffall command -- remove multiple worn items */
int
doddo_remove_armor(void)
{
    int result = 0;

    if (gc.context.takeoff.what || gc.context.takeoff.mask) {
        You("continue %s.", gc.context.takeoff.disrobing);
        set_occupation(take_off, gc.context.takeoff.disrobing, 0);
        return ECMD_OK;
    } else if (!player_weapon && !player_secondary_weapon && !player_quiver && !player_amulet && !player_blindfold
               && !player_finger_left && !player_finger_right && !wearing_armor()) {
        You("are not wearing anything.");
        return ECMD_OK;
    }

    add_valid_menu_class(0); /* reset */
    if (flags.menu_style != MENU_TRADITIONAL
        || (result = ggetobj("take off", select_off, 0, FALSE,
                             (unsigned *) 0)) < -1)
        result = menu_remove_armor(result);

    if (gc.context.takeoff.mask) {
        (void) strncpy(gc.context.takeoff.disrobing,
                       (((gc.context.takeoff.mask & ~WEARING_WEAPONS) != 0)
                        /* default activity for armor and/or accessories,
                           possibly combined with weapons */
                        ? "disrobing"
                        /* specific activity when handling weapons only */
                        : "disarming"), CONTEXTVERBSZ);
        (void) take_off();
    }
    /* The time to perform the command is already completely accounted for
     * in take_off(); if we return 1, that would add an extra turn to each
     * disrobe.
     */
    return ECMD_OK;
}

/* #altunwield - just unwield alternate weapon, item-action '-' when picking
   uswapwep from context-sensitive inventory */
int
remover_armor_swap_weapon(void)
{
    struct _cmd_queue cq, *cmdq;
    unsigned oldbknown;

    if ((cmdq = cmdq_pop()) != 0) {
        /* '-' uswapwep item-action picked from context-sensitive invent */
        cq = *cmdq;
        free(cmdq);
    } else {
        cq.typ = CMDQ_KEY;
        cq.key = '\0'; /* something other than '-' */
    }
    if (cq.typ != CMDQ_KEY || cq.key != '-' || !player_secondary_weapon)
        return ECMD_FAIL;

    oldbknown = player_secondary_weapon->bknown; /* when deciding whether this command
                                   * has done something that takes time,
                                   * behave as if a cursed secondary weapon
                                   * can't be unwielded even though things
                                   * don't work that way... */
    reset_remove_all_armor();
    gc.context.takeoff.what = gc.context.takeoff.mask = WEARING_SECONDARY_WEAPON;
    (void) do_takeoff();
    return (!player_secondary_weapon || player_secondary_weapon->bknown != oldbknown) ? ECMD_TIME : ECMD_OK;
}

staticfn int
menu_remove_armor(int retry)
{
    int n, i = 0;
    menu_item *pick_list;
    boolean all_worn_categories = TRUE;

    if (retry) {
        all_worn_categories = (retry == -2);
    } else if (flags.menu_style == MENU_FULL) {
        all_worn_categories = FALSE;
        n = query_category("What type of things do you want to take off?",
                           gi.invent, (WORN_TYPES | ALL_TYPES
                                    | UNPAID_TYPES | BUCX_TYPES),
                           &pick_list, PICK_ANY);
        if (!n)
            return 0;
        for (i = 0; i < n; i++) {
            if (pick_list[i].item.a_int == ALL_TYPES_SELECTED)
                all_worn_categories = TRUE;
            else
                add_valid_menu_class(pick_list[i].item.a_int);
        }
        free((genericptr_t) pick_list);
    } else if (flags.menu_style == MENU_COMBINATION) {
        unsigned ggofeedback = 0;

        i = ggetobj("take off", select_off, 0, TRUE, &ggofeedback);
        if (ggofeedback & ALL_FINISHED)
            return 0;
        all_worn_categories = (i == -2);
    }
    if (menu_class_present('u')
        || menu_class_present('B') || menu_class_present('U')
        || menu_class_present('C') || menu_class_present('X'))
        all_worn_categories = FALSE;

    n = query_objlist("What do you want to take off?", &gi.invent,
                      (SIGNAL_NOMENU | USE_INVLET | INVORDER_SORT),
                      &pick_list, PICK_ANY,
                      all_worn_categories ? is_worn : is_worn_by_type);
    if (n > 0) {
        for (i = 0; i < n; i++)
            (void) select_off(pick_list[i].item.a_obj);
        free((genericptr_t) pick_list);
    } else if (n < 0 && flags.menu_style != MENU_COMBINATION) {
        There("is nothing else you can remove or unwield.");
    }
    return 0;
}

/* Take off the specific worn object, and if it still exists after that,
   destroy it. (Taking off the item might already destroy it by dunking
   hero into lava.) */
staticfn void
worn_armor_destroyed(struct obj* worn_armor)
{
    struct obj *invobj;
    unsigned wornoid = worn_armor->o_id;

    /* cancel_don() resets 'afternmv' when appropriate but doesn't reset
       uarmc/uarm/&c so doing this now won't interfere with the tests in
       'if (wornarm==uarmc) ... else if (wornarm==uarm) ... else ...' */
    if (donning(worn_armor))
        cancel_don();

    if (worn_armor == player_armor_cloak)
        (void) Cloak_off();
    else if (worn_armor == player_armor)
        (void) Armor_off();
    else if (worn_armor == player_armor_undershirt)
        (void) Shirt_off();
    else if (worn_armor == player_armor_hat)
        (void) Helmet_off();
    else if (worn_armor == player_armor_gloves)
        (void) Gloves_off();
    else if (worn_armor == player_armor_footwear)
        (void) Boots_off();
    else if (worn_armor == player_armor_shield)
        (void) Shield_off();

    /* 'wornarm' might be destroyed as a side-effect of xxx_off() so
       using carried() to check wornarm->where==OBJ_INVENT is not viable;
       scan invent instead; if already freed it shouldn't be possible to
       have re-used the stale memory for a new item yet but verify o_id
       just in case */
    for (invobj = gi.invent; invobj; invobj = invobj->nobj)
        if (invobj == worn_armor && invobj->o_id == wornoid) {
            useup(worn_armor);
            break;
        }
}

/*
 * returns impacted armor with its in_use bit set,
 * or Null. *resisted is updated to reflect whether
 * it resisted or not */
staticfn struct obj *
maybe_destroy_armor(struct obj *armor, struct obj *atmp, boolean *resisted)
{
    if ((armor != 0) && (!atmp || atmp == armor)
        && ((*resisted = obj_resists(armor, 0, 90)) == FALSE)) {
        armor->in_use = 1;
        return armor;
    }
    return (struct obj *) 0;
}

/* hit by destroy armor scroll/black dragon breath/monster spell */
int
destroy_arm(struct obj *atmp)
{
    struct obj *otmp = (struct obj *) 0;
    boolean losing_gloves = FALSE, resisted = FALSE,
            resistedc = FALSE, resistedsuit = FALSE;
    /*
     * Note: if the cloak resisted, then the suit or shirt underneath
     * wouldn't be impacted either. Likewise, if the suit resisted, the
     * shirt underneath wouldn't be impacted. Since there are no artifact
     * cloaks or suits right now, this is unlikely to come into effect,
     * but it should behave appropriately if/when the situation changes.
     */

    if ((otmp = maybe_destroy_armor(player_armor_cloak, atmp, &resistedc)) != 0) {
        urgent_pline("Your %s crumbles and turns to dust!",
                     /* cloak/robe/apron/smock (ID'd apron)/wrapping */
                     cloak_simple_name(otmp));
    } else if (!resistedc
             && (otmp = maybe_destroy_armor(player_armor, atmp, &resistedsuit)) != 0) {
        const char *suit = suit_simple_name(otmp);

        /* for gold DSM, we don't want Armor_gone() to report that it
           stops shining _after_ we've been told that it is destroyed */
        if (otmp->lamplit)
            end_burn(otmp, FALSE);
        urgent_pline("Your %s %s to dust and %s to the %s!",
                     /* suit might be "dragon scales" so vtense() is needed */
                     suit, vtense(suit, "turn"), vtense(suit, "fall"),
                     surface(u.ux, u.uy));
    } else if (!resistedc && !resistedsuit
             && (otmp = maybe_destroy_armor(player_armor_undershirt, atmp, &resisted)) != 0) {
        urgent_pline("Your %s crumbles into tiny threads and falls apart!",
                     shirt_simple_name(otmp)); /* always "shirt" */
    } else if ((otmp = maybe_destroy_armor(player_armor_hat, atmp, &resisted)) != 0) {
        urgent_pline("Your %s turns to dust and is blown away!",
                     helm_simple_name(otmp)); /* "helm" or "hat" */
    } else if ((otmp = maybe_destroy_armor(player_armor_gloves, atmp, &resisted)) != 0) {
        urgent_pline("Your %s vanish!", gloves_simple_name(otmp));
        losing_gloves = TRUE;
    } else if ((otmp = maybe_destroy_armor(player_armor_footwear, atmp, &resisted)) != 0) {
        urgent_pline("Your %s disintegrate!", boots_simple_name(otmp));
    } else if ((otmp = maybe_destroy_armor(player_armor_shield, atmp, &resisted)) != 0) {
        urgent_pline("Your %s crumbles away!", shield_simple_name(otmp));
    } else {
        return 0; /* could not destroy anything */
    }

    /* cancel_don() if applicable, Cloak_off()/Armor_off()/&c, and useup() */
    worn_armor_destroyed(otmp);
    /* glove loss means wielded weapon will be touched */
    if (losing_gloves)
        selftouch("You");

    stop_occupation();
    return 1;
}

void
adj_abon(struct obj *otmp, schar delta)
{
    if (player_armor_gloves && player_armor_gloves == otmp && otmp->otyp == GAUNTLETS_OF_DEXTERITY) {
        if (delta) {
            makeknown(player_armor_gloves->otyp);
            ATTRIBUTE_BONUS(ATTRIBUTE_DEXTERITY) += (delta);
        }
        disp.bottom_line = TRUE;
    }
    if (player_armor_hat && player_armor_hat == otmp && otmp->otyp == HELM_OF_BRILLIANCE) {
        if (delta) {
            makeknown(player_armor_hat->otyp);
            ATTRIBUTE_BONUS(ATTRIBUTE_INTELLIGENCE) += (delta);
            ATTRIBUTE_BONUS(ATTRIBUTE_WISDOM) += (delta);
        }
        disp.bottom_line = TRUE;
    }
}

/* decide whether a worn item is covered up by some other worn item,
   used for dipping into liquid and applying grease;
   some criteria are different than select_off()'s */
boolean
inaccessible_equipment(struct obj *obj,
                       const char *verb, /* "dip" or "grease", or null to
                                             avoid messages */
                       boolean only_if_known_cursed) /* ignore covering unless
                                                        known to be cursed */
{
    static NEARDATA const char need_to_take_off_outer_armor[] =
        "need to take off %s to %s %s.";
    char buf[BUFSZ];
    boolean anycovering = !only_if_known_cursed; /* more comprehensible... */
#define BLOCKSACCESS(x) (anycovering || ((x)->cursed && (x)->bknown))

    if (!obj || !obj->owornmask)
        return FALSE; /* not inaccessible */

    /* check for suit covered by cloak */
    if (obj == player_armor && player_armor_cloak && BLOCKSACCESS(player_armor_cloak)) {
        if (verb) {
            Strcpy(buf, yname(player_armor_cloak));
            You(need_to_take_off_outer_armor, buf, verb, yname(obj));
        }
        return TRUE;
    }
    /* check for shirt covered by suit and/or cloak */
    if (obj == player_armor_undershirt
        && ((player_armor && BLOCKSACCESS(player_armor)) || (player_armor_cloak && BLOCKSACCESS(player_armor_cloak)))) {
        if (verb) {
            char cloaktmp[QBUFSZ], suittmp[QBUFSZ];
            /* if sameprefix, use yname and xname to get "your cloak and suit"
               or "Manlobbi's cloak and suit"; otherwise, use yname and yname
               to get "your cloak and Manlobbi's suit" or vice versa */
            boolean sameprefix = (player_armor && player_armor_cloak
                                  && !strcmp(shk_your(cloaktmp, player_armor_cloak),
                                             shk_your(suittmp, player_armor)));

            *buf = '\0';
            if (player_armor_cloak)
                Strcat(buf, yname(player_armor_cloak));
            if (player_armor && player_armor_cloak)
                Strcat(buf, " and ");
            if (player_armor)
                Strcat(buf, sameprefix ? xname(player_armor) : yname(player_armor));
            You(need_to_take_off_outer_armor, buf, verb, yname(obj));
        }
        return TRUE;
    }
    /* check for ring covered by gloves */
    if ((obj == player_finger_left || obj == player_finger_right) && player_armor_gloves && BLOCKSACCESS(player_armor_gloves)) {
        if (verb) {
            Strcpy(buf, yname(player_armor_gloves));
            You(need_to_take_off_outer_armor, buf, verb, yname(obj));
        }
        return TRUE;
    }
    /* item is not inaccessible */
    return FALSE;
}

/* not a getobj callback - unifies code among the other 4 getobj callbacks */
staticfn int
equip_ok(struct obj *obj, boolean removing, boolean accessory)
{
    boolean is_worn;
    long dummymask = 0;

    if (!obj)
        return GETOBJ_EXCLUDE;

    /* ignore for putting on if already worn, or removing if not worn */
    is_worn = ((obj->owornmask & (WEARING_ARMOR | WEARING_ACCESSORY)) != 0);
    if (removing ^ is_worn)
        return GETOBJ_EXCLUDE_INACCESS;

    /* exclude most object classes outright */
    if (obj->oclass != ARMOR_CLASS && obj->oclass != RING_CLASS
        && obj->oclass != AMULET_CLASS) {
        /* ... except for a few wearable exceptions outside these classes */
        if (obj->otyp != MEAT_RING && obj->otyp != BLINDFOLD
            && obj->otyp != TOWEL && obj->otyp != LENSES)
            return GETOBJ_EXCLUDE;
    }

    /* armor with 'P' or 'R' or accessory with 'W' or 'T' */
    if (accessory ^ (obj->oclass != ARMOR_CLASS))
        return GETOBJ_DOWNPLAY;

    /* armor we can't wear, e.g. from polyform */
    if (obj->oclass == ARMOR_CLASS && !removing
        && !canwearobj(obj, &dummymask, FALSE))
        return GETOBJ_DOWNPLAY;

    /* Possible extension: downplay items (both accessories and armor) which
     * can't be worn because the slot is filled with something else. */

    /* removing inaccessible equipment */
    if (removing && inaccessible_equipment(obj, (const char *) 0,
                                           (obj->oclass == RING_CLASS)))
        return GETOBJ_EXCLUDE_INACCESS;

    /* all good to go */
    return GETOBJ_SUGGEST;
}

/* getobj callback for P command */
staticfn int
puton_ok(struct obj *obj)
{
    return equip_ok(obj, FALSE, TRUE);
}

/* getobj callback for R command */
staticfn int
remove_ok(struct obj *obj)
{
    return equip_ok(obj, TRUE, TRUE);
}

/* getobj callback for W command */
staticfn int
wear_ok(struct obj *obj)
{
    return equip_ok(obj, FALSE, FALSE);
}

/* getobj callback for T command */
staticfn int
takeoff_ok(struct obj *obj)
{
    return equip_ok(obj, TRUE, FALSE);
}

/*do_wear.c*/
