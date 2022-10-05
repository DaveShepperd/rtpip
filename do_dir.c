/*  $Id: do_dir.c,v 1.1 2022/07/29 00:58:55 dave Exp dave $ $

	do_dir.c - display directory found in RT11 container file
	
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
 * @file do_dir.c
 * Display directory found in RT11 container file. Called by
 * rtpip.
 */

typedef struct
{
	int totFiles;
	int totEmpties;
	int totUsed;
	int totFree;
} FileDetails_t;

/**
 * Print contents of directory entry.
 * @param options - pointer to options.
 * @param wdp - pointer to directory entry.
 * @return nothing
 */
static int showDirEnt(Options_t *options, InWorkingDir_t *wdp, FileDetails_t *counts)
{
	int needLF = 0;
	Rt11DirEnt_t *dirptr;
	char dStr[INSTR_LEN];

	dirptr = &wdp->rt11;
	if ( !(options->cmdOpts & CMDOPT_DBG_NORMAL) && (dirptr->control & (ENDBLK | PERM)) == ENDBLK )
	{
		return 0;
	}
	if ( (dirptr->control & PERM) )
	{
		if ( !filterFilename(options, wdp->ffull) )
			return 0;
		counts->totUsed += dirptr->blocks;
		++counts->totFiles;
		printf("%-10.10s %5d %s",
			   wdp->ffull,
			   dirptr->blocks,
			   dateStr(dStr, dirptr->date)
			  );
		needLF = 1;
	}
	else
	{
		++counts->totEmpties;
		counts->totFree += dirptr->blocks;
		if ( (options->lsOpts & (LSOPTS_FULL | LSOPTS_ALL)) )
		{
			printf(" <EMPTY>   %5d            ", dirptr->blocks);
			needLF = 1;
		}
	}
	if ( (options->lsOpts & LSOPTS_ALL) )
	{
		printf(" %6d %d:%d %06o",
			   wdp->lba, dirptr->channel, dirptr->procid, dirptr->control);
		if ( (dirptr->control & PROTEK) )
		{
			printf(" RO    ");
		}
		if ( (dirptr->control & ENDBLK) )
		{
			printf(" ENDBLK");
		}
		if ( (dirptr->control & PERM) )
		{
			printf(" PERM  ");
		}
		if ( (dirptr->control & EMPTY) )
		{
			printf(" MT    ");
		}
		if ( (dirptr->control & TENT) )
		{
			printf(" TNT   ");
		}
		printf("%3d:%3d", wdp->segNo, wdp->segIdx);
		needLF = 1;
	}
	if ( needLF )
	{
		printf("\n");
	}
	return needLF;
}

/**
 * Display RT11 directory.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
int do_directory(Options_t *options)
{
	Rt11DirEnt_t *dirptr;
	FileDetails_t counts;
	int ii;
	InWorkingDir_t *wdp;
	InWorkingDir_t * *permFiles,**pwdp,**laPtr;

	memset(&counts,0,sizeof(counts));
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("do_directory: sortby=0x%X, columns=%d, numWdirs=%d\n",
			   options->sortby, options->columns, options->numWdirs);
	}
	if ( options->numWdirs > 2 && options->sortby )
	{
		ii = (options->sortby & SORTBY_REV) ? 4 : 0;
		if ( (options->sortby & SORTBY_TYPE) )
		{
			ii |= 1;
		}
		else if ( (options->sortby & SORTBY_DATE) )
		{
			ii |= 2;
		}
		else if ( (options->sortby & SORTBY_SIZE) )
		{
			ii |= 3;
		}
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("do_directory: using cmpFunc[%d]\n", ii);
		}
		qsort(options->linArray, options->numWdirs, sizeof(InWorkingDir_t *), cmpFuncs[ii]);
	}
	if ( options->columns > 0 )
	{
		int idx, row, col, numRows;

		laPtr = options->linArray;
		permFiles = (InWorkingDir_t **)calloc(options->numWdirs, sizeof(InWorkingDir_t *));
		if ( !permFiles )
		{
			fprintf(stderr, "Ran out of memory calloc()ing %ld bytes for permlist\n",
					options->numWdirs * sizeof(InWorkingDir_t *));
			return 1;
		}
		pwdp = permFiles;
		for ( row = 0; row < options->numWdirs; ++row )
		{
			wdp = *laPtr++;
			dirptr = &wdp->rt11;
			if ( !(dirptr->control & PERM) )
			{
				counts.totFree += dirptr->blocks;
				/* ++counts.totEmpties; */
			}
			else
			{
				if ( !filterFilename(options, wdp->ffull) )
					continue;
				counts.totUsed += dirptr->blocks;
				++counts.totFiles;
				*pwdp++ = wdp;
			}
		}
		numRows = (counts.totFiles + (options->columns - 1)) / options->columns;
		for ( row = 0; row < numRows; ++row )
		{
			for ( col = 0; col < options->columns; ++col )
			{
				idx = (col * numRows) + row;
				if ( idx >= counts.totFiles )
					break;
				wdp = permFiles[idx];
				printf("%-10.10s    ", wdp->ffull);
			}
			printf("\n");
		}
		free(permFiles);
	}
	else
	{
		
		laPtr = options->linArray;
		if ( (options->lsOpts & LSOPTS_ALL) )
			printf("Name        Size    Date        LBA p:c  Flags Type  Seg:Idx\n");
		for ( ii = 0; ii < options->numWdirs; ++ii )
		{
			wdp = *laPtr++;
			showDirEnt(options, wdp, &counts);
		}
	}
	printf("Total files: %d, Blocks used: %d, Blocks free: %d\n",
		   counts.totFiles, counts.totUsed, counts.totFree);
	return 0;
}


