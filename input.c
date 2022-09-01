/*  $Id: input.c,v 1.1 2022/07/29 02:47:07 dave Exp dave $

	input.c - Read contents of container file.
	
	Copyright (C) 2008 David Shepperd

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "rtpip.h"

/**
 * @file input.c
 * Read contents of container file.
 */

/**
 * Read input file and do any crlf processing.
 * @param options - pointer to options.
 * @return 0 if success, 1 if failure
 */
int readInpFile(Options_t *options, const char *fileName)
{
	int retv;
	struct stat st;
	FILE *inp;
	InHandle_t *ihp;

	retv = stat(fileName, &st);
	if ( retv )
	{
		fprintf(stderr, "Unable to stat '%s': %s\n", fileName, strerror(errno));
		return 1;
	}
	ihp = &options->iHandle;
	ihp->fileTimeStamp = st.st_ctime;
	retv = (st.st_size + BLKSIZ - 1) & -BLKSIZ;
	if ( retv > ihp->inFileBufSize )
	{
		ihp->inFileBuf = (char *)realloc(ihp->inFileBuf, retv);
		if ( !ihp->inFileBuf )
		{
			fprintf(stderr, "Unable to allocate %d bytes for input file: %s\n",
					retv, strerror(errno));
			return 1;
		}
		ihp->inFileBufSize = retv;
	}
	inp = fopen(fileName, "rb");
	if ( !inp )
	{
		fprintf(stderr, "Error opening '%s' for input: %s\n",
				fileName, strerror(errno));
		return 1;
	}
	retv = fread(ihp->inFileBuf, 1, st.st_size, inp);
	if ( retv != st.st_size )
	{
		fprintf(stderr, "Error reading '%s'. Expected %ld bytes, got %d: %s\n",
				fileName, st.st_size, retv, strerror(errno));
		fclose(inp);
		return 1;
	}
	fclose(inp);
	if ( (options->inOpts & INOPTS_ASC) )
	{
		char *oBuf, *src, *dst;
		int oBufSize, hist;

		oBufSize = (st.st_size * 2 + 1 + BLKSIZ - 1) & -BLKSIZ;
		oBuf = (char *)calloc(oBufSize, 1);
		if ( !oBuf )
		{
			fprintf(stderr, "Unable to allocate %d bytes for input file converted to crlf: %s\n",
					oBufSize, strerror(errno));
			return 1;
		}
		hist = 0;
		dst = oBuf;
		src = ihp->inFileBuf;
		while ( src < ihp->inFileBuf + st.st_size )
		{
			if ( *src == '\n' && hist != '\r' )
				*dst++ = '\r';
			hist = *src++;
			*dst++ = hist;
		}
		if ( (options->inOpts&INOPTS_CTLZ) )
			*dst++ = 'Z' & 63;   /* Add Control-Z at end if asked for */
		retv = (dst - oBuf);
		free(ihp->inFileBuf);
		ihp->inFileBuf = oBuf;
		ihp->inFileBufSize = oBufSize;
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("readInpFile: expanded '%s' from %ld bytes (%ld blocks) to %d bytes (%d blocks).\n",
				   fileName,
				   st.st_size, (st.st_size + BLKSIZ - 1) / BLKSIZ,
				   retv, (retv + BLKSIZ - 1) / BLKSIZ);
		}
	}
	else if ( retv - st.st_size )
		memset(ihp->inFileBuf + st.st_size, 0, retv - st.st_size);
	ihp->fileBlks = (retv + BLKSIZ - 1) / BLKSIZ;
	return 0;
}


