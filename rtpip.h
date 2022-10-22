/** $Id: rtpip.h,v 1.9 2022/07/29 03:02:44 dave Exp dave $
 
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
 *  @file rtpip.h
 *  Describes structures of the @b rtpip program
 */

#ifndef _RTPIP_H_
	#define _RTPIP_H_ 1

	#define _ISOC99_SOURCE

	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <errno.h>
	#include <ctype.h>
	#include <unistd.h>
	#include <getopt.h>
	#include <regex.h>
	#include <errno.h>
	#include <sys/stat.h>
	#include <time.h>
	#include <utime.h>

/*
 * Module definitions
 */

	#define DIRBLK	6		    /**< first block of rt-11 directory */
	#define MAXSEGMENTS 32      /**< Maximum number of directory segments */
	#define MAX_SGL_FLPY_SEGS 2 /**< Maximum number of directory segments on single density floppy */
	#define MAX_DBL_FLPY_SEGS 4 /**< Maximum number of directory segments on double density floppy */
	#define BLKSIZ  512         /**< Bytes per disk block           */
	#define BLKS_P_SEGMENT (2)  /**< Blocks per segment */
	#define SEGSIZ	(BLKSIZ*BLKS_P_SEGMENT) 	/**< directory segment size (bytes) */
	#define HOME_BLK_LBA (1)    /**< Disk LBA where home block can be found */

typedef unsigned char U8;       /* Some useful types */
typedef char S8;
typedef unsigned short U16;
typedef short S16;

/** Defines the RT11 Home block 
  */
typedef struct
{
	U8   badBlocks[130];    /* 0000-0201 */
	U16  spare0;            /* 0202-0203 */
	U8   initData[22+16];   /* 0204-0251 */
	U8   bup[18];           /* 0252-0273 */
	U8   filler1[260];      /* 0274-0677 */
	U16  reserve0;          /* 0700-0701 */
	U16  reserve1;          /* 0702-0703 */
	U8   filler2[14];       /* 0704-0721 */
	U16  clusterSize;       /* 0722-0723 */
	U16  firstSegment;      /* 0724-0725 */
	U16  version;           /* 0726-0727 */
	char volumeID[12];      /* 0730-0743 */
	char owner[12];         /* 0744-0757 */
	char sysID[12];         /* 0760-0773 */
	U16  filler3;           /* 0774-0775 */
	U16  checksum;          /* 0776-0777 */
} Rt11HomeBlock_t;

/**  Defines the RT11 segment structure.
 * 
 * The RT11 directory structure consists of 'n' 1024 byte segments contigiously placed on the drive
 * starting at a default sector (LBA) of 6. An Rt11SegEnt_t 
 * header is present at the head of each segment followed by 'm'
 * Rt11DirEnt_t structs where 'm' is decided by how many 
 * Rt11DirEnt_t's plus any 'extra' bytes will fit in the space 
 * remaining of the 1024 bytes after the segment header. The 
 * smax, last and extra members are specified to be maintained 
 * only in the first directory segment. Subsequent segments may 
 * or may not hold accurate information in those members. 
 */
typedef struct
{
	unsigned short smax;    /**< Number of segments resident on disk  */
	unsigned short link;    /**< link to next segment (relative to 1) */
	unsigned short last;    /**< last segment used (relative to 1; only accurate in first segment) */
	unsigned short extra;   /**< extra bytes in directory entry */
	unsigned short start;   /**< starting LBA of this segment */
} Rt11SegEnt_t;
	#define SEGLEN sizeof(Rt11SegEnt_t)

/** Defines the RT11 directory entry
 * 
 * An Rt11SegEnt_t header is present at the head of each segment followed by 'm' Rt11DirEnt_t structs
 * where 'm' is decided by how many Rt11DirEnt_t's plus any 'extra' bytes will fit in the space
 * remaining of the 1024 bytes after the segment header.
 */
typedef struct
{
	unsigned short control;     /**< Control bits */
#define PROTEK	0100000		    /**< protected file */
#define ENDBLK	0004000		    /**< end of segment marker */
#define PERM	0002000		    /**< permanent file */
#define EMPTY	0001000		    /**< empty entry    */
#define TENT	0000400		    /**< tentative file */
	unsigned short name[3];     /**< Rad50 Filename and type */
	unsigned short blocks;      /**< blocks assigned to file */
	unsigned char  channel;     /**< channel number */
	unsigned char  procid;      /**< process ID     */
	unsigned short date;        /**< creation date  */
} Rt11DirEnt_t;
	#define DIRLEN sizeof(Rt11DirEnt_t)

/** Cheater to allow me to add command line options.
 */

typedef struct
{
	int orig_argc;
	char *const *orig_argv;
	int new_argc;
	char *const *new_argv;
	int new_argv_max;
	char *bpool;
} Fakeargs_t;

/** Defines the states the command line parser can be in.
 */
typedef enum {
	CMDSTATE_CONT,      /**< Looking for container filename */
	CMDSTATE_CMD,       /**< Looking for command */
	CMDSTATE_LS,        /**< Parsing ls command options */
	CMDSTATE_IN,        /**< Parsing in command options */segnum,
	CMDSTATE_OUT,       /**< Parsing out command options */
	CMDSTATE_SQZ,       /**< Parsing squeeze command options */
	CMDSTATE_DEL,       /**< Parsing del command options */
	CMDSTATE_NEW        /**< Parsing new command options */
} CmdState_t;

	#if 0
/** Defines internal working directory entry.
 * 
 * The internals of rtpip just access a linear array of pointers representing the
 * on disk directory structure. This defines the layout of the internal directory.
 */
typedef struct
{
	Rt11SegEnt_t *segptr;       /**< Pointer to segment in which directory lives */
	Rt11DirEnt_t *dirptr;       /**< Pointer to directory entry */
	int startBlock;             /**< Starting block number of this file */
}
WorkingDir_t;
	#endif

/** Defines internal working directory entry for the input function.
 * 
 * The internals of rtpip just access a linear array of pointers representing the
 * on disk directory structure. This defines the layout of the internal directory for IN command.
 */
typedef struct
{
	Rt11DirEnt_t rt11;      /**< As recorded in container file */
	char ffull[6+1+3+1];    /**< Filename converted to null terminated ASCII (includes '.') */
	int lba;                /**< Logical block (index to starting 512 byte block on disk) */
	U8 segNo;               /**< Directory segment entry found in */
	U8 segIdx;              /**< Index into directory segment where entry found */
} InWorkingDir_t;

	#if 0
/** Defines array useful for sorting.
 * 
 * The internals of rtpip just access a linear array of pointers representing the
 * on disk directory structure. This defines the layout of the internal directory.
 */
typedef struct
{
	InWorkingDir_t *wdp; /**< Pointer to directory entry */
}
LinearDir_t;
	#endif

typedef struct
{
	int totIns;
	int totUsed;
	InWorkingDir_t *sizeMatch;
	char *inFileBuf;
	int inFileBufSize;
/*	char *outFileBuf; */
/*	int outFileBufSize; */
	int fileBlks;
	char *argFN;
	int argFNLen;
	unsigned short iNameR50[3];
	time_t fileTimeStamp;
} InHandle_t;

/** Defines the command options and other interfaces between internal functions.
 */
typedef struct
{
	Rt11HomeBlock_t homeBlk;        /**< Home block is read into this */
	U8 *floppyImage;                /**< Floppy disk image (scrambled) */
	int floppyImageSize;            /**< number of bytes in floppy image */
	U8 *floppyImageUnscrambled;     /**< Floppy disk image (un-scrambled) */
	U8 *directory;                  /**< Pointer to player in wholeHeader where the RT11 directory can be found */
	int directorySize;              /**< Size of directory buffer in bytes */
	InWorkingDir_t *wDirArray;      /**< Pointer to internal representation of directory */
	InWorkingDir_t **linArray;      /**< Pointer to array of pointers used for sorting */
	int numWdirs;                   /**< Number of items in wDirArray and linArray */
	int totEmpty;                   /**< Total empty blocks */
	int totEmptyEntries;            /**< Total empty entries in all segments */
	int emptyAdds;					/**< Number of blocks added due to container size differences */
	int totPerm;                    /**< Total non-empty blocks (less boot+directory) */
	int totPermEntries;             /**< Total perm entries in all segments */
	int largestPerm;                /**< Largest file found in list */
	InWorkingDir_t *lastEmpty;      /**< Pointer to last empty entry in last segment */
	int diskSize;                   /**< Total blocks available on volume */
	int dirDirty;                   /**< Directory is dirty */
	InHandle_t iHandle;             /**< Places for in cmd arguments */
	FILE *inp;                      /**< Pointer to containter file */
	int openedWrite;                /**< Input file (re)opened for writing */
	unsigned long seg1LBA;          /**< LBA of segment 1 of directory */
	const char *container;          /**< Pointer to containter filename (from command line) */
	int containerSize;              /**< Size of entire container file in bytes */
	int containerBlocks;			/**< Size of entire container file in blocks */
	const char *cmd;                /**< Pointer to cmd (from command line) */
	char *const *argFiles;         /**< Pointer to arry of files (from command line) */
	int numArgFiles;                /**< Number of filenames in argFiles */
	regex_t *rexts;                 /**< Pointer to regex compiles */
	char *normExprs;                /**< Pointer to array of filename strings each 10 chars in length (6+3+null) */
	CmdState_t cmdState;            /**< Current state of command line parser */
	int segnum;                     /**< Number of segments used in RT11 directory */
	int maxseg;                     /**< Number of segments available in RT11 directory */
	int numdent;                    /**< Number of directory entries in a segment */
	int dirEntrySize;               /**< Size in bytes of each directory entry */
	int cmdOpts;                    /**< Command line options */
#define CMDOPT_DBG_NORMAL  (0x01)   /**< Debug normal mode */
/* #define CMDOPT_DBG_EMPTY   (0x02)   **< Debug but don't consolidate empty spaces * */
#define CMDOPT_SINGLE_FLPY (0x04)   /**< Container is single density floppy disk. */
#define CMDOPT_DOUBLE_FLPY (0x08)   /**< Container is double density floppy disk. */
#define CMDOPT_NOWRITE     (0x10)   /**< Do not write anything. Just say what would do. */
	int verbose;                    /**< verbose mode (set via command line) */
	int columns;                    /**< output columns for ls cmd (set via command line) */
	int fileOpts;
#define FILEOPTS_REGEXP    (1)      /**< File list is regular expression */
#define FILEOPTS_TIMESTAMP (2)      /**< File list is regular expression */
	int sortby;                     /**< sort options (set via command line) */
#define SORTBY_NAME (1)             /**< Sort by name */
#define SORTBY_TYPE (2)             /**< Sort by filetype */
#define SORTBY_DATE (4)             /**< Sort by date */
#define SORTBY_SIZE (8)             /**< Sort by file size */
#define SORTBY_REV  (16)            /**< Sort descending order */
	int lsOpts;                     /**< Holds ls cmd options */
#define LSOPTS_HELP (1)             /**< Show help for ls command */
#define LSOPTS_ALL  (2)             /**< Show all details in ls */
#define LSOPTS_FULL (4)             /**< Show full details in ls */
	int outOpts;                    /**< Holds out cmd options */
#define OUTOPTS_HELP (1)            /**< Help mode */
#define OUTOPTS_VERB (2)            /**< Verbose */
#define OUTOPTS_NOASK (4)           /**< No prompts */
#define OUTOPTS_ASC  (8)            /**< Convert crlf to lf, strip ^Z at eof */
/* #define OUTOPTS_CTLZ (16)           **< Copy to ^Z but otherwise leave as binary */
#define OUTOPTS_OVR  (32)           /**< Overwrite existing output files */
#define OUTOPTS_LC   (64)           /**< Change filename to lowercase */
	int inOpts;                     /**< Holds out cmd options */
#define INOPTS_HELP (1)             /**< Help mode */
#define INOPTS_VERB (2)             /**< Verbose */
#define INOPTS_NOASK (4)            /**< No prompts */
#define INOPTS_ASC  (8)             /**< Ascii file: Convert lf to crlf */
#define INOPTS_OVR  (16)            /**< Overwrite existing output files */
/* #define INOPTS_CTLZ (32)            **< Add Control-Z to end of ascii file */
	unsigned short inDate;          /**< Date to use while copying in files */
	const char *outDir;             /**< output directory */
	int delOpts;
#define DELOPTS_HELP (1)            /**< Help mode */
#define DELOPTS_VERB (2)            /**< Verbose */
#define DELOPTS_NOASK (4)           /**< No prompts */
#define DELOPTS_REGEXP (8)          /**< File list is regular expression */
	int sqzOpts;
#define SQZOPTS_HELP (1)            /**< Help mode */
#define SQZOPTS_VERB (2)            /**< Verbose */
#define SQZOPTS_NOASK (4)           /**< No prompt */
	int newOpts;
#define NEWOPTS_HELP (1)            /**< Help mode */
#define NEWOPTS_VERB (2)            /**< Verbose */
#define NEWOPTS_NOASK (4)           /**< No prompts */
	int newMaxSeg;                  /**< New number of segments to use during a new */
	int newDiskSize;                /**< Size of new container file */
	int todo;                       /**< Command to execute */
#define TODO_LIST (1)               /**< Directory listing */
#define TODO_INP  (2)               /**< Copy files into container */
#define TODO_OUT  (4)               /**< Copy files out of container */
#define TODO_HELP (8)               /**< Show help message */
#define TODO_SQZ  (16)              /**< Squeeze empty space */
#define TODO_DEL  (32)              /**< Delete file(s) */
#define TODO_NEW  (64)              /**< New container file */
} Options_t;

/* Defines for floppy diskette support functions */
	#define NUM_SECTORS     26  /* Sectors per track on floppy */
	#define NUM_TRACKS      77  /* Number of tracks per diskette */

/* Functions found in getcmd.c */

/** getcmds - get command line options.
 * @param options - pointer to options.
 * @param argc - count of command arguments.
 * @param argv - pointer to array of command line arguments.
 * 
 * @return - 0 if success. 1 if failure.
 */
extern int getcmds(Options_t *options, int argc, char *const *argv);

/* Functions found in rtutils.c */

	#define R50_DOLLAR  (27)
	#define R50_DOT     (28)
	#define R50_PERCENT (29)

/**
 * char2r50 - Convert a single ascii character to a rad50
 * integer.
 * @param src - ascii character
 * @return - character converted to rad50. Unconvertable characters
 *           are changed to rad50 space (0).
 */
extern int char2r50(char src);

/**
 * fromRad50 - Convert 3 rad50 chars to ascii.
 * @param ans - Pointer to 4 byte array into which ascii is deposited.
 * @param src - rad50 input
 * @return nothing
 */
extern void fromRad50(char ans[4], unsigned short src);

/**
 * sqzSpaces - Squeeze out spaces from string.
 * @param ptr - pointer to null terminated ASCII string.
 * @return nothing.
 * Spaces will have been squeezed out of string.
 */
extern void sqzSpaces(char *ptr);

	#define YN_YES  (0)
	#define YN_NO   (1)
	#define YN_QUIT (2)

/**
 * getYN - Get a Y/N/Q response to a query
 * @param msg - prompt message.
 * @param def - 0=default yes, 1=default no, 2=default quit
 * @return 0 if yes, 1 if no, 2 if quit.
 */
extern int getYN(const char *prompt, int def);

/**
 * dateStr - convert RT11 date to ASCII
 * @param outStr - pointer to output string
 * @param date - RT11 date
 * @return pointer to output string
 */
	#define INSTR_LEN (12)
extern char* dateStr(char outStr[INSTR_LEN], unsigned short date);

extern int mkOFBuf(InHandle_t *ihp, int *need);

extern int cvtName(Options_t *options, const char *fileName);

/* Functions found in floppy.c */

/** descramble - Rearranges the diskette container file
 *  contents.
 *  @param options - pointer to options data.
 *  @return 0 on success. Contents pointed to by floppyImage
 *          have been rearranged into buffer pointed to by
 *          floppyImageUnscrambled.
 **/
extern int descramble(Options_t *options);

/** rescramble - Scrambles the file contents in logical
 *  order into diskette format.
 *  @param options - pointer to options data.
 *  @return 0 on success. Contents pointed to by
 *          floppyImageUnscrambled have been rearranged into
 *          buffer pointed to by floppyImage.
 **/
extern int rescramble(Options_t *options, U8 *optionalInput);

/* Functions found in parse.c */

/** checkHeader - read and verify RT11 disk image header.
 *  @param options - pointer to options
 *  @return 0 on success, non-zero on failure. Error messages
 *          will have been displayed as appropriate.
 **/
extern int checkHeader(Options_t *options);

/**
 * parse_directory - Parse the RT11 directory making it linear.
 * @param options - pointer to working area.
 * @return 0 if success, 1 if failure.
 * 
 * @note The disk image of the directory is pointed to by 
 * options->directory. This function walks through the disk 
 * image and forms a pair of linear arrays. One is a copy of 
 * this disk image with all the ENDBLKs removed and files listed
 * in LBA order without regard to which directory segment in 
 * which they were present. The other array is a list of 
 * pointers into the first array along with the file's actual 
 * LBA. It is this second array that can be sorted by the 
 * various sort options (sorts just by shuffling pointers). 
 */
extern int parse_directory(Options_t *options);

/**
 * linearToDisk - Unpack linear directory back to disk image
 * format.
 * @param options - pointer to working area.
 * @return 0 if success, 1 if failure.
 */
extern int linearToDisk(Options_t *options);

/* Functions found in sort.c */

extern int (*cmpFuncs[8])(const void *a1, const void *a2);

/**
 * Compare filename against a filter.
 * @param filter - pointer to filter filename.
 * @param name - pointer to null terminated filename to check.
 * @return 0 if match, 1 if no match
 */
extern int normexec(const char *filter, const char *name);

/**
 * Filter filenames based on input from command line.
 * @param options - pointer to options
 * @param name - pointer to null terminated filename to check.
 * @return 1 if to handle file; 0 if to ignore file.
 */
extern int filterFilename(Options_t *options, const char *name);

/* Functions found in do_dir.c */

/**
 * do_dir - Display RT11 directory.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
extern int do_directory(Options_t *options);

/* Functions found in do_out.c */

/**
 * do_out - Copy an RT11 file out of container.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
extern int do_out(Options_t *options);

/* Functions found in do_del.c */

/**
 * do_del - Delete RT11 files from container.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
extern int do_del(Options_t *options);

/**
 * preDelete - delete a file from container.
 * @param options - pointer to options.
 * @return 0 on success, 1 on failure
 */
extern int preDelete(Options_t *options);

/* Functions found in input.c */

/**
 * Read input file and do any crlf processing.
 * @param options - pointer to options.
 * @return 0 if success, 1 if failure
 */
extern int readInpFile(Options_t *options, const char *fileName);

/* Functions found in do_in.c */

/**
 * do_in - Copy a file into RT11 container.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
extern int do_in(Options_t *options);

/**
 * Write a file into container
 * @param options - pointer to options
 * @param wdp - pointer to directory entry
 * @return 0 on success, 1 on error
 */
extern int writeFileToContainer(Options_t *options, InWorkingDir_t *wdp);

/* functions found in output.c */

/**
 * Create a new container file squeezing out all the empty space.
 * @param options - pointer to options
 * @return 0 if success; 1 if failure
 */
extern int createNewContainer(Options_t *options);

/**
 * Write an updated directory to disk over the top of the existing container file.
 * @param options - pointer to options
 * @return 0 if success; 1 if failure
 */
extern int writeNewDir(Options_t *options);

/**
 * Compress container squeezing all empty space into one place.
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
extern int do_sqz(Options_t *options);

/**
 * Create an empty container. 
 * @param options - pointer to options.
 * @return 0 if success; 1 if failure.
 */
extern int do_new(Options_t *options);

#endif  /* _RTPIP_H_ */

