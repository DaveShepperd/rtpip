/*  $Id: getcmd.c,v 1.9 2022/07/29 02:55:28 dave Exp dave $

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

/**
 * @file getcmd.c
 * Contains functions used to parse the command line arguments.
 */

#include "rtpip.h"

static int option_index;

#ifndef DEBUG_ARGS
	#define DEBUG_ARGS 1
#endif

static int get_files(Options_t *options, int regExpFlg, int exprType, int argc, char *const *argv)
{
	void *retv;
	int xit = 0, cnt, ii;

	cnt = argc - (optind - 1);
#if DEBUG_ARGS
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("get_files: cnt=%d, rexflg=%d, exprType=%d, argc=%d, optind=%d, optarg='%s', todo=%d\n",
			   cnt, regExpFlg, exprType, argc, optind, optarg, options->todo);
		for ( ii = 0; ii < cnt; ++ii )
		{
			printf("  %d: '%s'\n", ii, argv[optind - 1 + ii]);
		}
	}
#endif
	options->numArgFiles = cnt;
	options->argFiles = argv + optind - 1;
	if ( regExpFlg && cnt > 0 )
	{
		const char *errMsg;
		size_t memSize;

		if ( exprType )
		{
			errMsg = "regular expression compiles";
			memSize = sizeof(regex_t);
		}
		else
		{
			errMsg = "filename wildcards";
			memSize = sizeof(char) * 10;
		}
		retv = calloc(cnt, memSize);
		if ( !retv )
		{
			fprintf(stderr, "Ran out of memory allocating %d bytes for %s\n",
					(int)(cnt * memSize), errMsg);
			return 1;
		}
#if !NO_REGEXP
		if ( exprType )
		{
			options->rexts = (regex_t *)retv;
			for ( ii = 0; ii < cnt; ++ii )
			{
				int cv;
				cv = regcomp(options->rexts + ii, options->argFiles[ii], REG_ICASE | REG_NOSUB);
#if DEBUG_ARGS
				if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
				{
					printf("Compiled a regexp for '%s'. Returned value %d\n",
						   options->argFiles[ii], cv);
				}
#endif
				if ( cv )
				{
					char tmp[512];
					regerror(cv, options->rexts + ii, tmp, sizeof(tmp));
					fprintf(stderr, "Error performing regex() on '%s': %s\n",
							options->argFiles[ii], tmp);
					xit = 1;
					break;
				}
			}
		}
		else
#endif	/* !NO_REGEXP */
		{
			char *homePtr, *filePtr, cc;
			const char *src, *ext;

			options->normExprs = (char *)retv;
			homePtr = (char *)retv;
			for ( ii = 0; ii < cnt; ++ii, homePtr += 10 )
			{
				/* point to temp filename */
				filePtr = homePtr;
				/* prefill with ' ' */
				memset(filePtr, ' ', 9);
				filePtr[9] = 0;
				src = options->argFiles[ii];
				ext = strchr(src, '.');
				if ( ext )
				{
					if ( ext - src > 6 )
					{
						fprintf(stderr, "Filename is too long: '%s'. Cannot contain more than 6 characters.\n", src);
						xit = 1;
						break;
					}
					if ( strlen(ext + 1) > 3 )
					{
						fprintf(stderr, "Filetype is too long: '%s'. Cannot contain more than 3 characters.\n", src);
						xit = 1;
						break;
					}
				}
				else if ( strlen(src) > 6 )
				{
					fprintf(stderr, "Filename is too long: '%s'. Cannot contain more than 6 characters.\n", src);
					xit = 1;
					break;
				}
				while ( *src && (ext && src < ext) )
				{
					cc = *src++;
					if ( cc == '*' )
					{
						memset(filePtr, '?', homePtr + 6 - filePtr);
						break;
					}
					if ( islower(cc) )
						cc = toupper(cc);
					*filePtr++ = cc;
				}
				if ( ext )
				{
					src = ext + 1;
					filePtr = homePtr + 6;
					while ( *src )
					{
						cc = *src++;
						if ( cc == '*' )
						{
							memset(filePtr, '?', homePtr + 9 - filePtr);
							break;
						}
						if ( islower(cc) )
							cc = toupper(cc);
						*filePtr++ = cc;
					}
				}
			}
		}
	}
	return xit;
}

static struct option long_dir_opts[] = {
	{ "all", 0, 0, 'a' },
	{ "col", 1, 0, 'c' },
	{ "help", 0, 0, 'h' },
	{ "full", 0, 0, 'f' },
	{ "reverse", 0, 0, 'r' },
#if !NO_REGEXP
	{ "rexp", 0, 0, 'R' },
#endif
	{ "sort", 1, 0, 's' },
	{ "verbose", 0, 0, 'v' },
	{ 0, 0, 0, 0 }
};

static int get_ls(Options_t *options, int argc, char *const *argv)
{
	int goptret, sopt;
	char *retv;

	options->todo |= TODO_LIST;
	while ( 1 )
	{
#if !NO_REGEXP
		static const char Opts[] = "-ac:fh?rRs:v123456789";
#else
		static const char Opts[] = "-ac:fh?rs:v123456789";
#endif
		goptret = getopt_long(argc, argv, Opts, long_dir_opts, &option_index);
#if DEBUG_ARGS
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("get_ls():, goptret=%d(%c), optarg=%p(\"%s\"), optind=%d, optopt=%d\n",
				   goptret,
				   isprint(goptret) ? goptret : '?',
				   optarg, optarg, optind, optopt);
		}
#endif
		if ( goptret < 0 )
			return 0;
		switch (goptret)
		{
		case 1:
			return get_files(options, 1, (options->fileOpts & FILEOPTS_REGEXP), argc, argv);
		case 'a':
			options->lsOpts |= LSOPTS_ALL;
			continue;
		case 'c':
			retv = NULL;
			options->columns = strtoul(optarg, &retv, 0);
			if ( !options->columns || options->columns > 9 || !retv || *retv )
			{
				fprintf(stderr, "Invalid column spec: \"%s\"\n", optarg);
				return 1;
			}
			continue;
		case 'f':
			options->lsOpts |= LSOPTS_FULL;
			continue;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			options->columns = goptret - '0';
			continue;
		case 'h':
		case '?':
			options->lsOpts |= LSOPTS_HELP;
			continue;
		case 'r':
			options->sortby |= SORTBY_REV;
			continue;
#if !NO_REGEXP
		case 'R':
			options->fileOpts |= FILEOPTS_REGEXP;
			continue;
#endif
		case 's':
			sopt = 0;
			for (; optarg && *optarg; ++optarg )
			{
				if ( *optarg == 'n' )
				{
					sopt |= SORTBY_NAME;
					continue;
				}
				if ( *optarg == 'd' )
				{
					sopt |= SORTBY_DATE;
					continue;
				}
				if ( *optarg == 't' )
				{
					sopt |= SORTBY_TYPE;
					continue;
				}
				if ( *optarg == 's' )
				{
					sopt |= SORTBY_SIZE;
					continue;
				}
				fprintf(stderr, "Undefined sort option(s): --sort='%s'\n", optarg);
				return 1;
			}
			if ( !sopt )
			{
				fprintf(stderr, "No sort option provided\n");
				return 1;
			}
			options->sortby |= sopt;
			continue;
		case 'v':
			options->verbose = 1;
			continue;
		default:
			break;
		}
		break;
	}
	options->lsOpts = LSOPTS_HELP;
	return 1;
}

static struct option long_in_opts[] = {
	{ "ascii", 0, 0, 'a' },
	{ "binary", 0, 0, 'b' },
	{ "date", 1, 0, 'd' },
#if !NO_REGEXP
	{ "rexp", 0, 0, 'R' },
#endif
	{ "assumeyes", 0, 0, 'y' },
	{ "time", 0, 0, 't' },
	{ "verbose", 0, 0, 'v' },
	{ 0, 0, 0, 0 }
};

static int get_inp(Options_t *options, int argc, char *const *argv)
{
	int goptret;

	options->todo |= TODO_INP;
	while ( 1 )
	{
#if !NO_REGEXP
		static const char Opts[] = "-abd:Rtvhy?";
#else
		static const char Opts[] = "-abd:tvhy?";
#endif
		goptret = getopt_long(argc, argv, Opts, long_in_opts, &option_index);
#if DEBUG_ARGS
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("get_inp:, goptret=%d(%c), optarg=%p(\"%s\"), optind=%d, optopt=%d\n",
				   goptret,
				   isprint(goptret) ? goptret : '?',
				   optarg, optarg, optind, optopt);
		}
#endif
		if ( goptret < 0 )
		{
			options->inOpts = INOPTS_HELP;
			return 0;
		}
		switch (goptret)
		{
		case 1:
			return get_files(options, 0, (options->fileOpts & FILEOPTS_REGEXP), argc, argv);
		case 'a':
			options->inOpts |= INOPTS_ASC;
			continue;
		case 'b':
			options->inOpts &= ~INOPTS_ASC;
			continue;
		case 'd':
			{
				int day, year;
				char *retv, *mon, mons[4];

				retv = NULL;
				day = strtol(optarg, &retv, 0);
				if ( day > 0 && retv && *retv == '-' )
				{
					mon = retv + 1;
					retv = strchr(mon, '-');
					if ( retv )
					{
						if ( retv - mon == 3 )
						{
							int mi;
							static const char *const Months[12] = {
								"jan", "feb", "mar", "apr", "may", "jun",
								"jul", "aug", "sep", "oct", "nov", "dec"
							};
							static const int DOM[12] = {
								31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
							};
							for ( mi = 0; mi < 3; ++mi )
							{
								mons[mi] = mon[mi];
								if ( islower(mons[mi]) )
									mons[mi] = tolower(mons[mi]);
							}
							mons[3] = 0;
							for ( mi = 0; mi < 12; ++mi )
							{
								if ( !strcmp(mons, Months[mi]) )
									break;
							}
							if ( mi < 12 && day <= DOM[mi] )
							{
								++retv;
								year = strtol(retv, &retv, 0);
								if ( retv && !*retv )
								{
									if ( year > 1900 )
										year -= 1900;
									if ( year >= 72 && year <= 99 )
									{
										options->inDate = ((mi + 1) << 10) | (day << 5) | ((year - 72) & 31);
										continue;
									}
								}
							}
						}
					}
				}
				fprintf(stderr, "Invalid date syntax '%s'. S/B dd-mmm-yy (72<=yy<=99)\n", optarg);
				break;
			}
		case 'y':
			options->inOpts |= INOPTS_NOASK;
			continue;
#if !NO_REGEXP
		case 'R':
			options->fileOpts |= FILEOPTS_REGEXP;
			continue;
#endif
		case 't':
			options->fileOpts |= FILEOPTS_TIMESTAMP;
			continue;
		case 'v':
			options->inOpts |= INOPTS_VERB;
			continue;
		case 'h':
		case '?':
			options->inOpts |= INOPTS_HELP;
			continue;
		default:
			break;
		}
		break;
	}
	options->inOpts = INOPTS_HELP;
	return 0;
}

static struct option long_out_opts[] = {
	{ "ascii", 0, 0, 'a' },
	{ "binary", 0, 0, 'b' },
	{ "help", 0, 0, 'h' },
	{ "lower", 0, 0, 'l' },
	{ "outdir", 1, 0, 'o' },
#if !NO_REGEXP
	{ "rexp", 0, 0, 'R' },
#endif
	{ "assumeyes", 0, 0, 'y' },
	{ "time", 0, 0, 't' },
	{ "verbose", 0, 0, 'v' },
	{ 0, 0, 0, 0 }
};

static int get_out(Options_t *options, int argc, char *const *argv)
{
	int goptret;

	options->todo |= TODO_OUT;
	while ( 1 )
	{
#if !NO_REGEXP
		static const char Opts[] = "-ablno:Rtvyh?";
#else
		static const char Opts[] = "-ablno:tvyh?";
#endif
		goptret = getopt_long(argc, argv, Opts, long_out_opts, &option_index);
#if DEBUG_ARGS
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("get_out:, goptret=%d(%c), optarg=%p(\"%s\"), optind=%d, optopt=%d\n",
				   goptret,
				   isprint(goptret) ? goptret : '?',
				   optarg, optarg, optind, optopt);
		}
#endif
		if ( goptret < 0 )
		{
			options->outOpts = OUTOPTS_HELP;
			return 0;
		}
		switch (goptret)
		{
		case 1:
			return get_files(options, 1, (options->fileOpts & FILEOPTS_REGEXP), argc, argv);
		case 'a':
			options->outOpts |= OUTOPTS_ASC;
			continue;
		case 'b':
			options->outOpts &= ~OUTOPTS_ASC;
			continue;
		case 'l':
			options->outOpts |= OUTOPTS_LC;
			continue;
		case 'o':
			options->outDir = optarg;
			continue;
		case 'y':
			options->outOpts |= OUTOPTS_NOASK;
			continue;
#if !NO_REGEXP
		case 'R':
			options->fileOpts |= FILEOPTS_REGEXP;
			continue;
#endif
		case 't':
			options->fileOpts |= FILEOPTS_TIMESTAMP;
			continue;
		case 'v':
			options->outOpts |= OUTOPTS_VERB;
			continue;
		case 'h':
		case '?':
			options->outOpts |= OUTOPTS_HELP;
			continue;
		default:
			break;
		}
		break;
	}
	options->outOpts = OUTOPTS_HELP;
	return 0;
}

static struct option long_del_opts[] = {
	{ "help", 0, 0, 'h' },
#if !NO_REGEXP
	{ "rexp", 0, 0, 'R' },
#endif
	{ "assumeyes", 0, 0, 'y' },
	{ "verbose", 0, 0, 'v' },
	{ 0, 0, 0, 0 }
};

static int get_del(Options_t *options, int argc, char *const *argv)
{
	int goptret;

	options->todo |= TODO_DEL;
	while ( 1 )
	{
#if !NO_REGEXP
		static const char Opts[] = "-hRvy?";
#else
		static const char Opts[] = "-hvy?";
#endif
		goptret = getopt_long(argc, argv, Opts, long_del_opts, &option_index);
#if DEBUG_ARGS
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("get_del:, goptret=%d(%c), optarg=%p(\"%s\"), optind=%d, optopt=%d\n",
				   goptret,
				   isprint(goptret) ? goptret : '?',
				   optarg, optarg, optind, optopt);
		}
#endif
		if ( goptret < 0 )
		{
			options->delOpts = DELOPTS_HELP;
			return 0;
		}
		switch (goptret)
		{
		case 1:
			return get_files(options, 1, (options->lsOpts & DELOPTS_REGEXP), argc, argv);
		case 'y':
			options->delOpts |= DELOPTS_NOASK;
			continue;
		case 'v':
			options->delOpts |= DELOPTS_VERB;
			continue;
		case 'h':
		case '?':
			options->delOpts |= DELOPTS_HELP;
			continue;
#if !NO_REGEXP
		case 'R':
			options->delOpts |= DELOPTS_REGEXP;
			continue;
#endif
		default:
			break;
		}
		break;
	}
	options->delOpts = DELOPTS_HELP;
	return 0;
}

static struct option long_sqz_opts[] = {
	{ "assumeyes", 0, 0, 'y' },
	{ "verbose", 0, 0, 'v' },
	{ "help", 0, 0, 'h' },
	{ "segments", 1, 0, 's' },
	{ 0, 0, 0, 0 }
};

static int get_sqz(Options_t *options, int argc, char *const *argv)
{
	int goptret;
	char *retv;

	options->todo |= TODO_SQZ;
	while ( 1 )
	{
		goptret = getopt_long(argc, argv, "-vh?s:y", long_sqz_opts, &option_index);
#if DEBUG_ARGS
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("get_sqz:, goptret=%d(%c), optarg=%p(\"%s\"), optind=%d, optopt=%d\n",
				   goptret,
				   isprint(goptret) ? goptret : '?',
				   optarg, optarg, optind, optopt);
		}
#endif
		if ( goptret < 0 )
			return 0;
		switch (goptret)
		{
		case 1:
			return get_files(options, 0, 0, argc, argv);
		case 'y':
			options->sqzOpts |= SQZOPTS_NOASK;
			continue;
		case 'v':
			options->sqzOpts |= SQZOPTS_VERB;
			continue;
		case 'h':
		case '?':
			options->sqzOpts |= SQZOPTS_HELP;
			continue;
		case 's':
			retv = NULL;
			options->newMaxSeg = strtoul(optarg, &retv, 0);
			if ( options->newMaxSeg >= 32 || options->newMaxSeg < 1 || !retv || *retv )
			{
				fprintf(stderr, "Invalid segment number: \"%s\". Can only be 1 through 31.\n", optarg);
				return 1;
			}
			continue;
		default:
			break;
		}
		break;
	}
	options->sqzOpts = SQZOPTS_HELP;
	return 0;
}

static struct option long_new_opts[] = {
	{ "blocks", 1, 0, 'b' },
	{ "help", 0, 0, 'h' },
	{ "assumeyes", 0, 0, 'y' },
	{ "segments", 1, 0, 's' },
	{ "verbose", 0, 0, 'v' },
	{ 0, 0, 0, 0 }
};

static int get_new(Options_t *options, int argc, char *const *argv)
{
	int goptret;
	char *retv;

	options->todo |= TODO_NEW;
	while ( 1 )
	{
		goptret = getopt_long(argc, argv, "-b:h?s:vy", long_new_opts, &option_index);
#if DEBUG_ARGS
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("get_new:, goptret=%d(%c), optarg=%p(\"%s\"), optind=%d, optopt=%d\n",
				   goptret,
				   isprint(goptret) ? goptret : '?',
				   optarg, optarg, optind, optopt);
		}
#endif
		if ( goptret < 0 )
			return 0;
		switch (goptret)
		{
		case 1:
			return get_files(options, 0, 0, argc, argv);
		case 'y':
			options->newOpts |= NEWOPTS_NOASK;
			continue;
		case 'v':
			options->newOpts |= NEWOPTS_VERB;
			continue;
		case 'h':
		case '?':
			options->newOpts |= NEWOPTS_HELP;
			continue;
		case 's':
			retv = NULL;
			options->newMaxSeg = strtoul(optarg, &retv, 0);
			if ( options->newMaxSeg >= 32 || options->newMaxSeg < 1 || !retv || *retv )
			{
				fprintf(stderr, "Invalid segment number: \"%s\". Can only be 1 through 31\n", optarg);
				return 1;
			}
			continue;
		case 'b':
			retv = NULL;
			options->newDiskSize = strtoul(optarg, &retv, 0);
			if ( options->newDiskSize < 400 || options->newDiskSize > 65535 || !retv || *retv )
			{
				fprintf(stderr, "Invalid disk size in 512 byte blocks: \"%s\". Must be 400 < n < 65535\n", optarg);
				return 1;
			}
			continue;
		default:
			break;
		}
		break;
	}
	options->newOpts = NEWOPTS_HELP;
	return 0;
}

static int get_cmd(Options_t *options, int argc, char *const *argv)
{
	int goptret;

	while ( 1 )
	{
		goptret = getopt(argc, argv, "-h?v");
#if DEBUG_ARGS
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("get_cmd(): state=%d, getopt()=%d(%c), optarg=%p(\"%s\"), optind=%d, optopt=%d\n",
				   options->cmdState, goptret,
				   isprint(goptret) ? goptret : '?',
				   optarg, optarg, optind, optopt);
		}
#endif
		switch (goptret)
		{
		case 1:
			options->cmd = optarg;
			if ( optarg && (!strcmp(optarg, "ls") || !strcmp(optarg, "dir")) )
			{
				options->cmdState = CMDSTATE_LS;
				return 0;
			}
			if ( optarg && !strcmp(optarg, "in") )
			{
				options->cmdState = CMDSTATE_IN;
				return 0;
			}
			if ( optarg && !strcmp(optarg, "out") )
			{
				options->cmdState = CMDSTATE_OUT;
				return 0;
			}
			if ( optarg && (!strcmp(optarg, "del") || !strcmp(optarg, "rm")) )
			{
				options->cmdState = CMDSTATE_DEL;
				return 0;
			}
			if ( optarg && !strcmp(optarg, "sqz") )
			{
				options->cmdState = CMDSTATE_SQZ;
				return 0;
			}
			if ( optarg && !strcmp(optarg, "new") )
			{
				options->cmdState = CMDSTATE_NEW;
				return 0;
			}
			break;
		case 'v':
			options->verbose = 1;
			continue;
		default:
			break;
		}
		break;
	}
	options->todo = TODO_HELP;
	return 1;
}

static struct option long_cont_options[] = {
	{ "debug", 0, 0, 'd' },
	{ "floppy", 0, 0, 'f' },
	{ "double", 0, 0, 'F' },
	{ "help", 0, 0, '?' },
	{ "lba", 1, 0, 'L' },
	{ "nowrite", 0, 0, 'n' },
	{ "verbose", 0, 0, 'v' },
	{ 0, 0, 0, 0 }
};
static int get_container(Options_t *options, int argc, char *const *argv)
{
	int goptret;
	char *retv;

	while ( 1 )
	{
		goptret = getopt_long(argc, argv, "-dfFh?l:nv", long_cont_options, &option_index);
#if DEBUG_ARGS
		if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
		{
			printf("get_container(): state=%d, getopt()=%d(%c), optarg=%p(\"%s\"), optind=%d, optopt=%d\n",
				   options->cmdState, goptret,
				   isprint(goptret) ? goptret : '?',
				   optarg, optarg, optind, optopt);
		}
#endif
		switch (goptret)
		{
		case 1:
			options->cmdState = CMDSTATE_CMD;
			options->container = optarg;
			return 0;
		default:
		case 0:
			break;
		case 'h':
		case '?':
		case ':':
			options->todo = TODO_HELP;
			return 1;
		case 'l':
			retv = NULL;
			options->seg1LBA = strtoul(optarg, &retv, 0);
			if ( !options->seg1LBA || !retv || *retv )
			{
				fprintf(stderr, "Invalid starting LBA: \"%s\"\n", optarg);
				return 1;
			}
			continue;
		case 'v':
			++options->verbose;
			continue;
		case 'd':
			options->cmdOpts |= CMDOPT_DBG_NORMAL;
			continue;
		case 'f':
			options->cmdOpts |= CMDOPT_SINGLE_FLPY;
			continue;
		case 'F':
			options->cmdOpts |= CMDOPT_DOUBLE_FLPY;
			continue;
		case 'n':
			options->cmdOpts |= CMDOPT_NOWRITE;
			continue;
		}
		options->todo = TODO_HELP;
		return 1;
	}
	options->todo = TODO_HELP;
	return 1;
}

/*
 * Process the command line arguments.
 * @param options - pointer to options list.
 * @param argc - number of command line arguments.
 * @param argv - pointer to array of command line arguments.
 * @return 0 if success; non-zero if failure.
 */
int getcmds(Options_t *options, int argc, char *const *argv)
{
	if ( get_container(options, argc, argv) )
		return 1;
	if ( get_cmd(options, argc, argv) )
		return 1;
	switch (options->cmdState)
	{
	case CMDSTATE_LS:
		return get_ls(options, argc, argv);
	case CMDSTATE_IN:
		return get_inp(options, argc, argv);
	case CMDSTATE_OUT:
		return get_out(options, argc, argv);
	case CMDSTATE_SQZ:
		return get_sqz(options, argc, argv);
	case CMDSTATE_DEL:
		return get_del(options, argc, argv);
	case CMDSTATE_NEW:
		return get_new(options, argc, argv);
	default:
		break;
	}
	options->todo =  TODO_HELP;
	return 1;
}

