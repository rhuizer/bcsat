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

#ifndef GATEHASH_HH
#define GATEHASH_HH

class GateHash;

#include <cstdio>
#include <vector>
#include "defs.hh"
#include "bc.hh"
#include "gate.hh"

/**
 * A set of gates implemented with a hash table.
 */
class GateHash
{
  class Entry {
  public:
    Gate *gate;
    unsigned int hash_value;
    unsigned int next;
    Entry(Gate *g, unsigned int h, unsigned int n) :
      gate(g), hash_value(h), next(n) {}
  };
  int nof_rows;
  unsigned int *rows;
  std::vector<Entry> entries;
public:
  GateHash(unsigned int);
  ~GateHash();
  /** Insert the gate \a gate in the set.
      If a Gate::comp equivalent gate is already present,
      return that and leaves the set unmodified */
  Gate *test_and_set(Gate *);
  /** Check if the gate \a gate is in the set. */
  bool is_in(Gate *gate) const;
  void print_distribution(FILE * const fp) const;
};

#endif
