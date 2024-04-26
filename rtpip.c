/*  $Id: rtpip.c,v 1.12 2022/07/29 02:55:28 dave Exp dave $

	rtpip.c - Read/Write files from/to RT-11 container file
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
 * @file rtpip.c
 * Program to manipulate the contents of an RT11 container file.
 */

/** 
 * @mainpage
 * There is a terrific program to simulate various computers from the old days called
 * <a href=http://simh.trailing-edge.com/>SIMH</a>. The simulator allows these computers
 * to run various operating systems. Among them is the PDP11 running RT-11. The simulator
 * reads and writes data using container filesystems to simulate real disks and tapes.
 * 
 * There is a program, PUTR, available to manipulate the contents of these container files
 * organised in various operating system file formats, but this program was written in
 * 8086 assembly language for PC-DOS, so has limited ability to run on various other platforms.
 * PUTR can be found <a href=http://dbit.com/putr/>HERE</a>.
 * 
 * So with a slight need to be able to manipulate the contents of an RT-11 container file
 * and having nothing but free time, I undertook to write this little program to aid in
 * that endeavor. Version 1.0 of this program program only manipulates RT-11 disk container files.
 *
 * @section sec1 How to run rtpip
 * @b rtpip [mainOpts] @b <container> @b <cmd> [cmdOpts]
 *    [filename ...]
 * 
 * where: @n
 * [] denotes optional input. @b Bold parameters are required.
 * @n The optional mainOpts available are:
 * @n
 * --help or -h or -? = Display help for running @b rtpip. @n
 * --nowrite or -n = Do not write anything. Just say what would
 *   be written. @n
 * --verbose or -v = sets verbose mode. @n
 * --debug or -d = sets normal debug mode. @n
 * --empty or -e = sets debug mode except do not squeeze when
 *   writing. @n
 * --floppy or -f = indicates container is a single density
 *  floppy disk image. @n
 * --double or -F = indicates container is a double density
 *   floppy disk image. @n
 * -lN = @b N is the starting block number of the directory
 *  (default=6). @n
 * 
 * <container> = path to container file. @n
 * <cmd> = one of @ref ls, @ref in, @ref out, @ref del, @ref new
 * or @ref sqz
 * 
 * @subsection ls
 * Optional cmdOpts available for ls (or dir) command: @n
 * --help or -h or -? = help specific to ls command. @n
 * --cols=N or -cN or -N = @b N is a number 1 through 9 indicating columns to output. @n
 * --full or -f = Output full directory information (only if column is not specifed). @n
 * --all or -a = Output all directory information (only if column is not specified). @n
 * --sort=n or -sn = Sort by filename. @n
 * --sort=t or -st = Sort by filetype. @n
 * --sort=d or -sd = Sort by date. @n
 * --sort=s or -ss = Sort by size. @n
 * --reverse or -r = Reverse sort. @n
 * --rexp or -R = filename list is zero or more regular
 *   expressions. @n
 * verbose or -v = Set verbose mode. @n @n
 * Filename = zero or more expressions to filter the filenames
 * displayed. The default for these are a simple expression such
 * as @b foo.mac, @b "*.mac" or @b "*.*". Use a @b '?' to
 * indicate a single character. I.e. @b "??.*" means show all
 * files with a two character filename and any file type. If the
 * @b --rexp or
 * @b
 * -R option is selected, then the name is interpreted as a
 *  regular expression. Those are described in
 * @b man @b 7 @b regex or @b man @b grep. I.e. to
 * display only .MAC filenames, use
 * @b "mac$" instead of the more typical @b "*.mac". The case of
 * the names used in the filters does not matter (upper or
 * lowercase will work equally well). If the filename expression
 * includes shell specific characters, you will need to escape
 * them from the shell.
 * 
 * 
 * @subsection in
 * Optional cmdOpts available for @b in command: @n Need to
 * write this. @n
 * @subsection out
 * Optional cmdOpts available for @b out command: @n Need to
 * write this. @n
 * @subsection sqz
 * Optional cmdOpts available for @b sqz command: @n Need to
 * write this. @n
 * @subsection del 
 * Optional cmdOpts available for @b del (or @b rm) command: @n
 * Need to write this. @n
 * @subsection new 
 * Optional cmdOpts available for @b new command: @n Need to
 * write this. @n
 * @section exam Examples
 * @b rtpip @b -F @b rt11_dy0.dsk @b ls @b -sn @b -6 @b "*.mac"
 *   @b "*.sys" @b "rt*.*" @b "??.*" @n
 * @b rtpip @b -F @b rt11_dy0.dsk @b ls @b -Rsn @b -6 @b "mac$"
 *   @b "sys$" @b "^rt" @b "..\.*"
 * @n @n Either will display using 6 columns horizontally all
 * the files contained in the floppy disk image container file,
 * rt11_dy0.dsk, whose names end in .mac, .sys, start with RT or
 * have only 2 characters in their names before the file
 * extension.
 * 
 * 
 * @author Dave Shepperd
 * @version 1.0 Oct 23, 2008 14:41
 * 
 */

#include "rtpip.h"

/**
 * Add some arguments to the command line args.
 * @param filename - pointer to null terminated ascii filename.
 * @param args - pointer to argument list.
 * @return 0
 */
static int fakeArgv(const char *filename, Fakeargs_t *args)
{
	args->new_argc = args->orig_argc;
	args->new_argv = args->orig_argv;
	args->bpool = NULL;
	args->new_argv_max = args->orig_argc;
	return 0;
}

/**
 * Display help for ls command.
 */
static int help_ls(void)
{
	printf("rtpip [-?] container ls [-afrvh?] [-c N] [-1..9] [-sX] [Filters ...]\n"
		   "ls or dir command: Get directory listing of files in container.\n"
		   "--help or -h or -? = This message.\n"
		   "--cols=N or -cN or -N = 'N' is a number 1 through 9 indicating columns to output.\n"
		   "--full or -f = Output full directory information (only if column is not specifed).\n"
		   "--all or -a = Output all directory information (only if column is not specified).\n"
		   "--sort=n or -sn = Sort by filename.\n"
		   "--sort=t or -st = Sort by filetype.\n"
		   "--sort=d or -sd = Sort by date.\n"
		   "--sort=s or -ss = Sort by size.\n"
		   "--reverse or -r = Reverse sort.\n"
#if !NO_REGEXP
		   "--rexp or -R = filenames are regular expressions.\n"
#endif
		   "--verbose or -v = Set verbose mode.\n"
		   "Filters = zero or more filter strings.\n"
#if !NO_REGEXP
		   "If -R or --rexp then the strings are regular expressions. Either are used as\""
		   "filters as to what to display.\n"
		   "NOTE: the regular expressions are defined in \"man 7 regex\" or \"man grep\".\n"
		   "If the regular expression includes shell specific characters, they will\n"
		   "need to be escaped. I.e. to display only .MAC filenames, use \"mac$\" instead\n"
		   "of the more typical \"*.mac\".\n"
#endif
		   "The case of the names used in the filters does not matter (upper or lowercase will work equally well).\n"
		  );
	return 1;
}

/**
 * Display help for out command.
 */
static int help_out(void)
{
	printf("rtpip [opts] container out [-abh?lnv] file [file...]\n"
		   "out command: Copy file(s) out of the container.\n"
		   "--help or -h or -? = This message.\n"
		   "--ascii or -a = Change crlf to just lf. Write until control Z. Doesn't write control Z.\n"
		   "--binary or -b = Write file as image (default).\n"
		   "--ctlz or -z = Write output until control Z found otherwise leave as binary. Doesn't write control Z.\n"
		   "--outdir=X or -o X = set default output directory to X\n"
		   "--lower or -l = Change filename to lowercase.\n"
#if !NO_REGEXP
		   "--rexp or -R = Filenames are regular expressions.\n"
#endif
		   "--time or -t = maintain file timestamps\n"
		   "--assumeyes or -y = Assume YES instead of prompting.\n"
		   "--verbose or -v = Sets verbose mode.\n"
		  );
	printf("file = one or more name to select the file(s) to copy out.\n"
#if !NO_REGEXP
		   "If the -R or --rexp option is provided, then the name(s) are interpreted as\n"
		   "regular expressions as defined in \"man 7 regex\" or \"man grep\".\n"
		   "I.e. with regular expressions, to select only .MAC filenames, use \"mac$\" instead\n"
		   "of the more typical \"*.mac\". If the regular expression includes shell specific\n"
		   "characters, you will need to escape them from the shell.\n"
#endif
		   "The case of the names specified does not matter (upper or lowercase will work equally well).\n"
		  );
	return 1;
}

/**
 * Display help for out command.
 */
static int help_in(void)
{
	printf("rtpip [opts] container in [-abh?qRtvz][d xx] file [file...]\n"
		   "in command: Copy file(s) into the container.\n"
		   "--help or -h or -? = This message.\n"
		   "--ascii or -a = Change lone lf's to crlf's while copying.\n"
		   "--assumeyes or -y = Assume YES instead of prompting.\n"
		   "--binary or -b = Write file as image (default).\n"
		   "--date=xx or -d xx = Set rt11 date for files. dd-mmm-yy where 72<=yy<=99.\n"
		   "--query or -q = Prompt before copying each file.\n"
#if !NO_REGEXP
		   "--rexp or -R = Filenames are regular expressions.\n"
#endif
		   "--time or -t = maintain file timestamps\n"
		   "--verbose or -v = Sets verbose mode.\n"
		   "file = one or more input files to copy.\n"
		  );
	return 1;
}

/**
 * Display help for out command.
 */
static int help_sqz(void)
{
	printf("rtpip [opts] container sqz [-h?s]\n"
		   " sqz command: Consolidate all container empty space to one contigious space.\n"
		   "--help or -h or -? = This message.\n"
		   "--assumeyes or -y = Assume YES instead of prompting.\n"
		   "--segment=n or -s n = Sets the number of segments in the new container file. 1<=n<=31.\n"
		   "--verbose or -v = Sets verbose mode.\n"
		  );
	return 1;
}

/**
 * Display help for out command.
 */
static int help_new(void)
{
	printf("rtpip [opts] container new [-h?v] -b N -s N\n"
		   "new command: Create a new empty container file.\n"
		   "--help or -h or -? = This message.\n"
		   "--assumeyes or -y = Assume YES instead of prompting.\n"
		   "--blocks=N or -b N = Sets number of (512 byte) blocks in new container file. Must be 400<=N<=65535\n"
		   "--segment=N or -s N = Sets the number of segments in the new container file. 1<=n<=31.\n"
		   "--verbose or -v = Sets verbose mode.\n"
		  );
	return 1;
}

/**
 * Display help for global options.
 * @param msg - pointer to string to display as prefix (NULL if none).
 * @return 1
 */
static int help_em(const char *msg)
{
	if ( msg )
	{
		printf("%s\n", msg);
	}
	printf("Usage: rtpip [-dfFh?v][-l N] container cmd [cmdOpts] [file...]\n"
		   "where:\n"
		   " -d or --debug = set debug mode\n"
		   " -f or --floppy = image is of a floppy disk\n"
		   " -F or --double = image is of a double density floppy disk\n"
		   " -h, -? or --help = This message.\n"
		   " -lN or --lba=N = set starting LBA to 'N' (defaults to 6)\n"
		   " -v or --verbose = set verbose mode\n"
		   " container - path to existing RT11 container file.\n"
		   " cmd - one of 'del', 'dir', 'in', 'ls', 'new', 'out', 'rm' or 'sqz'.\n"
		   " [cmdOpts] = optional options for specific command\n"
		   " [file...] = optional input or output filename expressions\n\n"
		   "For help on a specific cmd, use 'rtpip anything cmd -h'\n"
		  );
	return 1;
}

/**
 * Program main entry point.
 * @param argc - number of command line arguments.
 * @param argv - pointer to array of command line arguments.
 * @return 0 on success, non-zero on failure.
 */
int main(int argc, char *const *argv)
{
	int ii;
	static Fakeargs_t fargs;
	Options_t options;

	fargs.orig_argc = argc;
	fargs.orig_argv = argv;
	fakeArgv(".dmprt", &fargs);
	memset(&options, 0, sizeof(options));
	options.seg1LBA = DIRBLK;

	if ( (ii = getcmds(&options, argc, argv)) || !options.todo || (options.todo & TODO_HELP) )
	{
		if ( (options.cmdOpts & CMDOPT_DBG_NORMAL) || options.verbose )
			printf("main(): getcmds() returned %d, options.todo=%d\n", ii, options.todo);
		if ( !options.todo || options.todo == TODO_HELP )
			return help_em(NULL);
		return 1;
	}
	if ( (options.lsOpts & LSOPTS_HELP) )
	{
		return help_ls();
	}
	if ( (options.outOpts & OUTOPTS_HELP) )
	{
		return help_out();
	}
	if ( (options.inOpts & INOPTS_HELP) )
	{
		return help_in();
	}
	if ( (options.sqzOpts & SQZOPTS_HELP) )
	{
		return help_sqz();
	}
	if ( (options.newOpts & NEWOPTS_HELP) )
	{
		return help_new();
	}
	if ( (options.cmdOpts&CMDOPT_DBG_NORMAL) && !options.verbose )
		++options.verbose;
	if ( !(options.todo & TODO_NEW) )
	{
		int sts, bufLen;

		/* As I understand it, RT11 won't have more than 32 directory segments
		 * And in all the cases I've run into, the segments start at the default block 6.
		 * So the cheapskate way to do this is to just read the maximum number of blocks
		 * that will include the home block and all the potential directory blocks. It may
		 * be more than we need, but so what?
		 */
		options.inp = fopen(options.container, "rb");
		if ( !options.inp )
		{
			fprintf(stderr, "Unable to open input file '%s': %s\n",
					options.container, strerror(errno));
			return 1;
		}
		sts = fseek(options.inp, HOME_BLK_LBA * BLKSIZ, SEEK_SET);
		if ( sts < 0 || ferror(options.inp) || (ftell(options.inp) != HOME_BLK_LBA*BLKSIZ) )
		{
			fprintf(stderr, "Error seeking conainer to home block. Wanted %d: %s\n",
					HOME_BLK_LBA * BLKSIZ,
					strerror(errno));
			return 1;
		}
		bufLen = fread(&options.homeBlk, 1, BLKSIZ, options.inp);
		if ( bufLen != BLKSIZ )
		{
			fprintf(stderr, "Error reading home block 0. Expected %d bytes, got %d. %s\n",
					BLKSIZ, bufLen, strerror(errno));
			return 1;
		}
		if ( checkHeader(&options) )
		{
			return 1;
		}
		if ( !parse_directory(&options) )
		{
			sts = 0;
			if ( (options.todo & TODO_LIST) )
			{
				sts = do_directory(&options);
			}
			else if ( (options.todo & TODO_OUT) )
			{
				sts = do_out(&options);
			}
			else if ( (options.todo & TODO_INP) )
			{
				sts = do_in(&options);
			}
			else if ( (options.todo & TODO_SQZ) )
			{
				sts = do_sqz(&options);
			}
			else if ( (options.todo & TODO_NEW) )
			{
				sts = do_new(&options);
			}
			else if ( (options.todo & TODO_DEL) )
			{
				sts = do_del(&options);
			}
			if ( !sts && options.dirDirty )
			{
				writeNewDir(&options);
			}
		}
	}
	else
	{
		do_new(&options);
	}
#if !NO_REGEXP
	if ( options.rexts )
	{
		for ( ii = 0; ii < options.numArgFiles; ++ii )
			regfree(options.rexts + ii);
		free(options.rexts);
		options.rexts = NULL;
	}
#endif
	if ( options.normExprs )
	{
		free(options.normExprs);
		options.normExprs = NULL;
	}
	if ( options.inp )
	{
		fclose(options.inp);
		options.inp = NULL;
	}
	if ( options.floppyImage )
	{
		free(options.floppyImage);
		options.floppyImage = NULL;
		options.floppyImageUnscrambled = NULL;
		options.floppyImageSize = 0;
		options.directory = NULL;
		options.directorySize = 0;
	}
	if ( options.directory )
	{
		free(options.directory);
		options.directory = NULL;
		options.directorySize = 0;
	}
	if ( options.wDirArray )
	{
		free(options.wDirArray);
		options.wDirArray = NULL;
	}
	return 0;
}
