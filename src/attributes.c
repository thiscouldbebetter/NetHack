/* NetHack 3.7	attrib.c	$NHDT-Date: 1651908297 2022/05/07 07:24:57 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.86 $ */
/*      Copyright 1988, 1989, 1990, 1992, M. Stephenson           */
/* NetHack may be freely redistributed.  See license for details. */

/* Modified by This Could Be Better, 2024. */

/*  attribute modification routines. */

#include "hack.h"
#include <ctype.h>

/* part of the output on gain or loss of attribute */
static const char
    *const attributesPositive[] = { "strong", "smart", "wise",
                          "agile",  "tough", "charismatic" },
    *const attributesNegative[] = { "weak",    "stupid",
                           "foolish", "clumsy",
                           "fragile", "repulsive" };
/* also used by enlightenment in insight.c for non-abbreviated status info */
extern const char *const attrname[6];

const char
    *const attrname[] = { "strength", "intelligence", "wisdom",
                          "dexterity", "constitution", "charisma" };

static const struct innate {
    schar ulevel;
    long *ability;
    const char *gainstr, *losestr;
} abilities_archaeologist[] = { { 1, &(HSearching), "", "" },
                 { 5, &(HStealth), "stealthy", "" },
                 { 10, &(HFast), "quick", "slow" },
                 { 0, 0, 0, 0 } },

  abilities_barbarian[] = { { 1, &(HPoison_resistance), "", "" },
                 { 7, &(HFast), "quick", "slow" },
                 { 15, &(HStealth), "stealthy", "" },
                 { 0, 0, 0, 0 } },

  abilities_cavedweller[] = { { 7, &(HFast), "quick", "slow" },
                 { 15, &(HWarning), "sensitive", "" },
                 { 0, 0, 0, 0 } },

  abilities_healer[] = { { 1, &(HPoison_resistance), "", "" },
                 { 15, &(HWarning), "sensitive", "" },
                 { 0, 0, 0, 0 } },

  abilities_knight[] = { { 7, &(HFast), "quick", "slow" }, { 0, 0, 0, 0 } },

  abilities_monk[] = { { 1, &(HFast), "", "" },
                 { 1, &(HSleep_resistance), "", "" },
                 { 1, &(HSee_invisible), "", "" },
                 { 3, &(HPoison_resistance), "healthy", "" },
                 { 5, &(HStealth), "stealthy", "" },
                 { 7, &(HWarning), "sensitive", "" },
                 { 9, &(HSearching), "perceptive", "unaware" },
                 { 11, &(HFire_resistance), "cool", "warmer" },
                 { 13, &(HCold_resistance), "warm", "cooler" },
                 { 15, &(HShock_resistance), "insulated", "conductive" },
                 { 17, &(HTeleport_control), "controlled", "uncontrolled" },
                 { 0, 0, 0, 0 } },

  abilities_pri[] = { { 15, &(HWarning), "sensitive", "" },
                 { 20, &(HFire_resistance), "cool", "warmer" },
                 { 0, 0, 0, 0 } },

  abilities_ranger[] = { { 1, &(HSearching), "", "" },
                 { 7, &(HStealth), "stealthy", "" },
                 { 15, &(HSee_invisible), "", "" },
                 { 0, 0, 0, 0 } },

  abilities_rogue[] = { { 1, &(HStealth), "", "" },
                 { 10, &(HSearching), "perceptive", "" },
                 { 0, 0, 0, 0 } },

  abilities_sam[] = { { 1, &(HFast), "", "" },
                 { 15, &(HStealth), "stealthy", "" },
                 { 0, 0, 0, 0 } },

  abilities_tourist[] = { { 10, &(HSearching), "perceptive", "" },
                 { 20, &(HPoison_resistance), "hardy", "" },
                 { 0, 0, 0, 0 } },

  abilities_valkyrie[] = { { 1, &(HCold_resistance), "", "" },
                 { 3, &(HStealth), "stealthy", "" },
                 { 7, &(HFast), "quick", "slow" },
                 { 0, 0, 0, 0 } },

  abilities_wizard[] = { { 15, &(HWarning), "sensitive", "" },
                 { 17, &(HTeleport_control), "controlled", "uncontrolled" },
                 { 0, 0, 0, 0 } },

  /* Intrinsics conferred by race */
  abilities_dwarf[] = { { 1, &HInfravision, "", "" },
                 { 0, 0, 0, 0 } },

  abilities_elf[] = { { 1, &HInfravision, "", "" },
                 { 4, &HSleep_resistance, "awake", "tired" },
                 { 0, 0, 0, 0 } },

  abilities_gnome[] = { { 1, &HInfravision, "", "" },
                 { 0, 0, 0, 0 } },

  abilities_orc[] = { { 1, &HInfravision, "", "" },
                 { 1, &HPoison_resistance, "", "" },
                 { 0, 0, 0, 0 } },

  abilities_human[] = { { 0, 0, 0, 0 } };

staticfn void exerper(void);
staticfn int rnd_attr(void);
staticfn int initialize_attribute_role_redist(int, boolean);
staticfn void post_adjust_ability(long *);
staticfn const struct innate *role_abilities(int);
staticfn const struct innate *check_innate_ability(long *, long);
staticfn int innately(long *);

/* adjust an attribute; return TRUE if change is made, FALSE otherwise */
boolean
adjust_attribute(
    int attribute_index,    /* which characteristic */
    int amount_to_add,   /* amount of change */
    int message_flag) /* positive => no message, zero => message, and */
{               /* negative => conditional (msg if change made) */
    int old_acurr, old_abase, old_amax, decr;
    boolean abonflg;
    const char *attrstr;

    if (Fixed_abil || !amount_to_add)
        return FALSE;

    if ((attribute_index == ATTRIBUTE_INTELLIGENCE || attribute_index == ATTRIBUTE_WISDOM) && player_armor_hat && player_armor_hat->otyp == DUNCE_CAP) {
        if (message_flag == 0)
            Your("cap constricts briefly, then relaxes again.");
        return FALSE;
    }

    old_acurr = ATTRIBUTE_CURRENT(attribute_index);
    old_abase = ATTRIBUTE_BASE(attribute_index);
    old_amax = ATTRIBUTE_MAX(attribute_index);
    ATTRIBUTE_BASE(attribute_index) += amount_to_add; /* when incr is negative, this reduces ABASE() */
    if (amount_to_add > 0) {
        if (ATTRIBUTE_BASE(attribute_index) > ATTRIBUTE_MAX(attribute_index)) {
            ATTRIBUTE_MAX(attribute_index) = ATTRIBUTE_BASE(attribute_index);
            if (ATTRIBUTE_MAX(attribute_index) > ATTRMAX(attribute_index))
                ATTRIBUTE_BASE(attribute_index) = ATTRIBUTE_MAX(attribute_index) = ATTRMAX(attribute_index);
        }
        attrstr = attributesPositive[attribute_index];
        abonflg = (ATTRIBUTE_BONUS(attribute_index) < 0);
    } else { /* incr is negative */
        if (ATTRIBUTE_BASE(attribute_index) < ATTRMIN(attribute_index)) {
            /*
             * If base value has dropped so low that it is trying to be
             * taken below the minimum, reduce max value (peak reached)
             * instead.  That means that restore ability and repeated
             * applications of unicorn horn will not be able to recover
             * all the lost value.  As of 3.6.2, we only take away
             * some (average half, possibly zero) of the excess from max
             * instead of all of it, but without intervening recovery, it
             * can still eventually drop to the minimum allowed.  After
             * that, it can't be recovered, only improved with new gains.
             *
             * This used to assign a new negative value to incr and then
             * add it, but that could affect messages below, possibly
             * making a large decrease be described as a small one.
             *
             * decr = rn2(-(ABASE - ATTRMIN) + 1);
             */
            decr = random_integer_between_zero_and(ATTRMIN(attribute_index) - ATTRIBUTE_BASE(attribute_index) + 1);
            ATTRIBUTE_BASE(attribute_index) = ATTRMIN(attribute_index);
            ATTRIBUTE_MAX(attribute_index) -= decr;
            if (ATTRIBUTE_MAX(attribute_index) < ATTRMIN(attribute_index))
                ATTRIBUTE_MAX(attribute_index) = ATTRMIN(attribute_index);
        }
        attrstr = attributesNegative[attribute_index];
        abonflg = (ATTRIBUTE_BONUS(attribute_index) > 0);
    }
    if (ATTRIBUTE_CURRENT(attribute_index) == old_acurr) {
        if (message_flag == 0 && flags.verbose) {
            if (ATTRIBUTE_BASE(attribute_index) == old_abase && ATTRIBUTE_MAX(attribute_index) == old_amax) {
                pline("You're %s as %s as you can get.",
                      abonflg ? "currently" : "already", attrstr);
            } else {
                /* current stayed the same but base value changed, or
                   base is at minimum and reduction caused max to drop */
                Your("innate %s has %s.", attrname[attribute_index],
                     (amount_to_add > 0) ? "improved" : "declined");
            }
        }
        return FALSE;
    }

    disp.bottom_line = TRUE;
    if (message_flag <= 0)
        You_feel("%s%s!", (amount_to_add > 1 || amount_to_add < -1) ? "very " : "", attrstr);
    if (gp.program_state.in_moveloop && (attribute_index == ATTRIBUTE_STRENGTH || attribute_index == ATTRIBUTE_CONSTITUTION))
        (void) encumbered_message();
    return TRUE;
}

/* strength gain */
void
gainstr(struct obj *otmp, int incr, boolean givemsg)
{
    int num = incr;

    if (!num) {
        if (ATTRIBUTE_BASE(ATTRIBUTE_STRENGTH) < 18)
            num = (random_integer_between_zero_and(4) ? 1 : random(6));
        else if (ATTRIBUTE_BASE(ATTRIBUTE_STRENGTH) < STRENGTH18(85))
            num = random(10);
        else
            num = 1;
    }
    (void) adjust_attribute(ATTRIBUTE_STRENGTH, (otmp && otmp->cursed) ? -num : num,
                     givemsg ? -1 : 1);
}

/* strength loss, may kill you; cause may be poison or monster like 'a' */
void
losestr(int num, const char *knam, schar k_format)
{
    int uhpmin = minuhpmax(1), olduhpmax = u.hit_points_max;
    int ustr = ATTRIBUTE_BASE(ATTRIBUTE_STRENGTH) - num, amt, dmg;
    boolean waspolyd = Upolyd;

    if (num <= 0 || ATTRIBUTE_BASE(ATTRIBUTE_STRENGTH) < ATTRMIN(ATTRIBUTE_STRENGTH)) {
        impossible("losestr: %d - %d", ATTRIBUTE_BASE(ATTRIBUTE_STRENGTH), num);
        return;
    }
    dmg = 0;
    while (ustr < ATTRMIN(ATTRIBUTE_STRENGTH)) {
        ++ustr;
        --num;
        amt = rn1(4, 3); /* (0..(4-1))+3 => 3..6; used to use flat 6 here */
        dmg += amt;
    }
    if (dmg) {
        /* in case damage is fatal and caller didn't supply killer reason */
        if (!knam || !*knam) {
            knam = "terminal frailty";
            k_format = KILLED_BY;
        }
        losehp(dmg, knam, k_format);

        if (Upolyd) {
            /* if still polymorphed, reduce you-as-monst maxHP; never below 1 */
            u.mhmax -= min(dmg, u.mhmax - 1);
        } else if (!waspolyd) {
            /* not polymorphed now and didn't rehumanize when taking damage;
               reduce max HP, but not below uhpmin */
            if (u.hit_points_max > uhpmin)
                setuhpmax(max(u.hit_points_max - dmg, uhpmin));
        }
        disp.bottom_line = TRUE;
    }
#if 0   /* only possible if uhpmax was already less than uhpmin */
    if (!Upolyd && u.uhpmax < uhpmin) {
        setuhpmax(min(olduhpmax, uhpmin));
        if (!Drain_resistance)
            losexp(NULL); /* won't be fatal when no 'drainer' is supplied */
    }
#else
    nhUse(olduhpmax);
#endif
    /* 'num' could have been reduced to 0 in the minimum strength loop;
       '(Upolyd || !waspolyd)' is True unless damage caused rehumanization */
    if (num > 0 && (Upolyd || !waspolyd))
        (void) adjust_attribute(ATTRIBUTE_STRENGTH, -num, 1);
}

/* combined strength loss and damage from some poisons */
void
poison_strdmg(int strloss, int dmg, const char *knam, schar k_format)
{
    losestr(strloss, knam, k_format);
    losehp(dmg, knam, k_format);
}

static const struct poison_effect_message {
    void (*delivery_func)(const char *, ...);
    const char *effect_msg;
} poiseff[] = {
    { You_feel, "weaker" },             /* ATTRIBUTE_STRENGTH */
    { Your, "brain is on fire" },       /* ATTRIBUTE_INTELLIGENCE */
    { Your, "judgement is impaired" },  /* ATTRIBUTE_WISDOM */
    { Your, "muscles won't obey you" }, /* ATTRIBUTE_DEXTERITY */
    { You_feel, "very sick" },          /* ATTRIBUTE_CONSTITUTION */
    { You, "break out in hives" }       /* ATTRIBUTE_CHARISMA */
};

/* feedback for attribute loss due to poisoning */
void
poisontell(int typ,         /* which attribute */
           boolean exclaim) /* emphasis */
{
    void (*func)(const char *, ...) = poiseff[typ].delivery_func;
    const char *msg_txt = poiseff[typ].effect_msg;

    /*
     * "You feel weaker" or "you feel very sick" aren't appropriate when
     * wearing or wielding something (gauntlets of power, Ogresmasher)
     * which forces the attribute to maintain its maximum value.
     * Phrasing for other attributes which might have fixed values
     * (dunce cap) is such that we don't need message fixups for them.
     */
    if (typ == ATTRIBUTE_STRENGTH && ATTRIBUTE_CURRENT(ATTRIBUTE_STRENGTH) == STRENGTH19(25))
        msg_txt = "innately weaker";
    else if (typ == ATTRIBUTE_CONSTITUTION && ATTRIBUTE_CURRENT(ATTRIBUTE_CONSTITUTION) == 25)
        msg_txt = "sick inside";

    (*func)("%s%c", msg_txt, exclaim ? '!' : '.');
}

/* called when an attack or trap has poisoned hero (used to be in mon.c) */
void
poisoned(
    const char *reason,    /* controls what messages we display */
    int typ,
    const char *pkiller,   /* for score+log file if fatal */
    int fatal,             /* if fatal is 0, limit damage to adjattrib */
    boolean thrown_weapon) /* thrown weapons are less deadly */
{
    int i, loss, kprefix = KILLED_BY_AN;
    boolean blast = !strcmp(reason, "blast");

    /* inform player about being poisoned unless that's already been done;
       "blast" has given a "blast of poison gas" message; "poison arrow",
       "poison dart", etc have implicitly given poison messages too... */
    if (!blast && !strstri(reason, "poison")) {
        boolean plural = (reason[strlen(reason) - 1] == 's') ? 1 : 0;

        /* avoid "The" Orcus's sting was poisoned... */
        pline("%s%s %s poisoned!",
              isupper((uchar) *reason) ? "" : "The ", reason,
              plural ? "were" : "was");
    }
    if (Poison_resistance) {
        if (blast)
            shieldeff(u.ux, u.uy);
        pline_The("poison doesn't seem to affect you.");
        return;
    }

    /* suppress killer prefix if it already has one */
    i = name_to_mon(pkiller, (int *) 0);
    if (ismnum(i) && (mons[i].geno & G_UNIQ)) {
        kprefix = KILLED_BY;
        if (!type_is_pname(&mons[i]))
            pkiller = the(pkiller);
    } else if (!strncmpi(pkiller, "the ", 4) || !strncmpi(pkiller, "an ", 3)
               || !strncmpi(pkiller, "a ", 2)) {
        /*[ does this need a plural check too? ]*/
        kprefix = KILLED_BY;
    }

    i = !fatal ? 1 : random_integer_between_zero_and(fatal + (thrown_weapon ? 20 : 0));
    if (i == 0 && typ != ATTRIBUTE_CHARISMA) {
        /* sometimes survivable instant kill */
        loss = 6 + d(4, 6);
        if (u.hit_points <= loss) {
            u.hit_points = -1;
            disp.bottom_line = TRUE;
            pline_The("poison was deadly...");
        } else {
            /* survived, but with severe reaction */
            u.hit_points_max = max(3, u.hit_points_max - (loss / 2));
            losehp(loss, pkiller, kprefix); /* poison damage */
            if (adjust_attribute(ATTRIBUTE_CONSTITUTION, (typ != ATTRIBUTE_CONSTITUTION) ? -1 : -3, TRUE))
                poisontell(ATTRIBUTE_CONSTITUTION, TRUE);
            if (typ != ATTRIBUTE_CONSTITUTION && adjust_attribute(typ, -3, 1))
                poisontell(typ, TRUE);
        }
    } else if (i > 5) {
        boolean cloud = !strcmp(reason, "gas cloud");

        /* HP damage; more likely--but less severe--with missiles */
        loss = thrown_weapon ? random(6) : rn1(10, 6);
        if ((blast || cloud) && Half_gas_damage) /* worn towel */
            loss = (loss + 1) / 2;
        losehp(loss, pkiller, kprefix); /* poison damage */
    } else {
        /* attribute loss; if typ is ATTRIBUTE_STRENGTH, reduction in current and
           maximum HP will occur once strength has dropped down to 3 */
        loss = (thrown_weapon || !fatal) ? 1 : d(2, 2); /* was rn1(3,3) */
        /* check that a stat change was made */
        if (adjust_attribute(typ, -loss, 1))
            poisontell(typ, TRUE);
    }

    if (u.hit_points < 1) {
        gk.killer.format = kprefix;
        Strcpy(gk.killer.name, pkiller);
        /* "Poisoned by a poisoned ___" is redundant */
        done(strstri(pkiller, "poison") ? DIED : POISONING);
    }
    (void) encumbered_message();
}

void
change_luck(schar n)
{
    u.uluck += n;
    if (u.uluck < 0 && u.uluck < LUCKMIN)
        u.uluck = LUCKMIN;
    if (u.uluck > 0 && u.uluck > LUCKMAX)
        u.uluck = LUCKMAX;
}

int
stone_luck(boolean parameter) /* So I can't think up of a good name.  So sue me. --KAA */
{
    struct obj *otmp;
    long bonchance = 0;

    for (otmp = gi.invent; otmp; otmp = otmp->nobj)
        if (confers_luck(otmp)) {
            if (otmp->cursed)
                bonchance -= otmp->quan;
            else if (otmp->blessed)
                bonchance += otmp->quan;
            else if (parameter)
                bonchance += otmp->quan;
        }

    return sgn((int) bonchance);
}

/* there has just been an inventory change affecting a luck-granting item */
void
set_moreluck(void)
{
    int luckbon = stone_luck(TRUE);

    if (!luckbon && !carrying(LUCKSTONE))
        u.moreluck = 0;
    else if (luckbon >= 0)
        u.moreluck = LUCKADD;
    else
        u.moreluck = -LUCKADD;
}

/* (not used) */
void
restore_attrib(void)
{
    int i, equilibrium;;

    /*
     * Note:  this used to get called by moveloop() on every turn but
     * ATIME() is never set to non-zero anywhere so didn't do anything.
     * Presumably it once supported something like potion of heroism
     * which conferred temporary characteristics boost(s).
     *
     * ATEMP() is used for strength loss from hunger, which doesn't
     * time out, and for dexterity loss from wounded legs, which has
     * its own timeout routine.
     */

    for (i = 0; i < ATTRIBUTE_COUNT; i++) { /* all temporary losses/gains */
        equilibrium = ((i == ATTRIBUTE_STRENGTH && u.uhs >= WEAK)
                       || (i == ATTRIBUTE_DEXTERITY && Wounded_legs)) ? -1 : 0;
        if (ATTRIBUTE_TEMPORARY(i) != equilibrium && ATTRIBUTE_TEMPORARY_TIME(i) != 0) {
            if (!(--(ATTRIBUTE_TEMPORARY_TIME(i)))) { /* countdown for change */
                ATTRIBUTE_TEMPORARY(i) += (ATTRIBUTE_TEMPORARY(i) > 0) ? -1 : 1;
                disp.bottom_line = TRUE;
                if (ATTRIBUTE_TEMPORARY(i)) /* reset timer */
                    ATTRIBUTE_TEMPORARY_TIME(i) = 100 / ATTRIBUTE_CURRENT(ATTRIBUTE_CONSTITUTION);
            }
        }
    }
    if (disp.bottom_line)
        (void) encumbered_message();
}

#define AVAL 50 /* tune value for exercise gains */

void
exercise(int i, boolean inc_or_dec)
{
    debugpline0("Exercise:");
    if (i == ATTRIBUTE_INTELLIGENCE || i == ATTRIBUTE_CHARISMA)
        return; /* can't exercise these */

    /* no physical exercise while polymorphed; the body's temporary */
    if (Upolyd && i != ATTRIBUTE_WISDOM)
        return;

    if (abs(ATTRIBUTE_FROM_EXERCISE(i)) < AVAL) {
        /*
         *      Law of diminishing returns (Part I):
         *
         *      Gain is harder at higher attribute values.
         *      79% at "3" --> 0% at "18"
         *      Loss is even at all levels (50%).
         *
         *      Note: *YES* ACURR is the right one to use.
         */
        ATTRIBUTE_FROM_EXERCISE(i) += (inc_or_dec) ? (random_integer_between_zero_and(19) > ATTRIBUTE_CURRENT(i)) : -random_integer_between_zero_and(2);
        debugpline3("%s, %s AEXE = %d",
                    (i == ATTRIBUTE_STRENGTH) ? "Str" : (i == ATTRIBUTE_WISDOM) ? "Wis" : (i == ATTRIBUTE_DEXTERITY)
                                                                      ? "Dex"
                                                                      : "Con",
                    (inc_or_dec) ? "inc" : "dec", ATTRIBUTE_FROM_EXERCISE(i));
    }
    if (gm.moves > 0 && (i == ATTRIBUTE_STRENGTH || i == ATTRIBUTE_CONSTITUTION))
        (void) encumbered_message();
}

staticfn void
exerper(void)
{
    if (!(gm.moves % 10)) {
        /* Hunger Checks */
        int hs = (u.uhunger > 1000) ? SATIATED
                 : (u.uhunger > 150) ? NOT_HUNGRY
                   : (u.uhunger > 50) ? HUNGRY
                     : (u.uhunger > 0) ? WEAK
                       : FAINTING;

        debugpline0("exerper: Hunger checks");
        switch (hs) {
        case SATIATED:
            exercise(ATTRIBUTE_DEXTERITY, FALSE);
            if (Role_if(PM_MONK))
                exercise(ATTRIBUTE_WISDOM, FALSE);
            break;
        case NOT_HUNGRY:
            exercise(ATTRIBUTE_CONSTITUTION, TRUE);
            break;
        case WEAK:
            exercise(ATTRIBUTE_STRENGTH, FALSE);
            if (Role_if(PM_MONK)) /* fasting */
                exercise(ATTRIBUTE_WISDOM, TRUE);
            break;
        case FAINTING:
        case FAINTED:
            exercise(ATTRIBUTE_CONSTITUTION, FALSE);
            break;
        }

        /* Encumbrance Checks */
        debugpline0("exerper: Encumber checks");
        switch (near_capacity()) {
        case MODERATELY_ENCUMBERED:
            exercise(ATTRIBUTE_STRENGTH, TRUE);
            break;
        case HEAVILY_ENCUMBERED:
            exercise(ATTRIBUTE_STRENGTH, TRUE);
            exercise(ATTRIBUTE_DEXTERITY, FALSE);
            break;
        case EXTREMELY_ENCUMBERED:
            exercise(ATTRIBUTE_DEXTERITY, FALSE);
            exercise(ATTRIBUTE_CONSTITUTION, FALSE);
            break;
        }
    }

    /* status checks */
    if (!(gm.moves % 5)) {
        debugpline0("exerper: Status checks");
        if ((HClairvoyant & (INTRINSIC | TIMEOUT)) && !BClairvoyant)
            exercise(ATTRIBUTE_WISDOM, TRUE);
        if (HRegeneration)
            exercise(ATTRIBUTE_STRENGTH, TRUE);

        if (Sick || Vomiting)
            exercise(ATTRIBUTE_CONSTITUTION, FALSE);
        if (Confusion || Hallucination)
            exercise(ATTRIBUTE_WISDOM, FALSE);
        if ((Wounded_legs && !u.monster_being_ridden) || Fumbling || HStun)
            exercise(ATTRIBUTE_DEXTERITY, FALSE);
    }
}

/* exercise/abuse text (must be in attribute order, not botl order);
   phrased as "You must have been [][0]." or "You haven't been [][1]." */
static NEARDATA const char *const exertext[ATTRIBUTE_COUNT][2] = {
    { "exercising diligently", "exercising properly" },           /* Str */
    { 0, 0 },                                                     /* Int */
    { "very observant", "paying attention" },                     /* Wis */
    { "working on your reflexes", "working on reflexes lately" }, /* Dex */
    { "leading a healthy life-style", "watching your health" },   /* Con */
    { 0, 0 },                                                     /* Cha */
};

void
exerchk(void)
{
    int i, ax, mod_val, lolim, hilim;

    /*  Check out the periodic accumulations */
    exerper();

    if (gm.moves >= gc.context.next_attrib_check) {
        debugpline1("exerchk: ready to test. multi = %ld.", gm.multi);
    }
    /*  Are we ready for a test? */
    if (gm.moves >= gc.context.next_attrib_check && !gm.multi) {
        debugpline0("exerchk: testing.");
        /*
         *      Law of diminishing returns (Part II):
         *
         *      The effects of "exercise" and "abuse" wear
         *      off over time.  Even if you *don't* get an
         *      increase/decrease, you lose some of the
         *      accumulated effects.
         */
        for (i = 0; i < ATTRIBUTE_COUNT; ++i) {
            ax = ATTRIBUTE_FROM_EXERCISE(i);
            /* nothing to do here if no exercise or abuse has occurred
               (Int and Cha always fall into this category) */
            if (!ax)
                continue; /* ok to skip nextattrib */

            mod_val = sgn(ax); /* +1 or -1; used below */
            /* no further effect for exercise if at max or abuse if at min;
               can't exceed 18 via exercise even if actual max is higher */
            lolim = ATTRMIN(i); /* usually 3; might be higher */
            hilim = ATTRMAX(i); /* usually 18; maybe lower or higher */
            if (hilim > 18)
                hilim = 18;
            if ((ax < 0) ? (ATTRIBUTE_BASE(i) <= lolim) : (ATTRIBUTE_BASE(i) >= hilim))
                goto nextattrib;
            /* can't exercise non-Wisdom while polymorphed; previous
               exercise/abuse gradually wears off without impact then */
            if (Upolyd && i != ATTRIBUTE_WISDOM)
                goto nextattrib;

            debugpline2("exerchk: testing %s (%d).",
                        (i == ATTRIBUTE_STRENGTH) ? "Str"
                        : (i == ATTRIBUTE_INTELLIGENCE) ? "Int?"
                          : (i == ATTRIBUTE_WISDOM) ? "Wis"
                            : (i == ATTRIBUTE_DEXTERITY) ? "Dex"
                              : (i == ATTRIBUTE_CONSTITUTION) ? "Con"
                                : (i == ATTRIBUTE_CHARISMA) ? "Cha?"
                                  : "???",
                        ax);
            /*
             *  Law of diminishing returns (Part III):
             *
             *  You don't *always* gain by exercising.
             *  [MRS 92/10/28 - Treat Wisdom specially for balance.]
             */
            if (random_integer_between_zero_and(AVAL) > ((i != ATTRIBUTE_WISDOM) ? (abs(ax) * 2 / 3) : abs(ax)))
                goto nextattrib;

            debugpline1("exerchk: changing %d.", i);
            if (adjust_attribute(i, mod_val, -1)) {
                debugpline1("exerchk: changed %d.", i);
                /* if you actually changed an attrib - zero accumulation */
                ATTRIBUTE_FROM_EXERCISE(i) = ax = 0;
                /* then print an explanation */
                You("%s %s.",
                    (mod_val > 0) ? "must have been" : "haven't been",
                    exertext[i][(mod_val > 0) ? 0 : 1]);
            }
 nextattrib:
            /* this used to be ``AEXE(i) /= 2'' but that would produce
               platform-dependent rounding/truncation for negative vals */
            ATTRIBUTE_FROM_EXERCISE(i) = (abs(ax) / 2) * mod_val;
        }
        gc.context.next_attrib_check += rn1(200, 800);
        debugpline1("exerchk: next check at %ld.",
                    gc.context.next_attrib_check);
    }
}

/* return random hero attribute (by role's attr distribution).
   returns ATTRIBUTE_MAX if failed. */
staticfn int
rnd_attr(void)
{
    int i, x = random_integer_between_zero_and(100);

    /* 3.7: the x -= ... calculation used to have an off by 1 error that
       resulted in the values being biased toward Str and away from Cha */
    for (i = 0; i < ATTRIBUTE_COUNT; ++i)
        if ((x -= gu.urole.attrdist[i]) < 0)
            break;
    return i;
}

/* add or subtract np points from random attributes,
   adjusting the base and maximum values of the attributes.
   if subtracting, np must be negative.
   returns the left over points. */
staticfn int
initialize_attribute_role_redist(int np, boolean addition)
{
    int tryct = 0;
    int adj = addition ? 1 : -1;

    while ((addition ? (np > 0) : (np < 0)) && tryct < 100) {
        int i = rnd_attr();

        if (i >= ATTRIBUTE_COUNT || ATTRIBUTE_BASE(i) >= ATTRMAX(i)) {
            tryct++;
            continue;
        }
        tryct = 0;
        ATTRIBUTE_BASE(i) += adj;
        ATTRIBUTE_MAX(i) += adj;
        np -= adj;
    }
    return np;
}

/* allocate hero's initial characteristics */
void
init_attr(int np)
{
    int i;

    for (i = 0; i < ATTRIBUTE_COUNT; i++) {
        ATTRIBUTE_BASE(i) = ATTRIBUTE_MAX(i) = gu.urole.attrbase[i];
        ATTRIBUTE_TEMPORARY(i) = ATTRIBUTE_TEMPORARY_TIME(i) = 0;
        np -= gu.urole.attrbase[i];
    }

    /* distribute leftover points */
    np = initialize_attribute_role_redist(np, TRUE);
    /* if we went over, remove points */
    np = initialize_attribute_role_redist(np, FALSE);
}

void
redist_attr(void)
{
    int i, tmp;

    for (i = 0; i < ATTRIBUTE_COUNT; i++) {
        if (i == ATTRIBUTE_INTELLIGENCE || i == ATTRIBUTE_WISDOM)
            continue;
        /* Polymorphing doesn't change your mind */
        tmp = ATTRIBUTE_MAX(i);
        ATTRIBUTE_MAX(i) += (random_integer_between_zero_and(5) - 2);
        if (ATTRIBUTE_MAX(i) > ATTRMAX(i))
            ATTRIBUTE_MAX(i) = ATTRMAX(i);
        if (ATTRIBUTE_MAX(i) < ATTRMIN(i))
            ATTRIBUTE_MAX(i) = ATTRMIN(i);
        ATTRIBUTE_BASE(i) = ATTRIBUTE_BASE(i) * ATTRIBUTE_MAX(i) / tmp;
        /* ABASE(i) > ATTRMAX(i) is impossible */
        if (ATTRIBUTE_BASE(i) < ATTRMIN(i))
            ATTRIBUTE_BASE(i) = ATTRMIN(i);
    }
    /* (void) encumber_msg(); -- caller needs to do this */
}

/* apply minor variation to attributes */
void
vary_init_attr(void)
{
    int i;

    for (i = 0; i < ATTRIBUTE_COUNT; i++)
        if (!random_integer_between_zero_and(20)) {
            int xd = random_integer_between_zero_and(7) - 2; /* biased variation */

            (void) adjust_attribute(i, xd, TRUE);
            if (ATTRIBUTE_BASE(i) < ATTRIBUTE_MAX(i))
                ATTRIBUTE_MAX(i) = ATTRIBUTE_BASE(i);
        }
}

staticfn
void
post_adjust_ability(long *ability)
{
    if (!ability)
        return;
    if (ability == &(HWarning) || ability == &(HSee_invisible))
        see_monsters();
}

staticfn const struct innate *
role_abilities(int r)
{
    const struct {
        short role;
        const struct innate *abil;
    } roleabils[] = {
        { PM_ARCHEOLOGIST, abilities_archaeologist },
        { PM_BARBARIAN, abilities_barbarian },
        { PM_CAVE_DWELLER, abilities_cavedweller },
        { PM_HEALER, abilities_healer },
        { PM_KNIGHT, abilities_knight },
        { PM_MONK, abilities_monk },
        { PM_CLERIC, abilities_pri },
        { PM_RANGER, abilities_ranger },
        { PM_ROGUE, abilities_rogue },
        { PM_SAMURAI, abilities_sam },
        { PM_TOURIST, abilities_tourist },
        { PM_VALKYRIE, abilities_valkyrie },
        { PM_WIZARD, abilities_wizard },
        { 0, 0 }
    };
    int i;

    for (i = 0; roleabils[i].abil && roleabils[i].role != r; i++)
        continue;
    return roleabils[i].abil;
}

staticfn const struct innate *
check_innate_ability(long *ability, long frommask)
{
    const struct innate *abil = 0;

    if (frommask == FROMEXPER)
        abil = role_abilities(Role_switch);
    else if (frommask == FROMRACE)
        switch (Race_switch) {
        case PM_DWARF:
            abil = abilities_dwarf;
            break;
        case PM_ELF:
            abil = abilities_elf;
            break;
        case PM_GNOME:
            abil = abilities_gnome;
            break;
        case PM_ORC:
            abil = abilities_orc;
            break;
        case PM_HUMAN:
            abil = abilities_human;
            break;
        default:
            break;
        }

    while (abil && abil->ability) {
        if ((abil->ability == ability) && (u.ulevel >= abil->ulevel))
            return abil;
        abil++;
    }
    return (struct innate *) 0;
}

/* reasons for innate ability */
#define FROM_NONE 0
#define FROM_ROLE 1 /* from experience at level 1 */
#define FROM_RACE 2
#define FROM_INTR 3 /* intrinsically (eating some corpse or prayer reward) */
#define FROM_EXP  4 /* from experience for some level > 1 */
#define FROM_FORM 5
#define FROM_LYCN 6

/* check whether particular ability has been obtained via innate attribute */
staticfn int
innately(long *ability)
{
    const struct innate *iptr;

    if ((iptr = check_innate_ability(ability, FROMEXPER)) != 0)
        return (iptr->ulevel == 1) ? FROM_ROLE : FROM_EXP;
    if ((iptr = check_innate_ability(ability, FROMRACE)) != 0)
        return FROM_RACE;
    if ((*ability & FROMOUTSIDE) != 0L)
        return FROM_INTR;
    if ((*ability & FROMFORM) != 0L)
        return FROM_FORM;
    return FROM_NONE;
}

int
is_innate(int propidx)
{
    int innateness;

    /* innately() would report FROM_FORM for this; caller wants specificity */
    if (propidx == DRAIN_RESISTANCE && ismnum(u.ulycn))
        return FROM_LYCN;
    if (propidx == FAST && Very_fast)
        return FROM_NONE; /* can't become very fast innately */
    if ((innateness = innately(&u.uprops[propidx].intrinsic)) != FROM_NONE)
        return innateness;
    if (propidx == JUMPING && Role_if(PM_KNIGHT)
        /* knight has intrinsic jumping, but extrinsic is more versatile so
           ignore innateness if equipment is going to claim responsibility */
        && !u.uprops[propidx].extrinsic)
        return FROM_ROLE;
    if (propidx == BLINDED && !haseyes(gy.youmonst.data))
        return FROM_FORM;
    return FROM_NONE;
}

DISABLE_WARNING_FORMAT_NONLITERAL

char *
from_what(int propidx) /* special cases can have negative values */
{
    static char buf[BUFSZ];

    buf[0] = '\0';
    /*
     * Restrict the source of the attributes just to debug mode for now
     */
    if (wizard) {
        static NEARDATA const char because_of[] = " because of %s";

        if (propidx >= 0) {
            char *p;
            struct obj *obj = (struct obj *) 0;
            int innateness = is_innate(propidx);

            /*
             * Properties can be obtained from multiple sources and we
             * try to pick the most significant one.  Classification
             * priority is not set in stone; current precedence is:
             * "from the start" (from role or race at level 1),
             * "from outside" (eating corpse, divine reward, blessed potion),
             * "from experience" (from role or race at level 2+),
             * "from current form" (while polymorphed),
             * "from timed effect" (potion or spell),
             * "from worn/wielded equipment" (Firebrand, elven boots, &c),
             * "from carried equipment" (mainly quest artifacts).
             * There are exceptions.  Versatile jumping from spell or boots
             * takes priority over knight's innate but limited jumping.
             */
            if ((propidx == BLINDED && u.uroleplay.blind)
                || (propidx == DEAF && u.uroleplay.deaf))
                Sprintf(buf, " from birth");
            else if (innateness == FROM_ROLE || innateness == FROM_RACE)
                Strcpy(buf, " innately");
            else if (innateness == FROM_INTR) /* [].intrinsic & FROMOUTSIDE */
                Strcpy(buf, " intrinsically");
            else if (innateness == FROM_EXP)
                Strcpy(buf, " because of your experience");
            else if (innateness == FROM_LYCN)
                Strcpy(buf, " due to your lycanthropy");
            else if (innateness == FROM_FORM)
                Strcpy(buf, " from current creature form");
            else if (propidx == FAST && Very_fast)
                Sprintf(buf, because_of,
                        ((HFast & TIMEOUT) != 0L) ? "a potion or spell"
                          : ((EFast & WEARING_ARMOR_FOOTWEAR) != 0L && player_armor_footwear->dknown
                             && objects[player_armor_footwear->otyp].oc_name_known)
                              ? ysimple_name(player_armor_footwear) /* speed boots */
                                : EFast ? "worn equipment"
                                  : something);
            else if (wizard
                     && (obj = what_gives(&u.uprops[propidx].extrinsic)) != 0)
                Sprintf(buf, because_of, obj->oartifact
                                             ? bare_artifactname(obj)
                                             : ysimple_name(obj));
            else if (propidx == BLINDED && Blindfolded_only)
                Sprintf(buf, because_of, ysimple_name(player_blindfold));
            else if (propidx == BLINDED && u.ucreamed
                     && BlindedTimeout == (long) u.ucreamed
                     && !EBlinded && !(HBlinded & ~TIMEOUT))
                Sprintf(buf, "due to goop covering your %s",
                        body_part(FACE));

            /* remove some verbosity and/or redundancy */
            if ((p = strstri(buf, " pair of ")) != 0)
                copynchars(p + 1, p + 9, BUFSZ); /* overlapping buffers ok */
            else if (propidx == STRANGLED
                     && (p = strstri(buf, " of strangulation")) != 0)
                *p = '\0';

        } else { /* negative property index */
            /* if more blocking capabilities get implemented we'll need to
               replace this with what_blocks() comparable to what_gives() */
            switch (-propidx) {
            case BLINDED:
                /* wearing the Eyes of the Overworld overrides blindness */
                if (BBlinded && is_art(player_blindfold, ART_EYES_OF_THE_OVERWORLD))
                    Sprintf(buf, because_of, bare_artifactname(player_blindfold));
                break;
            case INVISIBLE:
                if (u.uprops[INVISIBLE].blocked & WEARING_ARMOR_CLOAK)
                    Sprintf(buf, because_of,
                            ysimple_name(player_armor_cloak)); /* mummy wrapping */
                break;
            case CLAIRVOYANT:
                if (wizard && (u.uprops[CLAIRVOYANT].blocked & WEARING_ARMOR_HELMET))
                    Sprintf(buf, because_of,
                            ysimple_name(player_armor_hat)); /* cornuthaum */
                break;
            }
        }

    } /*wizard*/
    return buf;
}

RESTORE_WARNING_FORMAT_NONLITERAL

void
adjabil(int oldlevel, int newlevel)
{
    const struct innate *abil, *rabil;
    long prevabil, mask = FROMEXPER;

    abil = role_abilities(Role_switch);

    switch (Race_switch) {
    case PM_ELF:
        rabil = abilities_elf;
        break;
    case PM_ORC:
        rabil = abilities_orc;
        break;
    case PM_HUMAN:
    case PM_DWARF:
    case PM_GNOME:
    default:
        rabil = 0;
        break;
    }

    while (abil || rabil) {
        /* Have we finished with the intrinsics list? */
        if (!abil || !abil->ability) {
            /* Try the race intrinsics */
            if (!rabil || !rabil->ability)
                break;
            abil = rabil;
            rabil = 0;
            mask = FROMRACE;
        }
        prevabil = *(abil->ability);
        if (oldlevel < abil->ulevel && newlevel >= abil->ulevel) {
            /* Abilities gained at level 1 can never be lost
             * via level loss, only via means that remove _any_
             * sort of ability.  A "gain" of such an ability from
             * an outside source is devoid of meaning, so we set
             * FROMOUTSIDE to avoid such gains.
             */
            if (abil->ulevel == 1)
                *(abil->ability) |= (mask | FROMOUTSIDE);
            else
                *(abil->ability) |= mask;
            if (!(*(abil->ability) & INTRINSIC & ~mask)) {
                if (*(abil->gainstr))
                    You_feel("%s!", abil->gainstr);
            }
        } else if (oldlevel >= abil->ulevel && newlevel < abil->ulevel) {
            *(abil->ability) &= ~mask;
            if (!(*(abil->ability) & INTRINSIC)) {
                if (*(abil->losestr))
                    You_feel("%s!", abil->losestr);
                else if (*(abil->gainstr))
                    You_feel("less %s!", abil->gainstr);
            }
        }
        if (prevabil != *(abil->ability)) /* it changed */
            post_adjust_ability(abil->ability);
        abil++;
    }

    if (oldlevel > 0) {
        if (newlevel > oldlevel)
            add_weapon_skill(newlevel - oldlevel);
        else
            lose_weapon_skill(oldlevel - newlevel);
    }
}

/* called when gaining a level (before u.ulevel gets incremented);
   also called with u.ulevel==0 during hero initialization or for
   re-init if hero turns into a "new man/woman/elf/&c" */
int
newhp(void)
{
    int hp, conplus;

    if (u.ulevel == 0) {
        /* Initialize hit points */
        hp = gu.urole.hpadv.infix + gu.urace.hpadv.infix;
        if (gu.urole.hpadv.inrnd > 0)
            hp += random(gu.urole.hpadv.inrnd);
        if (gu.urace.hpadv.inrnd > 0)
            hp += random(gu.urace.hpadv.inrnd);
        if (gm.moves <= 1L) { /* initial hero; skip for polyself to new man */
            /* Initialize alignment stuff */
            u.alignment.type = aligns[flags.initalign].value;
            u.alignment.record = gu.urole.initrecord;
        }
        /* no Con adjustment for initial hit points */
    } else {
        if (u.ulevel < gu.urole.xlev) {
            hp = gu.urole.hpadv.lofix + gu.urace.hpadv.lofix;
            if (gu.urole.hpadv.lornd > 0)
                hp += random(gu.urole.hpadv.lornd);
            if (gu.urace.hpadv.lornd > 0)
                hp += random(gu.urace.hpadv.lornd);
        } else {
            hp = gu.urole.hpadv.hifix + gu.urace.hpadv.hifix;
            if (gu.urole.hpadv.hirnd > 0)
                hp += random(gu.urole.hpadv.hirnd);
            if (gu.urace.hpadv.hirnd > 0)
                hp += random(gu.urace.hpadv.hirnd);
        }
        if (ATTRIBUTE_CURRENT(ATTRIBUTE_CONSTITUTION) <= 3)
            conplus = -2;
        else if (ATTRIBUTE_CURRENT(ATTRIBUTE_CONSTITUTION) <= 6)
            conplus = -1;
        else if (ATTRIBUTE_CURRENT(ATTRIBUTE_CONSTITUTION) <= 14)
            conplus = 0;
        else if (ATTRIBUTE_CURRENT(ATTRIBUTE_CONSTITUTION) <= 16)
            conplus = 1;
        else if (ATTRIBUTE_CURRENT(ATTRIBUTE_CONSTITUTION) == 17)
            conplus = 2;
        else if (ATTRIBUTE_CURRENT(ATTRIBUTE_CONSTITUTION) == 18)
            conplus = 3;
        else
            conplus = 4;
        hp += conplus;
    }
    if (hp <= 0)
        hp = 1;
    if (u.ulevel < MAXULEV) {
        /* remember increment; future level drain could take it away again */
        u.hit_points_increment[u.ulevel] = (xint16) hp;
    } else {
        /* after level 30, throttle hit point gains from extra experience;
           once max reaches 1200, further increments will be just 1 more */
        char lim = 5 - u.hit_points_max / 300;

        lim = max(lim, 1);
        if (hp > lim)
            hp = lim;
    }
    return hp;
}

/* minimum value for uhpmax is ulevel but for life-saving it is always at
   least 10 if ulevel is less than that */
int
minuhpmax(int altmin)
{
    if (altmin < 1)
        altmin = 1;
    return max(u.ulevel, altmin);
}

/* update u.uhpmax and values of other things that depend upon it */
void
setuhpmax(int newmax)
{
    if (newmax != u.hit_points_max) {
        u.hit_points_max = newmax;
        if (u.hit_points_max > u.hit_points_peak)
            u.hit_points_peak = u.hit_points_max;
        disp.bottom_line = TRUE;
    }
    if (u.hit_points > u.hit_points_max)
        u.hit_points = u.hit_points_max, disp.bottom_line = TRUE;
}

/* return the current effective value of a specific characteristic
   (the 'a' in 'acurr()' comes from outdated use of "attribute" for the
   six Str/Dex/&c characteristics; likewise for u.abon, u.atemp, u.acurr) */
schar
acurr(int chridx)
{
    int tmp, result = 0; /* 'result' will always be reset to positive value */

    assert(chridx >= 0 && chridx < ATTRIBUTE_COUNT);
    tmp = u.attributes_bonus.a[chridx] + u.attributes_temporary.a[chridx] + u.attributes_current.a[chridx];

    /* for Strength:  3 <= result <= 125;
       for all others:  3 <= result <= 25 */
    if (chridx == ATTRIBUTE_STRENGTH) {
        /* strength value is encoded:  3..18 normal, 19..118 for 18/xx (with
           1 <= xx <= 100), and 119..125 for other characteristics' 19..25;
           STR18(x) yields 18 + x (intended for 0 <= x <= 100; not used here);
           STR19(y) yields 100 + y (intended for 19 <= y <= 25) */
        if (tmp >= STRENGTH19(25) || (player_armor_gloves && player_armor_gloves->otyp == GAUNTLETS_OF_POWER))
            result = STRENGTH19(25); /* 125 */
        else
            /* need non-zero here to avoid 'if(result==0)' below because
               that doesn't deal with Str encoding; the cap of 25 applied
               there would limit Str to 18/07 [18 + 7] */
            result = max(tmp, 3);
    } else if (chridx == ATTRIBUTE_CHARISMA) {
        if (tmp < 18 && (gy.youmonst.data->mlet == S_NYMPH
                         || u.umonnum == PM_AMOROUS_DEMON))
            result = 18;
    } else if (chridx == ATTRIBUTE_CONSTITUTION) {
        if (u_wield_art(ART_OGRESMASHER))
            result = 25;
    } else if (chridx == ATTRIBUTE_INTELLIGENCE || chridx == ATTRIBUTE_WISDOM) {
        /* Yes, this may raise Int and/or Wis if hero is sufficiently
           stupid.  There are lower levels of cognition than "dunce". */
        if (player_armor_hat && player_armor_hat->otyp == DUNCE_CAP)
            result = 6;
    } else if (chridx == ATTRIBUTE_DEXTERITY) {
        ; /* there aren't any special cases for dexterity */
    }

    if (result == 0) /* none of the special cases applied */
        result = (tmp >= 25) ? 25 : (tmp <= 3) ? 3 : tmp;

    return (schar) result;
}

/* condense clumsy ACURR(ATTRIBUTE_STRENGTH) value into value that fits into formulas */
schar
acurrstr(void)
{
    int str = ATTRIBUTE_CURRENT(ATTRIBUTE_STRENGTH), /* 3..125 after massaging by acurr() */
        result; /* 3..25 */

    if (str <= STRENGTH18(0)) /* <= 18; max(,3) here is redundant */
        result = max(str, 3); /* 3..18 */
    else if (str <= STRENGTH19(21)) /* <= 121 */
        /* this converts
           18/01..18/31 into 19,
           18/32..18/81 into 20,
           18/82..18/100 and 19..21 into 21 */
        result = 19 + str / 50; /* map to 19..21 */
    else /* convert 122..125; min(,125) here is redundant */
        result = min(str, 125) - 100; /* 22..25 */

    return (schar) result;
}

/* when wearing (or taking off) an unID'd item, this routine is used
   to distinguish between observable +0 result and no-visible-effect
   due to an attribute not being able to exceed maximum or minimum */
boolean
extremeattr(int attrindx) /* does attrindx's value match its max or min? */
{
    /* Fixed_abil and racial MINATTR/MAXATTR aren't relevant here */
    int lolimit = 3, hilimit = 25, curval = ATTRIBUTE_CURRENT(attrindx);

    /* upper limit for Str is 25 but its value is encoded differently */
    if (attrindx == ATTRIBUTE_STRENGTH) {
        hilimit = STRENGTH19(25); /* 125 */
        /* lower limit for Str can also be 25 */
        if (player_armor_gloves && player_armor_gloves->otyp == GAUNTLETS_OF_POWER)
            lolimit = hilimit;
    } else if (attrindx == ATTRIBUTE_CONSTITUTION) {
        if (u_wield_art(ART_OGRESMASHER))
            lolimit = hilimit;
    }
    /* this exception is hypothetical; the only other worn item affecting
       Int or Wis is another helmet so can't be in use at the same time */
    if (attrindx == ATTRIBUTE_INTELLIGENCE || attrindx == ATTRIBUTE_WISDOM) {
        if (player_armor_hat && player_armor_hat->otyp == DUNCE_CAP)
            hilimit = lolimit = 6;
    }

    /* are we currently at either limit? */
    return (curval == lolimit || curval == hilimit) ? TRUE : FALSE;
}

/* avoid possible problems with alignment overflow, and provide a centralized
   location for any future alignment limits */
void
adjalign(int n)
{
    int newalign = u.alignment.record + n;

    if (n < 0) {
        unsigned newabuse = u.alignment.abuse - n;

        if (newalign < u.alignment.record)
            u.alignment.record = newalign;
        if (newabuse > u.alignment.abuse) {
            u.alignment.abuse = newabuse;
            adj_erinys(newabuse);
        }
    } else if (newalign > u.alignment.record) {
        u.alignment.record = newalign;
        if (u.alignment.record > ALIGNLIM)
            u.alignment.record = (int)ALIGNLIM;
    }
}

/* change hero's alignment type, possibly losing use of artifacts */
void
uchangealign(int newalign,
             int reason) /* A_CG_CONVERT, A_CG_HELM_ON, or A_CG_HELM_OFF */
{
    aligntyp oldalign = u.alignment.type;

    u.blessed = 0; /* lose divine protection */
    /* You/Your/pline message with call flush_screen(), triggering bot(),
       so the actual data change needs to come before the message */
    disp.bottom_line = TRUE; /* status line needs updating */
    if (reason == A_CG_CONVERT) {
        /* conversion via altar */
        livelog_printf(LL_ALIGNMENT, "permanently converted to %s",
                       aligns[1 - newalign].adj);
        u.ualignbase[A_CURRENT] = (aligntyp) newalign;
        /* worn helm of opposite alignment might block change */
        if (!player_armor_hat || player_armor_hat->otyp != HELM_OF_OPPOSITE_ALIGNMENT)
            u.alignment.type = u.ualignbase[A_CURRENT];
        You("have a %ssense of a new direction.",
            (u.alignment.type != oldalign) ? "sudden " : "");
    } else {
        /* putting on or taking off a helm of opposite alignment */
        u.alignment.type = (aligntyp) newalign;
        if (reason == A_CG_HELM_ON) {
            adjalign(-7); /* for abuse -- record will be cleared shortly */
            Your("mind oscillates %s.", Hallucination ? "wildly" : "briefly");
            make_confused(rn1(2, 3), FALSE);
            if (Is_astralevel(&u.uz) || ((unsigned) random_integer_between_zero_and(50) < u.alignment.abuse))
                summon_furies(Is_astralevel(&u.uz) ? 0 : 1);
            /* don't livelog taking it back off */
            livelog_printf(LL_ALIGNMENT, "used a helm to turn %s",
                           aligns[1 - newalign].adj);
        } else if (reason == A_CG_HELM_OFF) {
            Your("mind is %s.", Hallucination
                                    ? "much of a muchness"
                                    : "back in sync with your body");
        }
    }
    if (u.alignment.type != oldalign) {
        u.alignment.record = 0; /* slate is wiped clean */
        retouch_equipment(0);
    }
}

/*attrib.c*/
