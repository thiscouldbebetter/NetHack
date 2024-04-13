/* NetHack 3.7	wield.c	$NHDT-Date: 1707525193 2024/02/10 00:33:13 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.110 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2009. */
/* NetHack may be freely redistributed.  See license for details. */

/* Modified by This Could Be Better, 2024. */

#include "hack.h"

/* KMH -- Differences between the three weapon slots.
 *
 * The main weapon (uwep):
 * 1.  Is filled by the (w)ield command.
 * 2.  Can be filled with any type of item.
 * 3.  May be carried in one or both hands.
 * 4.  Is used as the melee weapon and as the launcher for
 *     ammunition.
 * 5.  Only conveys intrinsics when it is a weapon, weapon-tool,
 *     or artifact.
 * 6.  Certain cursed items will weld to the hand and cannot be
 *     unwielded or dropped.  See erodeable_wep() and will_weld()
 *     below for the list of which items apply.
 *
 * The secondary weapon (uswapwep):
 * 1.  Is filled by the e(x)change command, which swaps this slot
 *     with the main weapon.  If the "pushweapon" option is set,
 *     the (w)ield command will also store the old weapon in the
 *     secondary slot.
 * 2.  Can be filled with anything that will fit in the main weapon
 *     slot; that is, any type of item.
 * 3.  Is usually NOT considered to be carried in the hands.
 *     That would force too many checks among the main weapon,
 *     second weapon, shield, gloves, and rings; and it would
 *     further be complicated by bimanual weapons.  A special
 *     exception is made for two-weapon combat.
 * 4.  Is used as the second weapon for two-weapon combat, and as
 *     a convenience to swap with the main weapon.
 * 5.  Never conveys intrinsics.
 * 6.  Cursed items never weld (see #3 for reasons), but they also
 *     prevent two-weapon combat.
 *
 * The quiver (uquiver):
 * 1.  Is filled by the (Q)uiver command.
 * 2.  Can be filled with any type of item.
 * 3.  Is considered to be carried in a special part of the pack.
 * 4.  Is used as the item to throw with the (f)ire command.
 *     This is a convenience over the normal (t)hrow command.
 * 5.  Never conveys intrinsics.
 * 6.  Cursed items never weld; their effect is handled by the normal
 *     throwing code.
 * 7.  The autoquiver option will fill it with something deemed
 *     suitable if (f)ire is used when it's empty.
 *
 * No item may be in more than one of these slots.
 */

staticfn boolean cant_wield_corpse(struct obj *) NONNULLARG1;
staticfn int ready_weapon(struct obj *) NO_NNARGS;
staticfn int ready_ok(struct obj *) NO_NNARGS;
staticfn int wield_ok(struct obj *) NO_NNARGS;

/* used by will_weld() */
/* probably should be renamed */
#define erodeable_wep(optr)                             \
    ((optr)->oclass == WEAPON_CLASS || is_weptool(optr) \
     || (optr)->otyp == HEAVY_IRON_BALL || (optr)->otyp == IRON_CHAIN)

/* used by welded(), and also while wielding */
#define will_weld(optr) \
    ((optr)->cursed && (erodeable_wep(optr) || (optr)->otyp == TIN_OPENER))

/* to dual-wield, 'obj' must be a weapon or a weapon-tool, and not a bow
   or arrow or missile (dart, shuriken, boomerang), so not matching the
   one-handed weapons which yield "you begin bashing" if used for melee;
   empty hands and two-handed weapons have to be handled separately */
#define TWOWEAPOK(obj) \
    (((obj)->oclass == WEAPON_CLASS)                            \
     ? !(is_launcher(obj) || is_ammo(obj) || is_missile(obj))   \
     : is_weptool(obj))

static const char
    are_no_longer_twoweap[] = "are no longer using two weapons at once",
    can_no_longer_twoweap[] = "can no longer wield two weapons at once";

/*** Functions that place a given item in a slot ***/
/* Proper usage includes:
 * 1.  Initializing the slot during character generation or a
 *     restore.
 * 2.  Setting the slot due to a player's actions.
 * 3.  If one of the objects in the slot are split off, these
 *     functions can be used to put the remainder back in the slot.
 * 4.  Putting an item that was thrown and returned back into the slot.
 * 5.  Emptying the slot, by passing a null object.  NEVER pass
 *     cg.zeroobj!
 *
 * If the item is being moved from another slot, it is the caller's
 * responsibility to handle that.  It's also the caller's responsibility
 * to print the appropriate messages.
 */
void
setuwep(struct obj *obj)
{
    struct obj *olduwep = player_weapon;

    if (obj == player_weapon)
        return; /* necessary to not set gu.unweapon */
    /* This message isn't printed in the caller because it happens
     * *whenever* Sunsword is unwielded, from whatever cause.
     */
    setworn(obj, WEARING_WEAPON);
    if (player_weapon == obj && artifact_light(olduwep) && olduwep->lamplit) {
        end_burn(olduwep, FALSE);
        if (!Blind)
            pline("%s shining.", Tobjnam(olduwep, "stop"));
    }
    if (player_weapon == obj
        && (u_wield_art(ART_OGRESMASHER)
            || is_art(olduwep, ART_OGRESMASHER)))
        disp.bottom_line = TRUE;
    /* Note: Explicitly wielding a pick-axe will not give a "bashing"
     * message.  Wielding one via 'a'pplying it will.
     * 3.2.2:  Wielding arbitrary objects will give bashing message too.
     */
    if (obj) {
        gu.unweapon = (obj->oclass == WEAPON_CLASS)
                       ? is_launcher(obj) || is_ammo(obj) || is_missile(obj)
                             || (is_pole(obj) && !u.monster_being_ridden)
                       : !is_weptool(obj) && !is_wet_towel(obj);
    } else
        gu.unweapon = TRUE; /* for "bare hands" message */
}

staticfn boolean
cant_wield_corpse(struct obj *obj)
{
    char kbuf[BUFSZ];

    if (player_armor_gloves || obj->otyp != CORPSE || !touch_petrifies(&mons[obj->corpsenm])
        || Stone_resistance)
        return FALSE;

    /* Prevent wielding cockatrice when not wearing gloves --KAA */
    You("wield %s in your bare %s.",
        corpse_xname(obj, (const char *) 0, CXN_PFX_THE),
        makeplural(body_part(HAND)));
    Sprintf(kbuf, "wielding %s bare-handed", killer_xname(obj));
    instapetrify(kbuf);
    return TRUE;
}

/* description of hands when not wielding anything; also used
   by #seeweapon (')'), #attributes (^X), and #takeoffall ('A') */
const char *
empty_handed(void)
{
    return player_armor_gloves ? "empty handed" /* gloves imply hands */
           : humanoid(gy.youmonst.data)
             /* hands but no weapon and no gloves */
             ? "bare handed"
               /* alternate phrasing for paws or lack of hands */
               : "not wielding anything";
}

staticfn int
ready_weapon(struct obj *wep)
{
    /* Separated function so swapping works easily */
    int res = ECMD_OK;
    boolean was_twoweap = u.using_two_weapons, had_wep = (player_weapon != 0);

    if (!wep) {
        /* No weapon */
        if (player_weapon) {
            You("are %s.", empty_handed());
            setuwep((struct obj *) 0);
            res = ECMD_TIME;
        } else
            You("are already %s.", empty_handed());
    } else if (wep->otyp == CORPSE && cant_wield_corpse(wep)) {
        /* hero must have been life-saved to get here; use a turn */
        res = ECMD_TIME; /* corpse won't be wielded */
    } else if (player_armor_shield && bimanual(wep)) {
        You("cannot wield a two-handed %s while wearing a shield.",
            is_sword(wep) ? "sword" : wep->otyp == BATTLE_AXE ? "axe"
                                                              : "weapon");
        res = ECMD_FAIL;
    } else if (!retouch_object(&wep, FALSE)) {
        res = ECMD_TIME; /* takes a turn even though it doesn't get wielded */
    } else {
        /* Weapon WILL be wielded after this point */
        res = ECMD_TIME;
        if (will_weld(wep)) {
            const char *tmp = xname(wep), *thestr = "The ";

            if (strncmp(tmp, thestr, 4) && !strncmp(The(tmp), thestr, 4))
                tmp = thestr;
            else
                tmp = "";
            pline("%s%s %s to your %s%s!", tmp, aobjnam(wep, "weld"),
                  (wep->quan == 1L) ? "itself" : "themselves", /* a3 */
                  bimanual(wep) ? "" :
                      (URIGHTY ? "dominant right " : "dominant left "),
                  bimanual(wep) ? (const char *) makeplural(body_part(HAND))
                                : body_part(HAND));
            set_bknown(wep, 1);
        } else {
            /* The message must be printed before setuwep (since
             * you might die and be revived from changing weapons),
             * and the message must be before the death message and
             * Lifesaved rewielding.  Yet we want the message to
             * say "weapon in hand", thus this kludge.
             * [That comment is obsolete.  It dates from the days (3.0)
             * when unwielding Firebrand could cause hero to be burned
             * to death in Hell due to loss of fire resistance.
             * "Lifesaved re-wielding or re-wearing" is ancient history.]
             */
            long dummy = wep->owornmask;

            wep->owornmask |= WEARING_WEAPON;
            if (wep->otyp == AKLYS && (wep->owornmask & WEARING_WEAPON) != 0)
                You("secure the tether.");
            prinv((char *) 0, wep, 0L);
            wep->owornmask = dummy;
        }

        setuwep(wep);
        if (was_twoweap && !u.using_two_weapons && flags.verbose) {
            /* skip this message if we already got "empty handed" one above;
               also, Null is not safe for neither TWOWEAPOK() or bimanual() */
            if (player_weapon)
                You("%s.", ((TWOWEAPOK(player_weapon) && !bimanual(player_weapon))
                            ? are_no_longer_twoweap
                            : can_no_longer_twoweap));
        }

        /* KMH -- Talking artifacts are finally implemented */
        if (wep->oartifact) {
            res |= arti_speak(wep); /* sets ECMD_TIME bit if artifact speaks */
        }

        if (artifact_light(wep) && !wep->lamplit) {
            begin_burn(wep, FALSE);
            if (!Blind)
                pline("%s to shine %s!", Tobjnam(wep, "begin"),
                      arti_light_description(wep));
        }
#if 0
        /* we'll get back to this someday, but it's not balanced yet */
        if (Race_if(PM_ELF) && !wep->oartifact
            && objects[wep->otyp].oc_material == IRON) {
            /* Elves are averse to wielding cold iron */
            You("have an uneasy feeling about wielding cold iron.");
            change_luck(-1);
        }
#endif
        if (wep->unpaid) {
            struct monster *this_shkp;

            if ((this_shkp = shop_keeper(inside_shop(u.ux, u.uy)))
                != (struct monster *) 0) {
                pline("%s says \"You be careful with my %s!\"",
                      shkname(this_shkp), xname(wep));
            }
        }
    }
    if ((had_wep != (player_weapon != 0)) && condtests[bl_bareh].enabled)
        disp.bottom_line = TRUE;
    return res;
}

void
setuqwep(struct obj *obj)
{
    setworn(obj, WEARING_QUIVER);
    /* no extra handling needed; this used to include a call to
       update_inventory() but that's already performed by setworn() */
    return;
}

void
setuswapwep(struct obj *obj)
{
    setworn(obj, WEARING_SECONDARY_WEAPON);
    return;
}

/* getobj callback for object to ready for throwing/shooting;
   this filter lets worn items through so that caller can reject them */
staticfn int
ready_ok(struct obj *obj)
{
    if (!obj) /* '-', will empty quiver slot if chosen */
        return player_quiver ? GETOBJ_SUGGEST : GETOBJ_DOWNPLAY;

    /* downplay when wielded, unless more than one */
    if (obj == player_weapon || (obj == player_secondary_weapon && u.using_two_weapons))
        return (obj->quan == 1) ? GETOBJ_DOWNPLAY : GETOBJ_SUGGEST;

    if (is_ammo(obj)) {
        return ((player_weapon && ammo_and_launcher(obj, player_weapon))
                || (player_secondary_weapon && ammo_and_launcher(obj, player_secondary_weapon)))
                ? GETOBJ_SUGGEST
                : GETOBJ_DOWNPLAY;
    } else if (is_launcher(obj)) { /* part of 'possible extension' below */
        return GETOBJ_DOWNPLAY;
    } else {
        if (obj->oclass == WEAPON_CLASS || obj->oclass == COIN_CLASS)
            return GETOBJ_SUGGEST;
        /* Possible extension: exclude weapons that make no sense to throw,
           such as whips, bows, slings, rubber hoses. */
    }

#if 0   /* superseded by ammo_and_launcher handling above */
    /* Include gems/stones as likely candidates if either primary
       or secondary weapon is a sling. */
    if (obj->oclass == GEM_CLASS
        && (uslinging()
            || (player_secondary_weapon && objects[player_secondary_weapon->otyp].oc_skill == P_SLING)))
        return GETOBJ_SUGGEST;
#endif

    return GETOBJ_DOWNPLAY;
}

/* getobj callback for object to wield */
staticfn int
wield_ok(struct obj *obj)
{
    if (!obj)
        return GETOBJ_SUGGEST;

    if (obj->oclass == COIN_CLASS)
        return GETOBJ_EXCLUDE;

    if (obj->oclass == WEAPON_CLASS || is_weptool(obj))
        return GETOBJ_SUGGEST;

    return GETOBJ_DOWNPLAY;
}

/* the #wield command - wield a weapon */
int
dowield(void)
{
    char qbuf[QBUFSZ];
    struct obj *wep, *oldwep;
    boolean finish_splitting = FALSE;
    int result;

    /* May we attempt this? */
    gm.multi = 0;
    if (cantwield(gy.youmonst.data)) {
        pline("Don't be ridiculous!");
        return ECMD_FAIL;
    }
    /* Keep going even if inventory is completely empty, since wielding '-'
       to wield nothing can be construed as a positive act even when done
       so redundantly. */

    /* Prompt for a new weapon */
    clear_splitobjs();
    if (!(wep = getobj("wield", wield_ok, GETOBJ_PROMPT | GETOBJ_ALLOWCNT))) {
        /* Cancelled */
        return ECMD_CANCEL;
    } else if (wep == player_weapon) {
 already_wielded:
        You("are already wielding that!");
        if (is_weptool(wep) || is_wet_towel(wep))
            gu.unweapon = FALSE; /* [see setuwep()] */
        return ECMD_FAIL;
    } else if (welded(player_weapon)) {
        weldmsg(player_weapon);
        /* previously interrupted armor removal mustn't be resumed */
        reset_remove_all_armor();
        /* if player chose a partial stack but can't wield it, undo split */
        if (wep->o_id && wep->o_id == gc.context.objsplit.child_oid)
            unsplitobj(wep);
        return ECMD_FAIL;
    } else if (wep->o_id && wep->o_id == gc.context.objsplit.child_oid) {
        /* if wep is the result of supplying a count to getobj()
           we don't want to split something already wielded; for
           any other item, we need to give it its own inventory slot */
        if (player_weapon && player_weapon->o_id == gc.context.objsplit.parent_oid) {
            unsplitobj(wep);
            /* wep was merged back to uwep, already_wielded uses wep */
            wep = player_weapon;
            goto already_wielded;
        }
        finish_splitting = TRUE;
        goto wielding;
    }

    /* Handle no object, or object in other slot */
    if (wep == &hands_obj) {
        wep = (struct obj *) 0;
    } else if (wep == player_secondary_weapon) {
        return doswapweapon();
    } else if (wep == player_quiver) {
        /* offer to split stack if multiple are quivered */
        if (player_quiver->quan > 1L && inv_cnt(FALSE) < invlet_basic
                                    && splittable(player_quiver)) {
            Sprintf(qbuf, "You have %ld %s readied.  Wield one?",
                    player_quiver->quan, simpleonames(player_quiver));
            switch (ynq(qbuf)) {
            case 'q':
                return ECMD_OK;
            case 'y':
                /* leave N-1 quivered, split off 1 to wield */
                wep = splitobj(player_quiver, 1L);
                finish_splitting = TRUE;
                goto wielding;
            default:
                break;
            }
            Strcpy(qbuf, "Wield all of them instead?");
        } else {
            boolean use_plural = (is_plural(player_quiver) || pair_of(player_quiver));

            Sprintf(qbuf, "You have %s readied.  Wield %s instead?",
                    !use_plural ? "that" : "those",
                    !use_plural ? "it" : "them");
        }
        /* require confirmation to wield the quivered weapon */
        if (ynq(qbuf) != 'y') {
            (void) Shk_Your(qbuf, player_quiver); /* replace qbuf[] contents */
            pline("%s%s %s readied.", qbuf,
                  simpleonames(player_quiver), otense(player_quiver, "remain"));
            return ECMD_OK;
        }
        /* wielding whole readied stack, so no longer quivered */
        setuqwep((struct obj *) 0);
    } else if (wep->owornmask & (WEARING_ARMOR | WEARING_ACCESSORY | WEARING_SADDLE)) {
        You("cannot wield that!");
        return ECMD_FAIL;
    }

 wielding:
    if (finish_splitting) {
        /* wep was split off from something; give it its own invlet */
        freeinv(wep);
        addinv_nomerge(wep);
    }

    /* Set your new primary weapon */
    oldwep = player_weapon;
    result = ready_weapon(wep);
    if (flags.pushweapon && oldwep && player_weapon != oldwep)
        setuswapwep(oldwep);
    untwoweapon();

    return result;
}

/* the #swap command - swap wielded and secondary weapons */
int
doswapweapon(void)
{
    struct obj *oldwep, *oldswap;
    int result = 0;

    /* May we attempt this? */
    gm.multi = 0;
    if (cantwield(gy.youmonst.data)) {
        pline("Don't be ridiculous!");
        return ECMD_FAIL;
    }
    if (welded(player_weapon)) {
        weldmsg(player_weapon);
        return ECMD_FAIL;
    }

    /* Unwield your current secondary weapon */
    oldwep = player_weapon;
    oldswap = player_secondary_weapon;
    setuswapwep((struct obj *) 0);

    /* Set your new primary weapon */
    result = ready_weapon(oldswap);

    /* Set your new secondary weapon */
    if (player_weapon == oldwep) {
        /* Wield failed for some reason */
        setuswapwep(oldswap);
    } else {
        setuswapwep(oldwep);
        if (player_secondary_weapon)
            prinv((char *) 0, player_secondary_weapon, 0L);
        else
            You("have no secondary weapon readied.");
    }

    if (u.using_two_weapons && !can_twoweapon())
        untwoweapon();

    return result;
}

/* the #quiver command */
int
dowieldquiver(void)
{
    return doquiver_core("ready");
}

/* guts of #quiver command; also used by #fire when refilling empty quiver */
int
doquiver_core(const char *verb) /* "ready" or "fire" */
{
    char qbuf[QBUFSZ];
    struct obj *newquiver;
    int res;
    boolean finish_splitting = FALSE,
            was_uwep = FALSE, was_twoweap = u.using_two_weapons;

    /* Since the quiver isn't in your hands, don't check cantwield(),
       will_weld(), touch_petrifies(), etc. */
    gm.multi = 0;
    if (!gi.invent) {
        /* could accept '-' to empty quiver, but there's no point since
           inventory is empty so uquiver is already Null */
        You("have nothing to ready for firing.");
        return ECMD_OK;
    }

    /* forget last splitobj() before calling getobj() with GETOBJ_ALLOWCNT */
    clear_splitobjs();
    /* Prompt for a new quiver: "What do you want to {ready|fire}?" */
    newquiver = getobj(verb, ready_ok, GETOBJ_PROMPT | GETOBJ_ALLOWCNT);

    if (!newquiver) {
        /* Cancelled */
        return ECMD_CANCEL;
    } else if (newquiver == &hands_obj) { /* no object */
        /* Explicitly nothing */
        if (player_quiver) {
            You("now have no ammunition readied.");
            /* skip 'quivering: prinv()' */
            setuqwep((struct obj *) 0);
        } else {
            You("already have no ammunition readied!");
        }
        return ECMD_OK;
    } else if (newquiver->o_id == gc.context.objsplit.child_oid) {
        /* if newquiver is the result of supplying a count to getobj()
           we don't want to split something already in the quiver;
           for any other item, we need to give it its own inventory slot */
        if (player_quiver && player_quiver->o_id == gc.context.objsplit.parent_oid) {
            unsplitobj(newquiver);
            goto already_quivered;
        } else if (newquiver->oclass == COIN_CLASS) {
            /* don't allow splitting a stack of coins into quiver */
            You("can't ready only part of your gold.");
            unsplitobj(newquiver);
            return ECMD_OK;
        }
        finish_splitting = TRUE;
    } else if (newquiver == player_quiver) {
 already_quivered:
        pline("That ammunition is already readied!");
        return ECMD_OK;
    } else if (newquiver->owornmask & (WEARING_ARMOR | WEARING_ACCESSORY | WEARING_SADDLE)) {
        You("cannot %s that!", verb);
        return ECMD_OK;
    } else if (newquiver == player_weapon) {
        int weld_res = !player_weapon->bknown;

        if (welded(player_weapon)) {
            weldmsg(player_weapon);
            reset_remove_all_armor(); /* same as dowield() */
            return weld_res ? ECMD_TIME : ECMD_OK;
        }
        /* offer to split stack if wielding more than 1 */
        if (player_weapon->quan > 1L && inv_cnt(FALSE) < invlet_basic
                                    && splittable(player_weapon)) {
            Sprintf(qbuf, "You are wielding %ld %s.  Ready %ld of them?",
                    player_weapon->quan, simpleonames(player_weapon), player_weapon->quan - 1L);
            switch (ynq(qbuf)) {
            case 'q':
                return ECMD_OK;
            case 'y':
                /* leave 1 wielded, split rest off and put into quiver */
                newquiver = splitobj(player_weapon, player_weapon->quan - 1L);
                finish_splitting = TRUE;
                goto quivering;
            default:
                break;
            }
            Strcpy(qbuf, "Ready all of them instead?");
        } else {
            boolean use_plural = (is_plural(player_weapon) || pair_of(player_weapon));

            Sprintf(qbuf, "You are wielding %s.  Ready %s instead?",
                    !use_plural ? "that" : "those",
                    !use_plural ? "it" : "them");
        }
        /* require confirmation to ready the main weapon */
        if (ynq(qbuf) != 'y') {
            (void) Shk_Your(qbuf, player_weapon); /* replace qbuf[] contents */
            pline("%s%s %s wielded.", qbuf,
                  simpleonames(player_weapon), otense(player_weapon, "remain"));
            return ECMD_OK;
        }
        /* quivering main weapon, so no longer wielding it */
        setuwep((struct obj *) 0);
        untwoweapon();
        was_uwep = TRUE;
    } else if (newquiver == player_secondary_weapon) {
        if (player_secondary_weapon->quan > 1L && inv_cnt(FALSE) < invlet_basic
            && splittable(player_secondary_weapon)) {
            Sprintf(qbuf, "%s %ld %s.  Ready %ld of them?",
                    u.using_two_weapons ? "You are dual wielding"
                              : "Your alternate weapon is",
                    player_secondary_weapon->quan, simpleonames(player_secondary_weapon),
                    player_secondary_weapon->quan - 1L);
            switch (ynq(qbuf)) {
            case 'q':
                return ECMD_OK;
            case 'y':
                /* leave 1 alt-wielded, split rest off and put into quiver */
                newquiver = splitobj(player_secondary_weapon, player_secondary_weapon->quan - 1L);
                finish_splitting = TRUE;
                goto quivering;
            default:
                break;
            }
            Strcpy(qbuf, "Ready all of them instead?");
        } else {
            boolean use_plural = (is_plural(player_secondary_weapon) || pair_of(player_secondary_weapon));

            Sprintf(qbuf, "%s your %s weapon.  Ready %s instead?",
                    !use_plural ? "That is" : "Those are",
                    u.using_two_weapons ? "second" : "alternate",
                    !use_plural ? "it" : "them");
        }
        /* require confirmation to ready the alternate weapon */
        if (ynq(qbuf) != 'y') {
            (void) Shk_Your(qbuf, player_secondary_weapon); /* replace qbuf[] contents */
            pline("%s%s %s %s.", qbuf,
                  simpleonames(player_secondary_weapon), otense(player_secondary_weapon, "remain"),
                  u.using_two_weapons ? "wielded" : "as secondary weapon");
            return ECMD_OK;
        }
        /* quivering alternate weapon, so no more uswapwep */
        setuswapwep((struct obj *) 0);
        untwoweapon();
    }

 quivering:
    if (finish_splitting) {
        freeinv(newquiver);
        addinv_nomerge(newquiver);
    }

    if (!strcmp(verb, "ready")) {
        /* place item in quiver before printing so that inventory feedback
           includes "(at the ready)" */
        setuqwep(newquiver);
        prinv((char *) 0, newquiver, 0L);
    } else { /* verb=="fire", manually refilling quiver during 'f'ire */
        /* prefix item with description of action, so don't want that to
           include "(at the ready)" */
        prinv("You ready:", newquiver, 0L);
        setuqwep(newquiver);
    }

    /* quiver is a convenience slot and manipulating it ordinarily
       consumes no time, but unwielding primary or secondary weapon
       should take time (perhaps we're adjacent to a rust monster
       or disenchanter and want to hit it immediately, but not with
       something we're wielding that's vulnerable to its damage) */
    res = 0;
    if (was_uwep) {
        You("are now %s.", empty_handed());
        res = 1;
    } else if (was_twoweap && !u.using_two_weapons) {
        You("%s.", are_no_longer_twoweap);
        res = 1;
    }
    return res ? ECMD_TIME : ECMD_OK;
}

/* used for #rub and for applying pick-axe, whip, grappling hook or polearm */
boolean
wield_tool(struct obj *obj,
           const char *verb) /* "rub",&c */
{
    const char *what;
    boolean more_than_1;

    if (player_weapon && obj == player_weapon)
        return TRUE; /* nothing to do if already wielding it */

    if (!verb)
        verb = "wield";
    what = xname(obj);
    more_than_1 = (obj->quan > 1L || strstri(what, "pair of ") != 0
                   || strstri(what, "s of ") != 0);

    if (obj->owornmask & (WEARING_ARMOR | WEARING_ACCESSORY)) {
        You_cant("%s %s while wearing %s.", verb, yname(obj),
                 more_than_1 ? "them" : "it");
        return FALSE;
    }
    if (player_weapon && welded(player_weapon)) {
        if (flags.verbose) {
            const char *hand = body_part(HAND);

            if (bimanual(player_weapon))
                hand = makeplural(hand);
            if (strstri(what, "pair of ") != 0)
                more_than_1 = FALSE;
            pline(
               "Since your weapon is welded to your %s, you cannot %s %s %s.",
                  hand, verb, more_than_1 ? "those" : "that", xname(obj));
        } else {
            You_cant("do that.");
        }
        return FALSE;
    }
    if (cantwield(gy.youmonst.data)) {
        You_cant("hold %s strongly enough.", more_than_1 ? "them" : "it");
        return FALSE;
    }
    /* check shield */
    if (player_armor_shield && bimanual(obj)) {
        You("cannot %s a two-handed %s while wearing a shield.", verb,
            (obj->oclass == WEAPON_CLASS) ? "weapon" : "tool");
        return FALSE;
    }

    if (player_quiver == obj)
        setuqwep((struct obj *) 0);
    if (player_secondary_weapon == obj) {
        (void) doswapweapon();
        /* doswapweapon might fail */
        if (player_secondary_weapon == obj)
            return FALSE;
    } else {
        struct obj *oldwep = player_weapon;

        if (will_weld(obj)) {
            /* hope none of ready_weapon()'s early returns apply here... */
            (void) ready_weapon(obj);
        } else {
            You("now wield %s.", doname(obj));
            setuwep(obj);
        }
        if (flags.pushweapon && oldwep && player_weapon != oldwep)
            setuswapwep(oldwep);
    }
    if (player_weapon && player_weapon != obj)
        return FALSE; /* rewielded old object after dying */
    /* applying weapon or tool that gets wielded ends two-weapon combat */
    if (u.using_two_weapons)
        untwoweapon();
    if (obj->oclass != WEAPON_CLASS)
        gu.unweapon = TRUE;
    return TRUE;
}

int
can_twoweapon(void)
{
    struct obj *otmp;

    if (!could_twoweap(gy.youmonst.data)) {
        if (Upolyd)
            You_cant("use two weapons in your current form.");
        else
            pline("%s aren't able to use two weapons at once.",
                  makeplural((flags.female && gu.urole.name.f)
                             ? gu.urole.name.f : gu.urole.name.m));
    } else if (!player_weapon || !player_secondary_weapon) {
        const char *hand_s = body_part(HAND);

        if (!player_weapon && !player_secondary_weapon)
            hand_s = makeplural(hand_s);
        /* "your hands are empty" or "your {left|right} hand is empty" */
        Your("%s%s %s empty.", player_weapon ? "left " : player_secondary_weapon ? "right " : "",
             hand_s, vtense(hand_s, "are"));
    } else if (!TWOWEAPOK(player_weapon) || !TWOWEAPOK(player_secondary_weapon)) {
        otmp = !TWOWEAPOK(player_weapon) ? player_weapon : player_secondary_weapon;
        pline("%s %s suitable %s weapon%s.", Yname2(otmp),
              is_plural(otmp) ? "aren't" : "isn't a",
              (otmp == player_weapon) ? "primary" : "secondary",
              plur(otmp->quan));
    } else if (bimanual(player_weapon) || bimanual(player_secondary_weapon)) {
        otmp = bimanual(player_weapon) ? player_weapon : player_secondary_weapon;
        pline("%s isn't one-handed.", Yname2(otmp));
    } else if (player_armor_shield) {
        You_cant("use two weapons while wearing a shield.");
    } else if (player_secondary_weapon->oartifact) {
        pline("%s being held second to another weapon!",
              Yobjnam2(player_secondary_weapon, "resist"));
    } else if (player_secondary_weapon->otyp == CORPSE && cant_wield_corpse(player_secondary_weapon)) {
        /* [Note: !TWOWEAPOK() check prevents ever getting here...] */
        ; /* must be life-saved to reach here; return FALSE */
    } else if (Glib || player_secondary_weapon->cursed) {
        if (!Glib)
            set_bknown(player_secondary_weapon, 1);
        drop_uswapwep();
    } else
        return TRUE;
    return FALSE;
}

/* uswapwep has become cursed while in two-weapon combat mode or hero is
   attempting to dual-wield when it is already cursed or hands are slippery */
void
drop_uswapwep(void)
{
    char left_hand[QBUFSZ];
    struct obj *obj = player_secondary_weapon;

    /* this used to use makeplural(body_part(HAND)) but in order to be
       dual-wielded, or to get this far attempting to achieve that,
       uswapwep must be one-handed; since it's secondary, the hand must
       be the left one */
    Sprintf(left_hand, "left %s", body_part(HAND));
    if (!obj->cursed)
        /* attempting to two-weapon while Glib */
        pline("%s from your %s!", Yobjnam2(obj, "slip"), left_hand);
    else if (!u.using_two_weapons)
        /* attempting to two-weapon when uswapwep is cursed */
        pline("%s your grasp and %s from your %s!",
              Yobjnam2(obj, "evade"), otense(obj, "drop"), left_hand);
    else
        /* already two-weaponing but can't anymore because uswapwep has
           become cursed */
        Your("%s spasms and drops %s!", left_hand, yobjnam(obj, (char *) 0));
    dropx(obj);
}

void
set_twoweap(boolean on_off)
{
    u.using_two_weapons = on_off;
}

/* the #twoweapon command */
int
dotwoweapon(void)
{
    /* You can always toggle it off */
    if (u.using_two_weapons) {
        You("switch to your primary weapon.");
        set_twoweap(FALSE); /* u.twoweap = FALSE */
        update_inventory();
        return ECMD_OK;
    }

    /* May we use two weapons? */
    if (can_twoweapon()) {
        /* Success! */
        You("begin two-weapon combat.");
        set_twoweap(TRUE); /* u.twoweap = TRUE */
        update_inventory();
        return (random(20) > ATTRIBUTE_CURRENT(ATTRIBUTE_DEXTERITY)) ? ECMD_TIME : ECMD_OK;
    }
    return ECMD_OK;
}

/*** Functions to empty a given slot ***/
/* These should be used only when the item can't be put back in
 * the slot by life saving.  Proper usage includes:
 * 1.  The item has been eaten, stolen, burned away, or rotted away.
 * 2.  Making an item disappear for a bones pile.
 */
void
uwepgone(void)
{
    if (player_weapon) {
        if (artifact_light(player_weapon) && player_weapon->lamplit) {
            end_burn(player_weapon, FALSE);
            if (!Blind)
                pline("%s shining.", Tobjnam(player_weapon, "stop"));
        }
        setworn((struct obj *) 0, WEARING_WEAPON);
        gu.unweapon = TRUE;
        update_inventory();
    }
}

void
uswapwepgone(void)
{
    if (player_secondary_weapon) {
        setworn((struct obj *) 0, WEARING_SECONDARY_WEAPON);
        update_inventory();
    }
}

void
uqwepgone(void)
{
    if (player_quiver) {
        setworn((struct obj *) 0, WEARING_QUIVER);
        update_inventory();
    }
}

void
untwoweapon(void)
{
    if (u.using_two_weapons) {
        You("%s.", can_no_longer_twoweap);
        set_twoweap(FALSE); /* u.twoweap = FALSE */
        update_inventory();
    }
    return;
}

/* enchant wielded weapon */
int
chwepon(struct obj *otmp, int amount)
{
    const char *color = hcolor((amount < 0) ? NH_BLACK : NH_BLUE);
    const char *xtime, *wepname = "";
    boolean multiple;
    int otyp = STRANGE_OBJECT;

    if (!player_weapon || (player_weapon->oclass != WEAPON_CLASS && !is_weptool(player_weapon))) {
        char buf[BUFSZ];

        if (amount >= 0 && player_weapon && will_weld(player_weapon)) { /* cursed tin opener */
            if (!Blind) {
                Sprintf(buf, "%s with %s aura.",
                        Yobjnam2(player_weapon, "glow"), an(hcolor(NH_AMBER)));
                player_weapon->bknown = !Hallucination; /* ok to bypass set_bknown() */
            } else {
                /* cursed tin opener is wielded in right hand */
                Sprintf(buf, "Your right %s tingles.", body_part(HAND));
            }
            uncurse(player_weapon);
            update_inventory();
        } else {
            Sprintf(buf, "Your %s %s.", makeplural(body_part(HAND)),
                    (amount >= 0) ? "twitch" : "itch");
        }
        strange_feeling(otmp, buf); /* pline()+docall()+useup() */
        exercise(ATTRIBUTE_DEXTERITY, (boolean) (amount >= 0));
        return 0;
    }

    if (otmp && otmp->oclass == SCROLL_CLASS)
        otyp = otmp->otyp;

    if (player_weapon->otyp == WORM_TOOTH && amount >= 0) {
        multiple = (player_weapon->quan > 1L);
        /* order: message, transformation, shop handling */
        Your("%s %s much sharper now.", simpleonames(player_weapon),
             multiple ? "fuse, and become" : "is");
        player_weapon->otyp = CRYSKNIFE;
        player_weapon->oerodeproof = 0;
        if (multiple) {
            player_weapon->quan = 1L;
            player_weapon->owt = weight(player_weapon);
        }
        if (player_weapon->cursed)
            uncurse(player_weapon);
        /* update shop bill to reflect new higher value */
        if (player_weapon->unpaid)
            alter_cost(player_weapon, 0L);
        if (otyp != STRANGE_OBJECT)
            makeknown(otyp);
        if (multiple)
            (void) encumbered_message();
        return 1;
    } else if (player_weapon->otyp == CRYSKNIFE && amount < 0) {
        multiple = (player_weapon->quan > 1L);
        /* order matters: message, shop handling, transformation */
        Your("%s %s much duller now.", simpleonames(player_weapon),
             multiple ? "fuse, and become" : "is");
        costly_alteration(player_weapon, COST_DEGRD); /* DECHNT? other? */
        player_weapon->otyp = WORM_TOOTH;
        player_weapon->oerodeproof = 0;
        if (multiple) {
            player_weapon->quan = 1L;
            player_weapon->owt = weight(player_weapon);
        }
        if (otyp != STRANGE_OBJECT && otmp->bknown)
            makeknown(otyp);
        if (multiple)
            (void) encumbered_message();
        return 1;
    }

    if (has_oname(player_weapon))
        wepname = ONAME(player_weapon);
    if (amount < 0 && player_weapon->oartifact && restrict_name(player_weapon, wepname)) {
        if (!Blind)
            pline("%s %s.", Yobjnam2(player_weapon, "faintly glow"), color);
        return 1;
    }
    /* there is a (soft) upper and lower limit to uwep->spe */
    if (((player_weapon->spe > 5 && amount >= 0) || (player_weapon->spe < -5 && amount < 0))
        && random_integer_between_zero_and(3)) {
        if (!Blind)
            pline("%s %s for a while and then %s.",
                  Yobjnam2(player_weapon, "violently glow"), color,
                  otense(player_weapon, "evaporate"));
        else
            pline("%s.", Yobjnam2(player_weapon, "evaporate"));

        useupall(player_weapon); /* let all of them disappear */
        return 1;
    }
    if (!Blind) {
        xtime = (amount * amount == 1) ? "moment" : "while";
        pline("%s %s for a %s.",
              Yobjnam2(player_weapon, amount == 0 ? "violently glow" : "glow"), color,
              xtime);
        if (otyp != STRANGE_OBJECT && player_weapon->known
            && (amount > 0 || (amount < 0 && otmp->bknown)))
            makeknown(otyp);
    }
    if (amount < 0)
        costly_alteration(player_weapon, COST_DECHNT);
    player_weapon->spe += amount;
    if (amount > 0) {
        if (player_weapon->cursed)
            uncurse(player_weapon);
        /* update shop bill to reflect new higher price */
        if (player_weapon->unpaid)
            alter_cost(player_weapon, 0L);
    }

    /*
     * Enchantment, which normally improves a weapon, has an
     * addition adverse reaction on Magicbane whose effects are
     * spe dependent.  Give an obscure clue here.
     */
    if (u_wield_art(ART_MAGICBANE) && player_weapon->spe >= 0) {
        Your("right %s %sches!", body_part(HAND),
             (((amount > 1) && (player_weapon->spe > 1)) ? "flin" : "it"));
    }

    /* an elven magic clue, cookie@keebler */
    /* elven weapons vibrate warningly when enchanted beyond a limit */
    if ((player_weapon->spe > 5)
        && (is_elven_weapon(player_weapon) || player_weapon->oartifact || !random_integer_between_zero_and(7)))
        pline("%s unexpectedly.", Yobjnam2(player_weapon, "suddenly vibrate"));

    return 1;
}

int
welded(struct obj *obj)
{
    if (obj && obj == player_weapon && will_weld(obj)) {
        set_bknown(obj, 1);
        return 1;
    }
    return 0;
}

void
weldmsg(struct obj *obj)
{
    long savewornmask;
    const char *hand = body_part(HAND);

    if (bimanual(obj))
        hand = makeplural(hand);
    savewornmask = obj->owornmask;
    obj->owornmask = 0L; /* suppress doname()'s "(weapon in hand)";
                          * Yobjnam2() doesn't actually need this because
                          * it is based on xname() rather than doname() */
    pline("%s welded to your %s!", Yobjnam2(obj, "are"), hand);
    obj->owornmask = savewornmask;
}

/* test whether monster's wielded weapon is stuck to hand/paw/whatever */
boolean
mwelded(struct obj *obj)
{
    /* caller is responsible for making sure this is a monster's item */
    if (obj && (obj->owornmask & WEARING_WEAPON) && will_weld(obj))
        return TRUE;
    return FALSE;
}

/*wield.c*/
