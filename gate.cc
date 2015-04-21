/*
 Copyright (C) Tommi Junttila
 
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

#include <climits>
#include "defs.hh"
#include "bc.hh"
#include "gate.hh"

static const char *text_NI = "%s:%d: %s not implemented";
static const char *text_NPN = "%s:%d: not properly normalized";
static const bool should_not_happen = false;

#define _should_not_happen() internal_error("%s:%d: should not happen",__FILE__,__LINE__)


/**************************************************************************
 *
 * Routines for the parent-child association between gates
 *
 **************************************************************************/

ChildAssoc::ChildAssoc(Gate* const f, Gate* const c) :
  parent(0), child(0),
  prev_child(0), next_child(0),
  prev_parent(0), next_parent(0)
{
  DEBUG_ASSERT(f);
  DEBUG_ASSERT(c);
  link_parent(f);
  link_child(c);
}

ChildAssoc::~ChildAssoc()
{
  unlink_parent();
  unlink_child();
}

void ChildAssoc::change_child(Gate* const new_child)
{
  DEBUG_ASSERT(new_child);
  unlink_child();
  link_child(new_child);
}

void ChildAssoc::change_parent(Gate* const new_parent)
{
  /* This is safe only for commutative (parent) gates because the order
   * of the children may change! */
  DEBUG_ASSERT(new_parent);
  DEBUG_ASSERT(parent->is_commutative());
  DEBUG_ASSERT(new_parent->is_commutative());
  unlink_parent();
  link_parent(new_parent);
}

void ChildAssoc::link_parent(Gate* const p)
{
  DEBUG_ASSERT(p);
  DEBUG_ASSERT(parent == 0);
  DEBUG_ASSERT(prev_child == 0);
  DEBUG_ASSERT(next_child == 0);
  parent = p;
  next_child = parent->children;
  if(next_child) {
    DEBUG_ASSERT(next_child->prev_child == 0);
    next_child->prev_child = this;
  }
  prev_child = 0;
  parent->children = this;
  parent->_nof_children++;
}

void ChildAssoc::link_child(Gate* const c)
{
  DEBUG_ASSERT(c);
  DEBUG_ASSERT(child == 0);
  DEBUG_ASSERT(prev_parent == 0);
  DEBUG_ASSERT(next_parent == 0);
  child = c;
  next_parent = child->parents;
  if(next_parent) {
    DEBUG_ASSERT(next_parent->prev_parent == 0);
    next_parent->prev_parent = this;
  }
  prev_parent = 0;
  child->parents = this;
}

void ChildAssoc::unlink_parent()
{
  DEBUG_ASSERT(parent);
  if(next_child)
    next_child->prev_child = prev_child;

  if(prev_child)
    prev_child->next_child = next_child;
  else {
    DEBUG_ASSERT(parent->children == this);
    parent->children = next_child;
  }
  parent->_nof_children--;
  parent = 0;
  next_child = 0;
  prev_child = 0;
}

void ChildAssoc::unlink_child()
{
  DEBUG_ASSERT(child);
  if(next_parent)
    next_parent->prev_parent = prev_parent;
  if(prev_parent)
    prev_parent->next_parent = next_parent;
  else {
    DEBUG_ASSERT(child->parents == this);
    child->parents = next_parent;
  }
  child = 0;
  next_parent = 0;
  prev_parent = 0;
}





/**************************************************************************
 *
 * Basic routines for gates
 *
 **************************************************************************/

const char* const Gate::typeNames[tNOFTYPES] = {"EQUIV",
						"OR",
						"AND",
						"EVEN",
						"ODD",
						"ITE",
						"NOT",
						"TRUE",
						"FALSE",
						"VAR",
						"THRESHOLD",
						"ATLEAST",
						"REF",
						"UNDEF",
						"DELETED"};

void
Gate::init()
{
  _nof_children = 0;
  handles = 0;
  determined = false;
  value = false;
  temp = 0;
  next = 0;
  in_pstack = false;
  pstack_next = 0;
  handles = 0;
}


Gate::Gate(const Type t) : 
  type(t), index(UINT_MAX), children(0), parents(0)
{
  init();
}

Gate::Gate(const Type t, Gate* const child) : 
  type(t), index(UINT_MAX), children(0), parents(0)
{
  init();
  DEBUG_ASSERT(type == tNOT or type == tREF);
  DEBUG_ASSERT(child);
  add_child(child);
}


Gate::Gate(const Type t, Gate* const child1, Gate* const child2) : 
  type(t), index(UINT_MAX), children(0), parents(0)
{
  init();
  DEBUG_ASSERT(type == tOR || type == tAND || type == tODD || type == tEVEN ||
               type == tEQUIV || type == tTHRESHOLD || type == tATLEAST);
  DEBUG_ASSERT(child1);
  DEBUG_ASSERT(child2);
  add_child(child2);
  add_child(child1);
}


Gate::Gate(const Type t, Gate* const if_child,
	   Gate* const then_child, Gate* const else_child) : 
  type(t), index(UINT_MAX), children(0), parents(0)
{
  init();
  DEBUG_ASSERT(type == tITE);
  add_child(else_child);
  add_child(then_child);
  add_child(if_child);
}


Gate::Gate(const Type t, const std::list<Gate*>* const childs) :
  type(t), index(UINT_MAX), children(0), parents(0)
{
  init();
  DEBUG_ASSERT(type == tOR or type == tAND or type == tODD or type == tEVEN or
               type == tEQUIV or type == tTHRESHOLD or type == tATLEAST);
  DEBUG_ASSERT(!childs->empty());
  for(std::list<Gate*>::const_reverse_iterator ci = childs->rbegin();
      ci != childs->rend();
      ci++)
    {
      add_child(*ci);
    }
  DEBUG_ASSERT(children != 0);
}


Gate::~Gate()
{
  assert(!in_pstack);
  assert(!pstack_next);
  assert(index == UINT_MAX);
  while(children)
    delete children;
  children = 0;
  while(parents)
    delete parents;
  parents = 0;
  /* Free handles */
  while(handles)
    delete handles;
}



void
Gate::add_child(Gate* const child)
{
  DEBUG_ASSERT(child != this);
  new ChildAssoc(this, child);
}



void
Gate::remove_all_children()
{
  while(children)
    delete children;
}



const char*
Gate::get_first_name() const
{
  for(const Handle* handle = handles; handle; handle = handle->get_next())
    {
      if(handle->get_type() == Handle::ht_NAME)
	{
	  const char* const name = ((NameHandle *)handle)->get_name();
	  DEBUG_ASSERT(name);
	  return name;
	}
    }
  return 0;
}


void
Gate::print_name_list(FILE * const fp, const char * const separator) const
{
  const char* sep = "";
  for(const Handle *handle = handles; handle; handle = handle->get_next())
    {
      if(handle->get_type() == Handle::ht_NAME)
	{
	  const char* name = ((NameHandle *)handle)->get_name();
	  DEBUG_ASSERT(name);
	  fprintf(fp, "%s%s", sep, name);
	  sep = separator;	  
	}
    }
}


void
Gate::print_child_list(FILE* const fp) const
{
  const char* sep = "";
  for(const ChildAssoc* ca = children; ca; ca = ca->next_child)
    {
      const Gate* const child = ca->child;
      const char* const name = child->get_first_name();
      fprintf(fp, "%s", sep); sep = ",";
      if(name)
	fprintf(fp, "%s", name);
      else
	fprintf(fp, "_t%u", child->temp);
    }
}










/**************************************************************************
 *
 * Finding cycles in the circuit
 *
 **************************************************************************/

#define CT_UNTEMP 0
#define CT_IN_STACK    1
#define CT_CYCLE_ENTRY 2
#define CT_TEMP   3

#define CTR_NO_CYCLE_FOUND 0
#define CTR_IN_CYCLE       1
#define CTR_CYCLE_FOUND    2

int
Gate::test_acyclicity(std::list<const char*>& cycle)
{
  if(!(temp >= 0 && temp <= 3))
    assert(should_not_happen);
  
  if(temp == CT_TEMP)
    return CTR_NO_CYCLE_FOUND;
  
  if(temp == CT_IN_STACK) {
    const char *name = get_first_name();
    if(name)
      cycle.push_back(name);
    temp = CT_CYCLE_ENTRY;   // mark as the "root" of the cycle
    return CTR_IN_CYCLE;
  }
  
  temp = CT_IN_STACK;

  for(ChildAssoc *ca = children; ca; ca = ca->next_child)
    {
      Gate * const child = ca->child;
      int status = child->test_acyclicity(cycle);
      
      if(status == CTR_CYCLE_FOUND)
	{
	  /* In the prefix leading to a cycle */
	  temp = CT_TEMP;
	  return CTR_CYCLE_FOUND;
	}
      if(status == CTR_IN_CYCLE)
	{
	  const char *name = get_first_name();
	  if(name)
	    cycle.push_back(name);
	  if(temp == CT_IN_STACK)
	    {
	      temp = CT_TEMP;
	      return CTR_IN_CYCLE;
	    }
	  assert(temp == CT_CYCLE_ENTRY);
	  temp = CT_TEMP;
	  return CTR_CYCLE_FOUND;
	}
      assert(status == CTR_NO_CYCLE_FOUND);
    }
  temp = CT_TEMP;
  return CTR_NO_CYCLE_FOUND;
}










/**************************************************************************
 * Marks the cone of influence of the gate.
 * Assigns each gate in the cone with a unique number.
 * Assumes that the temp fields of gates have been reset to -1.
 **************************************************************************/

void
Gate::mark_coi(int& counter)
{
  if(temp >= 0)
    return;
  temp = counter;
  counter += 1;
  for(const ChildAssoc* ca = children; ca; ca = ca->next_child)
    ca->child->mark_coi(counter);
}





/**************************************************************************
 *
 * Routines for adding gates in pstack
 *
 **************************************************************************/

void
Gate::add_in_pstack(BC* const bc)
{
  if(!in_pstack) {
    in_pstack = true;
    pstack_next = bc->pstack;
    bc->pstack = this;
  }
}

void
Gate::add_parents_in_pstack(BC* const bc)
{
  for(const ChildAssoc* pa = parents; pa; pa = pa->next_parent)
    pa->parent->add_in_pstack(bc);
}

void
Gate::add_children_in_pstack(BC* const bc)
{
  for(const ChildAssoc* ca = children; ca; ca = ca->next_child)
    ca->child->add_in_pstack(bc);
}





/**************************************************************************
 *
 * Simplify the gate
 * Returns false if an inconsistency is derived (implying unsatisfiability)
 *
 **************************************************************************/

bool
Gate::simplify(BC* const bc, const SimplifyOptions& opts)
{
  DEBUG_ASSERT(index != UINT_MAX);
  DEBUG_ASSERT(index < bc->index_to_gate.size());
  DEBUG_ASSERT(bc->index_to_gate[index] == this);


  if(type == tDELETED)
    return true;
 
  /* A limited cone of influence simplification:
   * remove gates that have no parents, no handles and are not determined */
  if(opts.use_coi and !parents and !handles and !determined)
    {
      add_children_in_pstack(bc);
      remove_all_children();
      type = tDELETED;
      bc->changed = true;
      return true;
    }

  switch(type) {
  case tFALSE:
    {
      DEBUG_ASSERT(!children);
      if(determined) {
	if(value != false)
	  return false;
      } else {
	determined = true;
	value = false;
	add_parents_in_pstack(bc);
      }
      if(!handles and !parents)
	type = tDELETED;
      return true;
    }

  case tTRUE:
    {
      DEBUG_ASSERT(!children);
      if(determined) {
	if(value != true)
	  return false;
      } else {
	determined = true;
	value = true;
	add_parents_in_pstack(bc);
      }
      if(!handles and !parents)
	type = tDELETED;
      return true;
    }

  case tVAR:
    {
      DEBUG_ASSERT(!children);
      if(determined and opts.may_transform_input_gates)
	transform_into_constant(bc, value);
      return true;
    }

  case tREF:
    {
      /* References are only used internally to simplify the implementation
       * of some reductions; thus they are always removed here */
      DEBUG_ASSERT(nof_children() == 1);
      Gate* const child = first_child();
      if(determined) {
	if(child->determined) {
	  if(child->value != value)
	    return false;
	} else {
	  child->determined = true;
	  child->value = value;
	  child->add_in_pstack(bc);
	}
	transform_into_constant(bc, value);
	add_parents_in_pstack(bc);
	return true;
      }
      DEBUG_ASSERT(!determined);
      if(child->determined) {
	transform_into_constant(bc, child->value);
	add_parents_in_pstack(bc);
	return true;
      }
      DEBUG_ASSERT(!determined and !child->determined);
      add_parents_in_pstack(bc);
      while(parents)
	parents->change_child(child);
      while(handles)
	handles->change_gate(child);
      remove_all_children();
      type = tDELETED;
      bc->changed = true;
      return true;
    }

  case tNOT:
    {
      DEBUG_ASSERT(nof_children() == 1);
      Gate* const child = first_child();
      if(determined) {
	if(child->determined) {
	  if(child->value != !value)
	    return false;
	} else {
	  child->determined = true;
	  child->value = !value;
	  child->add_in_pstack(bc);
	}
	transform_into_constant(bc, value);
	add_parents_in_pstack(bc);
	return true;
      }
      DEBUG_ASSERT(!determined);
      if(child->determined) {
	transform_into_constant(bc, !child->value);
	add_parents_in_pstack(bc);
	return true;
      }
      DEBUG_ASSERT(!determined and !child->determined);
      if(child->type == tNOT) {
	Gate* const grandchild = child->first_child();
	type = tREF;
	remove_all_children();
	if(!child->parents)
	  child->add_in_pstack(bc);
	add_child(grandchild);
	add_in_pstack(bc);
	bc->changed = true;
	return true;
      }
      return true;
    }

  case tITE:
    {
      DEBUG_ASSERT(nof_children() == 3);
      Gate* const if_child = children->child;
      Gate* const then_child = children->next_child->child;
      Gate* const else_child = children->next_child->next_child->child;
      if(opts.constant_folding)
	{
	  if(if_child->determined and if_child->value == true) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tREF;
	    add_child(then_child);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(if_child->determined and if_child->value == false) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tREF;
	    add_child(else_child);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(then_child->determined and then_child->value == true) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tOR;
	    add_child(if_child);
	    add_child(else_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(then_child->determined and then_child->value == false) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tAND;
	    Gate *new_not = bc->new_NOT(if_child);
	    add_child(new_not);
	    add_child(else_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(else_child->determined and else_child->value == true) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tOR;
	    Gate *new_not = bc->new_NOT(if_child);
	    add_child(new_not);
	    add_child(then_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(else_child->determined and else_child->value == false) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tAND;
	    add_child(if_child);
	    add_child(then_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	}
      //DEBUG_ASSERT(!if_child->determined);
      //DEBUG_ASSERT(!then_child->determined);
      //DEBUG_ASSERT(!else_child->determined);
      if(opts.remove_duplicate_children)
	{
	  if(if_child == then_child) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tOR;
	    add_child(if_child);
	    add_child(else_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(if_child == else_child) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tAND;
	    add_child(if_child);
	    add_child(then_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(then_child == else_child) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tREF;
	    add_child(then_child);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	}
      if(opts.remove_g_not_g_children)
	{
	  if(then_child->type == tNOT and
	     if_child == then_child->first_child()) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tAND;
	    add_child(then_child);
	    add_child(else_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(if_child->type == tNOT and
	     if_child->first_child() == then_child) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tAND;
	    add_child(then_child);
	    add_child(else_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(else_child->type == tNOT and
	     if_child == else_child->first_child()) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tOR;
	    add_child(else_child);
	    add_child(then_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(if_child->type == tNOT and
	     if_child->first_child() == else_child) {
	    add_children_in_pstack(bc);
	    remove_all_children();
	    type = tOR;
	    add_child(else_child);
	    add_child(then_child);
	    add_parents_in_pstack(bc);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(else_child->type == tNOT and
	     else_child->first_child() == then_child) {
	    remove_all_children();
	    if(!else_child->parents) else_child->add_in_pstack(bc);
	    type = tEQUIV;
	    add_child(if_child);
	    add_child(then_child);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	  if(then_child->type == tNOT and
	     then_child->first_child() == else_child) {
	    remove_all_children();
	    if(!then_child->parents) then_child->add_in_pstack(bc);
	    type = tODD;
	    add_child(if_child);
	    add_child(else_child);
	    add_in_pstack(bc);
	    bc->changed = true;
	    return true;
	  }
	}
      return true;
    }

  case tOR:
    {
      DEBUG_ASSERT(nof_children() >= 1);

      if(opts.downward_bcp and determined and value == false) {
	for(ChildAssoc* ci = children; ci; ) {
	  Gate* const child = children->child;
	  ChildAssoc* const next_ci = ci->next_child;
	  if(child->determined) {
	    if(child->value != false)
	      return false;
	  } else {
	    child->determined = true;
	    child->value = false;
	    child->add_in_pstack(bc);
	  }
	  if(opts.constant_folding)
	    delete ci;
	  ci = next_ci;
	}
	if(opts.constant_folding) {
	  transform_into_constant(bc, false);
	  add_parents_in_pstack(bc);
	  return true;
	}
      }

      if(opts.constant_folding and (!determined or value == true)) {
	bool true_found = false;
	for(ChildAssoc* ci = children; ci; ) {
	  ChildAssoc* const next_ci = ci->next_child;
	  Gate* const child = ci->child;
	  if(child->determined) {
	    if(child->value == true) {
	      true_found = true;
	      break;
	    }
	    delete ci;
	    if(!child->parents) child->add_in_pstack(bc);
	  }
	  ci = next_ci;
	}
	if(true_found) {
	  transform_into_constant(bc, true);
	  add_parents_in_pstack(bc);
	  return true;
	}
      }


      if(nof_children() == 0) {
	if(determined and value != false)
	  return false;
	transform_into_constant(bc, false);
	add_parents_in_pstack(bc);
	return true;
      }
      if(nof_children() == 1) {
	type = tREF;
	add_in_pstack(bc);
	return true;
      }

      DEBUG_ASSERT(nof_children() >= 2);
      if(!remove_duplicate_and_g_not_g_children(bc,
						opts.remove_duplicate_children,
						opts.remove_g_not_g_children))
	return false;

      if(type != tOR)
	return true;

      const bool or_share = false;
      if(or_share and nof_children() >= 3) {
#ifdef DEBUG_EXPENSIVE_CHECKS
	for(Gate *g = bc->first_gate; g; g = g->next)
	  assert(g->temp == 0);
#endif
	for(const ChildAssoc *ci = children; ci; ci = ci->next_child)
	  ci->child->temp = 1;
	
	for(ChildAssoc* ci = children; ci; ci = ci->next_child) {
	  Gate * const child = ci->child;
	  for(ChildAssoc *fa = child->parents; fa; ) {
	    Gate* const parent = fa->parent;
	    ChildAssoc* next_fa = fa->next_parent;
	    while(next_fa and next_fa->parent == parent)
	      next_fa = next_fa->next_parent;
	    if(parent != this and parent->type == tOR and
	       (parent->determined or parent->parents)) {
	      bool all_same = true;
	      unsigned int nof_children = 0;
	      for(ChildAssoc* fca = parent->children; fca; fca=fca->next_child) {
		if(fca->child->temp != 1) {
		  all_same = false;
		  break;
		}
		nof_children++;
	      }
	      if(all_same and nof_children > 1 and
		 nof_children < count_children()) {
		for(ChildAssoc *fca = parent->children; fca;
		    fca = fca->next_child)
		  fca->child->temp = 0;
		for(ChildAssoc* ca2 = children; ca2; ) {
		  ChildAssoc* const next_ca2 = ca2->next_child;
		  if(ca2->child->temp == 0)
		    delete ca2;
		  ca2 = next_ca2;
		}
		add_child(parent);
		goto done;
	      }
	    }
	    fa = next_fa;
	  }
	}
      done:

	for(const ChildAssoc* ci = children; ci; ci = ci->next_child)
	  ci->child->temp = 0;
#ifdef DEBUG_EXPENSIVE_CHECKS
	for(Gate* g = bc->first_gate; g; g = g->next)
	  assert(g->temp == 0);
#endif
      }

      if(opts.absorb_children != SimplifyOptions::CHILDABSORB_NONE) {
	bool absorbed = false;
	for(ChildAssoc *ca = children; ca; ) {
	  Gate* const child = ca->child;
	  if(child->type == tOR and !child->determined and
	     (opts.absorb_children == SimplifyOptions::CHILDABSORB_ALL or
	      !child->has_many_parents())) {
	    absorbed = true;
	    for(const ChildAssoc* gca = child->children; gca;
		gca = gca->next_child)
	      add_child(gca->child);
	    ChildAssoc* next_ca = ca->next_child;
	    delete ca;
	    ca = next_ca;
	    if(!child->parents) child->add_in_pstack(bc);
	    continue;
	  }
	  ca = ca->next_child;
	}
	if(absorbed) {
	  add_in_pstack(bc);
	  return true;
	}
      }

      return true;
    }

  case tAND:
    {
      DEBUG_ASSERT(nof_children() >= 1);

      if(opts.downward_bcp and determined and value == true) {
	for(ChildAssoc* ci = children; ci; ) {
	  Gate* const child = children->child;
	  ChildAssoc* const next_ci = ci->next_child;
	  if(child->determined) {
	    if(child->value != true)
	      return false;
	  } else {
	    child->determined = true;
	    child->value = true;
	    child->add_in_pstack(bc);
	  }
	  if(opts.constant_folding)
	    delete ci;
	  ci = next_ci;
	}
	if(opts.constant_folding) {
	  transform_into_constant(bc, true);
	  add_parents_in_pstack(bc);
	  return true;
	}
      }

      if(opts.constant_folding and (!determined or value == false)) {
	bool false_found = false;
	for(ChildAssoc* ci = children; ci; ) {
	  ChildAssoc* const next_ci = ci->next_child;
	  Gate* const child = ci->child;
	  if(child->determined) {
	    if(child->value == false) {
	      false_found = true;
	      break;
	    }
	    delete ci;
	    if(!child->parents) child->add_in_pstack(bc);
	  }
	  ci = next_ci;
	}
	if(false_found) {
	  transform_into_constant(bc, false);
	  add_parents_in_pstack(bc);
	  return true;
	}
      }


      if(nof_children() == 0) {
	if(determined and value != true)
	  return false;
	transform_into_constant(bc, true);
	add_parents_in_pstack(bc);
	return true;
      }
      if(nof_children() == 1) {
	type = tREF;
	add_in_pstack(bc);
	return true;
      }

      DEBUG_ASSERT(nof_children() >= 2);
      if(!remove_duplicate_and_g_not_g_children(bc,
						opts.remove_duplicate_children,
						opts.remove_g_not_g_children))
	return false;

      if(type != tAND)
	return true;

#ifdef OLD
      const bool and_share = false;
      if(and_share and nof_children() >= 3) {
#ifdef DEBUG_EXPENSIVE_CHECKS
	for(const Gate* g = bc->first_gate; g; g = g->next)
	  assert(g->temp == 0);
#endif
	for(const ChildAssoc* ci = children; ci; ci = ci->next_child)
	  ci->child->temp = 1;
	
	for(const ChildAssoc* ci = children; ci; ci = ci->next_child)
	  {
	    Gate* const child = ci->child;
	    for(const ChildAssoc* opi = child->parents; opi; ) {
	      Gate* const other_parent = opi->parent;
	      const ChildAssoc* next_opi = opi->next_parent;
	      while(next_opi and next_opi->parent == other_parent)
		next_opi = next_opi->next_parent;
	      if(other_parent != this and other_parent->type == tAND and
		 (other_parent->determined or other_parent->parents))
		{
		  bool all_same = true;
		  unsigned int nof_children = 0;
		  for(const ChildAssoc* oci = other_parent->children; oci;
		      oci = oci->next_child) {
		    if(oci->child->temp != 1) {
		      all_same = false;
		      break;
		    }
		    nof_children++;
		  }
		  if(all_same and nof_children > 1 and
		     nof_children < count_children())
		    {
		      for(const ChildAssoc* oci = other_parent->children; oci;
			  oci = oci->next_child)
			oci->child->temp = 0;
		      for(ChildAssoc* ci2 = children; ci2; ) {
			ChildAssoc* next_ci2 = ci2->next_child;
			if(ci2->child->temp == 0)
			  delete ci2;
			ci2 = next_ci2;
		      }
		      add_child(other_parent);
		      goto done_and;
		    }
		}
	      opi = next_opi;
	    }
	  }
      done_and:
	
	for(const ChildAssoc* ci = children; ci; ci = ci->next_child)
	  ci->child->temp = 0;
#ifdef DEBUG_EXPENSIVE_CHECKS
	for(const Gate* g = bc->first_gate; g; g = g->next)
	  assert(g->temp == 0);
#endif
      }
#else //~OLD
      /* BROKEN, DO NOT USE! */
      const bool and_share = false;
      if(and_share and nof_children() >= 3) {
#ifdef DEBUG_EXPENSIVE_CHECKS
	for(const Gate* g = bc->first_gate; g; g = g->next)
	  assert(g->temp == 0);
#endif
	for(const ChildAssoc* ci = children; ci; ci = ci->next_child)
	  ci->child->temp = 1;
	
	for(const ChildAssoc* ci = children; ci; ci = ci->next_child)
	  {
	    Gate* const child = ci->child;
	    for(const ChildAssoc* opi = child->parents;
		opi;
		opi = opi->next_parent) {
	      Gate* const other_parent = opi->parent;
	      if(other_parent != this and
		 other_parent->type == tAND and
		 other_parent->nof_children() >= 2 and
		 other_parent->nof_children() < nof_children() and
		 (other_parent->determined or other_parent->parents))
		{
		  other_parent->temp += 2;
		  DEBUG_ASSERT((unsigned int)other_parent->temp / 2 <=
			       other_parent->nof_children());
		  if((unsigned int)other_parent->temp/2 ==
		     other_parent->nof_children()) {
		    for(const ChildAssoc* ci2 = children; ci2;
			ci2 = ci2->next_child) {
		      Gate* const child2 = ci->child;
		      child2->temp = 0;
		      for(const ChildAssoc* opi2 = child->parents; opi2;
			  opi2 = opi2->next_parent)
			opi2->parent->temp = 0;
		      if(ci2 == ci)
			break;
		    }
		    for(const ChildAssoc* oci = other_parent->children; oci;
			oci = oci->next_child)
		      oci->child->temp = 1;
		    for(ChildAssoc* ci2 = children; ci2; ) {
		      ChildAssoc* next_ci2 = ci2->next_child;
		      if(ci2->child->temp == 1)
			delete ci2;
		      ci2 = next_ci2;
		    }
		    add_child(other_parent);
		    for(const ChildAssoc* oci = other_parent->children; oci;
			oci = oci->next_child)
		      oci->child->temp = 0;
		    goto done_and;
		  }
		}
	    }
	  }
	for(const ChildAssoc* ci = children; ci; ci = ci->next_child) {
	  Gate* const child = ci->child;
	  child->temp = 0;
	  for(const ChildAssoc* opi = child->parents; opi;
	      opi = opi->next_parent) {
	    Gate* const other_parent = opi->parent;
	    other_parent->temp = 0;
	  }
	}
      done_and:
	;
#ifdef DEBUG_EXPENSIVE_CHECKS
	for(const Gate* g = bc->first_gate; g; g = g->next)
	  assert(g->temp == 0);
#endif
      }
#endif //ifdef OLD

      if(opts.absorb_children != SimplifyOptions::CHILDABSORB_NONE) {
	bool absorbed = false;
	for(ChildAssoc *ca = children; ca; ) {
	  Gate * const child = ca->child;
	  if(child->type == tAND and !child->determined and
	     (opts.absorb_children == SimplifyOptions::CHILDABSORB_ALL or
	      !child->has_many_parents())) {
	    absorbed = true;
	    for(ChildAssoc *gca = child->children; gca;
		gca = gca->next_child)
	      add_child(gca->child);
	    ChildAssoc *next_ca = ca->next_child;
	    delete ca;
	    ca = next_ca;
	    if(!child->parents) child->add_in_pstack(bc);
	    continue;
	  }
	  ca = ca->next_child;
	}
	if(absorbed) {
	  add_in_pstack(bc);
	  return true;
	}
      }

      return true;
    }

  case tODD:
  case tEVEN:
    {
      for(ChildAssoc* ci = children; ci; ) {
	Gate* const child = ci->child;
	if(!child->determined) {
	  ci = ci->next_child;
	  continue;
	}
	if(child->value == true) {
	  /* Swap type */
	  if(type == tODD) type = tEVEN;
	  else if(type == tEVEN) type = tODD;
	  else _should_not_happen();
	}
	/* Remove child */
	ChildAssoc* const next_ci = ci->next_child;
	delete ci;
	ci = next_ci;
	if(!child->has_parents())
	  child->add_in_pstack(bc);
      }
      if(nof_children() == 0)
	{
	  if(type == tODD) {
	    if(determined and value != false)
	      return false;
	    transform_into_constant(bc, false);
	    add_parents_in_pstack(bc);
	    return true;
	  }
	  else if(type == tEVEN) {
	    if(determined and value != true)
	      return false;
	    transform_into_constant(bc, true);
	    add_parents_in_pstack(bc);
	    return true;
	  }
	  _should_not_happen();
	}
      if(nof_children() == 1) {
	if(type == tODD) {
	  type = tREF;
	} else if(type == tEVEN) {
	  type = tNOT;
	  add_parents_in_pstack(bc);
	} else 
	  _should_not_happen();
	add_in_pstack(bc);
	return true;
      }

      DEBUG_ASSERT(nof_children() >= 2);

      bool has_determined_children = false;
      for(ChildAssoc *ca = children; ca; ) {
	Gate* const child = ca->child;
	if(child->type == tNOT) {
	  Gate* const grandchild = child->first_child();
	  ca->change_child(grandchild);
	  if(grandchild->determined)
	    has_determined_children = true;
	  if(!child->has_parents())
	    child->add_in_pstack(bc);
	  if(type == tODD) type = tEVEN;
	  else if(type == tEVEN) type = tODD;
	  else assert(should_not_happen);
	}
	ca = ca->next_child;
      }
      if(has_determined_children)
	{
	  add_in_pstack(bc);
	  return true;
	}


      if(!remove_duplicate_and_g_not_g_children(bc, opts.remove_duplicate_children, opts.remove_g_not_g_children))
	return false;
      if(in_pstack)
	return true;
      if(!(type == tODD or type == tEVEN))
	return true;
      
      DEBUG_ASSERT(nof_children() >= 2);

      /*
       * Inline equivalences
       */
      if(opts.inline_equivalences) 
	{
	  if(determined and nof_children() == 2 and
	     ((type == tEVEN and value == true) or
	      (type == tODD and value == false)))
	    {
	      Gate* child1 = children->child;
	      Gate* child2 = children->next_child->child;
	      if(child1 == child2) {
		transform_into_constant(bc, value);
		add_parents_in_pstack(bc);
		return true;
	      }
	      if(bc->may_transform_input_gates) {
		if(child1->type == tVAR && !bc->depends_on(child2, child1)) {
		  /* child1 is an input gate and child2 does not depend on it ==>
		     make child1 equivalent to child2 */
		  transform_into_constant(bc, value);
		  add_parents_in_pstack(bc);
		  DEBUG_ASSERT(!child1->determined && !child2->determined);
		  child1->type = tREF;
		  child1->add_child(child2);
		  child1->add_in_pstack(bc);
		  return true;
		}
		if(child2->type == tVAR && !bc->depends_on(child1, child2)) {
		  /* child2 is an input gate and child1 does not depend on it ==>
		     make child2 equivalent to child1 */
		  transform_into_constant(bc, value);
		  add_parents_in_pstack(bc);
		  DEBUG_ASSERT(!child1->determined && !child2->determined);
		  child2->type = tREF;
		  child2->add_child(child1);
		  child2->add_in_pstack(bc);
		  return true;
		}
	      }
	      if(child1->has_many_parents() and child2->has_many_parents()) {
		if(!bc->depends_on(child1, child2)) {
		  for(ChildAssoc *fa = child2->parents; fa; ) {
		    ChildAssoc *next_fa = fa->next_parent;
		    if(fa->parent != this)
		      fa->change_child(child1);
		    fa = next_fa;
		  }
		  child1->add_parents_in_pstack(bc);
		} else {
		  DEBUG_ASSERT(!bc->depends_on(child2, child1));
		  for(ChildAssoc *fa = child1->parents; fa; ) {
		    ChildAssoc *next_fa = fa->next_parent;
		    if(fa->parent != this)
		      fa->change_child(child2);
		    fa = next_fa;
		  }
		  child2->add_parents_in_pstack(bc);
		}
		//return true;
	      }
	      //return true;
	    }
	  
	  if(determined and nof_children() == 2 and
	     ((type == tEVEN and value == false) or
	      (type == tODD and value == true)))
	    {
	      Gate *child1 = children->child;
	      Gate *child2 = children->next_child->child;
	      if(child1 == child2)
		return false;
	      if(bc->may_transform_input_gates) {
		if(child1->type == tVAR && !bc->depends_on(child2, child1)) {
		  transform_into_constant(bc, value);
		  add_parents_in_pstack(bc);
		  DEBUG_ASSERT(!child1->determined && !child2->determined);
		  child1->type = tNOT;
		  child1->add_child(child2);
		  child1->add_parents_in_pstack(bc);
		  child1->add_in_pstack(bc);
		  return true;
		}
		if(child2->type == tVAR && !bc->depends_on(child1, child2)) {
		  transform_into_constant(bc, value);
		  add_parents_in_pstack(bc);
		  DEBUG_ASSERT(!child1->determined && !child2->determined);
		  child2->type = tNOT;
		  child2->add_child(child1);
		  child2->add_parents_in_pstack(bc);
		  child2->add_in_pstack(bc);
		  return true;
		}
	      }
	      if(child1->has_many_parents() and child2->has_many_parents()) {
		if(child1->type == tVAR || !bc->depends_on(child1, child2)) {
		  Gate *new_not = new Gate(tNOT, child1);
		  bc->install_gate(new_not);
		  for(ChildAssoc *fa = child2->parents; fa; ) {
		    ChildAssoc *next_fa = fa->next_parent;
		    if(fa->parent != this)
		      fa->change_child(new_not);
		    fa = next_fa;
		  }
		  new_not->add_parents_in_pstack(bc);
		  new_not->add_in_pstack(bc);
		} else {
		  DEBUG_ASSERT(!bc->depends_on(child2, child1));
		  Gate *new_not = new Gate(tNOT, child2);
		  bc->install_gate(new_not);
		  for(ChildAssoc *fa = child1->parents; fa; ) {
		    ChildAssoc *next_fa = fa->next_parent;
		    if(fa->parent != this)
		      fa->change_child(new_not);
		    fa = next_fa;
		  }
		  new_not->add_parents_in_pstack(bc);
		  new_not->add_in_pstack(bc);
		}
		//return true;
	      }
	      //return true;
	    }
	}

      if(opts.misc_reductions)
	{
	  if(type == tODD and nof_children() == 2) {
	    Gate* child1 = children->child;
	    Gate* child2 = children->next_child->child;
	    if(child2->type == tOR and !child2->has_many_parents()) {
	      bool found = false;
	      for(ChildAssoc *ca = child2->children; ca; ca = ca->next_child) {
		if(ca->child == child1) {
		  found = true;
		  break;
		}
	      }
	      if(found) {
		Gate *new_or = new Gate(tOR); bc->install_gate(new_or);
		for(ChildAssoc *ca = child2->children; ca; ca = ca->next_child)
		  if(ca->child != child1)
		    new_or->add_child(ca->child);
		remove_all_children();
		if(!child2->parents) child2->add_in_pstack(bc);
		Gate *new_not = new Gate(tNOT, child1); bc->install_gate(new_not);
		type = tAND;
		add_child(new_not);
		add_child(new_or);
		add_in_pstack(bc);
		new_not->add_in_pstack(bc);
		new_or->add_in_pstack(bc);
		return true;
	      }
	    }
	    else if(child1->type == tOR and !child1->has_many_parents()) {
	      bool found = false;
	      for(ChildAssoc *ca = child1->children; ca; ca = ca->next_child) {
		if(ca->child == child2) {
		  found = true;
		  break;
		}
	      }
	      if(found) {
		Gate *new_or = new Gate(tOR); bc->install_gate(new_or);
		for(ChildAssoc *ca = child1->children; ca; ca = ca->next_child)
		  if(ca->child != child2)
		    new_or->add_child(ca->child);
		remove_all_children();
		if(!child1->parents) child1->add_in_pstack(bc);
		Gate *new_not = new Gate(tNOT, child2); bc->install_gate(new_not);
		type = tAND;
		add_child(new_not);
		add_child(new_or);
		add_in_pstack(bc);
		new_not->add_in_pstack(bc);
		new_or->add_in_pstack(bc);
		return true;
	      }
	    }
	  }
	}

      if(!(type == tODD or type == tEVEN))
	return true;

      /* Incompatible with CNF normalization! */
      if(opts.absorb_children != SimplifyOptions::CHILDABSORB_NONE and
	 !opts.preserve_cnf_normalized_form) {
	if(type == tODD) {
	  bool absorbed = false;
	  for(ChildAssoc *ca = children; ca; ) {
	    Gate * const child = ca->child;
	    if(child->type == tODD and
	       (opts.absorb_children == SimplifyOptions::CHILDABSORB_ALL or
		!child->has_many_parents())) {
	      absorbed = true;
	      for(ChildAssoc *gca = child->children; gca;
		  gca = gca->next_child)
		add_child(gca->child);
	      ChildAssoc *next_ca = ca->next_child;
	      delete ca;
	      ca = next_ca;
	      if(!child->parents) child->add_in_pstack(bc);
	      continue;
	    }
	    if(child->type == tEVEN and
	       (opts.absorb_children == SimplifyOptions::CHILDABSORB_ALL or
		!child->has_many_parents())) {
	      absorbed = true;
	      type = tEVEN;
	      for(ChildAssoc *gca = child->children; gca;
		  gca = gca->next_child)
		add_child(gca->child);
	      ChildAssoc *next_ca = ca->next_child;
	      delete ca;
	      ca = next_ca;
	      child->add_in_pstack(bc);
	      continue;
	    }
	    ca = ca->next_child;
	  }
	  if(absorbed) {
	    add_in_pstack(bc);
	    return true;
	  }
	}

	if(type == tEVEN) {
	  bool absorbed = false;
	  for(ChildAssoc *ca = children; ca; ) {
	    Gate * const child = ca->child;
	    if(child->type == tODD and
	       (opts.absorb_children == SimplifyOptions::CHILDABSORB_ALL or
		!child->has_many_parents())) {
	      absorbed = true;
	      for(ChildAssoc *gca = child->children; gca;
		  gca = gca->next_child)
		add_child(gca->child);
	      ChildAssoc *next_ca = ca->next_child;
	      delete ca;
	      ca = next_ca;
	      child->add_in_pstack(bc);
	      continue;
	    }
	    if(child->type == tEVEN and
	       (opts.absorb_children == SimplifyOptions::CHILDABSORB_ALL or
		!child->has_many_parents())) {
	      for(ChildAssoc *gca = child->children; gca;
		  gca = gca->next_child)
		add_child(gca->child);
	      absorbed = true;
	      type = tODD;
	      ChildAssoc *next_ca = ca->next_child;
	      delete ca;
	      ca = next_ca;
	      child->add_in_pstack(bc);
	      continue;
	    }
	    ca = ca->next_child;
	  }
	  if(absorbed) {
	    add_in_pstack(bc);
	    return true;
	  }
	}
      }

      return true;
    }

  case tEQUIV:
    {
      assert(nof_children() > 0);
      
      if(nof_children() == 1)
	{
	  if(determined and value != true)
	    return false;
	  transform_into_constant(bc, true);
	  add_parents_in_pstack(bc);
	  return true;
	}
      if(nof_children() == 2)
	{
	  type = tEVEN;
	  add_parents_in_pstack(bc);
	  add_in_pstack(bc);
	  return true;
	}

      /*
       * Constant folding
       */
      if(opts.constant_folding)
	{
	  for(ChildAssoc* ci = children; ci; ci = ci->next_child)
	    {
	      Gate* const child = ci->child;
	      if(!child->determined)
		continue;
	      if(child->value == true) {
		type = tAND;
		add_parents_in_pstack(bc);
		add_in_pstack(bc);
		return true;
	      }
	      DEBUG_ASSERT(child->value == false);
	      Gate *new_or = new Gate(tOR);
	      bc->install_gate(new_or);
	      while(children)
		children->change_parent(new_or);
	      type = tNOT;
	      add_child(new_or);
	      add_parents_in_pstack(bc);
	      add_in_pstack(bc);
	      new_or->add_in_pstack(bc);
	      return true;
	    }
	}

      if(!remove_duplicate_and_g_not_g_children(bc,
						opts.remove_duplicate_children,
						opts.remove_g_not_g_children))
	return false;
      if(in_pstack)
	return true;
      if(type != tEQUIV)
	return true;

      if(opts.inline_equivalences and determined and value == true)
	{
	  if(bc->may_transform_input_gates) {
	    Gate *first_input_gate = 0;
	    bool unified = false;
	    for(ChildAssoc* ca = children; ca; ca = ca->next_child) {
	      Gate* const child = ca->child;
	      if(child->type != tVAR)
		continue;
	      if(!child->has_many_parents()) {
		DEBUG_ASSERT(child->parents->parent == this);
		Gate *other_child = 0;
		for(ChildAssoc* ci2 = children; ci2; ci2 = ci2->next_child) {
		  if(ci2->child != child) {
		    other_child = ci2->child;
		    break;
		  }
		}
		if(other_child) {
		  assert(other_child != child);
		  child->type = tREF;
		  child->add_child(other_child);
		  add_in_pstack(bc);
		  child->add_in_pstack(bc);
		  return true;
		}
	      }
	      if(first_input_gate == 0) {
		first_input_gate = child;
		continue;
	      }
	      if(child != first_input_gate) {
		unified = true;
		child->type = tREF;
		child->add_child(first_input_gate);
		child->add_in_pstack(bc);
	      }
	    }
	    if(unified) {
	      first_input_gate->add_parents_in_pstack(bc);
	      return true;
	    }
	  }
	  ChildAssoc *ca = children;
	  Gate *least_child = ca->child;
	  ca = ca->next_child;
	  for( ; ca; ca = ca->next_child) {
	    if(bc->depends_on(least_child, ca->child))
	      least_child = ca->child;
	  }
	  bool moved = false;
	  for(ca = children; ca; ca = ca->next_child) {
	    Gate * const child = ca->child;
	    if(child == least_child)
	      continue;
	    for(ChildAssoc *fa = child->parents; fa; ) {
	      ChildAssoc *next_fa = fa->next_parent;
	      if(fa->parent != this) {
		fa->change_child(least_child);
		moved = true;
	      }
	      fa = next_fa;
	    }
	    DEBUG_ASSERT(child->parents && !child->parents->next_parent);
	  }
	  if(moved) {
	    least_child->add_parents_in_pstack(bc);
	    return true;
	  }
	  return true;
	} /* if(determined && value == true) { */

      return true;
    }
    
  case tTHRESHOLD:
    {
#define NEW_CARDINALITY_SIMPLIFY
#ifdef NEW_CARDINALITY_SIMPLIFY

      if(tmin > tmax) {
	if(determined and value != false)
	  return false;
	transform_into_constant(bc, false);
	add_parents_in_pstack(bc);
	return true;
      }
      
      for(ChildAssoc *ca = children; ca; ) {
	assert(tmin <= tmax);
	if(tmax == 0) {
	  Gate *new_or = new Gate(tOR);
	  bc->install_gate(new_or);
	  new_or->add_in_pstack(bc);
	  while(children)
	    children->change_parent(new_or);
	  type = tNOT; tmin = 0; tmax = 0;
	  add_child(new_or);
	  add_in_pstack(bc);
	  return true;
	}
	Gate * const child = ca->child;
	if(!child->determined) {
	  ca = ca->next_child;
	  continue;
	}
	if(child->value == false) {
	  ChildAssoc *next_ca = ca->next_child;
	  delete ca;
	  ca = next_ca;
	  continue;
	}
	assert(tmax > 0);
	tmin = (tmin == 0)?0:tmin-1;
	tmax = tmax - 1;
	ChildAssoc *next_ca = ca->next_child;
	delete ca;
	ca = next_ca;
      }

      assert(tmin <= tmax);

      if(tmin > nof_children()) {
	if(determined and value != false)
	  return false;
	transform_into_constant(bc, false);
	add_parents_in_pstack(bc);
	return true;
      }
      if(tmax > nof_children())
	tmax = nof_children();
      
      assert(tmin <= tmax and tmax <= nof_children());

      if(!children) {
	assert(tmin == 0 and tmax == 0);
	if(determined && value != true)
	  return false;
	transform_into_constant(bc, true);
	add_parents_in_pstack(bc);
	return true;
      }
      if(tmax == 0) {
	Gate *new_or = new Gate(tOR);
	bc->install_gate(new_or);
	new_or->add_in_pstack(bc);
	while(children)
	  children->change_parent(new_or);
	type = tNOT; tmin = 0; tmax = 0;
	add_child(new_or);
	add_in_pstack(bc);
	add_parents_in_pstack(bc);
	return true;
      }
      if(tmin == nof_children()) {
	type = tAND; tmin = 0; tmax = 0;
	add_in_pstack(bc);
	return true;
      }
      if(tmin == 0 && tmax == nof_children()) {
	if(determined and value != true)
	  return false;
	transform_into_constant(bc, true);
	add_parents_in_pstack(bc);
	return true;
      }
      if(tmin == 0 and tmax + 1 == nof_children()) {
	Gate *new_and = new Gate(tAND);
	bc->install_gate(new_and);
	new_and->add_in_pstack(bc);
	while(children)
	  children->change_parent(new_and);
	type = tNOT; tmin = 0; tmax = 0;
	add_child(new_and);
	add_in_pstack(bc);
	add_parents_in_pstack(bc);
	return true;
      }
#else
      unsigned int nof_undet = 0, nof_true = 0, nof_false = 0;
      count_child_info(nof_true, nof_false, nof_undet);
      DEBUG_ASSERT(nof_children() == nof_true + nof_false + nof_undet);
      
      if(tmin > tmax or tmin > nof_children()) {
	if(determined && value != false)
	  return false;
	transform_into_constant(bc, false);
	add_parents_in_pstack(bc);
	return true;
      }
      if(tmax > nof_children())
	tmax = nof_children();
      
      assert(tmin <= tmax);
      assert(tmax <= nof_children());

      if(determined and value == true) {
	if(nof_true > tmax)
	  return false;
	/* The tmin limit cannot be reached anymore? */
	if(nof_children() - nof_false < tmin)
	  return false;
	/* Always within (inclusive) tmin and tmax? */
	if(nof_true >= tmin and nof_children() - nof_false <= tmax) {
	  transform_into_constant(bc, true);
	  add_parents_in_pstack(bc);
	  return true;
	}
	if(nof_true == tmax) {
	  while(children) {
	    Gate *child = children->child;
	    if(!child->determined) {
	      child->determined = true;
	      child->value = false;
	      child->add_in_pstack(bc);
	      child->add_parents_in_pstack(bc);
	    }
	    delete children;
	    if(!child->parents) child->add_in_pstack(bc);
	  }
	  transform_into_constant(bc, true);
	  add_parents_in_pstack(bc);
	  return true;
	}
	if(nof_children() - nof_false == tmin) {
	  while(children) {
	    Gate *child = children->child;
	    if(!child->determined) {
	      child->determined = true;
	      child->value = true;
	      child->add_in_pstack(bc);
	      child->add_parents_in_pstack(bc);
	    }
	    delete children;
	    if(!child->parents) child->add_in_pstack(bc);
	  }
	  transform_into_constant(bc, true);
	  add_parents_in_pstack(bc);
	  return true;
	}
	if(nof_true >= tmin)
	  tmin = 0;
	else
	  tmin = tmin - nof_true;
	assert(tmax > nof_true);
	tmax = tmax - nof_true;
	remove_determined_children(bc);
	return true;
      }
      if(determined && value == false) {
	if(nof_true >= tmin and nof_children() - nof_false <= tmax)
	  return false;
	if(nof_true > tmax or nof_children() - nof_false < tmin) {
	  transform_into_constant(bc, false);
	  add_parents_in_pstack(bc);
	  return true;
	}
	if(nof_true >= tmin)
	  tmin = 0;
	else
	  tmin = tmin - nof_true;
	assert(tmax > nof_true);
	tmax = tmax - nof_true;
	remove_determined_children(bc);
	return true;
      }
      assert(!determined);
      if(nof_true >= tmin and nof_children() - nof_false <= tmax) {
	transform_into_constant(bc, true);
	add_parents_in_pstack(bc);
	return true;
      }
      if(nof_true > tmax or nof_children() - nof_false < tmin) {
	transform_into_constant(bc, false);
	add_parents_in_pstack(bc);
	return true;
      }
      if(nof_true >= tmin)
	tmin = 0;
      else
	tmin = tmin - nof_true;
      assert(tmax > nof_true);
      tmax = tmax - nof_true;
      remove_determined_children(bc);
#endif


      if(opts.misc_reductions)
	{
	  if(determined and value == true) {
#ifdef DEBUG_EXPENSIVE_CHECKS
	    for(Gate* g = bc->first_gate; g; g = g->next)
	      assert(g->temp == 0);
#endif
	    for(const ChildAssoc* ci = children; ci; ci = ci->next_child)
	      ci->child->temp = 1;
	    
	    for(const ChildAssoc* ci = children; ci; ci = ci->next_child) {
	      Gate* const child = ci->child;
	      for(const ChildAssoc* pi = child->parents; pi; ) {
		Gate* const parent = pi->parent;
		const ChildAssoc* next_pi = pi->next_parent;
		while(next_pi and next_pi->parent == parent)
		  next_pi = next_pi->next_parent;
		if(parent->type == tAND and parent->nof_children() > tmax) {
		  unsigned int nof_common_children = 0;
		  for(ChildAssoc* pci = parent->children;
		      pci;
		      pci = pci->next_child) {
		    if(pci->child->temp == 1)
		      nof_common_children++;
		    if(nof_common_children > tmax)
		      break;
		  }
		  if(nof_common_children > tmax) {
		    if(parent->determined and parent->value != false) {
		      for(const ChildAssoc* ci2 = children;
			  ci2; ci2 = ci2->next_child)
			ci2->child->temp = 0;
		      return false;
		    }
		    parent->transform_into_constant(bc, false);
		    parent->add_parents_in_pstack(bc);
		  }
		}
		pi = next_pi;
	      }
	    }
	    for(const ChildAssoc* ci = children; ci; ci = ci->next_child)
	      ci->child->temp = 0;
	  }
	}
      
      if(type != tTHRESHOLD)
	return true;

      if(opts.remove_g_not_g_children and
	 !remove_cardinality_g_not_g(bc))
	return false;

      return true;
    }


  case tATLEAST:
    {
      /*
       * Remove determined children
       */
      if(opts.constant_folding) 
	{
	  for(ChildAssoc* ci = children; ci; )
	    {
	      Gate* const child = ci->child;
	      ChildAssoc* next_ci = ci->next_child;
	      if(!child->determined) {
		ci = next_ci;
		continue;
	      }
	      if(child->value == false)
		{
		  delete ci;
		  ci = next_ci;
		  continue;
		}
	      DEBUG_ASSERT(child->value == true);
	      tmin = (tmin == 0)?0:tmin-1;
	      delete ci;
	      ci = next_ci;
	    }
	}
      if(tmin == 0)
	{
	  if(determined and value != true)
	    return false;
	  transform_into_constant(bc, true);
	  add_parents_in_pstack(bc);
	  return true;
	}
      if(tmin > nof_children())
	{
	  if(determined and value != false)
	    return false;
	  transform_into_constant(bc, false);
	  add_parents_in_pstack(bc);
	  return true;
	}
      
      assert(nof_children() > 0);
      assert(tmin <= nof_children());

      if(tmin == nof_children())
	{
	  type = tAND; tmin = 0; tmax = 0;
	  add_parents_in_pstack(bc);
	  add_in_pstack(bc);
	  return true;
	}
      

      /*
      if(!remove_atleast_g_not_g(bc))
	return false;
      */

      return true;
    }

  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }

  _should_not_happen();
  return true;
}





/**************************************************************************
 *
 * Transforms the gate into a constant gate
 * Used by simplify and cnf_normalize
 *
 **************************************************************************/

void Gate::transform_into_constant(BC* const bc, const bool v)
{
  if(determined) {
    if(value != v)
      _should_not_happen();
  } else {
    determined = true;
    value = v;
  }
  type = value?tTRUE:tFALSE;
  while(children) {
    Gate* const child = children->child;
    delete children;
    if(!child->has_parents())
      child->add_in_pstack(bc);
  }
  bc->changed = true;
}







/**************************************************************************
 *
 * Simplifies AND(x,~x,y,z) to F, OR(x,~x,y,z) to T, and EQUIV(x,~x,y,z) to F
 * Also removes duplicate children of AND, OR, and EQUIV
 * Returns false if an inconsistency is found (implying unsatisfiability)
 *
 **************************************************************************/

bool
Gate::remove_duplicate_and_g_not_g_children(BC* const bc,
					    const bool simplify_duplicates,
					    const bool simplify_g_not_g)
{
  if(type == tODD or type == tEVEN)
    return remove_parity_duplicate_and_g_not_g_children(bc,
							simplify_duplicates,
							simplify_g_not_g);
  
  if(!(type == tOR or type == tAND or type == tEQUIV))
    return true;
  
  for(const ChildAssoc* ca = children; ca; ca = ca->next_child) {
    ca->child->temp = 0;
    if(ca->child->type == tNOT)
      ca->child->children->child->temp = 0;
  }

  bool g_not_g_found = false;

  for(ChildAssoc* ca = children; ca; ) {
    Gate* child = ca->child;
    if(child->determined) {
      ca = ca->next_child;
      continue;
    }
    if(simplify_duplicates and child->temp == 1) {
      /* A duplicate child found, remove because
       * AND(x,x,y,z) == AND(x,y,z), OR(x,x,y,z) == OR(x,y,z)
       * EQUIV(x,x,y,z) == EQUIV(x,y,z) */
      ChildAssoc* next_ca = ca->next_child;
      delete ca;
      ca = next_ca;
      continue;
    }
    if(simplify_g_not_g and child->temp == 2) {
      /* child already seen in negative phase! */
      g_not_g_found = true;
      break;
    }
    /* Child not seen in either negative or positive phase,
     * mark it seen in the positive phase */
    child->temp = 1;
    
    if(simplify_g_not_g and child->type == tNOT) {
      Gate* const grandchild = child->first_child();
      if(grandchild->temp == 1) {
	/* grandchild already seen in positive phase! */
	g_not_g_found = true;
	break;
      }
      grandchild->temp = 2;
    }
    ca = ca->next_child;
    continue;
  }

  for(const ChildAssoc* ca = children; ca; ca = ca->next_child) {
    ca->child->temp = 0;
    if(ca->child->type == tNOT)
      ca->child->children->child->temp = 0;
  }

  if(simplify_g_not_g and g_not_g_found)
    {
      if(type == tAND) {
	if(determined and value != false)
	  return false;
	transform_into_constant(bc, false);
	add_parents_in_pstack(bc);
	return true;
      }
      else if(type == tOR) {
	if(determined and value != true)
	  return false;
	transform_into_constant(bc, true);
	add_parents_in_pstack(bc);
	return true;
      }
      else if(type == tEQUIV) {
	if(determined and value != false)
	  return false;
	transform_into_constant(bc, false);
	add_parents_in_pstack(bc);
	return true;
      }
      _should_not_happen();
    }

  if(nof_children() == 1) {
    add_in_pstack(bc);
  }

  return true;
}




#ifdef DEVELOPEMENT
#if 0
/**************************************************************************
 *
 * Removes duplicate children of ODD and EVEN by using equations
 * ODD(x,x,y,z) = ODD(y,z) and EVEN(x,x,y,z) = EVEN(y,z)
 * Returns false if an inconsistency is found (implying unsatisfiability)
 *
 **************************************************************************/

bool
Gate::remove_parity_duplicate_children(BC *bc)
{
  if(!(type == tODD or type == tEVEN))
    return true;

  for(const ChildAssoc* ca = children; ca; ca = ca->next_child)
    ca->child->temp = 0;
  
  for(ChildAssoc *ca = children; ca; ) {
    Gate* child = ca->child;
    if(child->determined) {
      ca = ca->next_child;
      continue;
    }
    if(child->temp == 1) {
      /* A duplicate child found, remove this and previous occurrence because
       * ODD(x,x,y,z) == ODD(y,z) and EVEN(x,x,y,z) == EVEN(y,z) */
      child->temp = 0;
      ChildAssoc *ca2 = children;
      while(ca2 != ca) {
	if(ca2->child == child) {
	  delete ca2;
	  break;
	}
	ca2 = ca2->next_child;
      }
      assert(ca2 != ca);
      ChildAssoc *next_ca = ca->next_child;
      delete ca;
      ca = next_ca;
      if(!child->parents)
	child->add_in_pstack(bc);
      continue;
    }
    child->temp = 1;

    ca = ca->next_child;
    continue;
  }

  for(const ChildAssoc* ca = children; ca; ca = ca->next_child)
    ca->child->temp = 0;

  if(!children) {
    if(type == tODD) {
      if(determined and value != false)
	return false;
      transform_into_constant(bc, false);
      add_parents_in_pstack(bc);
      return true;
    }
    else if(type == tEVEN) {
      if(determined and value != true)
	return false;
      transform_into_constant(bc, true);
      add_parents_in_pstack(bc);
      return true;
    }
    else
      assert(should_not_happen);
  }

  if(nof_children() == 1) {
    add_in_pstack(bc);
  }

  return true;
}
#endif
#endif




/**************************************************************************
 *
 * Simplifies [L,U](x,~x,y,z) to [L-1,U-1](y,z)
 * Returns false if an inconsistency is found (implying unsatisfiability)
 *
 **************************************************************************/

bool
Gate::remove_cardinality_g_not_g(BC *bc)
{
  if(type != tTHRESHOLD)
    return true;

  for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
    ca->child->temp = 0;
    if(ca->child->type == tNOT)
      ca->child->children->child->temp = 0;
  }

  for(ChildAssoc *ca = children; ca; ) {
    Gate *child = ca->child;
    if(child->temp == 2) {
      /* child already seen in negative phase: simplify
       * [L,U](~x,y,x,z) to [L-1,U-1](y,z) */
      child->temp = 0;
      ChildAssoc *ca2 = children;
      while(ca2 != ca) {
	Gate * const child2 = ca2->child;
	if(child2->type == tNOT && child2->children->child == child) {
	  child2->temp = 0;
	  delete ca2;
	  if(!child2->parents)
	    child2->add_in_pstack(bc);
 	  break;
	}
	ca2 = ca2->next_child;
      }
      assert(ca2 != ca);
      ChildAssoc *next_ca = ca->next_child;
      delete ca;
      ca = next_ca;
      if(tmax == 0) {
	if(determined and value != false)
	  return false;
	transform_into_constant(bc, false);
	add_parents_in_pstack(bc);
	return true;
      }
      tmin = (tmin == 0)?0:tmin - 1;
      tmax = tmax - 1;
      continue;
    }
    if(child->temp == 1) {
      ;
    }
    child->temp = 1;

    if(child->type == tNOT) {
      Gate *grandchild = child->children->child;
      if(grandchild->temp == 1) {
	/* grandchild already seen in positive phase: simplify
	 * [L,U](x,y,~x,z) to [L-1,U-1](y,z) */
	child->temp = 0;
	grandchild->temp = 0;
	ChildAssoc *ca2 = children;
	while(ca2 != ca) {
	  if(ca2->child == grandchild) {
	    delete ca2;
	    break;
	  }
	  ca2 = ca2->next_child;
	}
	assert(ca2 != ca);
	ChildAssoc *next_ca = ca->next_child;
	delete ca;
	ca = next_ca;
	if(!child->parents)
	  child->add_in_pstack(bc);
	if(tmax == 0) {
	  if(determined and value != false)
	    return false;
	  transform_into_constant(bc, false);
	  add_parents_in_pstack(bc);
	  return true;
	}
	tmin = (tmin == 0)?0:tmin - 1;
	tmax = tmax - 1;
	continue;
      }
      grandchild->temp = 2;
    }
    ca = ca->next_child;
    continue;
  }

  for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
    ca->child->temp = 0;
    if(ca->child->type == tNOT)
      ca->child->children->child->temp = 0;
  }

#ifdef DEBUG_EXPENSIVE_CHECKS
  for(const Gate* g = bc->first_gate; g; g = g->next)
    assert(g->temp == 0);
#endif

  return true;
}




/**************************************************************************
 *
 * Apply ODD(x,~x,y,z) -> EVEN(y,z) and EVEN(x,~x,y,z) = ODD(y,z).
 * Returns false if an inconsistency is found (implying unsatisfiability)
 *
 **************************************************************************/

bool
Gate::remove_parity_duplicate_and_g_not_g_children(BC* const bc,
						   const bool remove_duplicates,
						   const bool remove_g_not_g)
{
  if(!(type == tODD or type == tEVEN))
    return true;

  for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
    ca->child->temp = 0;
    if(ca->child->type == tNOT)
      ca->child->children->child->temp = 0;
  }

  for(ChildAssoc* ca = children; ca; )
    {
      Gate* child = ca->child;

      if(remove_duplicates and child->temp == 1)
	{
	  child->temp = 0;
	  if(child->type == tNOT) child->first_child()->temp = 0;
	  ChildAssoc* ci2 = children;
	  for( ; ci2 != ca; ci2 = ci2->next_child) {
	    Gate* const child2 = ci2->child;
	    if(child2 == child) {
	      delete ci2;
	      if(!child2->has_parents())
		child2->add_in_pstack(bc);
	      break;
	    }
	  }
	  DEBUG_ASSERT(ci2 != ca);
	  ChildAssoc *next_ca = ca->next_child;
	  delete ca;
	  ca = next_ca;
	  continue;
	}
      if(remove_g_not_g and child->temp == 2)
	{
	  child->temp = 0;
	  ChildAssoc *ci2 = children;
	  for( ; ci2 != ca; ci2 = ci2->next_child) {
	    Gate* const child2 = ci2->child;
	    if(child2->type == tNOT and child2->first_child() == child) {
	      child2->temp = 0;
	      delete ci2;
	      if(!child2->has_parents())
		child2->add_in_pstack(bc);
	      break;
	    }
	  }
	  assert(ci2 != ca);
	  ChildAssoc *next_ca = ca->next_child;
	  delete ca;
	  ca = next_ca;
	  if(type == tODD) type = tEVEN;
	  else if(type == tEVEN) type = tODD;
	  else _should_not_happen();
	  continue;
	}

      child->temp = 1;

      if(remove_g_not_g and child->type == tNOT)
	{
	  Gate* const grandchild = child->first_child();
	  if(grandchild->temp == 1) {
	    child->temp = 0;
	    grandchild->temp = 0;
	    if(grandchild->type == tNOT) grandchild->first_child()->temp = 0;
	    ChildAssoc *ca2 = children;
	    for( ; ca2 != ca; ca2 = ca2->next_child) {
	      if(ca2->child == grandchild) {
		delete ca2;
		break;
	      }
	    }
	    assert(ca2 != ca);
	    ChildAssoc *next_ca = ca->next_child;
	    delete ca;
	    ca = next_ca;
	    if(!child->has_parents())
	      child->add_in_pstack(bc);
	    if(type == tODD) type = tEVEN;
	    else if(type == tEVEN) type = tODD;
	    else _should_not_happen();
	    continue;
	  }
	  grandchild->temp = 2;
	}
      ca = ca->next_child;
      continue;
    }

  for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
    ca->child->temp = 0;
    if(ca->child->type == tNOT)
      ca->child->children->child->temp = 0;
  }

#ifdef DEBUG_EXPENSIVE_CHECKS
  for(const Gate* g = bc->first_gate; g; g = g->next)
    assert(g->temp == 0);
#endif

  return true;
}





void
Gate::remove_determined_children(BC* const bc)
{
  for(ChildAssoc* ca = children; ca; ) {
    Gate* const child = ca->child;
    if(child->determined) {
      bc->changed = true;
      ChildAssoc* next_ca = ca->next_child;
      delete ca;
      ca = next_ca;
      if(!child->parents)
	child->add_in_pstack(bc);
      continue;
    }
    ca = ca->next_child;
  }
}















/**************************************************************************
 *
 * Transforms the gate into a form that can be easily translated into
 * conjunctive normal form clauses.
 * Returns false if an inconsistency is found (implying unsatisfiability).
 *
 **************************************************************************/

class mypair {
public:
  mypair(unsigned int i1, unsigned int j1) : i(i1), j(j1) {}
  unsigned int i;
  unsigned int j;
};

bool
Gate::cnf_normalize(BC* const bc)
{
  if(type == tDELETED)
    return true;
  
  /*
  if(is_commutative())
    sort_children();
  */

  switch(type) {
  case tFALSE:
    {
      DEBUG_ASSERT(!children);
      if(determined and value != false)
	return false;
      determined = true;
      value = false;
      return true;
    }

  case tTRUE:
    {
      DEBUG_ASSERT(!children);
      if(determined and value != true)
	return false;
      determined = true;
      value = true;
      return true;
    }

  case tVAR:
    {
      DEBUG_ASSERT(nof_children() == 0);
      return true;
    }

  case tREF:
    {
      /*
       * Remove REFs
       */
      DEBUG_ASSERT(nof_children() == 1);
      Gate* const child = children->child;
      DEBUG_ASSERT(child != this);
      if(determined) {
	if(child->determined and value != child->value)
	  return false;
	child->determined = true;
	child->value = value;
	child->add_in_pstack(bc);
      }
      /* Redirect parents and handles to the child */
      while(parents) parents->change_child(child);
      while(handles) handles->change_gate(child);
      /* Mark this gate as deleted */
      remove_all_children();
      type = tDELETED;
      return true;
    }

  case tNOT:
    {
      DEBUG_ASSERT(nof_children() == 1);
      Gate * const child = children->child;
      /*
       * The value of a determined NOT-gate must be propagated downwards!
       * Otherwise the NOT-less translation may fail...
       */
      if(determined) {
	if(child->determined && child->value == value)
	  return false;
	child->determined = true;
	child->value = !value;
	child->add_in_pstack(bc);
	transform_into_constant(bc, value);
	return true;
      }
      /*
       * Double negations must be eliminated in order to the NOT-less
       * translation to work properly
       */
      DEBUG_ASSERT(!determined);
      if(child->type == tNOT) {
	/* g := ~~h  --> g := h */
	DEBUG_ASSERT(child->nof_children() == 1);
	Gate * const grandchild = child->children->child;
	DEBUG_ASSERT(grandchild != this);
	while(parents) parents->change_child(grandchild);
	while(handles) handles->change_gate(grandchild);
	remove_all_children();
	type = tDELETED;
	return true;
      }
      return true;
    }

  case tOR:
  case tAND:
    {
      DEBUG_ASSERT(nof_children() >= 1);
      if(nof_children() == 1) {
	/* Unary ANDs and ORs are removed */
	type = tREF;
	add_in_pstack(bc);
	return true;
      }
      return true;
    }
    
  case tEQUIV:
    {
      DEBUG_ASSERT(nof_children() >= 1);
      if(nof_children() == 1) {
	if(determined and value != true)
	  return false;
	transform_into_constant(bc, true);
	return true;
      }
      if(nof_children() == 2) {
	/* binary EQUIVS are OK */
	return true;
      }
      DEBUG_ASSERT(nof_children() >= 3);
      Gate* const new_child1 = new Gate(tAND);
      bc->install_gate(new_child1);
      for(const ChildAssoc* ca = children; ca; ca = ca->next_child)
	new_child1->add_child(ca->child);
      new_child1->add_in_pstack(bc);
      
      Gate* const new_child2 = new Gate(tAND);
      bc->install_gate(new_child2);
      new_child2->add_in_pstack(bc);
      for(const ChildAssoc* ca = children; ca; ca = ca->next_child) {
	Gate* const not_child = new Gate(Gate::tNOT, ca->child);
	bc->install_gate(not_child);
	not_child->add_in_pstack(bc);
	new_child2->add_child(not_child);
      }
      
      type = tOR;
      remove_all_children();
      add_child(new_child1);
      add_child(new_child2);
      return true;
    }

  case Gate::tITE:
    {
      DEBUG_ASSERT(nof_children() == 3);
      return true;
    }

  case Gate::tTHRESHOLD:
    {
      /* Threshold gates must be eliminated! */
      DEBUG_ASSERT(nof_children() >= 1);
      if(tmin > nof_children()) {
	if(determined and value != false)
	  return false;
	transform_into_constant(bc, false);
	tmin = 0; tmax = 0;
	return true;
      }
      if(tmax > nof_children())
	tmax = nof_children();
      if(tmin > tmax) {
	if(determined and value != false)
	  return false;
	transform_into_constant(bc, false);
	tmin = 0; tmax = 0;
	return true;
      }

      DEBUG_ASSERT(tmin <= tmax);
      DEBUG_ASSERT(tmax <= nof_children());
      
      if(nof_children() == 1) {
	if(tmin == 0 and tmax == 0) {
	  type = Gate::tNOT;
	  tmin = 0; tmax = 0;
	  add_in_pstack(bc);
	  return true;
	}
	else if(tmin == 0 and tmax == 1) {
	  if(determined and value != true)
	    return false;
	  transform_into_constant(bc, true);
	  tmin = 0; tmax = 0;
	  return true;
	}
	else if(tmin == 1 and tmax == 1) {
	  type = Gate::tREF;
	  tmin = 0; tmax = 0;
	  add_in_pstack(bc);
	  return true;
	}
	assert(should_not_happen);
      }
      
      DEBUG_ASSERT(nof_children() >= 2);

      if(tmin == 0 and tmax == nof_children()) {
	if(determined and value != true)
	  return false;
	transform_into_constant(bc, true);
	tmin = 0; tmax = 0;
	return true;      
      }
      if(tmin == 1 and tmax == nof_children()) {
	type = tOR;
	tmin = 0; tmax = 0;
	add_in_pstack(bc);
	return true;
      }
      

#if 0
      /* [l,u](g1,g2,...,gn) == ITE(g1,[l-1,u-1](g2,...,gn),[l,u](g2,...,gn))
	 This is a simple recursive translation. */
      Gate *new_child1 = new Gate(tTHRESHOLD);
      new_child1->tmin = (tmin==0)?0:tmin-1;
      new_child1->tmax = (tmax==0)?0:tmax-1;
      for(ChildAssoc *ca = children->next_child; ca; ca = ca->next_child)
	new_child1->add_child(ca->child);
      bc->install_gate(new_child1);
      new_child1->add_in_pstack(bc);
      
      Gate *new_child2 = new Gate(tTHRESHOLD);
      new_child2->tmin = tmin;
      new_child2->tmax = tmax;
      for(ChildAssoc *ca = children->next_child; ca; ca = ca->next_child)
	new_child2->add_child(ca->child);
      bc->install_gate(new_child2);
      new_child2->add_in_pstack(bc);
      
      Gate *first_child = children->child;
      remove_all_children();
      type = tITE;
      add_child(new_child2);
      add_child(new_child1);
      add_child(first_child);    
      add_in_pstack(bc);
      return true;
#else
      /* A heuristic choice between adder and other construction... */
      if(!((tmax <= 2) or
	   (tmin + 2 >= nof_children()) or
	   (tmin <= 2 and tmax + 2 >= nof_children())))
	{
	  /* Do the adder construction */
	  std::list<Gate *> child_list;
	  for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	    child_list.push_back(ca->child);
	  
	  std::list <Gate *> *sum_gates = bc->add_true_gate_counter(&child_list);
	  std::list<Gate*> *tmin_gates = bc->add_unsigned_constant(tmin);
	  std::list<Gate*> *tmax_gates = bc->add_unsigned_constant(tmax);
	  Gate *tmin_result_gate = bc->add_unsigned_ge(sum_gates, tmin_gates);
	  Gate *tmax_result_gate = bc->add_unsigned_le(sum_gates, tmax_gates);
	  remove_all_children();
	  type = tAND;
	  add_child(tmin_result_gate);
	  add_child(tmax_result_gate);
	  
	  delete sum_gates;
	  delete tmin_gates;
	  delete tmax_gates;
	  return true;
	}

      /* The sharing decomposition construction */

      if(tmin == 0) {
	Gate *new_child = new Gate(Gate::tATLEAST);
	bc->install_gate(new_child);
	new_child->add_in_pstack(bc);
	new_child->tmin = tmax + 1;
	while(children)
	  children->change_parent(new_child);
	add_child(new_child);
	type = tNOT;
	add_in_pstack(bc);
	return true;
      }
      
      if(tmax == nof_children()) {
	DEBUG_ASSERT(tmin > 0);
	type = tATLEAST;
	add_in_pstack(bc);
	return true;
      }
      
      DEBUG_ASSERT(tmin > 0);
      DEBUG_ASSERT(tmax < nof_children());
      DEBUG_ASSERT(tmin <= tmax);
      
      Gate *new_child1 = new Gate(tATLEAST);
      bc->install_gate(new_child1);
      new_child1->add_in_pstack(bc);
      new_child1->tmin = tmin;
      for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	new_child1->add_child(ca->child);
      
      Gate *new_child2 = new Gate(tATLEAST);
      bc->install_gate(new_child2);
      new_child2->add_in_pstack(bc);
      new_child2->tmin = tmax+1;
      while(children)
	children->change_parent(new_child2);
      
      Gate *new_child3 = new Gate(tNOT, new_child2);
      bc->install_gate(new_child3);
      new_child3->add_in_pstack(bc);
      
      add_child(new_child1);
      add_child(new_child3);
      type = Gate::tAND;
#endif
      return true;
    }
    
  case tATLEAST:
    {
      /* The ATLEAST-gates must be eliminated */
      DEBUG_ASSERT(nof_children() >= 1);
      if(tmin == 0) {
	if(determined && value != true)
	  return false;
	transform_into_constant(bc, true);
	tmin = 0;
	return true;
      }
      if(tmin > nof_children()) {
	if(determined && value != false)
	  return false;
	transform_into_constant(bc, false);
	tmin = 0;
	return true;
      }
      if(tmin == 1) {
	type = tOR;
	tmin = 0;
	add_in_pstack(bc);
	return true;
      }
      if(tmin == nof_children()) {
	type = tAND;
	tmin = 0;
	add_in_pstack(bc);
	return true;
      }
      DEBUG_ASSERT(nof_children() >= 2);
      DEBUG_ASSERT(tmin < nof_children());

#define POLYNOMIAL_ATLEAST_REWRITING
#ifdef POLYNOMIAL_ATLEAST_REWRITING
      /* Based on the equivalence
	 (>= l)(g1,...,gn) == (g1 & (>= l-1)(g2,...,gn)) | (>= l)(g2,...,gn)
	 By sharing the subresults, the size of the translation is O(l * n) */

      /* Build the vector of the child gates */
      std::vector<Gate*> childs;
      for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	childs.push_back(ca->child);
      
      Gate ***array = (Gate***)malloc(sizeof(Gate**) * (tmin + 1));
      for(unsigned int i = 0; i <= tmin; i++) {
	array[i] = (Gate**)malloc(sizeof(Gate*) * (childs.size()+1));
	for(unsigned int j = 0; j <= childs.size(); j++) {
	  array[i][j] = new Gate(tUNDEF);
	  array[i][j]->temp = 0;
	}
      }
      
      delete array[tmin][childs.size()];
      array[tmin][childs.size()] = this;
      remove_all_children();
      array[tmin][childs.size()]->temp = 0;
      
      std::list<mypair> todo;
      todo.push_front(mypair(tmin,childs.size()));
      while(!todo.empty()) {
	mypair pair = todo.front();
	todo.pop_front();
	unsigned int i = pair.i;
	unsigned int j = pair.j;
	DEBUG_ASSERT(i > 0);
	DEBUG_ASSERT(i <= tmin);
	DEBUG_ASSERT(j <= childs.size());
	DEBUG_ASSERT(j >= i);
	if(array[i][j]->temp != 0)
	  continue;
	array[i][j]->temp = 1;
	if(array[i][j] != this) {
	  bc->install_gate(array[i][j]);
	  array[i][j]->add_in_pstack(bc);
	}
	if(i == j) {
	  if(i == 1) {
	    array[i][j]->type = tREF;
	    array[i][j]->remove_all_children();
	    array[i][j]->add_child(childs[j-1]);
	    continue;
	  }
	  array[i][j]->type = tAND;
	  array[i][j]->remove_all_children();
	  array[i][j]->add_child(childs[j-1]);
	  array[i][j]->add_child(array[i-1][j-1]);
	  todo.push_front(mypair(i-1,j-1));
	  continue;
	}
	if(i == 1) {
	  array[i][j]->type = tOR;
	  array[i][j]->remove_all_children();
	  array[i][j]->add_child(childs[j-1]);
	  array[i][j]->add_child(array[i][j-1]);
	  todo.push_front(mypair(i,j-1));
	  continue;
	}
	Gate *new_gate = new Gate(tAND,childs[j-1],array[i-1][j-1]);
	todo.push_front(mypair(i-1,j-1));
	bc->install_gate(new_gate);
	new_gate->add_in_pstack(bc);
	array[i][j]->type = tOR;
	array[i][j]->remove_all_children();
	array[i][j]->add_child(new_gate);
	array[i][j]->add_child(array[i][j-1]);
	todo.push_front(mypair(i,j-1));
      }
      
      for(unsigned int i = 0; i <= tmin; i++) {
	for(unsigned int j = 0; j <= childs.size(); j++) {
	  if(array[i][j]->type == tUNDEF) {
	    DEBUG_ASSERT(array[i][j]->temp == 0);
	    delete array[i][j];
	  } else {
	    DEBUG_ASSERT(array[i][j]->temp != 0);
	  }
	}
	free(array[i]);
      }
      free(array);
      
      return true;
#else      
      /* A simpler rewriting of ATLEASTs */

      DEBUG_ASSERT(tmin >= 2);
      /* (>= l)(g1,...,gn) == (g1 & (>= l-1)(g2,...,gn)) | (>= l)(g2,...,gn) */
      Gate *new_child1 = new Gate(tAND);
      bc->install_gate(new_child1);
      new_child1->add_in_pstack(bc);
      new_child1->add_child(children->child);
      delete children;

      Gate *new_child2 = new Gate(tATLEAST);
      bc->install_gate(new_child2);
      new_child2->add_in_pstack(bc);
      for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	new_child2->add_child(ca->child);
      new_child2->tmin = tmin - 1;
      new_child1->add_child(new_child2);
      
      Gate *new_child3 = new Gate(tATLEAST);
      bc->install_gate(new_child3);
      new_child3->add_in_pstack(bc);
      while(children) {
	new_child3->add_child(children->child);
	delete children;
      }
      new_child3->tmin = tmin;
      
      type = tOR;
      tmin = 0;
      add_child(new_child1);
      add_child(new_child3);
      return true;
#endif
    }

  case tEVEN:
    {
      DEBUG_ASSERT(nof_children() >= 1);
      if(nof_children() == 1) {
	/* EVEN(x) == NOT(x) */
	type = Gate::tNOT;
	add_in_pstack(bc);
	return true;
      }
      if(nof_children() == 2) {
	/* binary EVENs are OK */
	return true;
      }
      Gate* const new_odd = new Gate(tODD);
      bc->install_gate(new_odd);
      new_odd->add_in_pstack(bc);
      while(children) {
	new_odd->add_child(children->child);
	delete children;
      }
      type = tNOT;
      add_child(new_odd);
      add_in_pstack(bc);
      return true;
    }

  case tODD:
    {
      DEBUG_ASSERT(nof_children() >= 1);
      if(nof_children() == 1) {
	/* ODD(x) == x */
	type = Gate::tREF;
	add_in_pstack(bc);
	return true;
      }
      if(nof_children() == 2) {
	/* Binary ODDs are OK */
	return true;
      }
      Gate* const new_odd = new Gate(tODD);
      bc->install_gate(new_odd);
      new_odd->add_in_pstack(bc);
      Gate* const child1 = children->child;
      delete children;
      while(children) {
	new_odd->add_child(children->child);
	delete children;
      }
      add_child(new_odd);
      add_child(child1);
      return true;
    }

  case tDELETED:
    DEBUG_ASSERT(children == 0);
    DEBUG_ASSERT(parents == 0);
    return true;

  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }

  assert(should_not_happen);
  return true;
}




/**************************************************************************
 *
 * Transforms the gate into a form that can be easily translated into
 * edimacs format.
 * Returns false if an inconsistency is found (implying unsatisfiability)
 *
 **************************************************************************/

bool
Gate::edimacs_normalize(BC* const bc)
{
  if(type == tDELETED)
    return true;
  
  if(is_commutative())
    sort_children();
  
  switch(type) {
  case tFALSE:
    {
      DEBUG_ASSERT(nof_children() == 0);
      if(determined and value != false)
	return false;
      determined = true;
      value = false;
      return true;
    }

  case tTRUE:
    {
      DEBUG_ASSERT(nof_children() == 0);
      if(determined and value != true)
	return false;
      determined = true;
      value = true;
      return true;
    }

  case tVAR:
    {
      DEBUG_ASSERT(nof_children() == 0);
      return true;
    }

  case tREF:
    {
      /*
       * Remove REFs
       */
      DEBUG_ASSERT(nof_children() == 1);
      Gate * const child = first_child();
      DEBUG_ASSERT(child != this);
      if(determined) {
	if(child->determined && value != child->value)
	  return false;
	child->determined = true;
	child->value = value;
	child->add_in_pstack(bc);
      }
      /* Redirect parents and handles to the chiild */
      while(parents) parents->change_child(child);
      while(handles) handles->change_gate(child);
      /* Mark this gate deleted */
      remove_all_children();
      type = tDELETED;
      return true;
    }

  case tNOT:
    {
      DEBUG_ASSERT(nof_children() == 1);
      Gate * const child = first_child();
      /*
       * The value of a determined NOT-gate must be propagated downwards!
       * Otherwise the NOT-less translation may fail...
       */
      if(determined) {
	if(child->determined && child->value == value)
	  return false;
	child->determined = true;
	child->value = !value;
	child->add_in_pstack(bc);
	transform_into_constant(bc, value);
	return true;
      }
      /*
       * Double negations must be eliminated in order to the NOT-less
       * translation to work properly
       */
      DEBUG_ASSERT(!determined);
      if(child->type == tNOT) {
	/* g := NOT(NOT(h))  --> g := h */
	DEBUG_ASSERT(child->nof_children() == 1);
	Gate* const grandchild = child->first_child();
	DEBUG_ASSERT(grandchild != this);
	while(parents) parents->change_child(grandchild);
	while(handles) handles->change_gate(grandchild);
	remove_all_children();
	type = tDELETED;
	return true;
      }
      return true;
    }

  case tOR:
  case tAND:
    {
      DEBUG_ASSERT(nof_children() >= 1);
      if(nof_children() == 1) {
	/* Unary ANDs and ORs are removed */
	type = tREF;
	add_in_pstack(bc);
	return true;
      }
      return true;
    }
    
  case tEQUIV:
    {
      DEBUG_ASSERT(nof_children() >= 1);
      if(nof_children() == 1) {
	/* EQUIV(x) = T */
	if(determined and value == false)
	  return false;
	transform_into_constant(bc, true);
	return true;
      }
      return true;
    }

  case Gate::tITE:
    {
      DEBUG_ASSERT(nof_children() == 3);
      return true;
    }

  case Gate::tTHRESHOLD:
    {
      /* Threshold gates must be eliminated! */
      
      DEBUG_ASSERT(nof_children() >= 1);
      if(tmin > nof_children()) {
	/* trivially false */
	if(determined and value == true)
	  return false;
	transform_into_constant(bc, false);
	tmin = 0; tmax = 0;
	return true;
      }
      if(tmax > nof_children())
	tmax = nof_children();
      if(tmin > tmax) {
	/* trivially false */
	if(determined and value == true)
	  return false;
	transform_into_constant(bc, false);
	tmin = 0; tmax = 0;
	return true;
      }

      DEBUG_ASSERT(tmin <= tmax and tmax <= nof_children());
      
      if(nof_children() == 1) {
	if(tmin == 0 and tmax == 0) {
	  /* [0,0](x) == NOT(x) */
	  type = Gate::tNOT;
	  tmin = 0; tmax = 0;
	  add_in_pstack(bc);
	  return true;
	}
	else if(tmin == 0 and tmax == 1) {
	  /* [0,1](x) == T */
	  if(determined and value == false)
	    return false;
	  transform_into_constant(bc, true);
	  tmin = 0; tmax = 0;
	  return true;
	}
	else if(tmin == 1 and tmax == 1) {
	  /* [1,1](x) == x */
	  type = Gate::tREF;
	  tmin = 0; tmax = 0;
	  add_in_pstack(bc);
	  return true;
	}
	assert(should_not_happen);
      }
      
      DEBUG_ASSERT(nof_children() >= 2);

      if(tmin == 0 and tmax == nof_children()) {
	/* [0,n](g1,...,gn) == T */
	if(determined and value == false)
	  return false;
	transform_into_constant(bc, true);
	tmin = 0; tmax = 0;
	return true;      
      }
      
      if(tmin == 0)
	{
	  /* [0,k](g1,...,gn) = ~(>= k+1)(g1,...,gn) */
	  Gate *new_child = new Gate(Gate::tATLEAST);
	  bc->install_gate(new_child);
	  new_child->add_in_pstack(bc);
	  new_child->tmin = tmax + 1;
	  while(children)
	    children->change_parent(new_child);
	  add_child(new_child);
	  type = tNOT;
	  add_in_pstack(bc);
	  return true;
	}
      
      if(tmax == nof_children())
	{
	  DEBUG_ASSERT(tmin > 0);
	  /* [l,n](g1,...gn) = (>= l)(g1,...,gn) */
	  type = tATLEAST;
	  add_in_pstack(bc);
	  return true;
	}
      
      DEBUG_ASSERT(0 < tmin);
      DEBUG_ASSERT(tmin <= tmax);
      DEBUG_ASSERT(tmax < nof_children());
      
      /* [l,u](g1,...,gn) =  (>= l)(g1,...,gn) & ~(>= u+1)(g1,...,gn) */
      Gate *new_child1 = new Gate(tATLEAST);
      bc->install_gate(new_child1);
      new_child1->add_in_pstack(bc);
      new_child1->tmin = tmin;
      for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	new_child1->add_child(ca->child);
      
      Gate *new_child2 = new Gate(tATLEAST);
      bc->install_gate(new_child2);
      new_child2->add_in_pstack(bc);
      new_child2->tmin = tmax+1;
      while(children)
	children->change_parent(new_child2);
      Gate *new_child3 = new Gate(tNOT, new_child2);
      bc->install_gate(new_child3);
      new_child3->add_in_pstack(bc);
      
      add_child(new_child1);
      add_child(new_child3);
      type = Gate::tAND;

      return true;
    }
    
  case tATLEAST:
    {
      DEBUG_ASSERT(nof_children() >= 1);
      if(tmin == 0) {
	/* trivially true */
	if(determined and value != true)
	  return false;
	transform_into_constant(bc, true);
	tmin = 0;
	return true;
      }
      if(tmin > nof_children()) {
	/* trivially false */
	if(determined and value != false)
	  return false;
	transform_into_constant(bc, false);
	tmin = 0;
	return true;
      }
      if(tmin == nof_children()) {
	/* (>= n)(g1,...,gn) == AND(g1,...,gn) */
	type = tAND;
	tmin = 0;
	add_in_pstack(bc);
	return true;
      }
      DEBUG_ASSERT(nof_children() >= 2);
      DEBUG_ASSERT(tmin < nof_children());

      return true;
    }

  case tEVEN:
    {
      DEBUG_ASSERT(nof_children() >= 1);
      if(nof_children() == 1) {
	/* EVEN(x) == NOT(x) */
	type = Gate::tNOT;
	add_in_pstack(bc);
	return true;
      }
      return true;
    }

  case tODD:
    {
      DEBUG_ASSERT(nof_children() >= 1);
      if(nof_children() == 1) {
	/* ODD(x) == x */
	type = Gate::tREF;
	add_in_pstack(bc);
	return true;
      }
      return true;
    }

  case tDELETED:
    DEBUG_ASSERT(children == 0);
    DEBUG_ASSERT(parents == 0);
    return true;

  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }

  assert(should_not_happen);
  return true;
}



/*
 * Random bits generated by
 * http://www.fourmilab.ch/hotbits/
 */
static unsigned int rtab[256] = {
  0xAEAA35B8, 0x65632E16, 0x155EDBA9, 0x01349B39,
  0x8EB8BD97, 0x8E4C5367, 0x8EA78B35, 0x2B1B4072,
  0xC1163893, 0x269A8642, 0xC79D7F6D, 0x6A32DEA0,
  0xD4D2DA56, 0xD96D4F47, 0x47B5F48A, 0x2587C6BF,
  0x642B71D8, 0x5DBBAF58, 0x5C178169, 0xA16D9279,
  0x75CDA063, 0x291BC48B, 0x01AC2F47, 0x5416DF7C,
  0x45307514, 0xB3E1317B, 0xE1C7A8DE, 0x3ACDAC96,
  0x11B96831, 0x32DE22DD, 0x6A1DA93B, 0x58B62381,
  0x283810E2, 0xBC30E6A6, 0x8EE51705, 0xB06E8DFB,
  0x729AB12A, 0xA9634922, 0x1A6E8525, 0x49DD4E19,
  0xE5DB3D44, 0x8C5B3A02, 0xEBDE2864, 0xA9146D9F,
  0x736D2CB4, 0xF5229F42, 0x712BA846, 0x20631593,
  0x89C02603, 0xD5A5BF6A, 0x823F4E18, 0x5BE5DEFF,
  0x1C4EBBFA, 0x5FAB8490, 0x6E559B0C, 0x1FE528D6,
  0xB3198066, 0x4A965EB5, 0xFE8BB3D5, 0x4D2F6234,
  0x5F125AA4, 0xBCC640FA, 0x4F8BC191, 0xA447E537,
  0xAC474D3C, 0x703BFA2C, 0x617DC0E7, 0xF26299D7,
  0xC90FD835, 0x33B71C7B, 0x6D83E138, 0xCBB1BB14,
  0x029CF5FF, 0x7CBD093D, 0x4C9825EF, 0x845C4D6D,
  0x124349A5, 0x53942D21, 0x800E60DA, 0x2BA6EB7F,
  0xCEBF30D3, 0xEB18D449, 0xE281F724, 0x58B1CB09,
  0xD469A13D, 0x9C7495C3, 0xE53A7810, 0xA866C08E,
  0x832A038B, 0xDDDCA484, 0xD5FE0DDE, 0x0756002B,
  0x2FF51342, 0x60FEC9C8, 0x061A53E3, 0x47B1884E,
  0xDC17E461, 0xA17A6A37, 0x3158E7E2, 0xA40D873B,
  0x45AE2140, 0xC8F36149, 0x63A4EE2D, 0xD7107447,
  0x6F90994F, 0x5006770F, 0xC1F3CA9A, 0x91B317B2,
  0xF61B4406, 0xA8C9EE8F, 0xC6939B75, 0xB28BBC3B,
  0x36BF4AEF, 0x3B12118D, 0x4D536ECF, 0x9CF4B46B,
  0xE8AB1E03, 0x8225A360, 0x7AE4A130, 0xC4EE8B50,
  0x50651797, 0x5BB4C59F, 0xD120EE47, 0x24F3A386,
  0xBE579B45, 0x3A378EFC, 0xC5AB007B, 0x3668942B,
  0x2DBDCC3A, 0x6F37F64C, 0xC24F862A, 0xB6F97FCF,
  0x9E4FA23D, 0x551AE769, 0x46A8A5A6, 0xDC1BCFDD,
  0x8F684CF9, 0x501D811B, 0x84279F80, 0x2614E0AC,
  0x86445276, 0xAEA0CE71, 0x0812250F, 0xB586D18A,
  0xC68D721B, 0x44514E1D, 0x37CDB99A, 0x24731F89,
  0xFA72E589, 0x81E6EBA2, 0x15452965, 0x55523D9D,
  0x2DC47E14, 0x2E7FA107, 0xA7790F23, 0x40EBFDBB,
  0x77E7906B, 0x6C1DB960, 0x1A8B9898, 0x65FA0D90,
  0xED28B4D8, 0x34C3ED75, 0x768FD2EC, 0xFAB60BCB,
  0x962C75F4, 0x304F0498, 0x0A41A36B, 0xF7DE2A4A,
  0xF4770FE2, 0x73C93BBB, 0xD21C82C5, 0x6C387447,
  0x8CDB4CB9, 0x2CC243E8, 0x41859E3D, 0xB667B9CB,
  0x89681E8A, 0x61A0526C, 0x883EDDDC, 0x539DE9A4,
  0xC29E1DEC, 0x97C71EC5, 0x4A560A66, 0xBD7ECACF,
  0x576AE998, 0x31CE5616, 0x97172A6C, 0x83D047C4,
  0x274EA9A8, 0xEB31A9DA, 0x327209B5, 0x14D1F2CB,
  0x00FE1D96, 0x817DBE08, 0xD3E55AED, 0xF2D30AFC,
  0xFB072660, 0x866687D6, 0x92552EB9, 0xEA8219CD,
  0xF7927269, 0xF1948483, 0x694C1DF5, 0xB7D8B7BF,
  0xFFBC5D2F, 0x2E88B849, 0x883FD32B, 0xA0331192,
  0x8CB244DF, 0x41FAF895, 0x16902220, 0x97FB512A,
  0x2BEA3CC4, 0xAF9CAE61, 0x41ACD0D5, 0xFD2F28FF,
  0xE780ADFA, 0xB3A3A76E, 0x7112AD87, 0x7C3D6058,
  0x69E64FFF, 0xE5F8617C, 0x8580727C, 0x41F54F04,
  0xD72BE498, 0x653D1795, 0x1275A327, 0x14B499D4,
  0x4E34D553, 0x4687AA39, 0x68B64292, 0x5C18ABC3,
  0x41EABFCC, 0x92A85616, 0x82684CF8, 0x5B9F8A4E,
  0x35382FFE, 0xFB936318, 0x52C08E15, 0x80918B2E,
  0x199EDEE0, 0xA9470163, 0xEC44ACDD, 0x612D6735,
  0x8F88EA7D, 0x759F5EA4, 0xE5CC7240, 0x68CFEB8B,
  0x04725601, 0x0C22C23E, 0x5BC97174, 0x89965841,
  0x5D939479, 0x690F338A, 0x3C2D4380, 0xDAE97F2B
};

#if(CHAR_BIT != 8)
#error 8-bit character assumed
#endif



unsigned int
Gate::hash_value() const
{
  static const unsigned int shift_amount = (sizeof(unsigned int)*CHAR_BIT)-1;
  static const unsigned int mask = 1 << shift_amount;

  unsigned int h = 0;
  switch(type) {
  case tTRUE:
    h = 0xCA88E3DD;
    break;
  case tFALSE:
    h = 0xB0642F28;
    break;
  case tVAR:
    {
      h = 0x2A2C0FCF;
      const char* name = get_first_name();
      if(name)
	{
	  /* A variant of BUZhash, see
	   * http://www.serve.net/buz/hash.adt/java.002.html */
	  for(const char *p = name; *p != '\0'; p++) {
	    h = (h << 1) | ((h & mask) >> shift_amount);
	    h = h ^ rtab[((unsigned int)*p) & 0x00ff];
	  }
	}
      return h;
      break;
    }
  case tEQUIV:
    h = 0xA92BF860;
    break;
  case tOR:
    h = 0x122850E1;
    break;
  case tAND:
    h = 0x2390CABB;
    break;
  case tTHRESHOLD:
    h = (0xF6212680 ^ rtab[tmin % 256]) * rtab[tmax % 255];
    break;
  case tNOT:
    h = 0x737C65A6;
    break;
  case tREF:
    h = 0x908B5443;
    break;
  case tEVEN:
    h = 0x98526E2D;
    break;
  case tODD:
    h = 0xC9333644;
    break;
  case tITE:
    h = 0xB3F245DE;
    break;
  case tATLEAST:
    h = 0xA9378F6A * rtab[tmin % 256];
    break;
  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }
  for(const ChildAssoc* ca = children; ca; ca = ca->next_child)
    {
      for(unsigned int v = (unsigned int)ca->child->index; v != 0; v = v >> 8)
	{
	  h = (h << 1) | ((h & mask) >> shift_amount);
	  h = h ^ rtab[v & 0x00ff];
	}
    }
  return h;
}



int
Gate::comp(const Gate* const other) const
{
  if(this == other)
    return 0;
  if(type < other->type)
    return -1;
  if(type > other->type)
    return 1;

  /* gates are of the same type */
  switch(type)
    {
    case tFALSE:
    case tTRUE:
      {
	return 0;
      }
    case tVAR:
      {
	DEBUG_ASSERT(index != other->index);
	if(index < other->index)
	  return -1;
	return 1;
      }
    case tEQUIV:
    case tOR:
    case tAND:
    case tEVEN:
    case tODD:
    case tITE:
    case tNOT:
    case tREF:
      {
	/* Continue to check whether children are the same */
	break;
      }
    case tTHRESHOLD:
      {
	if(tmin < other->tmin) return -1;
	if(tmin > other->tmin) return 1;
	if(tmax < other->tmax) return -1;
	if(tmax > other->tmax) return 1;
	/* Continue to check whether children are the same */
	break;
      }
    case tATLEAST:
      {
	if(tmin < other->tmin) return -1;
	if(tmin > other->tmin) return 1;
	/* Continue to check whether children are the same */
	break;
      }
    default:
      internal_error(text_NI,__FILE__,__LINE__,typeNames[type]);
    }
  
  /*
   * Compare whether the children are the same
   */
  DEBUG_ASSERT(children and other->children);

  const ChildAssoc* ca1 = children;
  const ChildAssoc* ca2 = other->children;
  while(ca1 and ca2)
    {
      const Gate* const child1 = ca1->child;
      const Gate* const child2 = ca2->child;
      DEBUG_ASSERT((child1 == child2) == (child1->index == child2->index));
      if(child1->index < child2->index)
	return -1;
      if(child1->index > child2->index)
	return 1;
      ca1 = ca1->next_child;
      ca2 = ca2->next_child;
    }
  /* this has more children */
  if(ca1)
    return 1;
  /* other has more children */
  if(ca2)
    return -1;
  /* Gates are the same */
  return 0;
}


/*
 * Requires that the gates are uniquely numbered (from 0 to N-1) in temp fields
 */
bool Gate::share(BC * const bc,
		 GateHash * const ht,
		 Gate ** const cache)
{
  if(type == tDELETED)
    return true;
  if(type == tVAR)
    return true;
  if(cache[index])
    return true;
  /*
  if(ht->is_in(this))
    return true;
  */

  for(const ChildAssoc *ca = children; ca; ca = ca->next_child)
    ca->child->share(bc, ht, cache);

  /*
   * Sort the child pointers of commutative gate types
   */
  if(is_commutative())
    sort_children();

  Gate *existing_gate = ht->test_and_set(this);
  cache[index] = existing_gate;
  if(existing_gate != this) {
    /* check the consistency of values */
    if(determined) {
      if(existing_gate->determined) {
	if(value != existing_gate->value)
	  return false;
      } else {
	existing_gate->determined = true;
	existing_gate->value = value;
      }
    }
    remove_all_children();
    while(parents) parents->change_child(existing_gate);
    while(handles) handles->change_gate(existing_gate);
    type = tDELETED;
    bc->changed = true;
  }
  return true;
}


void
Gate::sort_children()
{
  if(!is_commutative())
    return;

  static const unsigned int SHIFT = 4;
  static const unsigned int N = 2 << SHIFT;
  static const unsigned int MASK = N-1;

  if(!children)
    return;

  typedef std::pair<unsigned int, Gate *> IndexGatePair;
  std::vector<IndexGatePair> *c1 = new std::vector<IndexGatePair>();
  unsigned int largest_index = 0;
  bool already_sorted = true;
  for(ChildAssoc* ca = children; ca; ca = ca->next_child)
    {
      DEBUG_ASSERT(ca->child);
      DEBUG_ASSERT(ca->child->index != UINT_MAX);
      c1->push_back(IndexGatePair(ca->child->index, ca->child));
      if(ca->child->index >= largest_index)
	largest_index = ca->child->index;
      else
	already_sorted = false;
    }
  if(already_sorted)
    {
      delete c1;
      return;
    }
  std::vector<IndexGatePair> *c2 = new std::vector<IndexGatePair>();
  const unsigned int nof_children = c1->size();
  c2->resize(nof_children);
  unsigned int shift = 0;
  unsigned int count[N];
  unsigned int start[N];
  for(unsigned int i = 0; i < N; i++)
    count[i] = 0;
  while(largest_index > 0)
    {
      for(unsigned int i = 0; i < nof_children; i++)
	count[((*c1)[i].first >> shift) & MASK]++;
      unsigned int start_index = 0;
      for(unsigned int i = 0; i < N; i++)
	{
	  start[i] = start_index;
	  start_index += count[i];
	  count[i] = 0;
	}
      for(unsigned int i = 0; i < nof_children; i++)
	(*c2)[start[((*c1)[i].first >> shift) & MASK]++] = (*c1)[i];
      largest_index = largest_index >> SHIFT;
      shift += SHIFT;
      std::vector<IndexGatePair> *tmp = c1;
      c1 = c2;
      c2 = tmp;
    }

  unsigned int i = 0;
  for(ChildAssoc *ca = children; ca; ca = ca->next_child)
    ca->change_child((*c1)[i++].second);

  delete c1;
  delete c2;
}



unsigned int Gate::count_parents() const
{
  unsigned int i = 0;
  for(const ChildAssoc *fa = parents; fa; fa = fa->next_parent)
    i++;
  return i;
}



unsigned int Gate::count_children() const
{
  unsigned int i = 0;
  for(const ChildAssoc *ca = children; ca; ca = ca->next_child)
    i++;
  return i;
}







void Gate::cnf_get_clauses(std::list<std::vector<int> *> &clauses,
			   const bool notless)
{
  std::vector<int> *clause;

  /* check that the numbering is valid */
  DEBUG_ASSERT(temp >= 1);

  clauses.clear();

  switch(type) {
  case tFALSE:
    {
      DEBUG_ASSERT(!children);
      return;
    }
  case tTRUE:
    {
      DEBUG_ASSERT(!children);
      return;
    }
  case tVAR:
    {
      DEBUG_ASSERT(!children);
      return;
    } 
  case tREF:
    {
      DEBUG_ASSERT(count_children() == 1);
      if(notless) {
	internal_error(text_NPN, __FILE__, __LINE__);
	return;
      }
      /* standard translation */
      Gate * const child = children->child;
      /* g -> c */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(child->temp);
      /* -g -> -c */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(-child->temp);
      return;
    }
  case tNOT:
    {
      DEBUG_ASSERT(count_children() == 1);
      if(notless) {
	if(determined or children->child->type == tNOT)
	  internal_error(text_NPN, __FILE__, __LINE__);
	return;
      }
      /* standard translation */
      Gate * const child = children->child;
      /* g -> -c */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(-child->temp);
      /* -g -> c */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(child->temp);
      return;
    }
  case tOR:
    {
      DEBUG_ASSERT(count_children() >= 1);
      /* g -> c1 | ... | cn */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
	if(notless) {
	  if(ca->child->type != tNOT) clause->push_back(ca->child->temp);
	  else clause->push_back(-ca->child->children->child->temp);
	} else {
	  clause->push_back(ca->child->temp);
	}
      }
      /* !g -> !ci */
      for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
	clause = new std::vector<int>(); clauses.push_back(clause);
	clause->push_back(temp);
	if(notless) {
	  if(ca->child->type != tNOT) clause->push_back(-ca->child->temp);
	  else clause->push_back(ca->child->children->child->temp);
	} else {
	  clause->push_back(-ca->child->temp);
	}
      }
      return;
    }
  case tAND:
    {
      DEBUG_ASSERT(children);
      /* !g -> !c1 | ... | !cn */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
	if(notless) {
	  if(ca->child->type != tNOT) clause->push_back(-ca->child->temp);
	  else clause->push_back(ca->child->children->child->temp);
	} else {
	  clause->push_back(-ca->child->temp);
	}
      }
      /* g -> ci */
      for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
	clause = new std::vector<int>(); clauses.push_back(clause);
	clause->push_back(-temp);
	if(notless) {
	  if(ca->child->type != tNOT) clause->push_back(ca->child->temp);
	  else clause->push_back(-ca->child->children->child->temp);
	} else {
	  clause->push_back(ca->child->temp);
	}
      }
      return;
    }
  case tEQUIV:
    {
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      ChildAssoc *ca = children;
      Gate *child1 = ca->child; ca = ca->next_child;
      Gate *child2 = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int c1lit = child1->temp;
      int c2lit = child2->temp;
      if(notless && child1->type == tNOT)
	c1lit = -child1->children->child->temp;
      if(notless && child2->type == tNOT)
	c2lit = -child2->children->child->temp;
      /* g := c1 == c2 */
      /* g -> (c1 -> c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(-c1lit);
      clause->push_back(c2lit); 
      /* g -> (~c1 -> ~c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(c1lit);
      clause->push_back(-c2lit);
      /* ~g -> (c1 -> ~c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(-c1lit);
      clause->push_back(-c2lit);
      /* ~g -> (~c1 -> c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(c1lit);
      clause->push_back(c2lit);
      return;
    }
  case tEVEN:
    {
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      ChildAssoc *ca = children;
      Gate *child1 = ca->child; ca = ca->next_child;
      Gate *child2 = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int c1lit = child1->temp;
      int c2lit = child2->temp;
      if(notless && child1->type == tNOT)
	c1lit = -child1->children->child->temp;
      if(notless && child2->type == tNOT)
	c2lit = -child2->children->child->temp;
      /* g := c1 == c2 */
      /* g -> (c1 -> c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(-c1lit);
      clause->push_back(c2lit); 
      /* g -> (~c1 -> ~c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(c1lit);
      clause->push_back(-c2lit); 
      /* ~g -> (c1 -> ~c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(-c1lit);
      clause->push_back(-c2lit); 
      /* ~g -> (~c1 -> c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(c1lit);
      clause->push_back(c2lit); 
      return;
    }
  case tODD:
    {
      if(count_children() != 2)
	fprintf(stderr, "%u ", count_children());
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      ChildAssoc *ca = children;
      Gate *child1 = ca->child; ca = ca->next_child;
      Gate *child2 = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int c1lit = child1->temp;
      int c2lit = child2->temp;
      if(notless && child1->type == tNOT)
	c1lit = -child1->children->child->temp;
      if(notless && child2->type == tNOT)
	c2lit = -child2->children->child->temp;
      /* g := c1 ^ c2 */
      /* g -> (c1 -> ~c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(-c1lit);
      clause->push_back(-c2lit); 
      /* g -> (~c1 -> c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(c1lit);
      clause->push_back(c2lit); 
      /* ~g -> (c1 -> c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(-c1lit);
      clause->push_back(c2lit); 
      /* ~g -> (~c1 -> ~c2) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(c1lit);
      clause->push_back(-c2lit); 
      return;
    }
  case tITE:
    {
      DEBUG_ASSERT(count_children() == 3);
      ChildAssoc *ca = children;
      Gate *if_child = ca->child; ca = ca->next_child;
      Gate *then_child = ca->child; ca = ca->next_child;
      Gate *else_child = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int if_lit = if_child->temp;
      int then_lit = then_child->temp;
      int else_lit = else_child->temp;;
      if(notless && if_child->type == tNOT)
	if_lit = -if_child->children->child->temp;
      if(notless && then_child->type == tNOT)
	then_lit = -then_child->children->child->temp;
      if(notless && else_child->type == tNOT)
	else_lit = -else_child->children->child->temp;
      /* g := ITE(i,t,e) */
      /* g -> (i -> t) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(-if_lit);
      clause->push_back(then_lit); 
      /* g -> (~i -> e) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(if_lit);
      clause->push_back(else_lit); 
      /* ~g -> (i -> ~t) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(-if_lit);
      clause->push_back(-then_lit); 
      /* ~g -> (~i -> ~e) */
      clause = new std::vector<int>(); clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(if_lit);
      clause->push_back(-else_lit); 
      return;
    }
  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }
  assert(should_not_happen);
}

void Gate::xcnf_get_clauses(std::list<std::vector<int> *> &cnf_clauses,
			    std::list<std::vector<int> *> &xor_clauses,
			    const bool notless)
{
  DEBUG_ASSERT(temp >= 1);

  cnf_clauses.clear();
  xor_clauses.clear();

  switch(type) {
  case tFALSE:
  case tTRUE:
  case tVAR:
  case tREF:
  case tNOT:
  case tOR:
  case tAND:
  case tITE:
    return cnf_get_clauses(cnf_clauses, notless);
  case tEQUIV:
    {
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      ChildAssoc *ca = children;
      Gate *child1 = ca->child; ca = ca->next_child;
      Gate *child2 = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int c1lit = child1->temp;
      int c2lit = child2->temp;
      if(notless && child1->type == tNOT)
	c1lit = -child1->children->child->temp;
      if(notless && child2->type == tNOT)
	c2lit = -child2->children->child->temp;
      /* g := c1 == c2 i.e. g ^ c1 ^ c2 = T */
      std::vector<int> *clause = new std::vector<int>();
      xor_clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(c1lit);
      clause->push_back(c2lit); 
      return;
    }
  case tEVEN:
    {
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      ChildAssoc *ca = children;
      Gate *child1 = ca->child; ca = ca->next_child;
      Gate *child2 = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int c1lit = child1->temp;
      int c2lit = child2->temp;
      if(notless && child1->type == tNOT)
	c1lit = -child1->children->child->temp;
      if(notless && child2->type == tNOT)
	c2lit = -child2->children->child->temp;
      /* g := even(c1,c2) i.e. g ^ c1 ^ c2 = T */
      std::vector<int> *clause = new std::vector<int>();
      xor_clauses.push_back(clause);
      clause->push_back(temp);
      clause->push_back(c1lit);
      clause->push_back(c2lit); 
      return;
    }
  case tODD:
    {
      if(count_children() != 2)
	fprintf(stderr, "%u ", count_children());
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      ChildAssoc *ca = children;
      Gate *child1 = ca->child; ca = ca->next_child;
      Gate *child2 = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int c1lit = child1->temp;
      int c2lit = child2->temp;
      if(notless && child1->type == tNOT)
	c1lit = -child1->children->child->temp;
      if(notless && child2->type == tNOT)
	c2lit = -child2->children->child->temp;
      /* g := c1 ^ c2 i.e. g ^ c1 ^ c2 = F i.e.  !g ^ c1 ^ c2 == T */
      std::vector<int> *clause = new std::vector<int>();
      xor_clauses.push_back(clause);
      clause->push_back(-temp);
      clause->push_back(c1lit);
      clause->push_back(c2lit); 
      return;
    }
  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }
  assert(should_not_happen);
}





void Gate::cnf_get_clauses_polarity(std::list<std::vector<int> *> &clauses,
				    const bool notless)
{
  std::vector<int> *clause;

  /* check that the numbering is valid */
  DEBUG_ASSERT(temp >= 1);

  clauses.clear();

  switch(type) {
  case tFALSE:
    {
      DEBUG_ASSERT(!children);
      return;
    }
  case tTRUE:
    {
      DEBUG_ASSERT(!children);
      return;
    }
  case tVAR:
    {
      DEBUG_ASSERT(!children);
      return;
    } 
  case tREF:
    {
      DEBUG_ASSERT(count_children() == 1);
      if(notless) {
	internal_error(text_NPN, __FILE__, __LINE__);
	return;
      }
      /* standard translation */
      Gate * const child = children->child;
      if(mir_pos)
	{
	  /* g -> c */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(child->temp);
	}
      if(mir_neg)
	{
	  /* -g -> -c */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(-child->temp);
	}
      return;
    }
  case tNOT:
    {
      DEBUG_ASSERT(count_children() == 1);
      if(notless) {
	if(determined || children->child->type == tNOT)
	  internal_error(text_NPN, __FILE__, __LINE__);
	return;
      }
      /* standard translation */
      Gate * const child = children->child;
      if(mir_pos)
	{
	  /* g -> -c */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(-child->temp);
	}
      if(mir_neg)
	{
	  /* -g -> c */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(child->temp);
	}
      return;
    }
  case tOR:
    {
      DEBUG_ASSERT(count_children() >= 1);
      if(mir_pos)
	{
	  /* g -> c1 | ... | cn */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
	    if(notless) {
	      if(ca->child->type != tNOT) clause->push_back(ca->child->temp);
	      else clause->push_back(-ca->child->children->child->temp);
	    } else {
	      clause->push_back(ca->child->temp);
	    }
	  }
	}
      if(mir_neg)
	{
	  /* !g -> !ci */
	  for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
	    clause = new std::vector<int>(); clauses.push_back(clause);
	    clause->push_back(temp);
	    if(notless) {
	      if(ca->child->type != tNOT) clause->push_back(-ca->child->temp);
	      else clause->push_back(ca->child->children->child->temp);
	    } else {
	      clause->push_back(-ca->child->temp);
	    }
	  }
	}
      return;
    }
  case tAND:
    {
      DEBUG_ASSERT(children);
      if(mir_pos)
	{
	  /* g -> ci */
	  for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
	    clause = new std::vector<int>(); clauses.push_back(clause);
	    clause->push_back(-temp);
	    if(notless) {
	      if(ca->child->type != tNOT) clause->push_back(ca->child->temp);
	      else clause->push_back(-ca->child->children->child->temp);
	    } else {
	      clause->push_back(ca->child->temp);
	    }
	  }
	}
      if(mir_neg)
	{
	  /* !g -> !c1 | ... | !cn */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
	    if(notless) {
	      if(ca->child->type != tNOT) clause->push_back(-ca->child->temp);
	  else clause->push_back(ca->child->children->child->temp);
	    } else {
	      clause->push_back(-ca->child->temp);
	    }
	  }
	}
      return;
    }
  case tEQUIV:
    {
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      ChildAssoc *ca = children;
      Gate *child1 = ca->child; ca = ca->next_child;
      Gate *child2 = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int c1lit = child1->temp;
      int c2lit = child2->temp;
      if(notless && child1->type == tNOT)
	c1lit = -child1->children->child->temp;
      if(notless && child2->type == tNOT)
	c2lit = -child2->children->child->temp;
      /* g := c1 <-> c2 */
      if(mir_pos)
	{
	  /* g -> (c1 -> c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(-c1lit);
	  clause->push_back(c2lit); 
	  /* g -> (~c1 -> ~c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(c1lit);
	  clause->push_back(-c2lit);
	}
      if(mir_neg)
	{
	  /* ~g -> (c1 -> ~c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(-c1lit);
	  clause->push_back(-c2lit);
	  /* ~g -> (~c1 -> c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(c1lit);
	  clause->push_back(c2lit);
	}
      return;
    }
  case tEVEN:
    {
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      ChildAssoc *ca = children;
      Gate *child1 = ca->child; ca = ca->next_child;
      Gate *child2 = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int c1lit = child1->temp;
      int c2lit = child2->temp;
      if(notless && child1->type == tNOT)
	c1lit = -child1->children->child->temp;
      if(notless && child2->type == tNOT)
	c2lit = -child2->children->child->temp;
      /* g := c1 <=> c2 */
      if(mir_pos)
	{
	  /* g -> (c1 -> c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(-c1lit);
	  clause->push_back(c2lit); 
	  /* g -> (~c1 -> ~c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(c1lit);
	  clause->push_back(-c2lit);
	}
      if(mir_neg)
	{
	  /* ~g -> (c1 -> ~c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(-c1lit);
	  clause->push_back(-c2lit); 
	  /* ~g -> (~c1 -> c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(c1lit);
	  clause->push_back(c2lit);
	}
      return;
    }
  case tODD:
    {
      if(count_children() != 2)
	fprintf(stderr, "%u ", count_children());
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      ChildAssoc *ca = children;
      Gate *child1 = ca->child; ca = ca->next_child;
      Gate *child2 = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int c1lit = child1->temp;
      int c2lit = child2->temp;
      if(notless && child1->type == tNOT)
	c1lit = -child1->children->child->temp;
      if(notless && child2->type == tNOT)
	c2lit = -child2->children->child->temp;
      /* g := c1 ^ c2 */
      if(mir_pos)
	{
	  /* g -> (c1 -> ~c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(-c1lit);
	  clause->push_back(-c2lit); 
	  /* g -> (~c1 -> c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(c1lit);
	  clause->push_back(c2lit);
	}
      if(mir_neg)
	{
	  /* ~g -> (c1 -> c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(-c1lit);
	  clause->push_back(c2lit); 
	  /* ~g -> (~c1 -> ~c2) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(c1lit);
	  clause->push_back(-c2lit); 
	}
      return;
    }
  case tITE:
    {
      DEBUG_ASSERT(count_children() == 3);
      ChildAssoc *ca = children;
      Gate *if_child = ca->child; ca = ca->next_child;
      Gate *then_child = ca->child; ca = ca->next_child;
      Gate *else_child = ca->child; ca = ca->next_child;
      DEBUG_ASSERT(ca == 0);
      int if_lit = if_child->temp;
      int then_lit = then_child->temp;
      int else_lit = else_child->temp;;
      if(notless && if_child->type == tNOT)
	if_lit = -if_child->children->child->temp;
      if(notless && then_child->type == tNOT)
	then_lit = -then_child->children->child->temp;
      if(notless && else_child->type == tNOT)
	else_lit = -else_child->children->child->temp;
      /* g := ITE(i,t,e) */
      if(mir_pos)
	{
	  /* g -> (i -> t) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(-if_lit);
	  clause->push_back(then_lit); 
	  /* g -> (~i -> e) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(-temp);
	  clause->push_back(if_lit);
	  clause->push_back(else_lit);
	}
      if(mir_neg)
	{
	  /* ~g -> (i -> ~t) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(-if_lit);
	  clause->push_back(-then_lit); 
	  /* ~g -> (~i -> ~e) */
	  clause = new std::vector<int>(); clauses.push_back(clause);
	  clause->push_back(temp);
	  clause->push_back(if_lit);
	  clause->push_back(-else_lit);
	}
      return;
    }
  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }
  assert(should_not_happen);
}



void Gate::xcnf_get_clauses_polarity(std::list<std::vector<int> *> &cnf_clauses,
				     std::list<std::vector<int> *> &xor_clauses,
				     const bool notless)
{
  DEBUG_ASSERT(temp >= 1);
  
  cnf_clauses.clear();
  xor_clauses.clear();

  switch(type) {
  case tFALSE:
  case tTRUE:
  case tVAR:
  case tREF:
  case tNOT:
  case tOR:
  case tAND:
  case tITE:
    return cnf_get_clauses_polarity(cnf_clauses, notless);
  case tEQUIV:
    {    
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      if(mir_pos && mir_neg)
	return xcnf_get_clauses(cnf_clauses, xor_clauses, notless);
      return cnf_get_clauses_polarity(cnf_clauses, notless);
    }
  case tEVEN:
    {
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      if(mir_pos && mir_neg)
	return xcnf_get_clauses(cnf_clauses, xor_clauses, notless);
      return cnf_get_clauses_polarity(cnf_clauses, notless);
    }
  case tODD:
    {
      if(count_children() != 2)
	internal_error(text_NPN, __FILE__, __LINE__);
      if(mir_pos && mir_neg)
	return xcnf_get_clauses(cnf_clauses, xor_clauses, notless);
      return cnf_get_clauses_polarity(cnf_clauses, notless);
    }
  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }
  assert(should_not_happen);
}





/*
 *
 * Routines for edimacs format
 *
 */

void
Gate::edimacs_print(FILE* const fp, const bool notless)
{
  switch(type)
    {
    case tTRUE:
      {
	fprintf(fp, "2 -1 %d 0\n", temp);
	break;
      }
    case tFALSE:
      {
	fprintf(fp, "1 -1 %d 0\n", temp);
	break;
      }
    case tVAR:
      {
	break;
      }
    case tEQUIV:
      {
	DEBUG_ASSERT(count_children() == 2);
	fprintf(fp, "11 -1 %d ", temp);
	edimacs_print_children(fp, notless);
	fprintf(fp, "0\n");
	break;
      }
    case tOR:
      {
	fprintf(fp, "6 -1 %d ", temp);
	edimacs_print_children(fp, notless);
	fprintf(fp, "0\n");
	break;
      }
    case tAND:
      {
	fprintf(fp, "4 -1 %d ", temp);
	edimacs_print_children(fp, notless);
	fprintf(fp, "0\n");
	break;
      }
    case tTHRESHOLD:
      {
	if(tmin != tmax)
	  internal_error("%s:%d: Circuit not properly normalized",
			 __FILE__, __LINE__);
	fprintf(fp, "15 1 %u %d ", tmin, temp);
	edimacs_print_children(fp, notless);
	fprintf(fp, "0\n");
	break;
      }
    case tNOT:
      {
	DEBUG_ASSERT(count_children() == 1);
	if(notless)
	  {
	    if(determined || children->child->type == tNOT)
	      internal_error("%s:%d: Circuit not properly normalized",
			     __FILE__, __LINE__);
	    break;
	  }
	fprintf(fp, "3 -1 %d ", temp);
	edimacs_print_children(fp, notless);
	fprintf(fp, "0\n");
	break;
      }
    case tREF:
      {
	internal_error("%s:%d: Circuit not properly normalized",
		       __FILE__, __LINE__);
	break;
      }
    case tEVEN:
      {
	fprintf(fp, "9 -1 %d ", temp);
	edimacs_print_children(fp, notless);
	fprintf(fp, "0\n");
	break;
      }
    case tODD:
      {
	fprintf(fp, "8 -1 %d ", temp);
	edimacs_print_children(fp, notless);
	fprintf(fp, "0\n");
	break;
      }
    case tITE:
      {
	DEBUG_ASSERT(count_children() == 3);
	fprintf(fp, "12 -1 %d ", temp);
	edimacs_print_children(fp, notless);
	fprintf(fp, "0\n");
	break;
      }
    case tATLEAST:
      {
	fprintf(fp, "13 1 %u %d ", tmin, temp);
	edimacs_print_children(fp, notless);
	fprintf(fp, "0\n");
	break;
      }
    default:
      internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
    }
}


void Gate::edimacs_print_children(FILE *fp, const bool notless)
{
  for(ChildAssoc *ca = children; ca; ca = ca->next_child)
    {
      if(notless && ca->child->type == tNOT)
	{
	  DEBUG_ASSERT(!ca->child->determined);
	  DEBUG_ASSERT(ca->child->children->child->type != tNOT);
	  fprintf(fp, "%d ", -ca->child->children->child->temp);
	}
      else
	fprintf(fp, "%d ", ca->child->temp);
    }
}





/*
 *
 * Routines for writing the circuit in the ISCAS89 format
 *
 */

void
Gate::write_iscas89(FILE* const fp) const
{
  switch(type)
    {
    case tFALSE:
      /*
	fprintf(fp, "INPUT(");
	write_iscas89_name(fp);
	fprintf(fp, ")\n");
	DEBUG_ASSERT(is_determined && value == false);
      */
      break;
    case tTRUE:
      /*
	fprintf(fp, "INPUT(");
	write_iscas89_name(fp);
	fprintf(fp, ")\n");
	DEBUG_ASSERT(is_determined && value == true);
      */
      break;
    case tVAR:
      /*
	fprintf(fp, "INPUT(");
	write_iscas89_name(fp);
	fprintf(fp, ")\n");
      */
      break;
    case tEQUIV:
    case tEVEN:
      if(count_children() != 2)
	internal_error("write_iscas89(): Circuit not properly normalized",
		       __FILE__, __LINE__);
      if(false) {
	write_iscas89_name(fp);
	fprintf(fp, " = IFF(");
	write_iscas89_children(fp);
	fprintf(fp, ")\n");
      } else {
	/* Write IFF(a,b) as NOT(XOR(a,b)) as the parser of another tool
	 * does not support IFF */
	write_iscas89_name(fp);
	fprintf(fp, "n = XOR(");
	write_iscas89_children(fp);
	fprintf(fp, ")\n");

	write_iscas89_name(fp);
	fprintf(fp, " = NOT(");
	write_iscas89_name(fp);
	fprintf(fp, "n)\n");
      }
      break;
    case tODD:
      if(count_children() != 2)
	internal_error("write_iscas89(): Circuit not properly normalized",
		       __FILE__, __LINE__);
      write_iscas89_name(fp);
      fprintf(fp, " = XOR(");
      write_iscas89_children(fp);
      fprintf(fp, ")\n");
      break;
    case tITE:
      DEBUG_ASSERT(count_children() == 3);
      write_iscas89_name(fp);
      fprintf(fp, " = ITE(");
      write_iscas89_children(fp);
      fprintf(fp, ")\n");
      break;
    case tNOT:
      DEBUG_ASSERT(count_children() == 1);
      write_iscas89_name(fp);
      fprintf(fp, " = NOT(");
      write_iscas89_children(fp);
      fprintf(fp, ")\n");
      break;
    case tOR:
      write_iscas89_name(fp);
      fprintf(fp, " = OR(");
      write_iscas89_children(fp);
      fprintf(fp, ")\n");
      break;
    case tAND:
      write_iscas89_name(fp);
      fprintf(fp, " = AND(");
      write_iscas89_children(fp);
      fprintf(fp, ")\n");
      break;
    case tREF:
    case tTHRESHOLD:
    case tATLEAST:
      internal_error("write_iscas89(): Circuit not properly normalized",
		     __FILE__, __LINE__);
      break;
    default:
      internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
    }
  return;
}

void
Gate::write_iscas89_children(FILE* const fp) const
{
  const char* sep = "";
  for(ChildAssoc *ca = children; ca; ca = ca->next_child)
    {
      fprintf(fp, "%s", sep);
      sep = ",";
      ca->child->write_iscas89_name(fp, true);
    }
}

void
Gate::write_iscas89_name(FILE* const fp, const bool positive) const
{
  DEBUG_ASSERT(index != UINT_MAX);
  DEBUG_ASSERT(type != tDELETED);
  
  if(!positive)
    fprintf(fp, "-");
  fprintf(fp, "g_%u", index);
}

void
Gate::write_iscas89_map(FILE* const fp) const
{
  DEBUG_ASSERT(index != UINT_MAX);
  if(type == tDELETED)
    return;

  for(const Handle* handle = handles; handle; handle = handle->get_next())
    {
      if(handle->get_type() == Handle::ht_NAME)
	{
	  const char* const name = ((NameHandle *)handle)->get_name();
	  DEBUG_ASSERT(name);
	  fprintf(fp, "# g_%u <- %s\n", index, name);
	}
    }
}





/*
 *
 * Some statistics
 *
 */
void
Gate::count_child_info(unsigned int& nof_true,
		       unsigned int& nof_false,
		       unsigned int& nof_undet) const
{
  nof_true = 0;
  nof_false = 0;
  nof_undet = 0;
  
  for(const ChildAssoc* ca = children; ca; ca = ca->next_child) {
    if(ca->child->determined) {
      if(ca->child->value) nof_true++;
      else nof_false++;
    } else {
      nof_undet++;
    }
  }
}





bool
Gate::is_justified()
{
  if(!determined)
    return false;

  unsigned int nof_true, nof_false, nof_undet;
  count_child_info(nof_true, nof_false, nof_undet);
  
  switch(type)
    {
    case tFALSE:
    case tTRUE:
    case tVAR:
      return true;
      
    case tNOT:
      DEBUG_ASSERT(!(nof_true > 0 and value == true));
      DEBUG_ASSERT(!(nof_false > 0 and value == false));
      return((value == true and nof_false > 0) or
	     (value == false and nof_true > 0));

    case tEQUIV:
      DEBUG_ASSERT(nof_children() >= 1);
      if(value == true) {
	if(nof_children() == 1)
	  return true;
	if(nof_true == nof_children())
	  return true;
	if(nof_false == nof_children())
	  return true;
      } else {
	if(nof_true > 0 and nof_false > 0)
	  return true;
      }
      return false;

    case tOR:
      if(value == true) {
	DEBUG_ASSERT(nof_false < nof_children());
	if(nof_true > 0)
	  return true;
      } else {
	DEBUG_ASSERT(nof_true == 0);
	if(nof_false == nof_children())
	  return true;
      }
      return false;
      
    case tAND:
      if(value == false) {
	DEBUG_ASSERT(nof_true < nof_children());
	if(nof_false > 0)
	  return true;
      } else {
	DEBUG_ASSERT(nof_false == 0);
	if(nof_true == nof_children())
	  return true;
      }
      return false;

    case tODD:
      if(value == true) {
	if(nof_true + nof_false == nof_children() and ((nof_true % 2) == 1))
	  return true;
      } else {
	if(nof_true + nof_false == nof_children() and ((nof_true % 2) == 0))
	  return true;
      }
      return false;

    case tEVEN:
      if(value == true) {
	if(nof_true + nof_false == nof_children() and ((nof_true % 2) == 0))
	  return true;
      } else {
	if(nof_true + nof_false == nof_children() and ((nof_true % 2) == 1))
	  return true;
      }
      return false;

    case tITE:
      {
	Gate* const if_child = children->child;
	Gate* const then_child = children->next_child->child;
	Gate* const else_child = children->next_child->next_child->child;
	if(value == true) {
	  if(if_child->determined and if_child->value == true and
	     then_child->determined and then_child->value == true)
	    return true;
	  if(if_child->determined and if_child->value == false and
	     else_child->determined and else_child->value == true)
	    return true;
	  if(then_child->determined and then_child->value == true and
	     else_child->determined and else_child->value == true)
	    return true;
	} else {
	  if(if_child->determined and if_child->value == true and
	     then_child->determined and then_child->value == false)
	    return true;
	  if(if_child->determined and if_child->value == false and
	     else_child->determined and else_child->value == false)
	    return true;
	  if(then_child->determined and then_child->value == false and
	     else_child->determined and else_child->value == false)
	    return true;
	}
	return false;
      }
      
    case tTHRESHOLD:
      if(value == true) {
	if(tmin <= nof_true and nof_children() - nof_false <= tmax)
	  return true;
      } else {
	if(nof_true > tmax)
	  return true;
	if(nof_children() - nof_false < tmin)
	  return true;
      }
      return false;
      
    case tATLEAST:
      if(value == true) {
	if(nof_true >= tmin)
	  return true;
      } else {
	if(nof_children() - nof_false < tmin)
	  return true;
      }
      return false;

    default:
      internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
    }

  assert(should_not_happen);
  return false;
}





/*
 * Routine that propagates polarity information needed in the
 * monotone variable rule
 */
void Gate::mir_propagate_polarity(bool polarity)
{
  unsigned int nof_true, nof_false, nof_undet;

  if(determined)
    {
      if(value != polarity)
	return;
      if(is_justified())
	return;
    }

  if(polarity)
    {
      if(mir_pos)
	return;
      mir_pos = true;
    }
  else
    {
      if(mir_neg)
	return;
      mir_neg = true;
    }

  switch(type) {
  case tFALSE:
  case tTRUE:
  case tVAR: {
    return;
  }
  case tNOT: {
    children->child->mir_propagate_polarity(!polarity);
    return;
  }
  case tOR:
  case tAND: {
    for(ChildAssoc *ca = children; ca; ca = ca->next_child)
      ca->child->mir_propagate_polarity(polarity);
    return;
  }
  case tEQUIV: {
    /* TODO: add some cases here... */
    /* The default case */
    for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
      ca->child->mir_propagate_polarity(polarity);
      ca->child->mir_propagate_polarity(!polarity);
    }
    return;
  }
  case tODD: {
    count_child_info(nof_true, nof_false, nof_undet);
    if(nof_undet == 1) {
      bool desired_value = polarity ^ ((nof_true % 2) == 1);
      for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	ca->child->mir_propagate_polarity(desired_value);
      return;
    }
    /* The default case */
    for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
      ca->child->mir_propagate_polarity(polarity);
      ca->child->mir_propagate_polarity(!polarity);
    }
    return;
  }
  case tEVEN: {
    count_child_info(nof_true, nof_false, nof_undet);
    if(nof_undet == 1) {
      bool desired_value = polarity ^ ((nof_true % 2) == 0);
      for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	ca->child->mir_propagate_polarity(desired_value);
      return;
    }
    /* The default case */
    for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
      ca->child->mir_propagate_polarity(polarity);
      ca->child->mir_propagate_polarity(!polarity);
    }
    return;
  }
  case tITE: {
    Gate *if_child = children->child;
    Gate *then_child = children->next_child->child;
    Gate *else_child = children->next_child->next_child->child;
    if_child->mir_propagate_polarity(polarity);
    if_child->mir_propagate_polarity(!polarity);
    then_child->mir_propagate_polarity(polarity);
    else_child->mir_propagate_polarity(polarity);
    return;
  }
  case tTHRESHOLD: {
    count_child_info(nof_true, nof_false, nof_undet);
    const unsigned int nof_children = nof_true + nof_false + nof_undet;
    if(polarity) {
      if(nof_true >= tmin) {
	for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	  ca->child->mir_propagate_polarity(false);
	return;
      }
      if(nof_true < tmin and nof_children - nof_false <= tmax) {
	for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	  ca->child->mir_propagate_polarity(true);
	return;
      }
    } else {
      /* polarity = false */
      if(nof_true >= tmin) {
	for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	  ca->child->mir_propagate_polarity(true);
	return;
      }
      if(nof_true < tmin and nof_children - nof_false <= tmax) {
	for(ChildAssoc *ca = children; ca; ca = ca->next_child)
	  ca->child->mir_propagate_polarity(false);
	return;
      }
    }
    /* The default case */
    for(ChildAssoc *ca = children; ca; ca = ca->next_child) {
      ca->child->mir_propagate_polarity(polarity);
      ca->child->mir_propagate_polarity(!polarity);
    }
    return;
  }
  case tATLEAST: {
    for(ChildAssoc *ca = children; ca; ca = ca->next_child)
      ca->child->mir_propagate_polarity(polarity);
    return;
  }
  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }
  assert(should_not_happen);
}





bool
Gate::evaluate()
{
  if(determined)
    return true;

  /*
   * Evaluate all children
   */
  unsigned int nof_false_children = 0;
  unsigned int nof_true_children = 0;
  for(const ChildAssoc* ca = children; ca; ca = ca->next_child)
    {
      Gate* const child = ca->child;
      if(!child->evaluate())
	return false;
      DEBUG_ASSERT(child->determined);
      if(child->value)
	nof_true_children++;
      else
	nof_false_children++;
    }

  switch(type) {
  case tVAR:
    /* Value cannot be evaluated! */
    return false;
  case tFALSE:
    value = false;
    break;
  case tTRUE:
    value = true;
    break;
  case tREF:
    value = (nof_true_children == 1);
    break;
  case tNOT:
    value = (nof_true_children == 0);
    break;
  case tEQUIV:
    if(nof_true_children > 0 and nof_false_children > 0)
      value = false;
    else
      value = true;
    break;
  case tOR:
    value = (nof_true_children > 0);
    break;
  case tAND:
    value = (nof_false_children == 0);
    break;
  case tODD:
    value = ((nof_true_children & 0x01) == 1);
    break;
  case tEVEN:
    value = ((nof_true_children & 0x01) == 0);
    break;
  case tITE: {
    const bool if_value = children->child->value; 
    const bool then_value = children->next_child->child->value; 
    const bool else_value = children->next_child->next_child->child->value; 
    if(if_value)
      value = then_value;
    else
      value = else_value;
    break;
  }
  case tTHRESHOLD:
    value = ((tmin <= nof_true_children) and (nof_true_children <= tmax));
    break;
  case tATLEAST:
    value = (tmin <= nof_true_children);
    break;
  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }
  determined = true;

  return true;
}


/*
 * Returns false if the current truth assignment is not consistent
 */
bool Gate::check_consistency()
{
  unsigned int nof_true, nof_false, nof_undet;
  
  if(!determined)
    return true;
  
  count_child_info(nof_true, nof_false, nof_undet);
  const unsigned int nof_children = nof_true + nof_false + nof_undet;

  switch(type) {
  case tFALSE:
    return(value == false);
  case tTRUE:
    return(value == true);
  case tVAR:
    return true;
  case tNOT:
    DEBUG_ASSERT(nof_children == 1);
    if(nof_true == 1)
      return(value == false);
    if(nof_false == 1)
      return(value == true);
    return true;
  case tREF:
    DEBUG_ASSERT(nof_children == 1);
    if(nof_true == 1)
      return(value == true);
    if(nof_false == 1)
      return(value == false);
    return true;
  case tEQUIV:
    if(value == true) {
      if(nof_true > 0 and nof_false > 0)
	return false;
      return true;
    }
    /* value == false */
    if(nof_true == nof_children || nof_false == nof_children)
      return false;
    return true;
  case tOR:
    if(value == false) {
      if(nof_true > 0)
	return false;
      return true;
    }
    /* value == true */
    if(nof_false == nof_children)
      return false;
    return true;
  case tAND:
    if(value == true) {
      if(nof_false > 0)
	return false;
      return true;
    }
    /* value == false */
    if(nof_true == nof_children)
      return false;
    return true;
  case tODD:
    if(nof_undet == 0)
      return(value == ((nof_true & 0x01) == 1));
    return true;
  case tEVEN:
    if(nof_undet == 0)
      return(value == ((nof_true & 0x01) == 0));
    return true;
  case tITE: {
    Gate *if_child = children->child;
    Gate *then_child = children->next_child->child;
    Gate *else_child = children->next_child->next_child->child;
    if(value == true) {
      if(if_child->determined and if_child->value == true and
	 then_child->determined and then_child->value == false)
	return false;
      if(if_child->determined and if_child->value == false and
	 else_child->determined and else_child->value == false)
	return false;
      if(then_child->determined and then_child->value == false and
	 else_child->determined and else_child->value == false)
	return false;
    } else {
      if(if_child->determined and if_child->value == true and
	 then_child->determined and then_child->value == true)
	return false;
      if(if_child->determined and if_child->value == false and
	 else_child->determined and else_child->value == true)
	return false;
      if(then_child->determined and then_child->value == true and
	 else_child->determined and else_child->value == true)
	return false;
    }
    return true;
  }
  case tTHRESHOLD:
    assert(tmin <= tmax);
    assert(tmax <= nof_children);
    if(value == true) {
      if(nof_true > tmax || nof_children - nof_false < tmin)
	return false;
      return true;
    }
    /* value == false */
    if(nof_true >= tmin and nof_children - nof_false <= tmax)
      return false;
    return true;
  default:
    internal_error(text_NI, __FILE__, __LINE__, typeNames[type]);
  }
  assert(should_not_happen);
  return false;
}





/**************************************************************************
 *
 * Miscellaneous routines
 *
 **************************************************************************/

/*
 * WARNING: uses temp fields (assumes that they are initialized to -1)
 */
unsigned int Gate::compute_min_height()
{
  if(temp >= 0)
    return (unsigned int)temp;
  
  if(!children)
    {
      temp = 0;
      return 0;
    }

  unsigned int min_height = UINT_MAX;
  for(ChildAssoc *ca = children; ca; ca = ca->next_child)
    {
      unsigned int height = ca->child->compute_min_height();
      if(height < min_height)
	min_height = height;
    }
  assert(min_height != UINT_MAX);
  temp = min_height + 1;

  return (unsigned int)temp;
}


/*
 * WARNING: uses temp fields (assumes that they are initialized to -1)
 */
unsigned int Gate::compute_max_height()
{
  if(temp >= 0)
    return (unsigned int)temp;
  
  if(!children)
    {
      temp = 0;
      return 0;
    }

  unsigned int max_height = 0;
  for(ChildAssoc *ca = children; ca; ca = ca->next_child)
    {
      unsigned int height = ca->child->compute_max_height();
      if(height > max_height)
	max_height = height;
    }
  temp = max_height + 1;

  return (unsigned int)temp;
}
