/*  $Id: floppy.c,v 1.1 2022/07/29 00:58:55 dave Exp dave $ $

	floppy.c - Misc floppy diskette utilties used by rtpip
	
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
 * @file floppy.c
 * Misc floppy utilities used by rtpip.
 */

/** DEC encoded their floppy disks using an interleaved
 *  sector mechanism. There are 77 tracks of 26 sectors on an
 *  8" floppy diskette. The sectors are read such that every
 *  other one is read in order to improve performance.
 *  Evidently the CPU and controller were not able to keep up
 *  with issuing a read command, reading the contents, then
 *  issuing a read command for the next sector before it had
 *  already passed beneath the read head. So they chose to
 *  skip the next sector and read them every other one. In
 *  addition, when crossing tracks, they advance the sector
 *  number by 6 to allow time for the read head to move.
 *  
 *  What this code does, is read the entire disk container
 *  file into memory then "descramble" the contents such that
 *  it appears to the rest of the program as though it is a
 *  "normal" disk contents. Then when it is time to write it
 *  back, it scrambles the entire disk contents and writes
 *  the whole container file back whether it all changed or
 *  not.
 **/

/** descramble - Rearranges the diskette container file
 *  contents.
 *  @param options - pointer to options data.
 *  @return 0 on success. Contents pointed to by floppyImage
 *          have been rearranged into buffer pointed to by
 *          floppyImageUnscrambled.
 **/
int descramble(Options_t *options)
{
	int blkNo, trackNo, sectorNo, sectorLen;
	int totBlocks;
	U8 * src,*dst;

	sectorLen = (options->cmdOpts & CMDOPT_SINGLE_FLPY) ? 128 : 256;
	totBlocks = (NUM_SECTORS * (NUM_TRACKS - 1));   /* assume block size is sector size */
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) || options->verbose )
		printf("Floppy image has %d total usable sectors, %d total usable blocks: %d tracks of 26 sectors of %d bytes each.\n",
			   NUM_SECTORS * (NUM_TRACKS - 1),
			   (totBlocks * sectorLen) / BLKSIZ,
			   NUM_TRACKS - 1,
			   sectorLen);
	dst = options->floppyImageUnscrambled;
	/* 1) We assume the "block" is a sector.
	 * 2) We loop through all the blocks in the input buffer 
	 * 3) What it does is use every other sector in a given track, 
	 * but when crossing a track boundary, it offsets the starting sector by 6. 
	 * It skips all sectors on track 0. 
	 */
	for ( blkNo = 0; blkNo < totBlocks; ++blkNo )
	{
		int ii;
		/* Compute a base track number */
		trackNo = (blkNo / NUM_SECTORS);
		/* Compute a base sector number times 2 */
		ii = ((blkNo % NUM_SECTORS) << 1);
		/* If the sector is > NUM_SECTORS, make it odd */
		if ( ii >= NUM_SECTORS )
			ii++;
		/* Compute the actual sector */
		sectorNo = (((ii + (6 * trackNo)) % NUM_SECTORS));
		/* skip all sectors on track 0, but those sectors do not participate in the scramble algorithm */
		++trackNo;
		/* Compute pointer into input buffer to the appropriate sector (LBA) */
		src = options->floppyImage + (trackNo * NUM_SECTORS + sectorNo) * sectorLen;
		/* Copy it to the unscrambled buffer */
		memcpy(dst, src, sectorLen);
		/* advance output pointer */
		dst += sectorLen;
	}
	return 0;
}

/** rescramble - Scrambles the file contents in logical
 *  order into diskette format.
 *  @param options - pointer to options data.
 *  @param optionalInput - pointer to optional input to
 *  					 descramble into options->floppyImage.
 *  @return 0 on success. Contents pointed to by
 *          floppyImageUnscrambled have been rearranged into
 *          buffer pointed to by floppyImage.
 **/
int rescramble(Options_t *options, U8 *optionalInput)
{
	int blkNo, trackNo, sectorNo, sectorLen;
	int totBlocks;
	U8 * src,*dst;

	sectorLen = (options->cmdOpts & CMDOPT_SINGLE_FLPY) ? 128 : 256;
	totBlocks = (NUM_SECTORS * (NUM_TRACKS - 1));   /* assume block size is sector size */
	src = optionalInput ? optionalInput : options->floppyImageUnscrambled;
	/* 1) We assume the "block" is a sector.
	 * 2) We loop through all the blocks in the input buffer 
	 * 3) What it does is use every other sector in a given track, 
	 * but when crossing a track boundary, it offsets the starting sector by 6. 
	 * It skips all sectors on track 0. 
	 */
	for ( blkNo = 0; blkNo < totBlocks; ++blkNo )
	{
		int ii;
		/* Compute a base track number */
		trackNo = (blkNo / NUM_SECTORS);
		/* Compute a base sector number times 2 */
		ii = ((blkNo % NUM_SECTORS) << 1);
		/* If the sector is > NUM_SECTORS, make it odd */
		if ( ii >= NUM_SECTORS )
			ii++;
		/* Compute the actual sector */
		sectorNo = (((ii + (6 * trackNo)) % NUM_SECTORS));
		/* skip all sectors on track 0, but those sectors do not participate in the scramble algorithm */
		++trackNo;
		/* Compute pointer into input buffer to the appropriate sector (LBA) */
		dst = options->floppyImage + (trackNo * NUM_SECTORS + sectorNo) * sectorLen;
		/* Copy it to the unscrambled buffer */
		memcpy(dst, src, sectorLen);
		/* advance output pointer */
		src += sectorLen;
	}
	return 0;
}


