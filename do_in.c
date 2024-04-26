/*  $Id: do_in.c,v 1.1 2022/07/29 02:47:07 dave Exp dave $

	do_in.c - Copy a file into container.
	
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
 * @file do_in.c
 * Copy a file into container. Called from rtpip.
 */

/**
 * Copy a file into RT11 container.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
int do_in(Options_t *options)
{
	Rt11DirEnt_t *dirptr;
	int ii, retv, outLBA;
	InWorkingDir_t *wdp;
	InHandle_t *ihp;

	for ( ii = 0; ii < options->numArgFiles; ++ii )
	{

		/* Convert name to RAD50 */
		if ( cvtName(options, options->argFiles[ii]) )
			continue;
		if ( !(options->inOpts & INOPTS_NOASK) )
		{
			char prompt[128];
			int yn;

			snprintf(prompt, sizeof(prompt) - 1, "Copy in '%s'?", options->iHandle.argFN);
			yn = getYN(prompt, YN_YES);
			if ( yn == YN_QUIT )
				break;
			if ( yn != YN_YES )
				continue;
		}
		if ( readInpFile(options, options->argFiles[ii]) )
			continue;
		if ( preDelete(options) )
			continue;
		wdp = options->iHandle.sizeMatch;
		if ( !wdp || wdp->rt11.blocks < options->iHandle.fileBlks )
		{
			fprintf(stderr, "Not enough contigiuos space left on disk for '%s'. Need %d blocks. Total free space: %d\n",
					options->argFiles[ii], options->iHandle.fileBlks,
					options->totEmpty);
			if ( options->totEmpty > options->iHandle.fileBlks )
			{
				fprintf(stderr, "Try doing an rtpip sqz command to consolidate all the free space\n");
			}
			continue;
		}
		dirptr = &wdp->rt11;
		outLBA = wdp->lba;
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("do_in: '%s', cvt: %s, outLBA:%d, fileBlks: %d\n",
				   options->argFiles[ii],
				   options->iHandle.argFN,
				   outLBA,
				   options->iHandle.fileBlks);
		}
		if ( dirptr->blocks != options->iHandle.fileBlks )
		{
			int moveAmt;
			retv = options->maxseg * options->numdent;
			/* We need to split the empty space (be sure to leave room for one last entry) */
			if ( retv - 1 <= options->numWdirs )
			{
				fprintf(stderr, "Ran out of directory entries. Currently has room for %d and used %d\n",
						retv - 1, options->numWdirs);
				break;
			}
			/* Compute starting index */
			retv = wdp - options->wDirArray;
			/* Compute number of entries to end of list */
			moveAmt = options->numWdirs - retv;
			dirptr->blocks -= options->iHandle.fileBlks;
			wdp->lba += options->iHandle.fileBlks;
			if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
			{
				printf("do_in: Inserted empty entry at index %d. New LBA: %d, new size: %d\n",
					   retv + 1, wdp->lba, dirptr->blocks);
			}
			memmove(wdp + 1, wdp, moveAmt * sizeof(InWorkingDir_t));
			++options->numWdirs;
		}
		else
		{
			if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
			{
				printf("do_in: Found an exact replacement entry at index %d\n",
					   (int)(wdp - options->wDirArray));
			}
		}
		dirptr->name[0] = options->iHandle.iNameR50[0];        /* Need to copy file here */
		dirptr->name[1] = options->iHandle.iNameR50[1];
		dirptr->name[2] = options->iHandle.iNameR50[2];
		dirptr->blocks = options->iHandle.fileBlks;
		if ( options->inDate )
			dirptr->date = options->inDate;
		else if ( (options->fileOpts & FILEOPTS_TIMESTAMP) )
		{
			int yr, mo, day, age;
			struct tm *tm;

			tm = localtime(&options->iHandle.fileTimeStamp);
			yr = tm->tm_year + 1900;
			mo = tm->tm_mon + 1;
			day = tm->tm_mday;
			age = 0;
			if ( yr >= 1972 && yr < 2004  )
			{
				yr -= 1972;
				age = 0;
			}
			else if ( yr >= 2004 && yr < 2036 )
			{
				yr -= 2004;
				age = 1;
			}
			else if ( yr >= 2036 && yr < 2068 )
			{
				yr -= 2036;
				age = 2;
			}
			else
			{
				yr -= 2068;
				age = 3;
			}
			dirptr->date = (age << 14) | ((mo & 15) << 10) | ((day & 31) << 5) | (yr & 31);
		}
		else
		{
			dirptr->date = ((1) << 10) | (1 << 5) | ((0) & 31);
		}
		ihp = &options->iHandle;
		dirptr->control = PERM;
		wdp->lba = outLBA;
		options->totEmpty -= ihp->fileBlks;
		options->totPerm += ihp->fileBlks;
		ihp->totUsed += ihp->fileBlks;
		++ihp->totIns;
		if ( !(options->cmdOpts & CMDOPT_NOWRITE) )
		{
			if ( (options->cmdOpts & (CMDOPT_DOUBLE_FLPY | CMDOPT_SINGLE_FLPY)) )
			{
				U8 *dst = options->floppyImageUnscrambled + wdp->lba * BLKSIZ;
				memcpy(dst, ihp->inFileBuf, ihp->fileBlks * BLKSIZ);
				if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) || options->verbose || (options->inOpts & INOPTS_VERB) )
				{
					printf("Copied '%s' to '%s', %d blocks\n",
						   options->argFiles[ii], options->iHandle.argFN, options->iHandle.fileBlks);
				}
			}
			else
			{
				retv = writeFileToContainer(options,wdp);
				if ( retv == 1 )
					return 1;
			}
		}
		else
		{
			printf("Would have copied '%s' to '%s', %d blocks at LBA %d\n",
				   options->argFiles[ii], options->iHandle.argFN, options->iHandle.fileBlks, wdp->lba);
		}
	}
	linearToDisk(options);
	if ( (options->cmdOpts & CMDOPT_NOWRITE) || (options->cmdOpts & CMDOPT_DBG_NORMAL) || options->verbose || (options->inOpts & INOPTS_VERB) )
	{
		fseek(options->inp,0,SEEK_END);
		printf("%sAdded a total of %d file%s, %d blocks. %d free blocks now. Container EOF block is %ld.\n",
			   (options->cmdOpts & CMDOPT_NOWRITE) ? "Would have " : "",
			   options->iHandle.totIns,
			   options->iHandle.totIns == 1 ? "" : "s",
			   options->iHandle.totUsed,
			   options->totEmpty,
			   ftell(options->inp)/BLKSIZ);
	}
	return 0;
}




