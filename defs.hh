/*
 Copyright (C) 2002-2006 Tommi A. Junttila
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 2
 as published by the Free Software Foundation.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef DEFS_HH
#define DEFS_HH

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string.h>
#include <cassert>

extern const char *BCPACKAGE_VERSION;

extern bool verbose;
extern FILE *verbstr;
void verbose_print(const char * fmt, ...);

void internal_error(const char * fmt, ...);


#define BCSLS

#ifdef DEBUG
#define DEBUG_EXPENSIVE_CHECKS
#define DEBUG_ASSERT(a) assert(a)
#else
#define DEBUG_ASSERT(a)
#endif

extern unsigned int my_log2(unsigned int);

extern unsigned int *my_perm(const unsigned int n, const unsigned int seed);

/** Count the number of one bits in \a v */
unsigned int count_ones(unsigned int v);

#endif
