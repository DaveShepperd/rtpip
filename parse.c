/*  $Id: parse.c,v 1.1 2022/07/29 00:58:55 dave Exp dave $ $

	parse.c - Header, segment and directory functions used by rtpip
	
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
 * @file parse.c
 * Header, segment and directory functions used by rtpip.
 */

/* From Wikipedia */
#if 0
track = (blkno / NSECT);
i = ((blkno % NSECT) << 1);
if (i >= NSECT)
i++;
sector = (((i + (6 * track)) % NSECT) + 1);
track++
#endif

/** checkHeader - read and verify RT11 disk image header.
 *  @param options - pointer to options
 *  @return 0 on success, non-zero on failure. Error messages
 *          will have been displayed as appropriate.
 **/
int checkHeader(Options_t *options)
{
	struct stat st;
	Rt11SegEnt_t *firstseg;
	Rt11HomeBlock_t *home;
	int sts, bufLen;

	sts = stat(options->container, &st);
	if ( sts )
	{
		fprintf(stderr, "ERROR: Failed to stat '%s': %s\n", options->container, strerror(errno));
		return 1;
	}
	options->containerSize = st.st_size;        /* Record size of entire container file */
	options->containerBlocks = options->containerSize/BLKSIZ;
	options->inp = fopen(options->container, "rb");
	if ( !options->inp )
	{
		fprintf(stderr, "Unable to open input file '%s': %s\n",
				options->container, strerror(errno));
		return 1;
	}
	if ( (options->cmdOpts & (CMDOPT_SINGLE_FLPY | CMDOPT_DOUBLE_FLPY)) )
	{
		/* We are to read a floppy diskette container file. */
		size_t lim;
		/* Compute the actual size of what a floppy diskette container file should be. */
		options->floppyImageSize = NUM_SECTORS * NUM_TRACKS * ((options->cmdOpts & CMDOPT_SINGLE_FLPY) ? 128 : 256);    /* Image size in bytes */
		/* get two buffers of that size */
		options->floppyImage = (U8 *)calloc(2, options->floppyImageSize);
		if ( !options->floppyImage )
		{
			fprintf(stderr, "ERROR: No memory for %d byte floppy image\n", 2 * options->floppyImageSize);
			return 1;
		}
		options->floppyImageUnscrambled = options->floppyImage + options->floppyImageSize;
		/* Read the container file into the scrambled buffer */
		lim = options->floppyImageSize;
		if ( lim > st.st_size )
			lim = st.st_size;
		bufLen = fread(options->floppyImage, 1, lim, options->inp);
		if ( bufLen != (int)lim )
		{
			fprintf(stderr, "Error reading floppy image. Expected %ld bytes, got %d. %s\n",
					lim, bufLen, strerror(errno));
			return 1;
		}
		/* From now on, all I/O is to the contents of the buffer. So close the input just to make sure. */
		fclose(options->inp);
		options->inp = NULL;
		/* Unscramble the diskette image into logical blocks. */
		descramble(options);
		/* Copy the home block into its expected destination */
		memcpy(&options->homeBlk, options->floppyImageUnscrambled + BLKSIZ, BLKSIZ);
	}
	else
	{
		/* Seek the file to where the home block is */
		sts = fseek(options->inp, HOME_BLK_LBA * BLKSIZ, SEEK_SET);
		if ( sts < 0 || ferror(options->inp) || (ftell(options->inp) != HOME_BLK_LBA*BLKSIZ) )
		{
			fprintf(stderr, "Error seeking conainer to home block. Wanted %d: %s\n",
					HOME_BLK_LBA * BLKSIZ,
					strerror(errno));
			return 1;
		}
		/* Read the home block */
		bufLen = fread(&options->homeBlk, 1, BLKSIZ, options->inp);
		if ( bufLen != BLKSIZ )
		{
			fprintf(stderr, "Error reading home block 0. Expected %d bytes, got %d. %s\n",
					BLKSIZ, bufLen, strerror(errno));
			return 1;
		}
	}
	/* Verify the home block contents */
	home = &options->homeBlk;
	if ( options->verbose > 1 || (options->cmdOpts & CMDOPT_DBG_NORMAL) || home->firstSegment != DIRBLK || strncmp(home->sysID, "DECRT11A    ", 12) )
	{
		char ascVer[4], volID[sizeof(home->volumeID) + 1], owner[sizeof(home->owner) + 1], sysID[sizeof(home->sysID) + 1];

		fromRad50(ascVer, home->version);
		ascVer[3] = 0;
		strncpy(volID, home->volumeID, sizeof(volID) - 1);
		volID[sizeof(volID) - 1] = 0;
		strncpy(owner, home->owner, sizeof(owner) - 1);
		owner[sizeof(owner) - 1] = 0;
		strncpy(sysID, home->sysID, sizeof(sysID) - 1);
		sysID[sizeof(volID) - 1] = 0;
		printf("Home block:\n"
			   "clusterSize=%d\n"   /* 0722-0723 */
			   "firstSegment=%d\n"  /* 0724-0725 */
			   "version=%s\n"       /* 0726-0727 */
			   "volumeID=%s\n"      /* 0730-0743 */
			   "owner=%s\n"         /* 0744-0757 */
			   "sysID=%s\n"         /* 0760-0773*/
			   "checksum=%d\n"      /* 0776-0777 */
			   , home->clusterSize
			   , home->firstSegment
			   , ascVer
			   , volID
			   , owner
			   , sysID
			   , home->checksum
			  );
		if ( strncmp(home->sysID, "DECRT11A    ", 12) )
		{
			fprintf(stderr, "ERROR: Not a valid RT11 home block. Expected sysID to be 'DECRT11A    '\n");
			return 1;
		}
		if ( home->firstSegment != DIRBLK )
		{
			fprintf(stderr, "WARNING: Starting directory segment is not %d. It is %d instead.\n", DIRBLK, home->firstSegment);
		}
	}
	if ( !(options->cmdOpts & (CMDOPT_SINGLE_FLPY | CMDOPT_DOUBLE_FLPY)) )
	{
		/* Not a floppy diskette so get a buffer to hold the first directory segment */
		options->directory = (U8 *)malloc(SEGSIZ);
		if ( !options->directory )
		{
			fprintf(stderr, "ERROR: Not enough memory for directory. Wanted %d bytes\n", SEGSIZ);
			return 1;
		}
		/* Seek file to the first segment */
		sts = fseek(options->inp, home->firstSegment * BLKSIZ, SEEK_SET);
		if ( sts < 0 || ferror(options->inp) || (ftell(options->inp) != home->firstSegment*BLKSIZ) )
		{
			fprintf(stderr, "ERROR: Failed to seek container to %d: %s\n", home->firstSegment * BLKSIZ, strerror(errno));
			free(options->directory);
			options->directory = NULL;
			return 1;
		}
		/* Read the first directory segment. This is to get the report of total segments available. */
		sts = fread(options->directory, 1, SEGSIZ, options->inp);
		if ( sts != SEGSIZ )
		{
			fprintf(stderr, "ERROR: Failed to read %d bytes of directory. Got %d: %s\n", SEGSIZ, sts, strerror(errno));
			free(options->directory);
			options->directory = NULL;
			options->directorySize = 0;
			return 1;
		}
		/* Point to the directory segment */
		firstseg = (Rt11SegEnt_t *)options->directory;
		/* Compute how much memory we need to hold all the segments */
		bufLen = firstseg->smax * SEGSIZ;
		/* Make the buffer big enough to hold all of them */
		firstseg = (Rt11SegEnt_t *)realloc(firstseg, bufLen);
		if ( !firstseg )
		{
			fprintf(stderr, "ERROR: Not enough memory for directory segments. Wanted %d bytes\n", bufLen);
			free(options->directory);
			options->directory = NULL;
			return 1;
		}
		/* Make a note of where the segments are */
		options->directory = (U8 *)firstseg;
		/* And how big they are (in bytes) */
		options->directorySize = bufLen;
		/* read the rest of the segments into the buffer */
		sts = fread(options->directory + SEGSIZ, 1, bufLen - SEGSIZ, options->inp);
		if ( sts != bufLen - SEGSIZ )
		{
			fprintf(stderr, "ERROR: Failed to read %d bytes of directory. Got %d: %s\n", bufLen - SEGSIZ, sts, strerror(errno));
			free(options->directory);
			options->directory = NULL;
			options->directorySize = 0;
			return 1;
		}
	}
	else
	{
		/* Is a floppy diskette image so just point to the directory segments in the unscrambled buffer */
		options->directory = options->floppyImageUnscrambled + home->firstSegment * BLKSIZ;
		options->directorySize = bufLen;
	}
	firstseg = (Rt11SegEnt_t *)options->directory;
	/* Record the total number of segments available */
	options->maxseg = firstseg->smax;
	/* Record the size of each directory entry */
	options->dirEntrySize = (sizeof(Rt11DirEnt_t) + firstseg->extra);
	/* Record the total number of directory entries in each segment */
	options->numdent = (SEGSIZ - sizeof(Rt11SegEnt_t)) / options->dirEntrySize;
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("Max segments: %d. Max dirents: %d, Entry size: %d\n",
			   options->maxseg, options->numdent, options->dirEntrySize);
	}
	return 0;
}

/**
 * Parse the RT11 directory making it linear.
 * @param options - pointer to working area.
 * @return 0 if success, 1 if failure.
 * 
 * The disk image of the directory is pointed to by 
 * options->directory. This function walks through the disk 
 * image and forms a pair of linear arrays. One is a copy of 
 * this disk image with all the ENDBLKs removed and files listed
 * in LBA order without regard to which directory segment in 
 * which they were present. The other array is a list of 
 * pointers into the first array along with the file's actual 
 * LBA. It is this second array that can be sorted by the 
 * various sort options (sorts just by shuffling pointers). 
 */
int parse_directory(Options_t *options)
{
	Rt11SegEnt_t * firstseg,*segptr;
	Rt11DirEnt_t *dirptr;
	int segnum, relseg;
	int ii, accumLBA;
	InWorkingDir_t *wdp;
	InWorkingDir_t **lap;

	/* Compute the worst case of live directory entries */
	ii = options->maxseg * options->numdent;
	/* Create an array with that many entries */
	options->linArray = (InWorkingDir_t **)calloc(ii, sizeof(InWorkingDir_t *));
	/* Create an array of pointers of that many entries */
	options->wDirArray = (InWorkingDir_t *)calloc(ii, sizeof(InWorkingDir_t));
	if ( !options->linArray || !options->wDirArray )
	{
		fprintf(stderr, "Unable to allocate %ld bytes for working dirs\n",
				ii * sizeof(InWorkingDir_t *) + ii * sizeof(InWorkingDir_t));
		return 1;
	}
	lap = options->linArray;
	wdp = options->wDirArray;
	/* Point to first directory segment */
	firstseg = (Rt11SegEnt_t *)options->directory;
	/* Assume need to walk through all segments starting with segment 1*/
	segptr = NULL;
	for ( segnum = 0; segnum < options->maxseg; ++segnum )
	{
		/* Compute pointer to segment in directory buffer */
		if ( !segptr )
		{
			segptr = (Rt11SegEnt_t *)options->directory;
			relseg = 1;
		}
		else
		{
			relseg = segptr->link;
			if ( relseg && relseg <= firstseg->last )
			{
				segptr = (Rt11SegEnt_t *)(options->directory + (relseg - 1) * SEGSIZ);
			}
			else
			{
				break;
			}
		}
		/* Maintain a running LBA pointer */
		accumLBA = segptr->start;
		/* Compute pointer to list of directory entries */
		dirptr = (Rt11DirEnt_t *)(segptr + 1);
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("Segment %d (relseg: %d): smax: %d, link: %d, last: %d, extra: %d, start: %d\n",
				   segnum, relseg, segptr->smax, segptr->link, segptr->last,
				   segptr->extra, segptr->start);
		}
		/* Assume need to walk the maximum number of entries in the segment */
		for ( ii = 0; ii < options->numdent; ++ii )
		{
			/* I'm not sure if ENDBLK can appear in conjunction with the other flags.
			 * If ENDBLK appears on its own, then we don't save it in our linear arrays.
			 */
			if ( (dirptr->control & (ENDBLK | PERM | EMPTY | TENT)) == ENDBLK )
			{
				if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
				{
					printf("parse_directory: Found solo ENDBLK at entry %d in segment %d\n",
						   ii, relseg);
				}
				break;
			}
			/* Make a copy of the RT11 directory entry */
			wdp->rt11 = *dirptr;
			/* But don't keep the ENDBLK marker if there is one */
			wdp->rt11.control &= ~ENDBLK;
			/* Record the starting LBA for this file in the container */
			wdp->lba = accumLBA;
			/* Record the segment index and the file index within the segment */
			wdp->segNo = relseg;
			wdp->segIdx = ii;
			/* Record the pointer to the copy */
			*lap = wdp;
			/* Figure out what the file is */
			if ( !(dirptr->control & PERM) )
			{
				options->lastEmpty = wdp;
				options->totEmpty += dirptr->blocks;
				++options->totEmptyEntries;
			}
			else
			{
				options->lastEmpty = NULL;
				fromRad50(wdp->ffull, dirptr->name[0]);
				fromRad50(wdp->ffull + 3, dirptr->name[1]);
				wdp->ffull[6] = '.';
				fromRad50(wdp->ffull + 7, dirptr->name[2]);
				sqzSpaces(wdp->ffull);
				options->totPerm += dirptr->blocks;
				++options->totPermEntries;
				if ( options->largestPerm < dirptr->blocks )
					options->largestPerm = dirptr->blocks;
			}
			++lap;
			++wdp;
			++options->numWdirs;
			accumLBA += dirptr->blocks;
			if ( (dirptr->control & ENDBLK) )
			{
				if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
				{
					printf("parse_directory: Found ENDBLK");
					if ( (dirptr->control & PERM) )
						printf(" + PERM");
					if ( (dirptr->control & EMPTY) )
						printf(" + MT");
					if ( (dirptr->control & TENT) )
						printf(" + TNT");
					printf(" at entry %d in segment %d\n",
						   ii, relseg);
				}
				/* Done with this segment */
				break;
			}
			/* Advance directory entry pointer */
			dirptr = (Rt11DirEnt_t *)((unsigned char *)dirptr + sizeof(Rt11DirEnt_t) + firstseg->extra);
		}
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) && ii >= options->numdent )
		{
			printf("parse_directory: No ENDBLK in segment %d\n", relseg);
		}
	}
	/* Compute how many blocks have been used or are available in this container file */
	options->diskSize = options->totEmpty + options->totPerm + options->seg1LBA + options->maxseg * 2;
	if ( options->diskSize != options->containerBlocks )
	{
		if ( (options->emptyAdds = options->containerBlocks - options->diskSize) < 0 )
			 options->emptyAdds = 0;
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("Disksize (in blocks) computed via directory entries: %d, container filesize (in blocks): %d\n",
				   options->diskSize,
				   options->containerBlocks);
		}
		options->diskSize = options->containerBlocks;
	}
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("Accumulated LBA: %d. totEmpty: %d, emptyAdds: %d, totPerm: %d, diskSize: %d\n",
			   accumLBA, options->totEmpty, options->emptyAdds, options->totPerm, options->diskSize);
	}
	return 0;
}

/**
 * Unpack linear directory back to disk image format.
 * @param options - pointer to working area.
 * @return 0 if success, 1 if failure.
 */
int linearToDisk(Options_t *options)
{
	Rt11SegEnt_t * firstseg,*segptr = NULL;
	Rt11DirEnt_t *dirptr;
	int dentnum, dentsPerSeg, relseg;
	int ii, accumLBA;
	InWorkingDir_t *wdp;

	wdp = options->wDirArray;
	firstseg = (Rt11SegEnt_t *)options->directory;
	dentnum = options->numdent;
	relseg = 0;
	accumLBA = options->seg1LBA;
	dirptr = NULL;
	/* Assume we can use 1/2 of each segment */
	dentsPerSeg = options->numdent / 2;
	if ( (options->numWdirs + dentsPerSeg - 1) / dentsPerSeg >= options->maxseg )
	{
		/* There's too many files for that, so compute what will work */
		if ( options->maxseg > 1 )
		{
			dentsPerSeg = options->numWdirs / (options->maxseg - 1);
		}
		else
		{
			dentsPerSeg = options->numdent;
		}
	}
	if ( dentsPerSeg > options->numdent || dentsPerSeg * options->maxseg < options->numWdirs )
	{
		int sugg;
		sugg = (2 * options->numWdirs + options->numdent - 1) / options->numdent;
		if ( sugg > 31 )
			sugg = 31;
		fprintf(stderr, "Too many files to fit into too few segments. Have only %d.\n"
				"Suggest you \"sqz --segment=%d\"\n",
				options->maxseg, sugg);
		return 1;
	}
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("linearToDisk: wDirs: %d, dentsPerSeg: %d, maxseg: %d\n",
			   options->numWdirs, dentsPerSeg, options->maxseg);
	}
	for ( ii = 0; ii < options->numWdirs; ++ii, ++wdp )
	{
		if ( dentnum >= dentsPerSeg )
		{
			++relseg;
			if ( segptr )
			{
				segptr->link = relseg;
			}
			if ( dirptr && dentnum < options->numdent )
			{
				memset(dirptr, 0, sizeof(Rt11DirEnt_t));
				dirptr->control = ENDBLK;
			}
			firstseg->last = relseg;
			segptr = (Rt11SegEnt_t *)(options->directory + (relseg - 1) * SEGSIZ);
			segptr->smax = firstseg->smax;
			segptr->extra = firstseg->extra;
			segptr->last = relseg;
			segptr->link = 0;
			segptr->start = wdp->lba;
			accumLBA = wdp->lba;
			dentnum = 0;
			dirptr = (Rt11DirEnt_t *)(segptr + 1);
			if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
			{
				printf("LinearToDisk: Segment %d (relseg: %d): smax: %d, link: %d, last: %d, extra: %d, start: %d\n",
					   relseg - 1, relseg, segptr->smax, segptr->link, segptr->last,
					   segptr->extra, segptr->start);
			}
		}
		*dirptr = wdp->rt11;
		dirptr = (Rt11DirEnt_t *)((unsigned char *)dirptr + sizeof(Rt11DirEnt_t) + firstseg->extra);
		accumLBA += wdp->rt11.blocks;
		++dentnum;
	}
	if ( dentnum < options->numdent )
	{
		dirptr->blocks = 0;
		dirptr->control = ENDBLK;
	}
	options->dirDirty = 1;
	return 0;
}


