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
#include "screen_access.h"
#include "viewport.h"
#include "itunesdb.h"
#include "slide.h"
#include "skin_engine.h"
#include "wps_internals.h"
#include "cover_drift.h"

#ifdef ITUNESDB_HAVE_ART

/* The side of a cover: the screen's height, which is the panel's, so that it
   is wider than the panel and has room to drift across it */
#define DRIFT_SIZE  LCD_HEIGHT
/* How long a cover is on the panel, its fades in and out included */
#define DRIFT_TICKS (9*HZ)
#define FADE_TICKS  (HZ*6/5)
#define FRAME_TICKS (HZ/10)

/* RGB565 spread over 32 bits, so that all three channels mix in one multiply */
#define SPREAD_MASK 0x07e0f81f
#define SPREAD(p)   (((uint32_t)(p) << 16 | (p)) & SPREAD_MASK)
#define UNSPREAD(x) ((fb_data)((x) >> 16 | (x)))

extern struct frame_buffer_t lcd_framebuffer_default;

struct drift_cover
{
    struct bitmap bm;
    int slide;      /* its album's, so that the next is another's */
    bool loaded;
    bool tried;     /* to load, since the other came */
    fb_data pixels[DRIFT_SIZE * DRIFT_SIZE]; /* 115 KB at 240 */
};

static struct drift_cover covers[2];
static int shown;           /* the one drifting; the other fades in after it */
static long start;          /* when it came */
static long next_frame;
static struct { int x, y, w, h; } panel;
static uint32_t random_state;

static uint32_t next_random(void)
{
    if (!random_state)
        random_state = current_tick | 1;
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

/* another cover, of an album other than the one shown and the one before */
static void load(struct drift_cover *c)
{
    const struct drift_cover *other = c == &covers[0] ? &covers[1] : &covers[0];
    struct itunesdb_art art;
    c->loaded = false;
    c->tried = true;
    if (!itunesdb_random_art(next_random(), other->slide, c->slide, &art))
        return;
    c->bm.data = (unsigned char *)c->pixels;
    c->bm.width = c->bm.height = DRIFT_SIZE;
    if (itunesdb_load_art(&art, &c->bm, sizeof(c->pixels) + sizeof(struct bitmap)) < 0
        || c->bm.width < panel.w)
        return;
    c->slide = art.slide;
    c->loaded = true;
}

/* the left of the panel on a cover this old: from its right end to its left,
   so that the picture goes right */
static int drift_x(const struct drift_cover *c, long age)
{
    const int span = c->bm.width - panel.w;
    age = MAX(0, MIN(age, DRIFT_TICKS));
    return span - span * age / DRIFT_TICKS;
}

static const fb_data *cover_row(const struct drift_cover *c, int x, int y)
{
    y += (c->bm.height - panel.h) / 2;
    y = MAX(0, MIN(y, c->bm.height - 1));
    return (const fb_data *)c->bm.data + y * c->bm.width + x;
}

/* the panel at time t, into the frame buffer from its top left */
static void render(fb_data *fb, long t)
{
    const struct drift_cover *now = &covers[shown], *next = &covers[!shown];
    const long age = t - start;
    const long next_age = age - (DRIFT_TICKS - FADE_TICKS);
    const int fade = next->loaded && next_age > 0 ?    /* of the next, in 32nds */
                     MIN(32, next_age * 32 / FADE_TICKS) : 0;
    const int now_x = drift_x(now, age), next_x = drift_x(next, next_age);

    for (int y = 0; y < panel.h; y++, fb += LCD_FBWIDTH)
    {
        const fb_data *a = cover_row(now, now_x, y);
        if (fade == 0 || fade == 32)
        {
            memcpy(fb, fade ? cover_row(next, next_x, y) : a,
                   panel.w * sizeof(fb_data));
            continue;
        }
        const fb_data *b = cover_row(next, next_x, y);
        for (int x = 0; x < panel.w; x++)
        {
            const uint32_t pa = SPREAD(a[x]), pb = SPREAD(b[x]);
            fb[x] = UNSPREAD(((pa * (32 - fade) + pb * fade) >> 5) & SPREAD_MASK);
        }
    }
}

/* the next cover takes over once it has faded in, and another is read to come
   after it once it's been on for a second */
static void advance(long t)
{
    struct drift_cover *next = &covers[!shown];
    if (t - start >= DRIFT_TICKS)
    {
        if (!next->loaded && !next->tried)
            load(next);
        if (next->loaded)
        {
            shown = !shown;
            start += DRIFT_TICKS - FADE_TICKS;
            covers[!shown].loaded = false;
            covers[!shown].tried = false;
        }
        else
            start = t - DRIFT_TICKS; /* stays at the end, the left of it shown */
    }
    else if (t - start >= HZ && !next->loaded && !next->tried)
        load(next);
}

bool cover_drift_available(void)
{
    return itunesdb_art_count() > 0;
}

void cover_drift_draw(struct screen *display, struct viewport *vp)
{
    (void)display;
    panel.x = vp->x;
    panel.y = vp->y;
    panel.w = MIN(vp->width, DRIFT_SIZE);
    panel.h = MIN(vp->height, DRIFT_SIZE);

    const long t = current_tick;
    struct drift_cover *now = &covers[shown];
    if (!now->loaded)
    {
        load(now);
        covers[!shown].tried = false;
        start = t;
    }
    else if (t - start > DRIFT_TICKS)
        start = t; /* back after a while: from the right end again */
    if (!now->loaded)
        return;

    advance(t);
    render(FBADDR(panel.x, panel.y), t);
    next_frame = t + FRAME_TICKS;
}

int cover_drift_tick(void)
{
    if (panel.w <= 0 || !covers[shown].loaded
        || !viewportmanager_theme_shown(SCREEN_MAIN))
        return -1;
#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
    if (!lcd_active())
        return -1;
#endif
    /* not over a screen that is about to slide in */
    if (slide_waiting(SLIDE_ENTER) || slide_waiting(SLIDE_LEAVE))
        return -1;
    struct wps_data *data = skin_get_gwps(CUSTOM_STATUSBAR, SCREEN_MAIN)->data;
    if (!data->wps_loaded)
        return -1;
    struct skin_viewport *svp = skin_find_item(COVER_DRIFT_LABEL, SKIN_FIND_VP, data);
    if (!svp || (svp->hidden_flags & (VP_DRAW_HIDDEN|VP_NEVER_VISIBLE)))
        return -1;

    const long t = current_tick;
    if (TIME_BEFORE(t, next_frame))
        return next_frame - t;
    advance(t);
    render(lcd_framebuffer_default.fb_ptr + panel.y * LCD_FBWIDTH + panel.x, t);
    lcd_update_rect(panel.x, panel.y, panel.w, panel.h);
    next_frame = t + FRAME_TICKS;
    return FRAME_TICKS;
}

#else /* no covers of Finder's */

bool cover_drift_available(void) { return false; }
void cover_drift_draw(struct screen *display, struct viewport *vp)
{
    (void)display;
    (void)vp;
}
int cover_drift_tick(void) { return -1; }

#endif
