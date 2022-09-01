/*  $Id: do_del.c,v 1.1 2022/07/29 02:47:07 dave Exp dave $

	do_del.c - Delete a file from container.
	
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
 * @file do_del.c
 * Delete a file from container. Called from rtpip.
 */

/**
 * Delete RT11 files from container.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
int do_del(Options_t *options)
{
	Rt11DirEnt_t *dirptr;
	int ii, totFiles = 0, totUsed = 0;
	InWorkingDir_t *wdp;

	wdp = options->wDirArray;
	for ( ii = 0; ii < options->numWdirs; ++ii, ++wdp )
	{
		dirptr = &wdp->rt11;
		if ( !(dirptr->control & PERM) )
		{
			continue;
		}
		if ( !filterFilename(options, wdp->ffull) )
			continue;
		if ( !(options->delOpts & DELOPTS_NOASK) )
		{
			char prompt[128];
			int yn;
			snprintf(prompt, sizeof(prompt) - 1, "Delete '%s'?", wdp->ffull);
			yn = getYN(prompt, YN_NO);
			if ( yn == YN_QUIT )
				break;
			if ( yn != YN_YES )
				continue;
		}
		dirptr->control = EMPTY;
		options->dirDirty = 1;
		if ( options->verbose || (options->delOpts & DELOPTS_VERB) )
		{
			printf("Deleted '%s'\n", wdp->ffull);
		}
		options->totEmpty += dirptr->blocks;
		options->totPerm -= dirptr->blocks;
		totUsed += dirptr->blocks;
		++totFiles;
	}
	linearToDisk(options);
	if ( options->verbose || (options->delOpts & DELOPTS_VERB) )
	{
		printf("Deleted a total of %d file%s, %d blocks.\n"
			   "Disk now has %d blocks used, %d blocks free.\n",
			   totFiles, totFiles == 1 ? "" : "s", totUsed,
			   options->totPerm, options->totEmpty);
	}
	return 0;
}

int preDelete(Options_t *options)
{
	InWorkingDir_t *wdp;
	Rt11DirEnt_t *dirptr;
	int ii;

	options->iHandle.sizeMatch = NULL;
	/* Now sweep through the list of files and pre-delete any existing entry */
	wdp = options->wDirArray;
	for ( ii = 0; ii < options->numWdirs; ++ii, ++wdp )
	{
		dirptr = &wdp->rt11;
		if ( (dirptr->control & PERM) )
		{
			if (    dirptr->name[0] == options->iHandle.iNameR50[0]
				 && dirptr->name[1] == options->iHandle.iNameR50[1]
				 && dirptr->name[2] == options->iHandle.iNameR50[2] )
			{
				dirptr->control = EMPTY;
				options->dirDirty = 1;
				if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
				{
					printf("preDelete: Found and deleted %s. LBA=%d, size=%d\n",
						   options->iHandle.argFN, wdp->lba, dirptr->blocks);
				}
				options->totEmpty += dirptr->blocks;
				options->totPerm -= dirptr->blocks;
			}
		}
		/* While we're sweeping, keep track of an entry we can use for the new file */
		if ( !(dirptr->control & PERM) )
		{
			if ( options->iHandle.fileBlks <= dirptr->blocks
				 && (!options->iHandle.sizeMatch
					 || dirptr->blocks < options->iHandle.sizeMatch->rt11.blocks)
			   )
			{
				if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
				{
					printf("preDelete: Found a size match for %s, size:%d at index %ld. LBA=%d, size=%d\n",
						   options->iHandle.argFN,
						   options->iHandle.fileBlks,
						   wdp - options->wDirArray,
						   wdp->lba, dirptr->blocks);
				}
				options->iHandle.sizeMatch = wdp;
			}
		}
	}
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("preDelete: After looking for '%s' with size: %d. sizeMatch %s, val=%d. Empty=%d, Perm=%d\n",
			   options->iHandle.argFN,
			   options->iHandle.fileBlks,
			   options->iHandle.sizeMatch ? "set" : "not set",
			   options->iHandle.sizeMatch ? options->iHandle.sizeMatch->rt11.blocks : 0,
			   options->totEmpty,
			   options->totPerm);
	}
	return 0;
}






