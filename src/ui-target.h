/**
 * \file ui-target.h
 * \brief UI for targetting code
 *
 * Copyright (c) 1997-2014 Angband contributors
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


#ifndef UI_TARGET_H
#define UI_TARGET_H

/*
 * Height of the help screen; any higher than 4 will overlap the health
 * bar which we want to keep in targeting mode.
 */
#define HELP_HEIGHT 3

/* Size of the array that is used for object names during targeting. */
#define TARGET_OUT_VAL_SIZE 256

#define TS_INITIAL_SIZE	20

void target_display_help(bool monster, bool free);
bool target_set_closest(int mode);
bool target_set_interactive(int mode, int x, int y);

#endif /* UI_TARGET_H */
