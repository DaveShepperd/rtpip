/*  $Id: utils.c,v 1.2 2022/07/29 02:09:54 dave Exp dave $ $

	rtutils.c - Misc utilties used by rtpip
	
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
 * @file rtutils.c
 * Misc utilities used by rtpip.
 */

/**
 * RT11 file dates are stored in aun signed short as:
 * (month<<10) | (day<<5) | ((year-72)&31)
 *  where month is 1-12 for Jan-Dec
 *       day is 1-31
 *       year is tens and units digits of year. It is not Y2K
 *       complient; wraps at 1999 back to base year 1972).
 *  However, there is another option specified, but it appears
 *  RT11 v5.3 doesn't support it. The top two bits of the date
 *  can specify the base year:
 *  0 - 1972
 *  1 - 2004
 *  2 - 2036
 *  3 - 2068
 *  
 */

static const char *const months[] = {
	"NUL",
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec",
	"?C?",
	"?D?",
	"?E?",
	"?F?"
};

/* Radix50 chars pack 3 ASCII characters to an unsigned 16 bit number. */

static const char r50[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ$.%0123456789????????????????????????";

/**
 * Convert a single ascii character to a rad50 integer.
 * @param src - ascii character
 * 
 * @return - character converted to rad50. Unconvertable characters
 *           are changed to rad50 space (0).
 */

int char2r50(char src)
{
	int tmp;
	if ( src != ' ' )
	{
		if ( src == '$' )
		{
			return R50_DOLLAR;     /* 27 */
		}
		if ( src == '.' )
		{
			return R50_DOT;        /* 28 */
		}
		if ( src == '%' )
		{
			return R50_PERCENT;     /* 29 */
		}
		if ( src >= '0' && src <= '9' )
		{
			return src - '0' + 30;    /* 30-39 */
		}
		tmp = toupper(src);
		if ( tmp >= 'A' && tmp <= 'Z' )
		{
			return tmp - 'A' + 1;       /* 1-26 */
		}
	}
	return 0;                       /* 0 */
}

/**
 * Convert 3 rad50 chars to ascii.
 * @param ans - Pointer to 4 byte array into which ascii is deposited.
 * @param src - rad50 input
 * @return nothing
 */
void fromRad50(char ans[4], unsigned short src)
{
	int ii;
	ii = src / (050 * 050);         /* modulo 40*40 goes into byte 0 */
	ans[0] = r50[ii & 63];
	src -= ii * 050 * 050;
	ii = src / 050;               /* modulo 40 goes into byte 1 */
	ans[1] = r50[ii & 63];
	src -= ii * 050;
	ans[2] = r50[src & 63];       /* LSBs goes into 2 */
	ans[3] = 0;                 /* null terminate the string */
}

/**
 * Squeeze out spaces from converted string.
 * @param ptr - pointer to null terminated ascii string.
 * @return nothing.
 * Spaces will have been squeezed out of string.
 */
void sqzSpaces(char *ptr)
{
	char *dst = ptr;
	for (; *ptr; ++ptr )
	{
		if ( *ptr != ' ' )
		{
			if ( dst == ptr )
				++dst;
			else
				*dst++ = *ptr;
		}
	}
	*dst = 0;
}

/**
 * Get a Y/N/Q response to a query
 * @param msg - prompt message.
 * @param def - 0=default yes, 1=default no, 2=default quit
 * @return 0 if yes, 1 if no, 2 if quit.
 */
int getYN(const char *prompt, int def)
{
	char yn[10];
	const char *defmsg;

	if ( prompt )
	{
		fputs(prompt, stdout);
	}
	if ( def == YN_YES )
	{
		defmsg = " [Y]/N/Q: ";
	}
	else if ( def == YN_NO )
	{
		defmsg = " Y/[N]/Q: ";
	}
	else
	{
		defmsg = " Y/N/[Q]: ";
	}
	fputs(defmsg, stdout);
	fflush(stdout);
	yn[0] = 0;
	fgets(yn, sizeof(yn), stdin);
	if ( !yn[0] || yn[0] == '\n' )
		return def;
	if ( yn[0] == 'Y' || yn[0] == 'y' )
		return YN_YES;
	if ( yn[0] == 'Q' || yn[0] == 'q' )
		return YN_QUIT;
	return YN_NO;
}

char* dateStr(char outStr[INSTR_LEN], unsigned short date)
{
	int yr = (date & 31), decade = (date >> 14) & 3;
	switch (decade)
	{
	case 0:
		yr += 1972;
		break;
	case 1:
		yr += 2004;
		break;
	case 2:
		yr += 2036;
		break;
	case 3:
		yr += 2068;
		break;
	}
	snprintf(outStr, INSTR_LEN, "%2d-%3s-%4d",
			 (date >> 5) & 31,
			 months[((date >> 10) & 15)],
			 yr);
	return outStr;
}

#if 0
/** mkOFBuf - create or expand output buffer 
 *  @param ihp - pointer to input details
 *  @param need - pointer to size of output needed
 *  @return 0 on success, 1 on error
 **/
int mkOFBuf(InHandle_t *ihp, int *need)
{
	if ( !ihp->outFileBuf || *need > ihp->outFileBufSize )
	{
		ihp->outFileBuf = (char *)realloc(ihp->outFileBuf, *need);
		if ( !ihp->outFileBuf )
		{
			fprintf(stderr, "Error mallocing %d buffer for input file '%s'.\n",
					ihp->outFileBufSize, ihp->argFN);
			return 1;
		}
		ihp->outFileBufSize = *need;
		*need = ((ihp->outFileBufSize + (ihp->outFileBufSize + 7) / 8) + BLKSIZ - 1) & -BLKSIZ;
	}
	return 0;
}
#endif

int cvtName(Options_t *options, const char *fileName)
{
	char *cp;
	const char *ccp;
	int retv, cc, r50;

	retv = strlen(fileName);
	if ( retv > options->iHandle.argFNLen )
	{
		options->iHandle.argFN = (char *)malloc(retv + 1);
		if ( !options->iHandle.argFN )
		{
			fprintf(stderr, "Unable to allocate %d bytes for filename '%s': %s\n",
					retv, fileName, strerror(errno));
			return 1;
		}
		options->iHandle.argFNLen = retv;
	}

	/* First trim off just the filename and convert it to uppercase. */
	ccp = strrchr(fileName, '/');
	if ( !ccp )
		ccp = fileName;
	else
		++ccp;

	strcpy(options->iHandle.argFN, ccp);
	cp = options->iHandle.argFN;
	retv = 0;
	options->iHandle.iNameR50[0] = 0;
	options->iHandle.iNameR50[1] = 0;
	options->iHandle.iNameR50[2] = 0;
	for ( retv = 0; (cc = *cp); ++cp )
	{
		/* If lowercase, make it upper */
		if ( tolower(cc) )
			cc = *cp = toupper(cc);
		/* convert character to RAD50 */
		r50 = char2r50(cc);
		/* If it returns as r50 space, it's illegal */
		if ( !r50 )
			break;
		/* if it's a dot, it is the filename/filetype separator. It doesn't get included */
		if ( r50 == R50_DOT )
		{
			/* if we've already seen a dot, then the name is illegal */
			if ( retv >= 7 )
				break;
			/* else jump to filetype conversion */
			retv = 7;
			continue;
		}
		switch (retv)
		{
		case 0:
			options->iHandle.iNameR50[0] = r50 * 050 * 050;
			++retv;
			continue;
		case 1:
			options->iHandle.iNameR50[0] += r50 * 050;
			++retv;
			continue;
		case 2:
			options->iHandle.iNameR50[0] += r50;
			++retv;
			continue;
		case 3:
			options->iHandle.iNameR50[1] = r50 * 050 * 050;
			++retv;
			continue;
		case 4:
			options->iHandle.iNameR50[1] += r50 * 050;
			++retv;
			continue;
		case 5:
			options->iHandle.iNameR50[1] += r50;
			++retv;
			continue;
		case 7:
			options->iHandle.iNameR50[2] = r50 * 050 * 050;
			++retv;
			continue;
		case 8:
			options->iHandle.iNameR50[2] += r50 * 050;
			++retv;
			continue;
		case 9:
			options->iHandle.iNameR50[2] += r50;
			++retv;
			continue;
			/* If there's more than 9 characters, it's illegal */
		default:
			break;
		}
		break;
	}
	/* If there's more stuff in the filenamoptions->iHandle.iNameR50[0],e, then it's illegal */
	if ( *cp )
	{
		fprintf(stderr, "Filename '%s' is incompatible with RT11 name convention.\n",
				fileName);
		return 1;
	}
	if ( (options->cmdOpts & CMDOPT_DBG_NORMAL) )
	{
		printf("cvt_name: Converted '%s' to '%s': 0x%4X 0x%04X 0x%04X\n",
			   fileName, options->iHandle.argFN,
			   options->iHandle.iNameR50[0],
			   options->iHandle.iNameR50[1],
			   options->iHandle.iNameR50[2]);
	}
	return 0;
}





