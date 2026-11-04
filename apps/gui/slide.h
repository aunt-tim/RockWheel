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
#ifndef _GUI_SLIDE_H_
#define _GUI_SLIDE_H_

#include <stdbool.h>

struct viewport;

/* The slide between two lists, as on a stock iPod: a list entered comes in
   from the right and pushes the old one off to the left, and a list left
   goes back the other way. */
enum slide_direction
{
    SLIDE_ENTER,
    SLIDE_LEAVE,
};

/* Navigation is about to go to another list: the screen as it is now is kept,
   and the next list or Now Playing drawn and shown slides over it. If none is
   shown within two seconds, nothing slides. */
void slide_prepare(enum slide_direction dir);

/* Navigation turned out not to change the list: nothing slides */
void slide_cancel(void);

/* True if a slide in this direction is waiting for its list */
bool slide_waiting(enum slide_direction dir);

/* How many slides have been shown, to tell whether one was since */
unsigned slide_count(void);

/* From the lists: one was drawn, in this viewport of the main screen */
void slide_list_drawn(const struct viewport *vp);

/* From Now Playing: it was drawn whole, under a title bar of its own where the
   lists have theirs, so all of the screen below that slides */
void slide_screen_drawn(void);

/* From the viewport manager: the theme was turned off, as by a plugin, so what's
   on the screen isn't one to slide from until a list or Now Playing is drawn */
void slide_forget(void);

/* From the viewport manager, in place of the main screen's update: true if a
   slide was waiting and has now brought the screen up to date */
bool slide_run(void);

#endif /* _GUI_SLIDE_H_ */
