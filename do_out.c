/*  $Id: do_out.c,v 1.1 2022/07/29 00:58:55 dave Exp dave $ $

	do_out.c - Copy file from RT11 container file
	
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

#if MSYS2 || MINGW
#define MKDIR(a,b) mkdir(a)
#else
#define MKDIR(a,b) mkdir(a,b)
#endif

/**
 * @file do_out.c
 * Copy file from RT11 container file. Called by rtpip.
 */

/**
 * Change working directory. Create it if necessary.
 * @param options - pointer to options.
 * @return 0 if success. 1 if failure. Error message(s) sent to stderr.
 */

static int doChDir(Options_t *options)
{
	struct stat st;
	int ii;

	ii = stat(options->outDir, &st);
	if ( ii )
	{
		if ( !(options->cmdOpts & CMDOPT_NOWRITE) )
		{
			ii = MKDIR(options->outDir, 0775);
			if ( ii )
			{
				fprintf(stderr, "Unable to create directory '%s': %s\n",
						options->outDir, strerror(errno));
				return 1;
			}
			ii = stat(options->outDir, &st);
		}
		else
		{
			printf("Would have created directory: '%s'", options->outDir);
			ii = 0;
		}
	}
	if ( !ii )
	{
		if ( !(options->cmdOpts & CMDOPT_NOWRITE) )
		{
			if ( S_ISDIR(st.st_mode) )
			{
				ii = chdir(options->outDir);
				if ( !ii )
				{
					if ( options->verbose || (options->outOpts & OUTOPTS_VERB) )
					{
						printf("Changed directory to '%s'\n", options->outDir);
					}
					return 0;
				}
				fprintf(stderr, "Unable to chdir() to '%s': %s\n",
						options->outDir, strerror(errno));
				return 1;
			}
			fprintf(stderr, "'%s' is not a directory\n", options->outDir);
			return 1;
		}
		printf("Would have changed directory to '%s'\n", options->outDir);
		return 0;
	}
	fprintf(stderr, "Internal error with doChDir(): '%s'\n",
			strerror(errno));
	return 1;
}

/**
 * Copy an RT11 file out of container.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
int do_out(Options_t *options)
{
	Rt11DirEnt_t *dirptr;
	int ii, filesCopied = 0, needChDir = 0;
	InWorkingDir_t *wdp;

	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("do_out: numWdirs=%d, numArgFiles=%d\n",
			   options->numWdirs, options->numArgFiles);
	}
	if ( options->outDir )
		needChDir = 1;
	wdp = options->wDirArray;
	if ( options->numArgFiles )
	{
		for ( ii = 0; ii < options->numWdirs; ++ii, ++wdp )
		{
			unsigned char *iBuf, *src, *dst;
			FILE *oFile;
			int retv, jj;

			dirptr = &wdp->rt11;
			if ( !(dirptr->control & PERM) )
				continue;
			if ( !filterFilename(options, wdp->ffull) )
				continue;
			if ( needChDir )
			{
				if ( doChDir(options) )
					return 1;
				needChDir = 0;
			}
			if ( !(options->outOpts & OUTOPTS_NOASK) )
			{
				char prompt[128];
				if ( !(options->inOpts&INOPTS_OVR) )
				{
					struct stat st;
					retv = stat(wdp->ffull,&st);
					if ( !retv )
					{
						if ( options->outDir )
							printf("Warning: Existing file %s/%s.  ", options->outDir ? options->outDir : ".", wdp->ffull);
						else
							printf("Warning: Existing file %s.  ", wdp->ffull);
					}
				}
				snprintf(prompt, sizeof(prompt) - 1, "Copy out '%s'?", wdp->ffull);
				jj = getYN(prompt, YN_YES);
				if ( jj == YN_QUIT )
					break;
				if ( jj != YN_YES )
					continue;
			}
			if ( (options->outOpts & OUTOPTS_LC) )
			{
				char *cp = wdp->ffull;
				while ( *cp )
				{
					if ( isupper(*cp) )
						*cp = tolower(*cp);
					++cp;
				}
			}
			iBuf = (unsigned char *)malloc(dirptr->blocks * BLKSIZ);
			if ( !iBuf )
			{
				fprintf(stderr, "Ran out of memory allocating %d bytes to read '%s'\n",
						dirptr->blocks * BLKSIZ, wdp->ffull);
				fclose(oFile);
				return 1;
			}
			if ( !(options->cmdOpts & (CMDOPT_SINGLE_FLPY | CMDOPT_DOUBLE_FLPY)) )
			{
				retv = fseek(options->inp, wdp->lba * BLKSIZ, SEEK_SET);
				if ( retv < 0 || ferror(options->inp) || (ftell(options->inp) != wdp->lba*BLKSIZ) )
				{
					fprintf(stderr, "Unable to seek to %d in input '%s': %s\n",
							wdp->lba, options->container, strerror(errno));
					fclose(oFile);
					free(iBuf);
					continue;
				}
				retv = fread(iBuf, 1, dirptr->blocks * BLKSIZ, options->inp);
				if ( retv != dirptr->blocks * BLKSIZ )
				{
					fprintf(stderr, "Error reading %d bytes from '%s' starting at LBA %d. Read %d: %s\n",
							dirptr->blocks * BLKSIZ, options->container,
							wdp->lba, retv, strerror(errno));
					fclose(oFile);
					free(iBuf);
					continue;
				}
			}
			else
			{
				if ( wdp->lba * BLKSIZ >= options->floppyImageSize )
				{
					fprintf(stderr, "Error seeking to %d. Outside of floppy image of %d bytes. Probably corruption in container directory.\n", wdp->lba * BLKSIZ, options->floppyImageSize);
					free(iBuf);
					continue;
				}
				if ( (wdp->lba+dirptr->blocks)*BLKSIZ > options->floppyImageSize )
				{
					fprintf(stderr, "Error in file size of %d. Would read beyond EOF of container of %d bytes. Probably corruption in container directory.\n", dirptr->blocks * BLKSIZ, options->floppyImageSize);
					free(iBuf);
					continue;
				}
				retv = dirptr->blocks*BLKSIZ;
				memcpy(iBuf,options->floppyImageUnscrambled+wdp->lba*BLKSIZ, retv);
			}
			src = iBuf;
			if ( (options->outOpts & OUTOPTS_ASC) )
			{
				dst = src;
				while ( src < iBuf + retv && *src && *src != ('Z' & 63) )
				{
					/* Leave lone cr's alone but change crlf to just lf */
					if ( *src && (*src != '\r' || src[1] != '\n') )
						*dst++ = *src;
					++src;
				}
				retv = dst - iBuf;
			}
			if ( !(options->cmdOpts & CMDOPT_NOWRITE) )
			{
				oFile = fopen(wdp->ffull, "wb");
				if ( !oFile )
				{
					fprintf(stderr, "Unable to open '%s' for output: %s\n",
							wdp->ffull, strerror(errno));
					continue;
				}
				if ( !(options->cmdOpts & CMDOPT_NOWRITE) )
				{
					oFile = fopen(wdp->ffull, "wb");
					if ( !oFile )
					{
						fprintf(stderr, "Unable to open '%s' for output: %s\n",
								wdp->ffull, strerror(errno));
						continue;
					}
				}
				jj = fwrite(iBuf, 1, retv, oFile);
				if ( jj != retv )
				{
					fprintf(stderr, "Error writing %d bytes to '%s'. Wrote %d. '%s'\n",
							retv, wdp->ffull, jj, strerror(errno));
				}
				fclose(oFile);
				if ( (options->fileOpts & FILEOPTS_TIMESTAMP) )
				{
					struct utimbuf uTime;
					struct tm tm;
					int yr, age;

					memset(&tm, 0, sizeof(tm));
					tm.tm_mday = (wdp->rt11.date >> 5) & 31;
					tm.tm_mon = ((wdp->rt11.date >> 10) & 15) - 1;
					yr = wdp->rt11.date & 31;
					age = (wdp->rt11.date >> 14) & 3;
					switch (age)
					{
					case 1:
						yr += 2004;
						break;
					case 2:
						yr += 2036;
						break;
					case 3:
						yr += 2068;
						break;
					default:
						yr += 1972;
						break;
					}
					tm.tm_year = yr - 1900;
					if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
					{
						printf("do_out(): preserve timestamp: file '%s', bDate=0x%04X, age=%d, date=%02d/%02d/%04d, tm_mday=%d, tm_mon=%d, tm_year=%d\n",
							   wdp->ffull, wdp->rt11.date, age, tm.tm_mday, tm.tm_mon + 1, tm.tm_year, tm.tm_mday, tm.tm_mon, tm.tm_year);
					}
					uTime.actime = time(NULL);
					uTime.modtime = mktime(&tm);
					utime(wdp->ffull, &uTime);
				}
			}
			free(iBuf);
			if ( !(options->cmdOpts & CMDOPT_NOWRITE) )
			{
				if ( options->verbose || (options->outOpts & OUTOPTS_VERB) )
				{
					printf("Copied %-12.12s %5d blocks @ LBA %6d, wrote %7d bytes.\n",
						   wdp->ffull, dirptr->blocks, wdp->lba, retv);
				}
			}
			else
			{
				printf("Would have Copied %-12.12s %5d blocks @ LBA %6d, would have written %7d bytes.\n",
					   wdp->ffull, dirptr->blocks, wdp->lba, retv);
			}
			++filesCopied;
		}
	}
	if ( options->verbose || (options->outOpts & OUTOPTS_VERB) )
	{
		if ( !(options->cmdOpts & CMDOPT_NOWRITE) )
			printf("%d files copied.\n", filesCopied);
		else
			printf("%d files potentially copied.\n", filesCopied);
	}
	return 0;
}


