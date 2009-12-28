/*
   Copyright (c) 2009 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#include "freerdp.h"
#include "xf_win.h"
#include "chan_man.h"

static int
set_default_params(rdpSet * settings)
{
	memset(settings, 0, sizeof(rdpSet));
	strcpy(settings->hostname, "test");
	settings->width = 1024;
	settings->height = 768;
	strcpy(settings->server, "127.0.0.1");
	strcpy(settings->username, "guest");
	settings->tcp_port_rdp = 3389;
	settings->encryption = 1;
	settings->server_depth = 16;
	settings->bitmap_cache = 1;
	settings->bitmap_compression = 1;
	settings->desktop_save = 0;
	settings->rdp5_performanceflags =
		RDP5_NO_WALLPAPER | RDP5_NO_FULLWINDOWDRAG | RDP5_NO_MENUANIMATIONS;
	settings->off_screen_bitmaps = 1;
	settings->triblt = 0;
	settings->new_cursors = 1;
	settings->rdp_version = 5;
	return 0;
}

static int
process_params(rdpSet * settings, int argc, char ** argv)
{
	int index;
	int max;
	char * p;
	struct passwd * pw;

	set_default_params(settings);
	pw = getpwuid(getuid());
	if (pw != 0)
	{
		if (pw->pw_name != 0)
		{
			max = sizeof(settings->username) - 1;
			strncpy(settings->username, pw->pw_name, max);
		}
	}
	printf("process_params\n");
	if (argc < 2)
	{
		return 1;
	}
	for (index = 1; index < argc; index++)
	{
		if (strcmp("-a", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				return 1;
			}
			settings->server_depth = atoi(argv[index]);
		}
		else if (strcmp("-u", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				return 1;
			}
			strncpy(settings->username, argv[index], 255);
			settings->username[255] = 0;
		}
		else if (strcmp("-p", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				return 1;
			}
			strncpy(settings->password, argv[index], 63);
			settings->password[63] = 0;
			settings->autologin = 1;
		}
		else if (strcmp("-g", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				return 1;
			}
			settings->width = strtol(argv[index], &p, 10);
			if (*p == 'x')
			{
				settings->height = strtol(p + 1, &p, 10);
			}
			if ((settings->width < 16) || (settings->height < 16) ||
				(settings->width > 4096) || (settings->height > 4096))
			{
				printf("invalid parameter\n");
				return 1;
			}
		}
		else if (strcmp("-t", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				return 1;
			}
			settings->tcp_port_rdp = atoi(argv[index]);
		}
		else if (strcmp("-z", argv[index]) == 0)
		{
			settings->bulk_compression = 1;
		}
		else if (strcmp("-x", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				return 1;
			}
			if (strncmp("m", argv[index], 1) == 0) /* modem */
			{
				settings->rdp5_performanceflags = RDP5_NO_WALLPAPER |
					RDP5_NO_FULLWINDOWDRAG |  RDP5_NO_MENUANIMATIONS |
					RDP5_NO_THEMING;
			}
			else if (strncmp("b", argv[index], 1) == 0) /* broadband */
			{
				settings->rdp5_performanceflags = RDP5_NO_WALLPAPER;
			}
			else if (strncmp("l", argv[index], 1) == 0) /* lan */
			{
				settings->rdp5_performanceflags = RDP5_DISABLE_NOTHING;
			}
			else
			{
				settings->rdp5_performanceflags = strtol(argv[index], 0, 16);
			}
		}
		else if (strcmp("-plugin", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				return 1;
			}
			chan_man_load_plugin(settings, argv[index]);
		}
		else
		{
			strncpy(settings->server, argv[index], 63);
			settings->server[63] = 0;
		}
	}
	return 0;
}

static int
run_xfreerdp(rdpSet * settings)
{
	rdpInst * inst;
	void * read_fds[32];
	void * write_fds[32];
	int read_count;
	int write_count;
	int index;
	int sck;
	int max_sck;
	fd_set rfds;
	fd_set wfds;

	printf("run_xfreerdp:\n");
	/* create an instance of the library */
	inst = freerdp_init(settings);
	if (inst == NULL)
	{
		printf("run_xfreerdp: freerdp_init failed\n");
		return 1;
	}
	if ((inst->version != FREERDP_INTERFACE_VERSION) ||
	    (inst->size != sizeof(rdpInst)))
	{
		printf("run_xfreerdp: freerdp_init size, version / size do not "
		       "match expecting v %d s %d got v %d s %d\n",
		       FREERDP_INTERFACE_VERSION, sizeof(rdpInst),
		       inst->version, inst->size);
		return 1;
	}
	if (xf_pre_connect(inst) != 0)
	{
		printf("run_xfreerdp: xf_pre_connect failed\n");
		return 1;
	}
	if (chan_man_pre_connect(inst) != 0)
	{
		printf("run_xfreerdp: chan_man_pre_connect failed\n");
		return 1;
	}
	/* call connect */
	printf("keyboard_layout: %X\n", inst->settings->keyboard_layout);
	if (inst->rdp_connect(inst) != 0)
	{
		printf("run_xfreerdp: inst->rdp_connect failed\n");
		return 1;
	}
	if (chan_man_post_connect(inst) != 0)
	{
		printf("run_xfreerdp: chan_man_post_connect failed\n");
		return 1;
	}
	if (xf_post_connect(inst) != 0)
	{
		printf("run_xfreerdp: xf_post_connect failed\n");
		return 1;
	}
	/* program main loop */
	while (1)
	{
		read_count = 0;
		write_count = 0;
		/* get libfreerdp fds */
		if (inst->rdp_get_fds(inst, read_fds, &read_count, write_fds, &write_count) != 0)
		{
			printf("run_xfreerdp: inst->rdp_get_fds failed\n");
			break;
		}
		/* get x fds */
		if (xf_get_fds(inst, read_fds, &read_count, write_fds, &write_count) != 0)
		{
			printf("run_xfreerdp: xf_get_fds failed\n");
			break;
		}
		/* get channel fds */
		if (chan_man_get_fds(inst, read_fds, &read_count, write_fds, &write_count) != 0)
		{
			printf("run_xfreerdp: chan_man_get_fds failed\n");
			break;
		}
		max_sck = 0;
		/* setup read fds */
		FD_ZERO(&rfds);
		for (index = 0; index < read_count; index++)
		{
			sck = (int) (read_fds[index]);
			if (sck > max_sck)
				max_sck = sck;
			FD_SET(sck, &rfds);
		}
		/* setup write fds */
		FD_ZERO(&wfds);
		for (index = 0; index < write_count; index++)
		{
			sck = (int) (write_fds[index]);
			if (sck > max_sck)
				max_sck = sck;
			FD_SET(sck, &wfds);
		}
		/* exit if nothing to do */
		if (max_sck == 0)
		{
			printf("run_xfreerdp: max_sck is zero\n");
			break;
		}
		/* do the wait */
		if (select(max_sck + 1, &rfds, &wfds, NULL, NULL) == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				printf("run_xfreerdp: select failed\n");
				break;
			}
		}
		/* check the libfreerdp fds */
		if (inst->rdp_check_fds(inst) != 0)
		{
			printf("run_xfreerdp: inst->rdp_check_fds failed\n");
			break;
		}
		/* check x fds */
		if (xf_check_fds(inst) != 0)
		{
			printf("run_xfreerdp: xf_check_fds failed\n");
			break;
		}
		/* check channel fds */
		if (chan_man_check_fds(inst) != 0)
		{
			printf("run_xfreerdp: chan_man_check_fds failed\n");
			break;
		}
	}
	/* cleanup */
	xf_deinit(inst);
	freerdp_deinit(inst);
	return 0;
}

int
main(int argc, char ** argv)
{
	rdpSet settings;
	int rv;

	setlocale(LC_CTYPE, "");
	chan_man_init();
	rv = process_params(&settings, argc, argv);
	if (rv == 0)
	{
		rv = run_xfreerdp(&settings);
	}
	chan_man_deinit();
	return rv;
}
