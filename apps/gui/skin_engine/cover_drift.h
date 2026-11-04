/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef _COVER_DRIFT_H_
#define _COVER_DRIFT_H_

#include <stdbool.h>

/* Covers of the library, picked by random, that drift slowly from left to right
   across the panel beside the menus, each fading into the next. The theme shows
   them with %Ck in a viewport labelled COVER_DRIFT_LABEL, and asks with %?Ck
   whether the library has any. */
#define COVER_DRIFT_LABEL "drift"

struct screen;
struct viewport;

bool cover_drift_available(void);

/* From the skin, as the viewport is drawn */
void cover_drift_draw(struct screen *display, struct viewport *vp);

/* From the lists, as they wait for a button: draws the next frame if it's
   time, and says in how many ticks the one after is due, or -1 if the covers
   aren't on the screen */
int cover_drift_tick(void);

#endif /* _COVER_DRIFT_H_ */
