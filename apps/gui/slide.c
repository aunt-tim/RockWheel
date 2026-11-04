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
#include "config.h"
#include <string.h>
#include "system.h"
#include "kernel.h"
#include "lcd.h"
#include "viewport.h"
#include "slide.h"

#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE

/* From the first frame to the last; a stock iPod takes about as long */
#define SLIDE_TICKS (HZ/5)
/* The longest the next screen may take to be drawn after the navigation, as
   when a song starts before Now Playing shows */
#define SLIDE_TIMEOUT (2*HZ)

struct slide_rect
{
    int x, y, w, h;
};

extern struct frame_buffer_t lcd_framebuffer_default;

/* The screen before the navigation and after it, whole: 150 KB each at 320x240 */
static fb_data before[LCD_HEIGHT * LCD_FBWIDTH];
static fb_data after[LCD_HEIGHT * LCD_FBWIDTH];

static struct
{
    bool waiting;
    bool list_drawn;        /* since slide_prepare() */
    enum slide_direction dir;
    long deadline;
    struct slide_rect from; /* the list that was on the screen */
} slide;

static bool list_shown;     /* what's on the screen is a list or Now Playing */
static struct slide_rect list_rect;
static unsigned slides;

void slide_prepare(enum slide_direction dir)
{
    slide.waiting = false;
    if (!list_shown)
        return;
#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
    if (!lcd_active())
        return;
#endif
    memcpy(before, lcd_framebuffer_default.data, sizeof before);
    slide.waiting = true;
    slide.list_drawn = false;
    slide.dir = dir;
    slide.deadline = current_tick + SLIDE_TIMEOUT;
    slide.from = list_rect;
}

void slide_cancel(void)
{
    slide.waiting = false;
}

bool slide_waiting(enum slide_direction dir)
{
    return slide.waiting && slide.dir == dir;
}

unsigned slide_count(void)
{
    return slides;
}

void slide_list_drawn(const struct viewport *vp)
{
    list_shown = true;
    list_rect = (struct slide_rect){ vp->x, vp->y, vp->width, vp->height };
    if (slide.waiting)
        slide.list_drawn = true;
}

void slide_screen_drawn(void)
{
    /* below the top of the last list, which is the title bar's height */
    int y = list_rect.h ? list_rect.y : 0;
    list_shown = true;
    list_rect = (struct slide_rect){ 0, y, LCD_WIDTH, LCD_HEIGHT - y };
    if (slide.waiting)
        slide.list_drawn = true;
}

void slide_forget(void)
{
    list_shown = false;
}

/* The region with the new screen p pixels in from its side */
static void draw_frame(const struct slide_rect *r, int p)
{
    fb_data *fb = lcd_framebuffer_default.fb_ptr;
    size_t in = p * sizeof(fb_data), out = (r->w - p) * sizeof(fb_data);

    for (int y = r->y; y < r->y + r->h; y++)
    {
        int row = y * LCD_FBWIDTH + r->x;
        if (slide.dir == SLIDE_ENTER)
        {
            /* the old list leaves by the left, the new one comes from the right */
            memcpy(&fb[row], &before[row + p], out);
            memcpy(&fb[row + r->w - p], &after[row], in);
        }
        else
        {
            memcpy(&fb[row], &after[row + r->w - p], in);
            memcpy(&fb[row + p], &before[row], out);
        }
    }
}

bool slide_run(void)
{
    if (!slide.waiting)
        return false;
    if (TIME_AFTER(current_tick, slide.deadline))
    {
        slide.waiting = false;
        return false;
    }
    if (!slide.list_drawn)
        return false;
    slide.waiting = false;
#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
    if (!lcd_active())
        return false;
#endif

    /* what slides is both lists: half the width beside the panel, or all of it
       if either list takes the whole width */
    struct slide_rect r = slide.from;
    int x2 = MAX(r.x + r.w, list_rect.x + list_rect.w);
    int y2 = MAX(r.y + r.h, list_rect.y + list_rect.h);
    r.x = MAX(MIN(r.x, list_rect.x), 0);
    r.y = MAX(MIN(r.y, list_rect.y), 0);
    r.w = MIN(x2, LCD_WIDTH) - r.x;
    r.h = MIN(y2, LCD_HEIGHT) - r.y;
    if (r.w <= 0 || r.h <= 0)
        return false;

    memcpy(after, lcd_framebuffer_default.data, sizeof after);

    /* the same list again, as when the main menu's Shuffle Songs finds no songs */
    int y;
    for (y = r.y; y < r.y + r.h; y++)
    {
        int row = y * LCD_FBWIDTH + r.x;
        if (memcmp(&before[row], &after[row], r.w * sizeof(fb_data)))
            break;
    }
    if (y == r.y + r.h)
        return false;

    /* the rest of the screen, the title above all, is new from the start */
    draw_frame(&r, 0);
    lcd_update();

    long start = current_tick;
    int shown = 0;
    for (;;)
    {
        yield();
        long t = current_tick - start;
        if (t >= SLIDE_TICKS)
            break;
        /* fast at first, and slowing down to a stop */
        long u = SLIDE_TICKS - t;
        int p = r.w - r.w * u * u * u / (SLIDE_TICKS * SLIDE_TICKS * SLIDE_TICKS);
        if (p == shown)
            continue;
        shown = p;
        draw_frame(&r, p);
        lcd_update_rect(r.x, r.y, r.w, r.h);
    }
    draw_frame(&r, r.w);
    lcd_update_rect(r.x, r.y, r.w, r.h);
    slides++;
    return true;
}

#else /* no slides on this screen */

void slide_prepare(enum slide_direction dir) { (void)dir; }
void slide_cancel(void) {}
bool slide_waiting(enum slide_direction dir) { (void)dir; return false; }
unsigned slide_count(void) { return 0; }
void slide_list_drawn(const struct viewport *vp) { (void)vp; }
void slide_screen_drawn(void) {}
void slide_forget(void) {}
bool slide_run(void) { return false; }

#endif
