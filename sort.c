/*  $Id: sort.c,v 1.1 2022/07/29 00:57:48 dave Exp dave $ $

	sort.c - Misc sort functions used by rtpip
	
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
 * @file sort.c
 * Misc sort functions used by rtpip.
 */
 
/*
 * Note: All the compare functions are set to cause any <empty> entries to percolate to the
 * end of the sorted list regardless of what field or sort order was selected and the empty
 * entries are in turn sorted by size smallest to largest, again, regardless of the sort option.
 */

/**
 * Compare filenames. Support function for qsort()
 * @param a1 - pointer to entry in linear directory array
 * @param a2 - pointer to entry in linear directory array
 * @return -1, 0, +1 depending on result of compare (a1-a2)
 */
static int cmpName( const void *a1, const void *a2 )
{
    int ii;
    InWorkingDir_t *a1_iwdP, *a2_iwdP;
    Rt11DirEnt_t *w1, *w2;
    
    a1_iwdP = *(InWorkingDir_t **)a1;
    a2_iwdP = *(InWorkingDir_t **)a2;
    w1 = &a1_iwdP->rt11;
    w2 = &a2_iwdP->rt11;
    ii = (((w1->control&PERM) != 0) << 1) | ((w2->control&PERM) != 0);
    switch(ii)
    {
    case 0:
        return w1->blocks - w2->blocks;
    case 1:
        return 1;
    case 2:
        return -1;
    case 3:
    default:
        break;
    }
    ii = w1->name[0] - w2->name[0];
    if ( !ii )
    {
        ii = w1->name[1] - w2->name[1];
        if ( !ii )
            ii = w1->name[2] - w2->name[2];
    }
    return ii;
}

/**
 * Compare filenames. Support function for qsort()
 * @param a1 - pointer to entry in linear directory array
 * @param a2 - pointer to entry in linear directory array
 * @return -1, 0, +1 depending on result of compare (a2-a1)
 */
static int cmpName_r( const void *a1, const void *a2 )
{
    InWorkingDir_t *a1_iwdP, *a2_iwdP;
    Rt11DirEnt_t *w1, *w2;
    int ii;
    a1_iwdP = *(InWorkingDir_t **)a1;
    a2_iwdP = *(InWorkingDir_t **)a2;
    w1 = &a1_iwdP->rt11;
    w2 = &a2_iwdP->rt11;
    ii = (((w1->control&PERM) != 0) << 1) | ((w2->control&PERM) != 0);
    switch(ii)
    {
    case 0:
        return w1->blocks - w2->blocks;
    case 1:
        return 1;
    case 2:
        return -1;
    case 3:
    default:
        break;
    }
    ii = w2->name[0] - w1->name[0];
    if ( ii )
        return ii;
    ii = w2->name[1] - w1->name[1];
    if ( ii )
        return ii;
    return w2->name[2] - w1->name[2];
}

/**
 * Compare filetypes. Support function for qsort()
 * @param a1 - pointer to entry in linear directory array
 * @param a2 - pointer to entry in linear directory array
 * @return -1, 0, +1 depending on result of compare (a1-a2)
 */
static int cmpType( const void *a1, const void *a2 )
{
    InWorkingDir_t *a1_iwdP, *a2_iwdP;
    Rt11DirEnt_t *w1, *w2;
    int ii;
    a1_iwdP = *(InWorkingDir_t **)a1;
    a2_iwdP = *(InWorkingDir_t **)a2;
    w1 = &a1_iwdP->rt11;
    w2 = &a2_iwdP->rt11;
    ii = (((w1->control&PERM) != 0) << 1) | ((w2->control&PERM) != 0);
    switch(ii)
    {
    case 0:
        return w1->blocks - w2->blocks;
    case 1:
        return 1;
    case 2:
        return -1;
    case 3:
    default:
        break;
    }
    ii = w1->name[2] - w2->name[2];
    if ( ii )
        return ii;
    ii = w1->name[0] - w2->name[0];
    if ( ii )
        return ii;
    return w1->name[1] - w2->name[1];
}

/**
 * Compare filetypes. Support function for qsort()
 * @param a1 - pointer to entry in linear directory array
 * @param a2 - pointer to entry in linear directory array
 * @return -1, 0, +1 depending on result of compare (a2-a1)
 */
static int cmpType_r( const void *a1, const void *a2 )
{
    InWorkingDir_t *a1_iwdP, *a2_iwdP;
    Rt11DirEnt_t *w1, *w2;
    int ii;
    a1_iwdP = *(InWorkingDir_t **)a1;
    a2_iwdP = *(InWorkingDir_t **)a2;
    w1 = &a1_iwdP->rt11;
    w2 = &a2_iwdP->rt11;
    ii = (((w1->control&PERM) != 0) << 1) | ((w2->control&PERM) != 0);
    switch(ii)
    {
    case 0:
        return w1->blocks - w2->blocks;
    case 1:
        return 1;
    case 2:
        return -1;
    case 3:
    default:
        break;
    }
    ii = w2->name[2] - w1->name[2];
    if ( ii )
        return ii;
    ii = w2->name[0] - w1->name[0];
    if ( ii )
        return ii;
    return w2->name[1] - w1->name[1];
}

/**
 * Compare filesizes. Support function for qsort()
 * @param a1 - pointer to entry in linear directory array
 * @param a2 - pointer to entry in linear directory array
 * @return -1, 0, +1 depending on result of compare (a1-a2)
 */
static int cmpSize( const void *a1, const void *a2 )
{
    InWorkingDir_t *a1_iwdP, *a2_iwdP;
    Rt11DirEnt_t *w1, *w2;
    int ii;
    a1_iwdP = *(InWorkingDir_t **)a1;
    a2_iwdP = *(InWorkingDir_t **)a2;
    w1 = &a1_iwdP->rt11;
    w2 = &a2_iwdP->rt11;
    ii = (((w1->control&PERM) != 0) << 1) | ((w2->control&PERM) != 0);
    switch(ii)
    {
    case 0:
        return w1->blocks - w2->blocks;
    case 1:
        return 1;
    case 2:
        return -1;
    case 3:
    default:
        break;
    }
    ii = w1->blocks - w2->blocks;
    if ( ii )
        return ii;
    return cmpName(a1,a2);
}

/**
 * Compare filesizes. Support function for qsort()
 * @param a1 - pointer to entry in linear directory array
 * @param a2 - pointer to entry in linear directory array
 * @return -1, 0, +1 depending on result of compare (a2-a1)
 */
static int cmpSize_r( const void *a1, const void *a2 )
{
    InWorkingDir_t *a1_iwdP, *a2_iwdP;
    Rt11DirEnt_t *w1, *w2;
    int ii;
    a1_iwdP = *(InWorkingDir_t **)a1;
    a2_iwdP = *(InWorkingDir_t **)a2;
    w1 = &a1_iwdP->rt11;
    w2 = &a2_iwdP->rt11;
    ii = (((w1->control&PERM) != 0) << 1) | ((w2->control&PERM) != 0);
    switch(ii)
    {
    case 0:
        return w1->blocks - w2->blocks;
    case 1:
        return 1;
    case 2:
        return -1;
    case 3:
    default:
        break;
    }
    ii = w2->blocks - w1->blocks;
    if ( ii )
        return ii;
    return cmpName_r(a1,a2);
}

/**
 * Compare file dates. Support function for qsort()
 * @param a1 - pointer to entry in linear directory array
 * @param a2 - pointer to entry in linear directory array
 * @return -1, 0, +1 depending on result of compare (a1-a2)
 */
static int cmpDate( const void *a1, const void *a2 )
{
    InWorkingDir_t *a1_iwdP, *a2_iwdP;
    Rt11DirEnt_t *w1, *w2;
    int ii;
    a1_iwdP = *(InWorkingDir_t **)a1;
    a2_iwdP = *(InWorkingDir_t **)a2;
    w1 = &a1_iwdP->rt11;
    w2 = &a2_iwdP->rt11;
    ii = (((w1->control&PERM) != 0) << 1) | ((w2->control&PERM) != 0);
    switch(ii)
    {
    case 0:
        return w1->blocks - w2->blocks;
    case 1:
        return 1;
    case 2:
        return -1;
    case 3:
    default:
        break;
    }
    ii = w1->date - w2->date;
    if ( ii )
        return ii;
    return cmpName(a1,a2);
}

/**
 * Compare file dates. Support function for qsort()
 * @param a1 - pointer to entry in linear directory array
 * @param a2 - pointer to entry in linear directory array
 * @return -1, 0, +1 depending on result of compare (a2-a1)
 */
static int cmpDate_r( const void *a1, const void *a2 )
{
    InWorkingDir_t *a1_iwdP, *a2_iwdP;
    Rt11DirEnt_t *w1, *w2;
    int ii;
    a1_iwdP = *(InWorkingDir_t **)a1;
    a2_iwdP = *(InWorkingDir_t **)a2;
    w1 = &a1_iwdP->rt11;
    w2 = &a2_iwdP->rt11;
    ii = (((w1->control&PERM) != 0) << 1) | ((w2->control&PERM) != 0);
    switch(ii)
    {
    case 0:
        return w1->blocks - w2->blocks;
    case 1:
        return 1;
    case 2:
        return -1;
    case 3:
    default:
        break;
    }
    ii = w2->date - w1->date;
    if ( ii )
        return ii;
    return cmpName_r(a1,a2);
}

#if 0
/**
 * Compare file starting LBAs. Support function for qsort()
 * @param a1 - pointer to entry in linear directory array
 * @param a2 - pointer to entry in linear directory array
 * @return -1, 0, +1 depending on result of compare (a1-a2)
 */
static int cmpLBA( const void *a1, const void *a2 )
{
    InWorkingDir_t *w1, *w2;
    w1 = (InWorkingDir_t *)a1;
    w2 = (InWorkingDir_t *)a2;
    return w1->lba - w2->lba;
}
#endif

/** Array of pointers to compare functions.
 */
int (*cmpFuncs[8])(const void *a1, const void *a2) =
{ cmpName,
  cmpType,
  cmpDate,
  cmpSize,
  cmpName_r,
  cmpType_r,
  cmpDate_r,
  cmpSize_r
};

/**
 * Compare filename against a filter.
 * @param filter - pointer to filter filename.
 * @param name - pointer to null terminated filename to check.
 * @return 0 if match, 1 if no match
 */
int normexec(const char *filter, const char *name )
{
    char tName[7], tType[4], *dst;
    int ii;
    const char *ext;
    
    ext = strchr(name,'.');
    dst = tName;
    memset(tName,' ',6);
    while ( dst < tName+6 && *name && (!ext || name < ext) )
        *dst++ = *name++;
    tName[6] = 0;
    memset(tType,' ',3);
    if ( ext )
    {
        ++ext;      /* skip the '.' */
        dst = tType;
        while ( *ext && dst < tType+3)
            *dst++ = *ext++;
    }
    tType[3] = 0;
    for (ii=0; ii < 6; ++ii)
    {
        if ( filter[ii] != '?' && filter[ii] != tName[ii] )
            return 1;
    }
    for (ii=0; ii < 3; ++ii)
    {
        if ( filter[ii+6] != '?' && filter[ii+6] != tType[ii] )
            return 1;
    }
    return 0;
}

/**
 * Filter filenames based on input from command line.
 * @param options - pointer to options
 * @param name - pointer to null terminated filename to check.
 * @return 1 if to handle file; 0 if to ignore file.
 */
int filterFilename( Options_t *options, const char *name )
{
    int ii, jj;
    if ( !options->numArgFiles )
        return 1;
    for (ii=0; ii < options->numArgFiles; ++ii)
    {
#if !NO_REGEXP
        if ( (options->fileOpts&FILEOPTS_REGEXP) )
        {
            jj = regexec(options->rexts + ii, name, 0, NULL, 0);
#if 0
            printf("filterFilename(): Checked file: '%s' against regular expression '%s'. Returned %s\n",
                name, options->argFiles[ii], jj ? "NOMATCH":"MATCH" );
#endif
        }
        else
#endif
        {
            jj = normexec(options->normExprs + ii*10, name);
#if 0
            printf("filterFilename(): Checked file: '%s' against normal expression '%s'. Returned %s\n",
                   name, options->normExprs+(ii*10), jj ? "NOMATCH":"MATCH");
#endif
        }
        if ( !jj )
            return 1;
    }
    return 0;
}



