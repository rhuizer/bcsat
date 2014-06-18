/*
 Copyright (C) 2002 Tommi Junttila
 
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

#include <cstdlib>
#include "gatehash.hh"


GateHash::GateHash(unsigned int n_rows)
{
  DEBUG_ASSERT(n_rows > 0);
  nof_rows = n_rows;
  rows = (unsigned int *)calloc(nof_rows, sizeof(unsigned int));
  if(!rows) { fprintf(stderr, "Out of memory\n"); exit(1); }
  /* The "null pointer" sentinel. */
  Entry e(0, 0, 0);
  entries.push_back(e);
}


GateHash::~GateHash()
{
  free(rows); rows = 0;
}


Gate *GateHash::test_and_set(Gate *gate)
{
  const unsigned int hash_value = gate->hash_value();
  const unsigned int row = hash_value % nof_rows;
  unsigned int entry_ptr = rows[row];
  while(entry_ptr)
    {
      Entry& entry = entries[entry_ptr];
      if((hash_value == entry.hash_value) && (entry.gate->comp(gate) == 0))
	return entry.gate;
      entry_ptr = entry.next;
    }
  Entry entry(gate, hash_value, rows[row]);
  rows[row] = entries.size();
  entries.push_back(entry);
  return gate;
}


bool GateHash::is_in(Gate *gate) const
{
  const unsigned int hash_value = gate->hash_value();
  unsigned int entry_ptr = rows[hash_value % nof_rows];
  while(entry_ptr)
    {
      const Entry& entry = entries[entry_ptr];
      if(entry.gate == gate)
	return true;
      entry_ptr = entry.next;
    }
  return false;
}


void GateHash::print_distribution(FILE * const fp) const
{
  for(int row = 0; row < nof_rows; row++)
    {
      unsigned int nof_elements = 0;
      unsigned int entry_ptr = rows[row];
      while(entry_ptr)
	{
	  nof_elements++;
	  entry_ptr = entries[entry_ptr].next;
	}
      if(nof_elements > 0)
	fprintf(fp, "%u ", nof_elements);
    }
  fprintf(fp, "\n");
}

