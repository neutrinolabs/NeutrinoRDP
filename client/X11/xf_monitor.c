/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Monitor Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#if defined(WITH_XRANDR)
#include <X11/extensions/Xrandr.h>
#elif defined(WITH_XINERAMA)
#include <X11/extensions/Xinerama.h>
#endif

#include "xf_monitor.h"

/* See MSDN Section on Multiple Display Monitors: http://msdn.microsoft.com/en-us/library/dd145071 */

tbool xf_detect_monitors(xfInfo* xfi, rdpSettings* settings)
{
	int i;
	VIRTUAL_SCREEN* vscreen;

#if defined(WITH_XRANDR)
	int ignored, ignored2;
	RROutput primary_output;
	XRRScreenResources *screen_resources;
	Window root;
	XRROutputInfo *oi;
	XRRCrtcInfo *ci;
	RROutput output;
	int count;
	int got_primary;
#elif defined(WITH_XINERAMA)
	int ignored, ignored2;
	XineramaScreenInfo* screen_info = NULL;
#endif

	printf("xf_detect_monitors:\n");

	if (settings->num_monitors > 0)
	{
		/* already setup in args */
		return true;
	}

	vscreen = &xfi->vscreen;

	if (xf_GetWorkArea(xfi) == false)
	{
		xfi->workArea.x = 0;
		xfi->workArea.y = 0;
		xfi->workArea.width = WidthOfScreen(xfi->screen);
		xfi->workArea.height = HeightOfScreen(xfi->screen);
	}

	if (settings->fullscreen)
	{
		settings->width = WidthOfScreen(xfi->screen);
		settings->height = HeightOfScreen(xfi->screen);
	}
	else if (settings->workarea)
	{
		settings->width = xfi->workArea.width;
		settings->height = xfi->workArea.height;
	}
	else if (settings->percent_screen)
	{
		settings->width = (xfi->workArea.width * settings->percent_screen) / 100;
		settings->height = (xfi->workArea.height * settings->percent_screen) / 100;
	}

	if (settings->fullscreen == false && settings->workarea == false)
		return true;

	vscreen->monitors = xzalloc(sizeof(MONITOR_INFO) * 16);

#if defined(WITH_XRANDR)
	if (XRRQueryExtension(xfi->display, &ignored, &ignored2))
	{
		root = RootWindowOfScreen(xfi->screen);
		primary_output = XRRGetOutputPrimary(xfi->display, root);
		screen_resources = XRRGetScreenResources(xfi->display, root);
		if (screen_resources != NULL)
		{
			count = 0;
			for (i = 0; i < screen_resources->noutput; i++)
			{
				output = screen_resources->outputs[i];
				oi = XRRGetOutputInfo(xfi->display, screen_resources, output);
				if (oi != NULL)
				{
					if (oi->connection == RR_Connected)
					{
						ci = XRRGetCrtcInfo(xfi->display, screen_resources, oi->crtc);
						if (ci != NULL)
						{
							if (count < 16)
							{
								vscreen->monitors[count].area.left = ci->x; 
								vscreen->monitors[count].area.top = ci->y;
								vscreen->monitors[count].area.right = ci->x + ci->width - 1;
								vscreen->monitors[count].area.bottom = ci->y + ci->height - 1;
								vscreen->monitors[count].primary = output == primary_output;
								count++;
							}
							XRRFreeCrtcInfo(ci);
						}
					}
					XRRFreeOutputInfo(oi); 
				}
			}
			vscreen->nmonitors = count;
			XRRFreeScreenResources(screen_resources); 
		}
	}
#elif defined(WITH_XINERAMA)
	if (XineramaQueryExtension(xfi->display, &ignored, &ignored2))
	{
		if (XineramaIsActive(xfi->display))
		{
			screen_info = XineramaQueryScreens(xfi->display, &vscreen->nmonitors);
			vscreen->nmonitors = MIN(16, vscreen->nmonitors);
			if (vscreen->nmonitors > 0)
			{
				for (i = 0; i < vscreen->nmonitors; i++)
				{
					vscreen->monitors[i].area.left = screen_info[i].x_org;
					vscreen->monitors[i].area.top = screen_info[i].y_org;
					vscreen->monitors[i].area.right = screen_info[i].x_org + screen_info[i].width - 1;
					vscreen->monitors[i].area.bottom = screen_info[i].y_org + screen_info[i].height - 1;
					if ((screen_info[i].x_org == 0) && (screen_info[i].y_org == 0))
						vscreen->monitors[i].primary = true;
				}
			}
			XFree(screen_info);
		}
	}
#endif

	settings->num_monitors = vscreen->nmonitors;
	got_primary = 0;
	printf("xf_detect_monitors: nmonitors %d\n", vscreen->nmonitors);
	for (i = 0; i < vscreen->nmonitors; i++)
	{
		settings->monitors[i].x = vscreen->monitors[i].area.left;
		settings->monitors[i].y = vscreen->monitors[i].area.top;
		settings->monitors[i].width = vscreen->monitors[i].area.right - vscreen->monitors[i].area.left + 1;
		settings->monitors[i].height = vscreen->monitors[i].area.bottom - vscreen->monitors[i].area.top + 1;
#if 1 /* todo get the right primary working, windows 10 needs primary to be 0, 0 */
		settings->monitors[i].is_primary = (settings->monitors[i].x == 0) && (settings->monitors[i].y == 0);
#else
		settings->monitors[i].is_primary = vscreen->monitors[i].primary;
#endif
		printf("  monitor %d left %d top %d right %d bottom %d primary %d\n", i, vscreen->monitors[i].area.left,
				vscreen->monitors[i].area.top, vscreen->monitors[i].area.right, vscreen->monitors[i].area.bottom,
				vscreen->monitors[i].primary);
		if (settings->monitors[i].is_primary)
		{
			/* primary must be 0, 0 */
			xfi->primary_adjust_x = settings->monitors[i].x;
			xfi->primary_adjust_y = settings->monitors[i].y;
			got_primary = 1;
		}
		vscreen->area.left = MIN(vscreen->monitors[i].area.left, vscreen->area.left);
		vscreen->area.right = MAX(vscreen->monitors[i].area.right, vscreen->area.right);
		vscreen->area.top = MIN(vscreen->monitors[i].area.top, vscreen->area.top);
		vscreen->area.bottom = MAX(vscreen->monitors[i].area.bottom, vscreen->area.bottom);
	}
	printf("  got_primary %d primary_adjust_x %d primary_adjust_y %d\n", got_primary,
			xfi->primary_adjust_x, xfi->primary_adjust_y);
	if (got_primary)
	{
		for (i = 0; i < settings->num_monitors; i++)
		{
			settings->monitors[i].x -= xfi->primary_adjust_x;
			settings->monitors[i].y -= xfi->primary_adjust_y;
			printf("  after adjust monitor %d x %d y %d width %d height %d primary %d\n", i,
					settings->monitors[i].x, settings->monitors[i].y,
					settings->monitors[i].width, settings->monitors[i].height,
					settings->monitors[i].is_primary);
		}
	}
	if (settings->num_monitors)
	{
		settings->width = vscreen->area.right - vscreen->area.left + 1;
		settings->height = vscreen->area.bottom - vscreen->area.top + 1;
	}
	return true;
}
