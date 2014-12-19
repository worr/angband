/*
 * File: target.c
 * Purpose: Targetting code
 *
 * Copyright (c) 1997-2007 Angband contributors
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cave.h"
#include "cmd-core.h"
#include "keymap.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-ignore.h"
#include "player-timed.h"
#include "project.h"
#include "target.h"
#include "ui-target.h"

/*** File-wide variables ***/

/* Is the target set? */
static bool target_set;

/* Current monster being tracked, or 0 */
static struct monster *target_who;

/* Target location */
static s16b target_x, target_y;

/*** Functions ***/

/*
 * Given a "source" and "target" location, extract a "direction",
 * which will move one step from the "source" towards the "target".
 *
 * Note that we use "diagonal" motion whenever possible.
 *
 * We return "5" if no motion is needed.
 *
 * XXX Change params to use two struct loc.
 */
int motion_dir(int y1, int x1, int y2, int x2)
{
	/* No movement required */
	if ((y1 == y2) && (x1 == x2)) return (DIR_NONE);

	/* South or North */
	if (x1 == x2) return ((y1 < y2) ? 2 : 8);

	/* East or West */
	if (y1 == y2) return ((x1 < x2) ? 6 : 4);

	/* South-east or South-west */
	if (y1 < y2) return ((x1 < x2) ? 3 : 1);

	/* North-east or North-west */
	if (y1 > y2) return ((x1 < x2) ? 9 : 7);

	/* Paranoia */
	return (5);
}


/*
 * Extract a direction (or zero) from a character
 */
int target_dir(struct keypress ch)
{
	return target_dir_allow(ch, FALSE);
}

int target_dir_allow(struct keypress ch, bool allow_5)
{
	int d = 0;

	/* Already a direction? */
	if (isdigit((unsigned char)ch.code)) {
		d = D2I(ch.code);
	} else if (isarrow(ch.code)) {
		switch (ch.code) {
			case ARROW_DOWN:  d = 2; break;
			case ARROW_LEFT:  d = 4; break;
			case ARROW_RIGHT: d = 6; break;
			case ARROW_UP:    d = 8; break;
		}
	} else {
		int mode;
		const struct keypress *act;

		if (OPT(rogue_like_commands))
			mode = KEYMAP_MODE_ROGUE;
		else
			mode = KEYMAP_MODE_ORIG;

		/* XXX see if this key has a digit in the keymap we can use */
		act = keymap_find(mode, ch);
		if (act) {
			const struct keypress *cur;
			for (cur = act; cur->type == EVT_KBRD; cur++) {
				if (isdigit((unsigned char) cur->code))
					d = D2I(cur->code);
			}
		}
	}

	/* Paranoia */
	if (d == 5 && !allow_5) d = 0;

	/* Return direction */
	return (d);
}

/*
 * Monster health description
 */
void look_mon_desc(char *buf, size_t max, int m_idx)
{
	monster_type *m_ptr = cave_monster(cave, m_idx);

	bool living = TRUE;

	/* Determine if the monster is "living" (vs "undead") */
	if (monster_is_unusual(m_ptr->race)) living = FALSE;

	/* Healthy monsters */
	if (m_ptr->hp >= m_ptr->maxhp)
	{
		/* No damage */
		my_strcpy(buf, (living ? "unhurt" : "undamaged"), max);
	}
	else
	{
		/* Calculate a health "percentage" */
		int perc = 100L * m_ptr->hp / m_ptr->maxhp;

		if (perc >= 60)
			my_strcpy(buf, (living ? "somewhat wounded" : "somewhat damaged"), max);
		else if (perc >= 25)
			my_strcpy(buf, (living ? "wounded" : "damaged"), max);
		else if (perc >= 10)
			my_strcpy(buf, (living ? "badly wounded" : "badly damaged"), max);
		else
			my_strcpy(buf, (living ? "almost dead" : "almost destroyed"), max);
	}

	if (m_ptr->m_timed[MON_TMD_SLEEP]) my_strcat(buf, ", asleep", max);
	if (m_ptr->m_timed[MON_TMD_CONF]) my_strcat(buf, ", confused", max);
	if (m_ptr->m_timed[MON_TMD_FEAR]) my_strcat(buf, ", afraid", max);
	if (m_ptr->m_timed[MON_TMD_STUN]) my_strcat(buf, ", stunned", max);
}



/*
 * Determine if a monster makes a reasonable target
 *
 * The concept of "targetting" was stolen from "Morgul" (?)
 *
 * The player can target any location, or any "target-able" monster.
 *
 * Currently, a monster is "target_able" if it is visible, and if
 * the player can hit it with a projection, and the player is not
 * hallucinating.  This allows use of "use closest target" macros.
 */
bool target_able(struct monster *m)
{
	return m && m->race && mflag_has(m->mflag, MFLAG_VISIBLE) &&
		!mflag_has(m->mflag, MFLAG_UNAWARE) &&
		projectable(cave, player->py, player->px, m->fy, m->fx, PROJECT_NONE) &&
		!player->timed[TMD_IMAGE];
}



/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * We return TRUE if the target is "okay" and FALSE otherwise.
 */
bool target_okay(void)
{
	/* No target */
	if (!target_set) return FALSE;

	/* Check "monster" targets */
	if (target_who) {
		if (target_able(target_who)) {
			/* Get the monster location */
			target_y = target_who->fy;
			target_x = target_who->fx;

			/* Good target */
			return TRUE;
		}
	} else if (target_x && target_y) {
		/* Allow a direction without a monster */
		return TRUE;
	}

	/* Assume no target */
	return FALSE;
}


/*
 * Set the target to a monster (or nobody)
 */
bool target_set_monster(struct monster *mon)
{
	/* Acceptable target */
	if (mon && target_able(mon))
	{
		target_set = TRUE;
		target_who = mon;
		target_y = mon->fy;
		target_x = mon->fx;
		return TRUE;
	}

	/* Reset target info */
	target_set = FALSE;
	target_who = NULL;
	target_y = 0;
	target_x = 0;

	return FALSE;
}


/*
 * Set the target to a location
 */
void target_set_location(int y, int x)
{
	/* Legal target */
	if (square_in_bounds_fully(cave, y, x))
	{
		/* Save target info */
		target_set = TRUE;
		target_who = NULL;
		target_y = y;
		target_x = x;
	}

	/* Clear target */
	else
	{
		/* Reset target info */
		target_set = FALSE;
		target_who = 0;
		target_y = 0;
		target_x = 0;
	}
}

/*
 * Temporary hack - NRM
 */
bool target_is_set(void)
{
	return target_set;
}

/*
 * Sorting hook -- comp function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by double-distance to the player.
 */
int cmp_distance(const void *a, const void *b)
{
	int py = player->py;
	int px = player->px;

	const struct loc *pa = a;
	const struct loc *pb = b;

	int da, db, kx, ky;

	/* Absolute distance components */
	kx = pa->x; kx -= px; kx = ABS(kx);
	ky = pa->y; ky -= py; ky = ABS(ky);

	/* Approximate Double Distance to the first point */
	da = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

	/* Absolute distance components */
	kx = pb->x; kx -= px; kx = ABS(kx);
	ky = pb->y; ky -= py; ky = ABS(ky);

	/* Approximate Double Distance to the first point */
	db = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

	/* Compare the distances */
	if (da < db)
		return -1;
	if (da > db)
		return 1;
	return 0;
}

/*
 * Hack -- help "select" a location (see below)
 */
s16b target_pick(int y1, int x1, int dy, int dx, struct point_set *targets)
{
	int i, v;

	int x2, y2, x3, y3, x4, y4;

	int b_i = -1, b_v = 9999;


	/* Scan the locations */
	for (i = 0; i < point_set_size(targets); i++)
	{
		/* Point 2 */
		x2 = targets->pts[i].x;
		y2 = targets->pts[i].y;

		/* Directed distance */
		x3 = (x2 - x1);
		y3 = (y2 - y1);

		/* Verify quadrant */
		if (dx && (x3 * dx <= 0)) continue;
		if (dy && (y3 * dy <= 0)) continue;

		/* Absolute distance */
		x4 = ABS(x3);
		y4 = ABS(y3);

		/* Verify quadrant */
		if (dy && !dx && (x4 > y4)) continue;
		if (dx && !dy && (y4 > x4)) continue;

		/* Approximate Double Distance */
		v = ((x4 > y4) ? (x4 + x4 + y4) : (y4 + y4 + x4));

		/* Penalize location XXX XXX XXX */

		/* Track best */
		if ((b_i >= 0) && (v >= b_v)) continue;

		/* Track best */
		b_i = i; b_v = v;
	}

	/* Result */
	return (b_i);
}


/**
 * Determine if a given location is "interesting"
 */
bool target_accept(int y, int x)
{
	object_type *obj;


	/* Player grids are always interesting */
	if (cave->squares[y][x].mon < 0) return (TRUE);


	/* Handle hallucination */
	if (player->timed[TMD_IMAGE]) return (FALSE);


	/* Visible monsters */
	if (cave->squares[y][x].mon > 0) {
		monster_type *m_ptr = square_monster(cave, y, x);

		/* Visible monsters */
		if (mflag_has(m_ptr->mflag, MFLAG_VISIBLE) &&
			!mflag_has(m_ptr->mflag, MFLAG_UNAWARE))
			return (TRUE);
	}

    /* Traps */
    if (square_isvisibletrap(cave, y, x))
		return(TRUE);

	/* Scan all objects in the grid */
	for (obj = square_object(cave, y, x); obj; obj = obj->next)
		/* Memorized object */
		if (obj->marked && !ignore_item_ok(obj)) return (TRUE);

	/* Interesting memorized features */
	if (square_ismark(cave, y, x) && !square_isboring(cave, y, x))
		return (TRUE);

	/* Nope */
	return (FALSE);
}

/*
 * Describe a location relative to the player position.
 * e.g. "12 S 35 W" or "0 N, 33 E" or "0 N, 0 E"
 */
void coords_desc(char *buf, int size, int y, int x)
{
	const char *east_or_west;
	const char *north_or_south;

	int py = player->py;
	int px = player->px;

	if (y > py)
		north_or_south = "S";
	else
		north_or_south = "N";

	if (x < px)
		east_or_west = "W";
	else
		east_or_west = "E";

	strnfmt(buf, size, "%d %s, %d %s",
		ABS(y-py), north_or_south, ABS(x-px), east_or_west);
}

/**
 * Obtains the location the player currently targets.
 *
 * Both `col` and `row` must point somewhere, and on function termination,
 * contain the X and Y locations respectively.
 */
void target_get(s16b *col, s16b *row)
{
	assert(col);
	assert(row);

	*col = target_x;
	*row = target_y;
}


/**
 * Returns the currently targeted monster index.
 */
struct monster *target_get_monster(void)
{
	return target_who;
}


/*
 * True if the player's current target is in LOS.
 */
bool target_sighted(void)
{
	return target_okay() &&
			panel_contains(target_y, target_x) &&
			 /* either the target is a grid and is visible, or it is a monster
			  * that is visible */
			((!target_who && player_can_see_bold(target_y, target_x)) ||
			 (target_who && mflag_has(target_who->mflag, MFLAG_VISIBLE)));
}

