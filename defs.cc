/*
 Copyright (C) 2002-2008 Tommi Junttila
 
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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "defs.hh"

const char *BCPACKAGE_VERSION = "version 0.40";

bool verbose = false;
FILE *verbstr = stderr;

void verbose_print(const char * fmt, ...)
{
  if(verbose && verbstr) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(verbstr, fmt, ap);
    va_end(ap);
    fflush(verbstr);
  }
}

void internal_error(const char * fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr,"Internal error: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\nAborting!\n");
  va_end(ap);
  exit(1);
}

unsigned int my_log2(unsigned int n)
{
  unsigned int result = 0;
  while(n > 0)
    {
      result++;
      n = n / 2;
    }
  return result;
}



/*
 * Generate a permutation of {1,...,n}
 */
unsigned int *my_perm(const unsigned int n, const unsigned int seed)
{
  unsigned int *perm = (unsigned int *)malloc((n + 1) * sizeof(unsigned int));

  for(unsigned int i = 0; i <= n; i++)
    perm[i] = i;

  srand(seed);

  for(unsigned int i = 1; i < n; i++)
    {
      const int j = (rand() % (n - i + 1)) + i;
      const unsigned int temp = perm[j];
      perm[j] = perm[i];
      perm[i] = temp;
    }

  return perm;
}



/*
 *
 */
unsigned int count_ones(unsigned int v)
{
  static const unsigned int sub[16] = {0,1,1,2,
				       1,2,2,3,
				       1,2,2,3,
				       2,3,3,4};
  unsigned int result = 0;
  while(v > 0)
    {
      result += sub[v & 0x0f];
      v = v >> 4;
    }
  return result;
}
