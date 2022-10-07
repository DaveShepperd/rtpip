/*  $Id: output.c,v 1.1 2022/07/29 02:46:21 dave Exp dave $

	output.c - Write contents of container file.
	
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
 * @file output.c
 * Write contents of container file.
 */

typedef struct
{
	Options_t *options;
	char *tmpContName;
	char *buContName;
} TmpBuf_t;

static int mkTmpName(TmpBuf_t *bufP)
{
	int ii;

	/* Prepare two new filenames based on container's name */
	ii = strlen(bufP->options->container) + 4;
	bufP->tmpContName = (char *)malloc(2 * ii + 2);
	if ( !bufP->tmpContName )
	{
		fprintf(stderr, "Ran out of memory getting %d bytes for tmp filenames: %s\n",
				2 * ii + 2, strerror(errno));
		return 1;
	}
	/* We need a name for a tmp file */
	bufP->buContName = bufP->tmpContName + ii + 1;
	strcpy(bufP->tmpContName, bufP->options->container);
	strcat(bufP->tmpContName, "-tmp");
	/* And we need a name to rename the current container to xxx.bak */
	strcpy(bufP->buContName, bufP->options->container);
	strcat(bufP->buContName, ".bak");
	/* Pre-delete any existing files */
	unlink(bufP->tmpContName);
	return 0;
}

/**
 * Write a file into container
 * @param options - pointer to options
 * @param wdp - pointer to directory entry
 * @return 0 on success, 1 on error
 */
int writeFileToContainer(Options_t *options, InWorkingDir_t *wdp)
{
	char *wBuf;
	int retv;
	Rt11DirEnt_t *dirptr = &wdp->rt11;
	InHandle_t *ihp = &options->iHandle;
	
	if ( !options->openedWrite )
	{
		fclose(options->inp);
		options->inp = fopen(options->container, "rb+");
		if ( !options->inp )
		{
			fprintf(stderr, "Error reopening '%s' for r/w: %s\n",
					options->container, strerror(errno));
			return 1;
		}
		options->openedWrite = 1;
		if ( (options->cmdOpts&CMDOPT_DBG_NORMAL) )
			printf("writeFileToContainer(): Reopened '%s' for r/w\n", options->container);
	}
	if ( (options->cmdOpts&CMDOPT_DBG_NORMAL) )
	{
		retv = fseek(options->inp, 0, SEEK_END);
		printf("writeFileToContainer(): Seeking to block %d to write %d blocks for file '%s' (current EOF block %ld)\n",
			   wdp->lba,
			   wdp->rt11.blocks,
			   ihp->argFN,
			   ftell(options->inp)/BLKSIZ);
	}
	retv = fseek(options->inp, wdp->lba * BLKSIZ, SEEK_SET);
	if ( retv < 0 || ferror(options->inp) || (ftell(options->inp) != wdp->lba * BLKSIZ) )
	{
		fprintf(stderr, "Error seeking to %d to write file '%s': %s\n",
				wdp->lba, options->iHandle.argFN, strerror(errno));
		return 0;
	}
	wBuf = ihp->inFileBuf;
	retv = fwrite(wBuf, BLKSIZ, dirptr->blocks, options->inp);
	if ( retv != dirptr->blocks )
	{
		fprintf(stderr, "Error writing %d blocks %d-%d for '%s': %s\n",
				dirptr->blocks, wdp->lba, wdp->lba + dirptr->blocks - 1,
				ihp->argFN, strerror(errno));
		return 0;
	}
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) || options->verbose || (options->inOpts & INOPTS_VERB) )
	{
		printf("writeFileToContainer(): Copied '%-10.10s': %6d bytes starting at LBA %6d\n",
			   ihp->argFN, dirptr->blocks * BLKSIZ, wdp->lba);
	}
	return 0;
}

/**
 * Create a new container file squeezing out all the empty space.
 * @param options - pointer to options
 * @return 0 if success; 1 if failure
 */
int createNewContainer(Options_t *options)
{
	int ans, retv, isFloppy;
	TmpBuf_t tmpBufS;
/*    char *tmpContName, *buContName; */
	U8 * iBuf,*oBuf = NULL,*oBufRunning = NULL;
	int iBufSize = 0, movedFiles = 0;
	FILE *tmp;
	int ii, dirNum, oSegNum, zLBA, dstLBA, iDstDent, wCnt; /* srcLBA, */
	int maxSeg, maxEntPSeg;
	const Rt11SegEnt_t *firstSrcSeg;
	Rt11SegEnt_t * firstDstSeg,*dstseg;
	InWorkingDir_t *wdp;
	Rt11DirEnt_t *dstdir;

#if 0
	if ((options->cmdOpts&CMDOPT_NOWRITE))
	{
		printf("Writes are disabled. The 'new' and 'sqz' commands are disabled\n");
		return 1;
	}
#endif
	if ( !options->emptyAdds )
	{
		if ( !options->totEmpty || !options->totEmptyEntries )
		{
			printf("ERROR: There is no empty space available\n");
			return 1;
		}
		if ( options->totEmptyEntries < 2 )
		{
			printf("Container is already squeezed\n");
			return 0;
		}
	}
	else
		++options->totEmptyEntries;
	options->totEmpty += options->emptyAdds;
	maxSeg = MAXSEGMENTS - 1;     /* Assume the maximum segments */
	if ( (options->cmdOpts & CMDOPT_SINGLE_FLPY) )
		maxSeg = MAX_SGL_FLPY_SEGS;
	if ( (options->cmdOpts & CMDOPT_DOUBLE_FLPY) )
		maxSeg = MAX_DBL_FLPY_SEGS;
	/* If user provided a segment count, use that */
	if ( options->totPermEntries >= options->numdent * maxSeg )
	{
		fprintf(stderr, "ERROR: Too many files (%d) to fit in %d segments at %d files each\n", options->totPermEntries, maxSeg, options->numdent);
		return 1;
	}
	firstSrcSeg = (Rt11SegEnt_t *)options->directory;
	if ( !options->newMaxSeg )
	{
		/* Compute how many segments it will take if each segment is half filled */
		maxEntPSeg = options->numdent / 2;
		maxSeg = options->totPermEntries / maxEntPSeg;
		if ( options->totPermEntries % maxEntPSeg )
			++maxSeg;
		if ( maxSeg > MAXSEGMENTS - 1 )
		{
			/* Too many segments if each is only 1/2 filled. */
			maxSeg = MAXSEGMENTS - 1;
		}
		if ( !maxSeg )
			maxSeg = 1;		/* At least one segment */
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) || options->verbose )
			printf("createNewContainer(): Computed a required maxSeg of %d\n", maxSeg);
	}
	else
	{
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) || options->verbose )
			printf("createNewContainer(): changing maxSeg from default %d to user provided %d\n", maxSeg, options->newMaxSeg);
		maxSeg = options->newMaxSeg;
	}
	if ( maxSeg < firstSrcSeg->smax )
	{
		printf("createNewContainer(): computed or provided maxSeg of %d which is less than %d. Using %d\n", maxSeg, firstSrcSeg->smax, firstSrcSeg->smax);
		maxSeg = firstSrcSeg->smax;
	}
	/* Evenly distribute all the files among all available segments */
	maxEntPSeg = options->totPermEntries / maxSeg;
	if ( options->totPermEntries / maxSeg >= 1 )
	{
		if ( (maxEntPSeg % (options->totPermEntries / maxSeg)) )
			++maxEntPSeg;
	}
	if ( maxEntPSeg >= options->numdent )
	{
		fprintf(stderr, "ERROR: Too many files (%d) to fit in %d segments at %d files each. (maxEntPSeg=%d)\n",
				options->totPermEntries, maxSeg, options->numdent, maxEntPSeg);
		return 1;
	}
	/* If what we end up with is less than existing, use existing */
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) || options->verbose )
	{
		printf("Creating new directory with %d segments each with %d entries of a potential %d used. Total files=%d\n",
			   maxSeg, maxEntPSeg, options->numdent, options->totPermEntries);
	}
	/* get tmp filenames */
	tmpBufS.options = options;
	if ( mkTmpName(&tmpBufS) )
		return 1;
	/* Create the tmp file and open for writes */
	tmp = fopen(tmpBufS.tmpContName, "wb");
	if ( !tmp )
	{
		fprintf(stderr, "Error creating temp file '%s' for write: %s\n",
				tmpBufS.tmpContName, strerror(errno));
		free(tmpBufS.tmpContName);
		return 1;
	}
	isFloppy = (options->cmdOpts & (CMDOPT_DOUBLE_FLPY | CMDOPT_SINGLE_FLPY)) ? 1 : 0;
	/* Allocate a buffer to hold the largest of the files */
	iBufSize = options->largestPerm * BLKSIZ;
	if ( iBufSize < options->seg1LBA * BLKSIZ )
		iBufSize = options->seg1LBA * BLKSIZ;     /* Minimum size of of boot sectors+home block */
	iBuf = (unsigned char *)malloc(iBufSize);
	if ( !iBuf )
	{
		fprintf(stderr, "Ran out of memory getting a %d byte buffer: %s\n",
				iBufSize, strerror(errno));
		free(tmpBufS.tmpContName);
		return 1;
	}
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("\ncreateNewContainer(): Allocated %d bytes for file copy: %p-%p\n", iBufSize, iBuf, (U8 *)iBuf + iBufSize);
	}
	if ( isFloppy )
	{
		oBuf = (unsigned char *)calloc(options->floppyImageSize, 1);
		if ( !oBuf )
		{
			fprintf(stderr, "Ran out of memory getting a %d byte floppy output buffer: %s\n",
					options->floppyImageSize, strerror(errno));
			free(iBuf);
			free(tmpBufS.tmpContName);
			return 1;
		}
		/* Copy home block */
		memcpy(oBuf + BLKSIZ, &options->homeBlk, BLKSIZ);
		/* Point to the fist segment */
		firstDstSeg = (Rt11SegEnt_t *)(oBuf + options->seg1LBA * BLKSIZ);
		/* Point to the first place to use after the segments */
		oBufRunning = (U8 *)firstDstSeg + maxSeg * SEGSIZ;
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("\ncreateNewContainer(): alloc'd %d bytes for new floppy disk image: %p-%p\n",
				   options->floppyImageSize,
				   (U8 *)oBuf,
				   (U8 *)oBuf + options->floppyImageSize - 1);
			printf("createNewContainer(): offset of firstDstSeg=%ld (block %ld), offset of first file data=%ld (block %ld)\n",
				   (U8 *)firstDstSeg - oBuf, ((U8 *)firstDstSeg - oBuf) / BLKSIZ,
				   oBufRunning - oBuf, (oBufRunning - oBuf) / BLKSIZ);
		}
	}
	else
	{
		/* Seek the input container back to 0 */
		fseek(options->inp, 0, SEEK_SET);
		/* Read the boot sectors and home block into our tmp buffer */
		ans = fread(iBuf, 1, options->seg1LBA * BLKSIZ, options->inp);
		if ( ans != options->seg1LBA * BLKSIZ )
		{
			fprintf(stderr, "Error reading %ld boot and home blocks from '%s':%s\n",
					options->seg1LBA, options->container, strerror(errno));
			free(iBuf);
			fclose(tmp);
			unlink(tmpBufS.tmpContName);
			free(tmpBufS.tmpContName);
			return 1;
		}
		/* And write the boot + home blocks to the tmp file */
		ii = fwrite(iBuf, 1, ans, tmp);
		if ( ii != ans )
		{
			fprintf(stderr, "Error writing %ld boot blocks to '%s':%s\n",
					options->seg1LBA, tmpBufS.tmpContName, strerror(errno));
			free(iBuf);
			fclose(tmp);
			unlink(tmpBufS.tmpContName);
			free(tmpBufS.tmpContName);
			return 1;
		}
		/* Get a buffer to use as the new directory segments */
		ii = maxSeg * SEGSIZ;
		firstDstSeg = (Rt11SegEnt_t *)calloc(ii, 1);
		if ( !firstDstSeg )
		{
			fprintf(stderr, "Ran out of memory calloc'ing %d bytes for output directory:%s\n",
					ii, strerror(errno));
			free(iBuf);
			fclose(tmp);
			unlink(tmpBufS.tmpContName);
			free(tmpBufS.tmpContName);
			return 1;
		}
		/* Write the, so far, blank directory segments to the tmp file (just temporarily instead of seeking past them) */
		ans = fwrite(firstDstSeg, 1, ii, tmp);
		if ( ii != ans )
		{
			fprintf(stderr, "Error writing %ld boot and home blocks to '%s':%s\n",
					options->seg1LBA, tmpBufS.tmpContName, strerror(errno));
			fclose(tmp);
			unlink(tmpBufS.tmpContName);
			free(firstDstSeg);
			free(tmpBufS.tmpContName);
			free(iBuf);
			return 1;
		}
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("\ncreateNewContainer(): alloc'd %d bytes for new segments: %p-%p\n", ii, (U8 *)firstDstSeg, (U8 *)firstDstSeg + ii - 1);
		}
	}
	/* Prepare to write a new directory tree */
	iDstDent = maxEntPSeg;
	dstseg = NULL;
	oSegNum = 0;
	dstdir = NULL;
	dstLBA = options->seg1LBA + maxSeg * (SEGSIZ / BLKSIZ);
	/* Point to the first source directory segment */
	wdp = options->wDirArray;
	for ( dirNum = 0; dirNum < options->numWdirs; ++dirNum, ++wdp )
	{
		if ( iDstDent >= maxEntPSeg )
		{
			if ( oSegNum > 0 )
			{
				dstdir->blocks = 0;
				dstdir->control = ENDBLK;
				dstseg->link = oSegNum + 1;
			}
			dstseg = (Rt11SegEnt_t *)((U8 *)firstDstSeg + oSegNum * SEGSIZ);
			++oSegNum;
			if ( oSegNum >= MAXSEGMENTS - 1 )
			{
				fprintf(stderr, "ERROR: Fatal internal error. oSegNum became %d after %d files moved\n", oSegNum, dirNum);
				fclose(tmp);
				unlink(tmpBufS.tmpContName);
				free(iBuf);
				if ( !isFloppy )
					free(firstDstSeg);
				free(tmpBufS.tmpContName);
				return 1;
			}
			dstseg->smax = maxSeg;
			dstseg->link = 0;
			dstseg->last = 0;
			dstseg->extra = firstSrcSeg->extra;
			dstseg->start = dstLBA;
			dstdir = (Rt11DirEnt_t *)(dstseg + 1);
			iDstDent = 0;
			if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
			{
				printf("\nOutput directory segment %d. Starting LBA: %d, dstseg=%p-%p, dstdir=%p-%p\n\n",
					   oSegNum, dstLBA, dstseg, (U8 *)dstseg + SEGSIZ - 1, dstdir, (U8 *)dstdir + maxEntPSeg * sizeof(Rt11DirEnt_t) - 1);
			}
		}
		if ( (wdp->rt11.control & PERM) )
		{
			*dstdir = wdp->rt11;
			dstdir = (Rt11DirEnt_t *)((unsigned char *)dstdir + sizeof(Rt11DirEnt_t) + firstSrcSeg->extra);
			++iDstDent;
#if 0
			if ( (options->cmdOpts&CMDOPT_DBG_NORMAL) )
			{
				printf("Moving '%s', %d blocks, inpLBA: %d, outLBA: %d, dstdir=%p-%p\n",
					   wdp->ffull, wdp->rt11.blocks, wdp->lba, dstLBA, dstdir, dstdir+sizeof(Rt11DirEnt_t) + firstSrcSeg->extra-1 );
			}
#endif
			if ( isFloppy )
			{
				int eof = wdp->lba + wdp->rt11.blocks;
				U8 *src;
				if ( eof > options->floppyImageSize / BLKSIZ )
				{
					fprintf(stderr, "ERROR: Fatal internal error. Read file '%s' with size of %d blocks at LBA %d is out of bounds. Disk size is %d blocks.\n",
							wdp->ffull, wdp->rt11.blocks, wdp->lba, options->floppyImageSize / BLKSIZ);
					fclose(tmp);
					unlink(tmpBufS.tmpContName);
					free(iBuf);
					free(tmpBufS.tmpContName);
					return 1;
				}
				if ( dstLBA > options->floppyImageSize )
				{
					fprintf(stderr, "ERROR: Fatal internal error. Write file '%s' with size of %d blocks at LBA %d is out of bounds. Disk size is %d blocks.\n",
							wdp->ffull, wdp->rt11.blocks, dstLBA, options->floppyImageSize / BLKSIZ);
					fclose(tmp);
					unlink(tmpBufS.tmpContName);
					free(iBuf);
					free(tmpBufS.tmpContName);
					return 1;
				}
				wCnt = wdp->rt11.blocks * BLKSIZ;
				src = options->floppyImageUnscrambled + wdp->lba * BLKSIZ;
				memcpy(oBufRunning, src, wCnt);
				oBufRunning += wCnt;
			}
			else
			{
				fseek(options->inp, wdp->lba * BLKSIZ, SEEK_SET);
				if ( ferror(tmp) )
				{
					fprintf(stderr, "Error seeking container file to %d: %s\n",
							wdp->lba * BLKSIZ, strerror(errno));
					free(iBuf);
					free(firstDstSeg);
					fclose(tmp);
					unlink(tmpBufS.tmpContName);
					free(tmpBufS.tmpContName);
					return 1;
				}
				wCnt = wdp->rt11.blocks * BLKSIZ;
				if ( wCnt > iBufSize )
				{
					U8 *newBP;
					fprintf(stderr, "Warning: Internal error. Need to copy %d byte file into %d byte buffer. Fixing it.\n",
							wCnt, iBufSize);
					newBP = (U8 *)realloc(iBuf, wCnt);
					if ( !newBP )
					{
						fprintf(stderr, "No memory to reallocate %d byte buffer.\n", wCnt);
						free(iBuf);
						free(firstDstSeg);
						fclose(tmp);
						unlink(tmpBufS.tmpContName);
						free(tmpBufS.tmpContName);
						return 1;
					}
					iBuf = newBP;
					iBufSize = wCnt;
				}
				retv = fread(iBuf, 1, wCnt, options->inp);
				if ( retv != wCnt )
				{
					fprintf(stderr, "Error reading %d bytes from container: %s\n",
							wCnt, strerror(errno));
					free(iBuf);
					free(firstDstSeg);
					fclose(tmp);
					unlink(tmpBufS.tmpContName);
					free(tmpBufS.tmpContName);
					return 1;
				}
				retv = fwrite(iBuf, 1, wCnt, tmp);
				if ( retv != wCnt )
				{
					fprintf(stderr, "Error writing %d bytes to tmp file: %s\n",
							wCnt, strerror(errno));
					free(iBuf);
					free(firstDstSeg);
					fclose(tmp);
					unlink(tmpBufS.tmpContName);
					free(tmpBufS.tmpContName);
					return 1;
				}
			}
			++movedFiles;
			if ( options->verbose || (options->sqzOpts & SQZOPTS_VERB) )
			{
				printf("Moved %-10.10s, srcLBA: %6d, dstLBA: %6d, blocks: %4d, dstdir=%p-%p\n",
					   wdp->ffull, wdp->lba, dstLBA, wdp->rt11.blocks, dstdir, (U8 *)dstdir + sizeof(Rt11DirEnt_t) + firstSrcSeg->extra - 1);
			}
			dstLBA += wdp->rt11.blocks;
		}
/*        srcLBA += wdp->rt11.blocks; */
	}
	dstdir->control = EMPTY;
	dstdir->blocks = options->diskSize - dstLBA;
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("Added <EMPTY> at entry %d. LBA: %d, blocks: %d\n",
			   iDstDent, dstLBA, dstdir->blocks);
	}
	zLBA = dstLBA;
	if ( !isFloppy )
	{
		memset(iBuf, 0, iBufSize);    /* get a bunch of zeros */
		for ( ii = 0; ii < dstdir->blocks; ii += iBufSize / BLKSIZ )
		{
			int sLBA;

			sLBA = zLBA;
			wCnt = iBufSize / BLKSIZ;
			if ( wCnt > dstdir->blocks - ii )
				wCnt = dstdir->blocks - ii;
			zLBA += wCnt;
			wCnt *= BLKSIZ;
			retv = fwrite(iBuf, 1, wCnt, tmp);
			if ( retv != wCnt )
			{
				fprintf(stderr, "Error writing %d bytes of zeros to tmp file starting at LBA %d: %s\n",
						wCnt, sLBA, strerror(errno));
				free(iBuf);
				free(firstDstSeg);
				fclose(tmp);
				unlink(tmpBufS.tmpContName);
				free(tmpBufS.tmpContName);
				return 1;
			}
		}
	}
	++iDstDent;
	dstLBA += dstdir->blocks;
	dstdir = (Rt11DirEnt_t *)((unsigned char *)dstdir + sizeof(Rt11DirEnt_t) + firstDstSeg->extra);
	dstdir->control = ENDBLK;
	dstdir->blocks = 0;
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("Added ENDBLK at entry %d. LBA: %d\n",
			   iDstDent, dstLBA);
	}
	dstseg->link = 0;
	firstDstSeg->last = oSegNum;
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("Last segment written: %d. Last LBA: %d\n", oSegNum, dstLBA);
	}
	if ( options->verbose || (options->sqzOpts & SQZOPTS_VERB) )
	{
		printf("Copied %d files.\n", movedFiles);
	}
	if ( isFloppy )
	{
		if ( rescramble(options, oBuf) )
		{
			fclose(tmp);
			unlink(tmpBufS.tmpContName);
			free(oBuf);
			free(iBuf);
			free(tmpBufS.tmpContName);
			return 1;
		}
		/* Write the entire new floppy image */
		retv = fwrite(options->floppyImage, 1, options->floppyImageSize, tmp);
		if ( retv != options->floppyImageSize )
		{
			fprintf(stderr, "Error writing %d bytes of floppy image to tmp: %s\n",
					options->floppyImageSize, strerror(errno));
			fclose(tmp);
			unlink(tmpBufS.tmpContName);
			free(firstDstSeg);
			free(iBuf);
			free(tmpBufS.tmpContName);
			return 1;
		}
	}
	else
	{
		/* Backup to segment area */
		fseek(tmp, options->seg1LBA * BLKSIZ, SEEK_SET);
		if ( ferror(tmp) )
		{
			fprintf(stderr, "Error seeking tmp file to %ld: %s\n",
					options->seg1LBA * BLKSIZ, strerror(errno));
			fclose(tmp);
			unlink(tmpBufS.tmpContName);
			free(firstDstSeg);
			free(iBuf);
			free(tmpBufS.tmpContName);
			return 1;
		}
		/* Write all the directory segments */
		retv = fwrite(firstDstSeg, 1, maxSeg * SEGSIZ, tmp);
		if ( retv != maxSeg * SEGSIZ )
		{
			fprintf(stderr, "Error writing %d bytes of directory at loc %ld to tmp: %s\n",
					maxSeg * SEGSIZ, options->seg1LBA * BLKSIZ, strerror(errno));
			fclose(tmp);
			unlink(tmpBufS.tmpContName);
			free(firstDstSeg);
			free(iBuf);
			free(tmpBufS.tmpContName);
			return 1;
		}
		/* we're done */
	}
	fclose(tmp);
	tmp = NULL;
	if ( options->inp )
	{
		fclose(options->inp);
		options->inp = NULL;
	}
	if ( !(options->cmdOpts & CMDOPT_NOWRITE) )
	{
		unlink(tmpBufS.buContName);
		rename(options->container, tmpBufS.buContName);
		rename(tmpBufS.tmpContName, options->container);
	}
	else
	{
		printf("Would have deleted '%s', renamed '%s' to '%s' and renamed '%s' to '%s'\n",
			   tmpBufS.buContName,
			   options->container,
			   tmpBufS.buContName,
			   tmpBufS.tmpContName, options->container);
	}
	if ( isFloppy )
		free(oBuf);
	else
		free(firstDstSeg);
	free(iBuf);
	free(tmpBufS.tmpContName);
	return 0;
}

/**
 * Write an updated directory to disk over the top of the existing container file.
 * @param options - pointer to options
 * @return 0 if success; 1 if failure
 */
int writeNewDir(Options_t *options)
{
	int ans, ret;
	if ( !(options->cmdOpts & CMDOPT_NOWRITE) )
	{
		if ( (options->cmdOpts & (CMDOPT_DOUBLE_FLPY | CMDOPT_SINGLE_FLPY)) )
		{
			TmpBuf_t tmpBufS;
			FILE *tmp;

			if ( rescramble(options, NULL) )
				return 1;
			tmpBufS.options = options;
			if ( mkTmpName(&tmpBufS) )
				return 1;
			tmp = fopen(tmpBufS.tmpContName, "wb");
			if ( !tmp )
			{
				fprintf(stderr, "ERROR: Failed to open '%s' for write: %s\n", tmpBufS.tmpContName, strerror(errno));
				free(tmpBufS.tmpContName);
				return 1;
			}
			/* Write the entire new floppy image */
			ret = fwrite(options->floppyImage, 1, options->floppyImageSize, tmp);
			if ( ret != options->floppyImageSize )
			{
				fprintf(stderr, "Error writing %d bytes of floppy image to '%s': %s\n",
						options->floppyImageSize, tmpBufS.tmpContName, strerror(errno));
				fclose(tmp);
				unlink(tmpBufS.tmpContName);
				free(tmpBufS.tmpContName);
				return 1;
			}
			fclose(tmp);
			unlink(tmpBufS.buContName);
			rename(options->container, tmpBufS.buContName);
			rename(tmpBufS.tmpContName, options->container);
			free(tmpBufS.tmpContName);
		}
		else
		{
			if ( !options->openedWrite )
			{
				if ( (options->cmdOpts&CMDOPT_DBG_NORMAL) )
				{
					fseek(options->inp,0,SEEK_END);
					printf("writeNewDir(): Before reopen as r+: EOF block is %ld\n", ftell(options->inp)/BLKSIZ);
				}
				fclose(options->inp);
				options->inp = fopen(options->container, "r+");
				if ( !options->inp )
				{
					fprintf(stderr, "Error reopening '%s' for r/w: %s\n",
							options->container, strerror(errno));
					return 1;
				}
				options->openedWrite = 1;
				if ( (options->cmdOpts&CMDOPT_DBG_NORMAL) )
				{
					fseek(options->inp,0,SEEK_END);
					printf("writeNewDir(): After reopen as r+: EOF block is %ld\n", ftell(options->inp)/BLKSIZ);
				}
			}
			if ( (options->cmdOpts&CMDOPT_DBG_NORMAL) )
			{
				ret = fseek(options->inp,0,SEEK_END);
				printf("writeNewDir(): Seeking to block %2ld to write %d directory segments. Current EOF is block %ld\n",
					   options->seg1LBA,
					   options->maxseg,
					   ftell(options->inp)/BLKSIZ);
			}
			ret = fseek(options->inp, options->seg1LBA * BLKSIZ, SEEK_SET);
			if ( ret < 0 || ferror(options->inp) || (ftell(options->inp) != options->seg1LBA * BLKSIZ) )
			{
				fprintf(stderr, "Error seeking to %ld: %s\n",
						options->seg1LBA, strerror(errno));
				return 1;
			}
			ans = options->maxseg * SEGSIZ;
			ret = fwrite(options->directory, 1, ans, options->inp);
			if ( ret != ans )
			{
				fprintf(stderr, "Error writing directory. Expected to write %d bytes. Wrote %d. %s\n",
						ans, ret, strerror(errno));
				return 1;
			}
			if ( (options->cmdOpts&CMDOPT_DBG_NORMAL) )
			{
				fseek(options->inp,0,SEEK_END);
				printf("writeNewDir(): Wrote %d bytes starting at LBA %ld to %s. (Current EOF is now block %ld)\n",
					   ans, options->seg1LBA, options->container, ftell(options->inp));
			}
		}
	}
	else
	{
		if ( (options->cmdOpts & (CMDOPT_DOUBLE_FLPY | CMDOPT_SINGLE_FLPY)) )
		{
			printf("Would have replaced floppy disk image of %4d (512 byte) blocks\n",
				   options->floppyImageSize / BLKSIZ);
		}
		else
		{
			printf("Would have written %4d (512 byte) blocks starting at LBA %3ld to '%s'\n",
				   options->maxseg * SEGSIZ / BLKSIZ,
				   options->seg1LBA,
				   options->container);
		}
	}
	fclose(options->inp);
	options->inp = NULL;
	options->openedWrite = 0;
	return 0;
}

/**
 * Compress container squeezing all empty space into one place.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
int do_sqz(Options_t *options)
{
	return createNewContainer(options);
}

#if 0
/**
 * Fake boot sectors. The following gets copied to the first 5 blocks of
 * newly created container file when using the 'new' command.
 */
static unsigned short idx_0000[]=
{
	0000240, 0000005, 0000404, 0000000, 0000000, 0054020, 0103420, 0000400,
	0004067, 0000044, 0000015, 0000000, 0005000, 0041077, 0047517, 0026524,
	0026525, 0067516, 0061040, 0067557, 0020164, 0067157, 0073040, 0066157,
	0066565, 0006545, 0005012, 0000200, 0105737, 0177564, 0100375, 0112037,
	0177566, 0100372, 0000777, 0000000, 0000000, 0000000, 0000000, 0000000,
	0000000, 0000000, 0000000, 0000000, 0000000, 0000000, 0000000, 0000000
};
/**/
static unsigned short idx_1000[]=
{
	0000000, 0170000, 0007777, 0000000, 0000000, 0000000, 0000000, 0000000,
	0000000, 0000000, 0000000, 0000000, 0000000, 0000000, 0000000, 0000000
};
/**/
static unsigned short idx_1700[]=
{
	0177777, 0000000, 0000000, 0000000, 0000000, 0000000, 0000000, 0000000,
	0000000, 0000001, 0000006, 0107123, 0052122, 0030461, 0020101, 0020040,
	0020040, 0020040, 0020040, 0020040, 0020040, 0020040, 0020040, 0020040,
	0042504, 0051103, 0030524, 0040461, 0020040, 0020040, 0000000, 0000000
};
/**/
#endif

/**
 * Create an empty container. 
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
int do_new(Options_t *options)
{
#if 1
	fprintf(stderr, "The 'new' command is not yet supported\n");
	return 1;
#else
	if ( !options.diskSize )
	{
		printf("Need to supply a disk size (-b option)\n");
		return help_new( );
	}
	if ( !options.newMaxSeg )
	{
		Rt11SegEnt_t *segptr;
		Rt11DirEnt_t *dirptr;
		options.newMaxSeg = 4 + options.diskSize/1000;
		if ( options.newMaxSeg > 31 )
		options.newMaxSeg = 31;
		options.directory = (unsigned char *)calloc(6*BLKSIZ+SEGSIZ, 1);
		if ( !options.directory )
		{
			fprintf(stderr,"Unable to allocate %ld bytes for buffer: %s\n",
					options.startLBA*BLKSIZ+SEGSIZ, strerror(errno));
			return 1;
		}
		options.inp = fopen( options.container, "wb");
		if ( !options.inp )
		{
			fprintf(stderr, "Error creating new container file '%s': %s\n",
					options.container, strerror(errno));
			return 1;
		}
		memcpy(options.directory+00000, idx_0000, sizeof(idx_0000));
		memcpy(options.directory+01000, idx_1000, sizeof(idx_1000));
		memcpy(options.directory+01700, idx_1700, sizeof(idx_1700));
		segptr = (Rt11SegEnt_t *)(options.directory+6*BLKSIZ);
		segptr->smax = options.newMaxSeg;
		segptr->link = 0;
		segptr->last = 1;
		segptr->extra = 0;
		segptr->start = 6+options.newMaxSeg*2;
		dirptr = (Rt11DirEnt_t *)(segptr+1);
		dirptr->control = EMPTY;
		dirptr->blocks = options.diskSize - 6 - options.newMaxSeg*2;
		++dirptr;
		dirptr->control = ENDBLK;
		ans = fwrite(options.directory,(6+2)*BLKSIZ, 1, options.inp);
		fclose(options.inp);
		options.inp = NULL;
		free(options.directory);
		options.directory = NULL;
	}
#endif
}




