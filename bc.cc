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

#include <cstdlib>
#include <cassert>
#include <cctype>
#include <climits>
#include <list>
#include <queue>
#include <algorithm>
#include "defs.hh"
#include "bc.hh"
#include "timer.hh"

static const char *text_NI = "%s:%d: %s not implemented";



BC::BC()
{
  first_gate = 0;
  may_transform_input_gates = true;
  preserve_all_solutions = false;
  pstack = 0;
  contradictory = false;
}



BC::~BC()
{
  for(GateNameMap::iterator ni = named_gates.begin();
      ni != named_gates.end();
      )
    {
      GateNameMap::iterator next_ni = ni;
      next_ni++;
      char * const name = (*ni).first;
      DEBUG_ASSERT(name);
      NameHandle *handle = (*ni).second;
      DEBUG_ASSERT(handle);
      named_gates.erase(ni);
      free(name);
      delete handle;
      ni = next_ni;
    }
  
  while(false && !named_gates.empty())
    {
      char * const name = (*named_gates.begin()).first;
      DEBUG_ASSERT(name);
      NameHandle *handle = (*named_gates.begin()).second;
      DEBUG_ASSERT(handle);
      named_gates.erase(named_gates.begin());
      free(name);
      delete handle;
    }

  while(first_gate)
    {
      Gate* const gate = first_gate;
      first_gate = gate->next;
      release_gate(gate);
    }
}



BC*
BC::parse_circuit(const char* const filename)
{
  if(!filename)
    return 0;
  FILE* const fp = fopen(filename, "r");
  if(!fp)
    return 0;
  BC* const result = parse_circuit(fp);
  fclose(fp);
  return result;
}



BC*
BC::parse_circuit(FILE* const fp)
{
  BC *circuit = new BC();

  /* Read the header line in order to get the file format version */
  const int max_header_line_length = 10;
  char header[max_header_line_length];
  if(fgets(header, max_header_line_length, fp) == 0) {
    fprintf(stderr, "Could not read the header line\n");
    goto error_exit;
  }
  if(strcmp("BC1.0\x0a", header) == 0 ||
     strcmp("BC1.0\x0d\x0a", header) == 0) {
    extern int bcp_parse();
    extern int bcp_restart(FILE *);
    extern void bcp_error(const char *, ...);
    extern void bcp_error2(const char *, ...);
    extern std::list<char*> bcp_true_gate_names;
    extern std::list<char*> bcp_false_gate_names;
    extern BC *bcp_circuit;
    extern int bcp_lineno;
    std::list<Gate*> gate_stack;
    
    verbose_print("Using file format version 1.0\n");

    bcp_true_gate_names.clear();
    bcp_false_gate_names.clear();
    
    bcp_circuit = circuit;
    
    /*
     * Parse the circuit
     */
    bcp_restart(fp);
    bcp_lineno++;
    if(bcp_parse()) {
      bcp_true_gate_names.clear();
      bcp_false_gate_names.clear();
      goto error_exit;
    }
    
    /*
     * Convert all undef gates into variable gates
     */
    for(Gate* gate = circuit->first_gate; gate; gate = gate->next)
      {
	if(gate->type == Gate::tUNDEF)
	  {
	    DEBUG_ASSERT(gate->children == 0);
	    gate->type = Gate::tVAR;
	  }
      }
    
    /*
     * Make constraints
     */
    for(std::list<char*>::iterator ci = bcp_true_gate_names.begin();
	ci != bcp_true_gate_names.end();
	ci++)
      {
	NameHandle *handle = circuit->find_gate(*ci);
	if(!handle) {
	  fprintf(stderr, "gate '%s' assigned to true but not defined\n", *ci);
	  goto error_exit;
	}
	/*circuit->force_true(handle->get_gate());*/
	circuit->assigned_to_true.push_back(handle->get_gate());
      }
    for(std::list<char*>::iterator ci = bcp_false_gate_names.begin();
	ci != bcp_false_gate_names.end();
	ci++)
      {
	NameHandle *handle = circuit->find_gate(*ci);
	if(!handle) {
	  fprintf(stderr, "gate '%s' assigned to false but not defined\n", *ci);
	  goto error_exit;
	}
	/*circuit->force_false(handle->get_gate());*/
	circuit->assigned_to_false.push_back(handle->get_gate());
      }
  }
  else if(strcmp("BC1.1\x0a", header) == 0 ||
	  strcmp("BC1.1\x0d\x0a", header) == 0) {
    extern int bcp11_parse();
    extern int bcp11_restart(FILE *);
    //extern void bcp11_error(const char *, ...);
    //extern void bcp11_error2(const char *, ...);
    extern std::list<Gate*> bcp11_assigned_gates;
    extern BC *bcp11_circuit;
    extern int bcp11_lineno;
    std::list<Gate*> gate_stack;
    
    verbose_print("Using file format version 1.1\n");

    bcp11_assigned_gates.clear();
    
    bcp11_circuit = circuit;
    
    /* Parse the circuit */
    bcp11_restart(fp);
    bcp11_lineno++;
    if(bcp11_parse()) {
      bcp11_assigned_gates.clear();
      goto error_exit;
    }
    
    /* Convert all undef gates into variable gates */
    for(Gate* gate = circuit->first_gate; gate; gate = gate->next) {
      if(gate->type == Gate::tUNDEF) {
	DEBUG_ASSERT(gate->children == 0);
	gate->type = Gate::tVAR;
      }
    }
    
    /*
     * Make constraints
     */
    for(std::list<Gate*>::const_iterator ci = bcp11_assigned_gates.begin();
	ci != bcp11_assigned_gates.end();
	ci++)
      {
	circuit->assigned_to_true.push_back(*ci);
      }
  }
  else {
    fprintf(stderr, "Illegal header line '%s'", header);
    goto error_exit;
  }

  /*
   * Test acyclicity
   */
  if(!circuit->test_acyclicity())
    goto error_exit;

  return circuit;

 error_exit:
  if(circuit) delete circuit;
  return 0;
}



Gate*
BC::new_EQUIV(Gate* const c1, Gate* const c2)
{
  DEBUG_ASSERT(c1);
  DEBUG_ASSERT(c2);
  Gate* const gate = new Gate(Gate::tEQUIV, c1, c2);
  install_gate(gate);
  return gate;
}

Gate*
BC::new_EQUIV(const std::list<Gate*>* const children)
{
  DEBUG_ASSERT(children->size() >= 1);
  Gate* const gate = new Gate(Gate::tEQUIV, children);
  install_gate(gate);
  return gate;
}



Gate*
BC::new_OR(Gate* const c1, Gate* const c2)
{
  DEBUG_ASSERT(c1);
  DEBUG_ASSERT(c2);
  Gate* const gate = new Gate(Gate::tOR, c1, c2);
  install_gate(gate);
  return gate;
}

Gate*
BC::new_OR(const std::list<Gate*>* const children)
{
  DEBUG_ASSERT(children->size() >= 1);
  Gate* const gate = new Gate(Gate::tOR, children);
  install_gate(gate);
  return gate;
}



Gate*
BC::new_ODD(Gate* const c1, Gate* const c2)
{
  DEBUG_ASSERT(c1);
  DEBUG_ASSERT(c2);
  Gate* const gate = new Gate(Gate::tODD, c1, c2);
  install_gate(gate);
  return gate;
}

Gate*
BC::new_ODD(const std::list<Gate*>* const children)
{
  DEBUG_ASSERT(children->size() >= 1);
  Gate* const gate = new Gate(Gate::tODD, children);
  install_gate(gate);
  return gate;
}



Gate*
BC::new_EVEN(Gate* const c1, Gate* const c2)
{
  DEBUG_ASSERT(c1);
  DEBUG_ASSERT(c2);
  Gate* const gate = new Gate(Gate::tEVEN, c1, c2);
  install_gate(gate);
  return gate;
}

Gate*
BC::new_EVEN(const std::list<Gate*>* const children)
{
  DEBUG_ASSERT(children->size() >= 1);
  Gate* const gate = new Gate(Gate::tEVEN, children);
  install_gate(gate);
  return gate;
}



Gate*
BC::new_AND(Gate* const c1, Gate* const c2)
{
  DEBUG_ASSERT(c1);
  DEBUG_ASSERT(c2);
  Gate* const gate = new Gate(Gate::tAND, c1, c2);
  install_gate(gate);
  return gate;
}

Gate*
BC::new_AND(const std::list<Gate*>* const children)
{
  DEBUG_ASSERT(children->size() >= 1);
  Gate* const gate = new Gate(Gate::tAND, children);
  install_gate(gate);
  return gate;
}



Gate*
BC::new_NOT(Gate* const c)
{
  DEBUG_ASSERT(c);
  Gate* const gate = new Gate(Gate::tNOT, c);
  install_gate(gate);
  return gate;
}



Gate*
BC::new_REF(Gate* const c)
{
  DEBUG_ASSERT(c);
  Gate* const gate = new Gate(Gate::tREF, c);
  install_gate(gate);
  return gate;
}



Gate*
BC::new_ITE(Gate* const i, Gate* const t, Gate* const e)
{
  DEBUG_ASSERT(i);
  DEBUG_ASSERT(t);
  DEBUG_ASSERT(e);
  Gate* const gate = new Gate(Gate::tITE, i, t, e);
  install_gate(gate);
  return gate;
}



Gate*
BC::new_CARDINALITY(const unsigned int tmin,
		    const unsigned int tmax,
		    const std::list<Gate*>* const children)
{
  DEBUG_ASSERT(children->size() >= 1);
  Gate* const gate = new Gate(Gate::tTHRESHOLD, children);
  install_gate(gate);
  gate->tmin = tmin;
  gate->tmax = tmax;
  return gate;
}



Gate*
BC::new_FALSE()
{
  Gate* const gate = new Gate(Gate::tFALSE);
  install_gate(gate);
  const bool result = force_false(gate);
  assert(result);
  return gate;
}



Gate*
BC::new_TRUE()
{
  Gate* const gate = new Gate(Gate::tTRUE);
  install_gate(gate);
  const bool result = force_true(gate);
  assert(result);
  return gate;
}



Gate*
BC::new_VAR()
{
  Gate* const gate = new Gate(Gate::tVAR);
  install_gate(gate);
  return gate;
}



Gate*
BC::new_UNDEF()
{
  Gate* const gate = new Gate(Gate::tUNDEF);
  install_gate(gate);
  return gate;
}



void
BC::install_gate(Gate* const gate)
{
  if(contradictory)
    internal_error("Circuit already contradictory");
  gate->next = first_gate;
  first_gate = gate;
  assert(gate->index == UINT_MAX);
  if(free_gate_indices.empty())
    {
      gate->index = index_to_gate.size();
      index_to_gate.push_back(gate);
    }
  else
    {
      const unsigned int free_index = free_gate_indices.back();
      free_gate_indices.pop_back();
      assert(index_to_gate.size() > free_index);
      assert(index_to_gate[free_index] == 0);
      index_to_gate[free_index] = gate;
    }
}



void BC::release_gate(Gate * const gate)
{
  assert(gate->index != UINT_MAX);
  assert(gate->index < index_to_gate.size());
  assert(index_to_gate[gate->index] == gate);
  index_to_gate[gate->index] = 0;
  gate->index = UINT_MAX;
  delete gate;
}



bool
BC::force_false(Gate* const g)
{
  if(contradictory)
    internal_error("Circuit already contradictory");

  if(g->determined and g->value != false)
    {
      contradictory = true;
      return false;
    }
  g->determined = true;
  g->value = false;
  return true;
}



bool
BC::force_true(Gate* const g)
{
  if(contradictory)
    internal_error("Circuit already contradictory");

  if(g->determined and g->value != true)
    {
      contradictory = true;
      return false;
    }
  g->determined = true;
  g->value = true;
  return true;
}



NameHandle*
BC::find_gate(char *name)
{
  GateNameMap::iterator iter = named_gates.find(name);
  if(iter != named_gates.end())
    {
      return (*iter).second;
    }
  return 0;
}



NameHandle*
BC::insert_gate_name(char *name, Gate *gate)
{
  DEBUG_ASSERT(name);
  DEBUG_ASSERT(gate);

#ifdef BC_USE_STL_MAP
  GateNameMap::iterator iter = named_gates.find(name);
  if(iter != named_gates.end())
    {
      return (*iter).second;
    }
  NameHandle *handle = new NameHandle(gate, name);
  named_gates[name] = handle;
  return handle;
#else
  NameHandle* handle = new NameHandle(gate, name);
  GateNameMap::iterator it = named_gates.add(name, handle);
  if((*it).second != handle)
    delete handle;
  return (*it).second;
#endif
}



void
BC::remove_underscore_names()
{
  GateNameMap::iterator ni = named_gates.begin();
  while(ni != named_gates.end())
    {
      char *name = (*ni).first;
      NameHandle *handle = (*ni).second;
      DEBUG_ASSERT(name);
      DEBUG_ASSERT(handle);
      DEBUG_ASSERT(name == handle->get_name());

      if(name[0] == '_')
	{
	  GateNameMap::iterator next_ni = ni;
	  next_ni++;
	  named_gates.erase(ni);
	  delete handle;
	  free(name);
	  ni = next_ni;
	}
      else
	ni++;
    }
}

void
BC::print_assignment(FILE* const fp)
{
  for(const Gate* gate = first_gate; gate; gate = gate->next)
    {
      DEBUG_ASSERT(gate->determined);
      const Handle* handle = gate->handles;
      while(handle)
	{
	  if(handle->get_type() == Handle::ht_NAME)
	    {
	      const char* const name = ((NameHandle *)handle)->get_name();
	      DEBUG_ASSERT(name);
	      fprintf(fp, "%s%s ", gate->value?"":"!", name);
	    }
	  handle = handle->get_next();
	}
    }
}





void
BC::reset_temp_fields(const int value)
{
  for(Gate* gate = first_gate; gate; gate = gate->next)
    gate->temp = value;
}



bool
BC::test_acyclicity()
{
  std::list<const char*> cycle;
  bool acyclic = true;

  reset_temp_fields();

  for(Gate *gate = first_gate; gate; gate = gate->next) {
    if(gate->test_acyclicity(cycle) != 0) {
      acyclic = false;
      fprintf(stderr, "circuit has a cycle: ");
      for(std::list<const char*>::iterator ci = cycle.begin(); ci != cycle.end();)
	{
	  fprintf(stderr, "%s", *ci);
	  ci++;
	  if(ci != cycle.end())
	    fprintf(stderr, "->");
	}
      fprintf(stderr, "\n");
      break;
    }
    DEBUG_ASSERT(cycle.empty());
  }

  reset_temp_fields();

  return acyclic;
}


void
BC::debug_check_temp_fields_zero()
{
#ifdef DEBUG_EXPENSIVE_CHECKS
  for(const Gate* gate = first_gate; gate; gate = gate->next)
    assert(gate->temp == 0);
#endif
}


bool
BC::depends_on(Gate * const gate1, Gate * const gate2)
{
  /* DEBUG preconditions */
  DEBUG_ASSERT(gate1);
  DEBUG_ASSERT(gate2);
  debug_check_temp_fields_zero();

  std::vector<Gate*> frontier;
  std::vector<Gate*> cone;

  frontier.push_back(gate1);
  cone.push_back(gate1);
  gate1->temp = 1;
  bool found = false;
  while(!frontier.empty())
    {
      const Gate* const gate = frontier.back();
      frontier.pop_back();
      assert(gate->temp == 1);
      if(gate == gate2) {
	found = true;
	break;
      }
      for(const ChildAssoc* ca = gate->children; ca; ca = ca->next_child) {
	if(ca->child->temp == 0) {
	  frontier.push_back(ca->child);
	  cone.push_back(ca->child);
	  ca->child->temp = 1;
	}
      }
    }
  frontier.clear();
  while(!cone.empty())
    {
      Gate* gate = cone.back();
      cone.pop_back();
      gate->temp = 0;
    }
  
  /* DEBUG postconditions */
  debug_check_temp_fields_zero();

  return found;
}





/**
 * The destructive "cone of incluence" reduction; removes all the gates
 * that are not descendants of at least one constrained gate.
 * If there are no constrained gates, the results is an empty circuit.
 * WARNING: gates with names, including input gates, may be deleted!
 */

void
BC::destructive_coi()
{
  reset_temp_fields(-1);
  int nof_relevant_gates = 0;
  for(Gate* gate = first_gate; gate; gate = gate->next)
    {
      assert(gate->type != Gate::tTRUE or
	     (gate->determined and gate->value == true));
      assert(gate->type != Gate::tFALSE or
	     (gate->determined and gate->value == false));
      if(gate->determined)
	gate->mark_coi(nof_relevant_gates);
    }
  for(Gate* gate = first_gate; gate; gate = gate->next)
    {
      if(gate->temp == -1) {
	gate->type = Gate::tDELETED;
	while(gate->parents) delete gate->parents;
	while(gate->children) delete gate->children;
      }
    }
  remove_deleted_gates();
}





/**************************************************************************
 *
 * Routines for printing circuits in the BC1.0 format
 *
 **************************************************************************/


void
BC::print(const char * const filename)
{
  FILE * const fp = fopen(filename, "w");
  if(!fp)
    return;
  print(fp);
  fclose(fp);
}

void
BC::print(FILE * const fp)
{
  static char temp_name[32];
  unsigned int temp_counter = 1;

  /* Remove all names that start with the underscore */
  remove_underscore_names();

  /* Enumerate the unnamed gates in the temp fields */
  for(Gate* gate = first_gate; gate; gate = gate->next)
    {
      gate->temp = 0;
      if(gate->get_first_name() == 0)
	gate->temp = temp_counter++;
    }

  fprintf(fp, "BC1.0\n");

  for(Gate* gate = first_gate; gate; gate = gate->next)
    {
      const char* name = 0;

      if(gate->temp > 0)
	{
	  /* A gate with no name, assign a temporary name */
	  sprintf(temp_name, "_t%d", gate->temp);
	  name = temp_name;
	}
      else
	{
	  /* Handle possible duplicate names */
	  Handle *handle = gate->handles;
	  while(handle)
	    {
	      if(handle->get_type() == Handle::ht_NAME)
		{
		  const char* n = ((NameHandle *)handle)->get_name();
		  DEBUG_ASSERT(n);
		  if(!name)
		    {
		      /* First name */
		      name = n;
		    }
		  else
		    {
		      /* A duplicate name */
		      fprintf(fp, "%s := %s;\n", n, name);
		    }
		}
	      handle = handle->get_next();
	    }
	  DEBUG_ASSERT(name);
	}

    /* Print the gate definition */
    if(gate->type == Gate::tVAR) {
      fprintf(fp, "%s", name);
    } else {
      fprintf(fp, "%s := ", name);
      switch(gate->type) {
      case Gate::tTRUE:
	fprintf(fp, "T");
	break;
      case Gate::tFALSE:
	fprintf(fp, "F");
	break;
      case Gate::tREF:
	DEBUG_ASSERT(gate->count_children() == 1);
	gate->print_child_list(fp);
	break;
      case Gate::tNOT:
	DEBUG_ASSERT(gate->count_children() == 1);
	fprintf(fp, "NOT(");
	gate->print_child_list(fp);
	fprintf(fp, ")");
	break;
      case Gate::tEQUIV:
	DEBUG_ASSERT(gate->count_children() >= 1);
	fprintf(fp, "EQUIV(");
	gate->print_child_list(fp);
	fprintf(fp, ")");
	break;
      case Gate::tOR:
	DEBUG_ASSERT(gate->count_children() >= 1);
	fprintf(fp, "OR(");
	gate->print_child_list(fp);
	fprintf(fp, ")");
	break;
      case Gate::tAND:
	DEBUG_ASSERT(gate->count_children() >= 1);
	fprintf(fp, "AND(");
	gate->print_child_list(fp);
	fprintf(fp, ")");
	break;
      case Gate::tEVEN:
	DEBUG_ASSERT(gate->count_children() >= 1);
	fprintf(fp, "EVEN(");
	gate->print_child_list(fp);
	fprintf(fp, ")");
	break;
      case Gate::tODD:
	DEBUG_ASSERT(gate->count_children() >= 1);
	fprintf(fp, "ODD(");
	gate->print_child_list(fp);
	fprintf(fp, ")");
	break;
      case Gate::tITE:
	DEBUG_ASSERT(gate->count_children() == 3);
	fprintf(fp, "ITE(");
	gate->print_child_list(fp);
	fprintf(fp, ")");
	break;
      case Gate::tTHRESHOLD:
	DEBUG_ASSERT(gate->count_children() >= 1);
	fprintf(fp, "[%u,%u](", gate->tmin, gate->tmax);
	gate->print_child_list(fp);
	fprintf(fp, ")");
	break;
      case Gate::tATLEAST:
	DEBUG_ASSERT(gate->count_children() >= 1);
	fprintf(fp, "[%u,%u](", gate->tmin, gate->count_children());
	gate->print_child_list(fp);
	fprintf(fp, ")");
	break;
      default:
	internal_error(text_NI,__FILE__,__LINE__,Gate::typeNames[gate->type]);
      }
    }
    fprintf(fp, ";\n");
    /* Print the gate constraint */
    if(gate->determined)
      fprintf(fp, "ASSIGN %s%s;\n", gate->value?"":"~", name);
  }

  reset_temp_fields();
}





/**************************************************************************
 *
 * Routines for printing circuits in the graphviz format
 *
 **************************************************************************/

void
BC::to_dot(const char * const filename) const
{
  FILE *fp = fopen(filename, "w");
  if(!fp)
    return;
  to_dot(fp);
  fclose(fp);
}

void
BC::to_dot(FILE * const fp) const
{
  fprintf(fp, "digraph circuit {\n");
  for(const Gate *gate = first_gate; gate; gate = gate->next)
    {
      fprintf(fp, "\"%u\" ", gate->index);
      if(gate->determined)
	fprintf(fp, "[color=red]");
      /* Label the vertex with type etc */
      fprintf(fp, "[label=\"");      
      if(gate->type == Gate::tEQUIV) {
	fprintf(fp, "EQUIV");
      }
      else if(gate->type == Gate::tOR) {
	fprintf(fp, "OR");
      }
      else if(gate->type == Gate::tAND) {
	fprintf(fp, "AND");
      }
      else if(gate->type == Gate::tEVEN) {
	fprintf(fp, "EVEN");
      }
      else if(gate->type == Gate::tODD) {
	fprintf(fp, "ODD");
      }
      else if(gate->type == Gate::tNOT) {
	fprintf(fp, "NOT");
      }
      else if(gate->type == Gate::tREF) {
	fprintf(fp, "REF");
      }
      else if(gate->type == Gate::tITE) {
	fprintf(fp, "ITE");
      }
      else if(gate->type == Gate::tTHRESHOLD) {
	fprintf(fp, "[%d,%d]", gate->tmin, gate->tmax);
      }
      else if(gate->type == Gate::tATLEAST) {
	fprintf(fp, ">=%d", gate->tmin);
      }
      else if(gate->type == Gate::tVAR) {
	DEBUG_ASSERT(gate->children == 0);
	fprintf(fp, "VAR");
      }
      else if(gate->type == Gate::tTRUE) {
	DEBUG_ASSERT(gate->children == 0);
	fprintf(fp, "T");
      }
      else if(gate->type == Gate::tFALSE) {
	DEBUG_ASSERT(gate->children == 0);
	fprintf(fp, "F");
      }
      else if(gate->type == Gate::tDELETED) {
	DEBUG_ASSERT(gate->children == 0);
	fprintf(fp, "!!!DELETED!!!");
      }
      else {
	internal_error("%s:%d: NYI %d", __FILE__, __LINE__, gate->type);
      }
      if(gate->determined) {
	fprintf(fp, " %s", gate->value?"T":"F");
      }
      fprintf(fp, "\\n");
      gate->print_name_list(fp, ",");
      fprintf(fp, "\"];\n");

      if(gate->type == Gate::tITE)
	{
	  /* The only noncommutative gate type, print edge annotations */
	  const ChildAssoc* ca = gate->children;
	  const Gate* child = ca->child;
	  fprintf(fp, "\"%u\" -> \"%u\" [label=\"if\"];\n",
		  gate->index, child->index);
	  ca = ca->next_child;
	  child = ca->child;
	  fprintf(fp, "\"%u\" -> \"%u\" [label=\"then\"];\n",
		  gate->index, child->index);
	  ca = ca->next_child;
	  child = ca->child;
	  fprintf(fp, "\"%u\" -> \"%u\" [label=\"else\"];\n",
		  gate->index, child->index);
	  DEBUG_ASSERT(ca->next_child == 0);
	}
      else
	{
	  /* Commutative gate type, print edges with no annotations */
	  DEBUG_ASSERT(gate->is_commutative());
	  for(const ChildAssoc *ca = gate->children; ca; ca = ca->next_child)
	    fprintf(fp, "\"%u\" -> \"%u\";\n", gate->index, ca->child->index);
	}
    }
  fprintf(fp, "}\n");
}

















/**************************************************************************
 *
 * Sharing of common substructure
 * Modifies the circuit structure
 *
 **************************************************************************/

bool BC::share()
{
  unsigned int nof_gates, nof_edges, nof_removed;

  if(!first_gate) return true;
  
  Gate **cache = (Gate**)calloc(index_to_gate.size(), sizeof(Gate*));
  GateHash *ht = new GateHash(index_to_gate.size() * 2 + 1);

  for(Gate *gate = first_gate; gate; gate = gate->next)
    if(!gate->share(this, ht, cache))
      goto conflict_exit;
  
  remove_deleted_gates(nof_removed, nof_gates);

  if(verbose) {
    compute_size(nof_gates, nof_edges);
    verbose_print("The circuit has %u gates and %u edges after sharing\n",
		  nof_gates, nof_edges);
  }
  
  //ht->print_distribution();
  delete ht; ht = 0;
  free(cache); cache = 0;
  return true;

 conflict_exit:
  delete ht; ht = 0;
  free(cache); cache = 0;
  return false;
}





/**************************************************************************
 *
 * Some auxiliary routines
 *
 **************************************************************************/


std::vector<Gate*>*
BC::get_top_down_ordering() const
{
  std::vector<Gate*>* ordering = new std::vector<Gate*>();
  const unsigned int N = index_to_gate.size();
  std::vector<unsigned int> nof_unvisited_parents(N, 0);
  std::list<Gate*> s;
  for(unsigned int i = 0; i < N; i++)
    {
      Gate* const g = index_to_gate[i];
      if(!g)
	continue;
      const unsigned int nof_p = g->count_parents();
      nof_unvisited_parents[g->index] = nof_p;
      if(nof_p == 0)
	s.push_back(g);
    }
  while(!s.empty())
    {
      Gate* const g = s.front();
      s.pop_front();
      ordering->push_back(g);
      for(const ChildAssoc* ca = g->children; ca; ca = ca->next_child)
	{
	  Gate* const child = ca->child;
	  DEBUG_ASSERT(nof_unvisited_parents[child->index] > 0);
	  nof_unvisited_parents[child->index]--;
	  if(nof_unvisited_parents[child->index] == 0)
	    s.push_back(child);
	}
    }
  return ordering;
}

std::vector<Gate*>*
BC::get_bottom_up_ordering() const
{
  std::vector<Gate*>* ordering = get_top_down_ordering();
  const unsigned int N = ordering->size();
  for(unsigned int i = 0; i < N / 2; i++)
    {
      Gate* tmp = ordering->operator[](i);
      ordering->operator[](i) = ordering->operator[](N-1-i);
      ordering->operator[](N-i-1) = tmp;
    }
  return ordering;
}



/*
 * WARNING: uses temp fields
 */
void
BC::compute_stats(unsigned int &max_min_height, unsigned int &max_max_height)
{
  max_min_height = 0;
  max_max_height = 0;

  reset_temp_fields(-1);

  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      const unsigned int min_height = gate->compute_min_height();
      if(min_height > max_min_height)
	max_min_height = min_height;
    }

  reset_temp_fields(-1);

  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      const unsigned int max_height = gate->compute_max_height();
      if(max_height > max_max_height)
	max_max_height = max_height;
    }

  reset_temp_fields(0);
}


void
BC::compute_size(unsigned int &nof_gates, unsigned int &nof_edges)
{
  nof_gates = 0;
  nof_edges = 0;
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      nof_gates++;
      nof_edges += gate->count_children();
    }
}

unsigned int
BC::count_gates()
{
  unsigned int nof_gates = 0;
  for(Gate *gate = first_gate; gate; gate = gate->next)
    nof_gates++;
  return nof_gates;
}

void
BC::remove_deleted_gates()
{
  unsigned int removed, remaining;
  remove_deleted_gates(removed, remaining);
}

void
BC::remove_deleted_gates(unsigned int &removed, unsigned int &remaining)
{
  removed = 0;
  remaining = 0;
  Gate **prev_next_ptr = &first_gate;
  Gate *gate = first_gate;
  while(gate) {
    if(gate->type == Gate::tDELETED) {
      DEBUG_ASSERT(!gate->pstack_next);
      DEBUG_ASSERT(!gate->parents);
      DEBUG_ASSERT(!gate->children);
      *prev_next_ptr = gate->next;
      release_gate(gate);
      //delete gate;
      gate = *prev_next_ptr;
      removed++;
    }
    else {
      prev_next_ptr = &gate->next;
      gate = gate->next;
      remaining++;
    }
  }
}





/**************************************************************************
 * Transform the circuit into a form that can be translated into CNF
 * Remove double negations and ref-gates
 * Translate threshold gates into normal gates
 * Explode n-ary equivs, odds and evens
 **************************************************************************/

bool
BC::cnf_normalize()
{
  unsigned int nof_gates, nof_removed;

  assert(!pstack);

  /* Add all the gates in pstack */
  pstack = 0;
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      gate->temp = 0;
      gate->in_pstack = true;
      gate->pstack_next = pstack;
      pstack = gate;
    }

  /* Normalize gates until pstack is empty */
  while(pstack)
    {
      Gate *gate = pstack;
      assert(gate->in_pstack);
      gate->in_pstack = false;
      pstack = gate->pstack_next;
      gate->pstack_next = 0;
      if(!gate->cnf_normalize(this))
	return false;
    }
  
  remove_deleted_gates(nof_removed, nof_gates);

  verbose_print("The circuit has %d gates after CNF normalization\n",
		nof_gates);

  return true;
}





bool
BC::edimacs_normalize()
{
  unsigned int nof_gates, nof_removed;

  assert(!pstack);

  /* Add all the gates in pstack */
  pstack = 0;
  for(Gate *gate = first_gate; gate; gate = gate->next) {
    gate->temp = 0;
    gate->in_pstack = true;
    gate->pstack_next = pstack;
    pstack = gate;
  }

  /* Normalize gates until pstack is empty */
  while(pstack) {
    Gate *gate = pstack;
    assert(gate->in_pstack);
    gate->in_pstack = false;
    pstack = gate->pstack_next;
    gate->pstack_next = 0;
    if(!gate->edimacs_normalize(this))
      return false;
  }

  remove_deleted_gates(nof_removed, nof_gates);

  verbose_print("The circuit has %d gates after normalization\n", nof_gates);

  return true;
}





/**************************************************************************
 *
 * Simplifies the circuit
 *
 **************************************************************************/

bool
BC::simplify(const SimplifyOptions& opts)
{
  unsigned int nof_gates, nof_removed, nof_edges;

  changed = true;
  while(changed)
    {
      changed = false;
      
      /* Add all the gates in pstack */
      assert(!pstack);
      for(Gate* gate = first_gate; gate; gate = gate->next)
	{
	  gate->temp = 0;
	  gate->in_pstack = true;
	  gate->pstack_next = pstack;
	  pstack = gate;
	}
      
      /* Simplify gates until pstack is empty */
      while(pstack)
	{
	  Gate* const gate = pstack;
	  assert(gate->in_pstack);
	  gate->in_pstack = false;
	  pstack = gate->pstack_next;
	  gate->pstack_next = 0;
	  if(!gate->simplify(this, opts))
	    goto conflict_exit;
	}
      
      remove_deleted_gates(nof_removed, nof_gates);
      
      if(verbose) {
	compute_size(nof_gates, nof_edges);
	verbose_print("The circuit has %u gates and %u edges after simplification\n", nof_gates, nof_edges);
      }
      
      if(!share())
	goto conflict_exit;
      
      if(!preserve_all_solutions)
	mir();
    }
  
  return true;
  
 conflict_exit:
  /* Clear pstack */
  while(pstack)
    {
      Gate *gate = pstack;
      assert(gate->in_pstack);
      gate->in_pstack = false;
      pstack = gate->pstack_next;
      gate->pstack_next = 0;
    }
  return false;
}















/**************************************************************************
 *
 * Print the circuit in edimacs format
 *
 **************************************************************************/

void
BC::to_edimacs(FILE *out, const bool notless, const bool do_simplify)
{
  int nof_variables;

  /*
   * Normalize gates
   */
  if(!edimacs_normalize())
    goto unsat_exit;
  
  
  /*
   * Simplify or at least share structure
   */
  if(do_simplify)
    {
      SimplifyOptions opts;
      if(!simplify(opts))
        goto unsat_exit;
    }
  else
    {
      if(!share())
        goto unsat_exit;
    }
  

  /*
   * Number the gates in temp-fields
   */
  {
    int gate_num = 1;
    for(Gate *gate = first_gate; gate; gate = gate->next) {
      assert(gate->type != Gate::tTRUE ||
             (gate->determined && gate->value == true));
      assert(gate->type != Gate::tFALSE ||
             (gate->determined && gate->value == false));
      if(notless && gate->type == Gate::tNOT)
        {
          assert(!gate->determined);
          assert(gate->children->child->type != Gate::tNOT);
          gate->temp = -1;
        }
      else
        {
          gate->temp = gate_num++;
        }
    }
    nof_variables = gate_num-1;
  }
  
  /*
   * Print translation table
   */
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      Handle *handle = gate->handles;
      while(handle) {
        if(handle->get_type() == Handle::ht_NAME) {
          const char *name = ((NameHandle *)handle)->get_name();
          DEBUG_ASSERT(name);
          if(notless && gate->type == Gate::tNOT) {
            fprintf(out, "c %s <-> %d\n", name, -gate->children->child->temp);
          } else {
            fprintf(out, "c %s <-> %d\n", name, gate->temp);
          }
        }
        handle = handle->get_next();
      }
    }

  /*
   * Print problem line
   */
  fprintf(out, "p noncnf %d\n", nof_variables+1);

  /*
   * Print gates
   */
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      if(gate->temp <= 0) {
        /* Not relevant */
        continue;
      }
      /*
       * Print
       */
      gate->edimacs_print(out, notless);
    }


  /*
   * Print the output gate
   */
  fprintf(out, "4 -1 %d ", nof_variables+1);
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      bool value = false;
      if(gate->type == Gate::tTRUE ||
         gate->type == Gate::tFALSE)
        continue;
      if(notless && gate->type == Gate::tNOT)
        {
          DEBUG_ASSERT(!gate->determined);
          DEBUG_ASSERT(gate->children->child->type != Gate::tNOT);
          continue;
        }
      if(gate->determined == false)
        continue;
      if(gate->value == true)
        value = value ^ true;
      fprintf(out, "%s%d ", (value == false)?"-":"", gate->temp);
    }
  fprintf(out, "0\n");

  return;

 unsat_exit:
  /*
   * Print a small unsatisfiable circuit
   */
  fprintf(out, "c The instance was unsatisfiable\n");
  fprintf(out, "p noncnf 1\n");
  fprintf(out, "1 -1 1 0\n");
  return;
}




void
BC::print_input_gate_names(FILE * const fp, const char *separator)
{
  bool is_first = true;
  const char *sep = " ";
  if(separator)
    sep = separator;
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      if(gate->type == Gate::tVAR)
	{
	  if(is_first)
	    is_first = false;
	  else
	    fprintf(verbstr, "%s", sep);
	  gate->print_name_list(fp, sep);
	}
    }
}



/**************************************************************************
 *
 * Routines related to the monotone variable rule
 *
 **************************************************************************/


void BC::mir()
{
  mir_compute_polarity_information();

  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      if(gate->type != Gate::tVAR)
	continue;
      if(gate->determined)
	continue;
      if(gate->mir_pos == false)
	{
	  gate->determined = true;
	  gate->value = false;
	  changed = true;
	  //fprintf(stderr, "MIR assigned a variable to false\n");
	}
      else if(gate->mir_neg == false)
	{
	  gate->determined = true;
	  gate->value = true;
	  changed = true;
	  //fprintf(stderr, "MIR assigned a variable to true\n");
	}
    }
}


void BC::mir_compute_polarity_information()
{
  /* Reset polarity fields */
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      gate->mir_pos = false;
      gate->mir_neg = false;
    }

  /* Compute polarity information */
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      if(gate->determined)
	gate->mir_propagate_polarity(gate->value);
    }
}





/**************************************************************************
 *
 * Consistency check routines
 *
 **************************************************************************/

/*
 * Returns false if the current truth assignment is not consistent
 */
bool BC::check_consistency()
{
  for(Gate *gate = first_gate; gate; gate = gate->next) {
    if(!gate->check_consistency()) {
      /* Uncomment this to have more info on the inconsistency
      fprintf(stderr, "An inconsistency at a gate of type %s(%s) having "
	      " the children ", Gate::typeNames[gate->type],
	      gate->determined?(gate->value?"true":"false"):"unassigned");
      for(ChildAssoc *ca = gate->children; ca; ca = ca->next_child) {
	fprintf(stderr, "%s(%s) ", Gate::typeNames[ca->child->type],
		ca->child->determined?(ca->child->value?"true":"false"):
		"unassigned");
      }
      */
      return false;
    }
  }
  return true;
}





/**************************************************************************
 *
 * Auxiliary routines 
 *
 **************************************************************************/

std::list<Gate*>*
BC::add_unsigned_constant(unsigned int c)
{
  std::list<Gate *>* result = new std::list<Gate*>();

  if(c == 0) {
    result->push_back(new_FALSE());
    return result;
  }
  while(c > 0)
    {
      if((c & 0x01) == 0)
	result->push_front(new_FALSE());
      else
	result->push_front(new_TRUE());
      c = c / 2;
    }
  return result;
}


Gate*
BC::add_unsigned_le(const std::list<Gate*>* const c1,
		    const std::list<Gate*>* const c2)
{
  assert(c1);
  assert(c2);
  assert(!c1->empty());
  assert(!c2->empty());
  
  Gate *equiv_part = new Gate(Gate::tAND);
  install_gate(equiv_part);

  Gate *lt_part = 0;
  
  std::list<Gate*>::const_reverse_iterator c1i = c1->rbegin();
  std::list<Gate*>::const_reverse_iterator c2i = c2->rbegin();
  while(c1i != c1->rend() && c2i != c2->rend())
    {
      Gate * const bit1 = *c1i++;
      Gate * const bit2 = *c2i++;
      
      /* The 'equal'-part */
      Gate * const new_equiv = new_EQUIV(bit1, bit2);
      equiv_part->add_child(new_equiv);
      
      /* The 'less than'-part */
      Gate *less_bit = new_AND(new_NOT(bit1), bit2);
      if(lt_part == 0) {
	lt_part = less_bit;
      } else {
	lt_part = new_OR(less_bit, new_AND(new_equiv, lt_part));
      }
    }

  /* Handle cases when inputs were of different length */
  if(c1i != c1->rend()) {
    while(c1i != c1->rend()) {
      Gate * const bit1 = *c1i++;
      /* bit2 is 0 */

      /* The 'equal'-part */
      Gate * const new_equiv = new_NOT(bit1);
      equiv_part->add_child(new_equiv);
      
      /* The 'less than'-part */
      assert(lt_part);
      lt_part = new_AND(new_equiv, lt_part);
    }
  }
  else if(c2i != c2->rend()) {
    while(c2i != c2->rend()) {
      /* bit1 is 0 */
      Gate * const bit2 = *c2i++;

      /* The 'equal'-part */
      Gate * const new_equiv = new_NOT(bit2);
      equiv_part->add_child(new_equiv);
      
      /* The 'less than'-part */
      assert(lt_part);
      lt_part = new_OR(bit2, lt_part);
    }
  }

  return new_OR(lt_part, equiv_part);
}



Gate*
BC::add_unsigned_ge(const std::list<Gate*>* const c1,
		    const std::list<Gate*>* const c2)
{
  return add_unsigned_le(c2, c1);
}



Gate*
BC::add_unsigned_lt(const std::list<Gate*>* const c1,
		    const std::list<Gate*>* const c2)
{
  assert(c1);
  assert(c2);
  assert(!c1->empty());
  assert(!c2->empty());
  
  Gate *lt_part = 0;
  
  std::list<Gate*>::const_reverse_iterator c1i = c1->rbegin();
  std::list<Gate*>::const_reverse_iterator c2i = c2->rbegin();
  while(c1i != c1->rend() and c2i != c2->rend())
    {
      Gate* const bit1 = *c1i++;
      Gate* const bit2 = *c2i++;
      
      /* The 'less than'-part */
      Gate* const less_bit = new_AND(new_NOT(bit1), bit2);
      if(lt_part == 0) {
	lt_part = less_bit;
      } else {
	lt_part = new_OR(less_bit, new_AND(new_EQUIV(bit1, bit2), lt_part));
      }
    }

  /* Handle cases when inputs were of different length */
  if(c1i != c1->rend()) {
    while(c1i != c1->rend()) {
      Gate* const bit1 = *c1i++;
      /* bit2 is 0 */
      
      /* The 'less than'-part */
      assert(lt_part);
      lt_part = new_AND(new_NOT(bit1), lt_part);
    }
  }
  else if(c2i != c2->rend()) {
    while(c2i != c2->rend()) {
      /* bit1 is 0 */
      Gate* const bit2 = *c2i++;

      /* The 'less than'-part */
      assert(lt_part);
      lt_part = new_OR(bit2, lt_part);
    }
  }

  return lt_part;
}



Gate*
BC::add_unsigned_gt(const std::list<Gate*>* const c1,
		    const std::list<Gate*>* const c2)
{
  return add_unsigned_lt(c2, c1);
}



std::list<Gate*>*
BC::add_unsigned_adder(const std::list<Gate*>* const augend,
		       const std::list<Gate*>* const addend)
{
  assert(augend);
  assert(addend);
  assert(augend->size() >= 1);
  assert(addend->size() >= 1);

  std::list<Gate*> *result = new std::list<Gate*>();

  std::list<Gate*>::const_reverse_iterator i1 = augend->rbegin();
  std::list<Gate*>::const_reverse_iterator i2 = addend->rbegin();
  Gate *carry_in = 0;
  while(i1 != augend->rend() && i2 != addend->rend())
    {
      Gate * const augend_bit = *i1; i1++;
      Gate * const addend_bit = *i2; i2++;
      /*
       * Add full adder
       */
      Gate *xor1 = new_ODD(augend_bit, addend_bit);
      Gate *and1 = new_AND(augend_bit, addend_bit);
      if(carry_in) {
	Gate *xor2 = new_ODD(xor1, carry_in);
	result->push_front(xor2);
	Gate *and2 = new_AND(xor1, carry_in);
	carry_in = new_OR(and2, and1);
      } else {
	/* no carry in for the first bits, a reduced full adder */
	result->push_front(xor1);
	carry_in = and1;
      }
    }
  /* Handle cases when inputs were of different length */
  if(i1 != augend->rend()) {
    while(i1 != augend->rend()) {
      Gate *augend_bit = *i1; i1++;
      /* A reduced full adder when addend bit = 0 */
      DEBUG_ASSERT(carry_in);
      Gate *result_bit = new_ODD(augend_bit, carry_in);
      result->push_front(result_bit);
      carry_in = new_AND(augend_bit, carry_in);
    }
  }
  else if(i2 != addend->rend()) {
    while(i2 != addend->rend()) {
      Gate *addend_bit = *i2; i2++;
      /* A reduced full adder when augend bit = 0 */
      DEBUG_ASSERT(carry_in);
      Gate *result_bit = new_ODD(addend_bit, carry_in);
      result->push_front(result_bit);
      carry_in = new_AND(addend_bit, carry_in);
    }
  }

  result->push_front(carry_in);
  return result;
}


/*
 * Adds a circuit counting the number of true gates in unsigned binary
 */
std::list<Gate *>*
BC::add_true_gate_counter(const std::list<Gate *>* const gates)
{
  std::list<Gate *> *result = 0;

  if(gates->empty())
    {
      result = new std::list<Gate *>();
      result->push_back(new_FALSE());
      return result;
    }

  std::list<std::list<Gate *> *> *sums_in_current_level = new std::list<std::list<Gate *> *>();
  std::list<std::list<Gate *> *> *sums_in_next_level = new std::list<std::list<Gate *> *>();

  for(std::list<Gate *>::const_iterator gi = gates->begin();
      gi != gates->end();
      )
    {
      std::list<Gate *> *sum = new std::list<Gate *>();
      Gate *g1 = *gi++;
      if(gi == gates->end()) {
	sum->push_front(g1);
      }
      else {
	Gate *g2 = *gi++;
	if(gi == gates->end()) {
	  sum->push_back(new_AND(g1,g2));
	  sum->push_back(new_ODD(g1,g2));
	}
	else {
	  Gate *g3 = *gi++;
	  sum->push_back(new_OR(new_AND(g1,g2),
				new_OR(new_AND(g1,g3),new_AND(g2,g3))));
	  sum->push_back(new_ODD(g1,new_ODD(g2,g3)));
	}
      }
      sums_in_current_level->push_back(sum);
    }

  assert(!sums_in_current_level->empty());

  while(sums_in_current_level->size() > 1)
    {
      while(!sums_in_current_level->empty())
	{
	  std::list<Gate *> *sum1 = sums_in_current_level->front();
	  sums_in_current_level->pop_front();
	  if(sums_in_current_level->empty())
	    {
	      sums_in_next_level->push_back(sum1);
	      break;
	    }
	  std::list<Gate *> *sum2 = sums_in_current_level->front();
	  sums_in_current_level->pop_front();
	  std::list<Gate *> *sum = add_unsigned_adder(sum1, sum2);
	  sums_in_next_level->push_back(sum);
	  free(sum1);
	  free(sum2);
	}
      std::list<std::list<Gate *> *> *tmp = sums_in_current_level;
      sums_in_current_level = sums_in_next_level;
      sums_in_next_level = tmp;
    }

  assert(!sums_in_current_level->empty());

  result = sums_in_current_level->front();
  delete sums_in_current_level;
  delete sums_in_next_level;

  return result;
}


std::list<Gate*>*
BC::add_signed_adder(const std::list<Gate*>* const augend,
		     const std::list<Gate*>* const addend)
{
  std::list<Gate*> *result = new std::list<Gate*>();

  assert(augend->size() >= 2);
  assert(addend->size() >= 2);

  std::list<Gate*>::const_reverse_iterator i1 = augend->rbegin();
  std::list<Gate*>::const_reverse_iterator i2 = addend->rbegin();
  Gate *carry_in = 0;
  Gate *augend_bit = 0;
  Gate *addend_bit = 0;
  while(i1 != augend->rend() && i2 != addend->rend()) {
    augend_bit = *i1; i1++;
    addend_bit = *i2; i2++;
    /*
     * Add full adder
     */
    Gate *xor1 = new_ODD(augend_bit, addend_bit);
    Gate *and1 = new_AND(augend_bit, addend_bit);
    if(carry_in) {
      Gate *xor2 = new_ODD(xor1, carry_in);
      result->push_front(xor2);
      Gate *and2 = new_AND(xor1, carry_in);
      carry_in = new_OR(and2, and1);
    } else {
      /* The first carry_in is false */
      result->push_front(xor1);
      carry_in = and1;
    }
  }
  /* Handle cases when inputs were of different length */
  if(i1 != augend->rend()) {
    /* repeat the sign bit of addend */
    while(i1 != augend->rend()) {
      augend_bit = *i1; i1++;
      Gate *xor1 = new_ODD(augend_bit, addend_bit);
      Gate *and1 = new_AND(augend_bit, addend_bit);
      Gate *xor2 = new_ODD(xor1, carry_in);
      result->push_front(xor2);
      Gate *and2 = new_AND(xor1, carry_in);
      carry_in = new_OR(and2, and1);
    }
  } else if(i2 != addend->rend()) {
    /* repeat the sign bit of augend */
    while(i2 != addend->rend()) {
      addend_bit = *i2; i2++;
      Gate *xor1 = new_ODD(augend_bit, addend_bit);
      Gate *and1 = new_AND(augend_bit, addend_bit);
      Gate *xor2 = new_ODD(xor1, carry_in);
      result->push_front(xor2);
      Gate *and2 = new_AND(xor1, carry_in);
      carry_in = new_OR(and2, and1);
    }
  }
  /*
   * One more round with the sign bits, carry out is discarded
   */
  Gate *xor1 = new_ODD(augend_bit, addend_bit);
  //Gate *and1 = new_AND(augend_bit, addend_bit);
  Gate *xor2 = new_ODD(xor1, carry_in);
  result->push_front(xor2);
   
  return result;
}




std::list<Gate*>*
BC::add_signed_subtractor(const std::list<Gate*>* const minuend,
			  const std::list<Gate*>* const subtrahend)
{
  std::list<Gate*> *result = new std::list<Gate*>();

  assert(minuend->size() >= 2);
  assert(subtrahend->size() >= 2);

  std::list<Gate*>::const_reverse_iterator i1 = minuend->rbegin();
  std::list<Gate*>::const_reverse_iterator i2 = subtrahend->rbegin();
  Gate *carry_in = 0;
  Gate *minuend_bit = 0;
  Gate *subtrahend_bit = 0;
  Gate *inversed_subtrahend_bit = 0;
  while(i1 != minuend->rend() && i2 != subtrahend->rend()) {
    minuend_bit = *i1; i1++;
    subtrahend_bit = *i2; i2++;
    /*
     * Add full adder, the subtrahend bit is inversed
     */
    inversed_subtrahend_bit = new Gate(Gate::tNOT, subtrahend_bit);
    install_gate(inversed_subtrahend_bit);
    Gate *xor1 = new Gate(Gate::tODD, minuend_bit, inversed_subtrahend_bit);
    install_gate(xor1);
    Gate *and1 = new Gate(Gate::tAND, minuend_bit, inversed_subtrahend_bit);
    install_gate(and1);
    if(carry_in) {
      Gate *xor2 = new Gate(Gate::tODD, xor1, carry_in);
      install_gate(xor2);
      result->push_front(xor2);
      Gate *and2 = new Gate(Gate::tAND, xor1, carry_in);
      install_gate(and2);
      Gate *carry_out = new Gate(Gate::tOR, and2, and1);
      install_gate(carry_out);
      carry_in = carry_out;
    } else {
      /* The first carry_in is true */
      Gate *xor2 = new Gate(Gate::tNOT, xor1);
      install_gate(xor2);
      result->push_front(xor2);
      Gate *carry_out = new Gate(Gate::tOR, xor1, and1);
      install_gate(carry_out);
      carry_in = carry_out;
    }
  }
  /* Handle cases when inputs were of different length */
  if(i1 != minuend->rend()) {
    /* repeat the sign bit of subtrahend */
    while(i1 != minuend->rend()) {
      minuend_bit = *i1; i1++;
      Gate *xor1 = new Gate(Gate::tODD, minuend_bit, inversed_subtrahend_bit);
      install_gate(xor1);
      Gate *and1 = new Gate(Gate::tAND, minuend_bit, inversed_subtrahend_bit);
      install_gate(and1);
      Gate *xor2 = new Gate(Gate::tODD, xor1, carry_in);
      install_gate(xor2);
      result->push_front(xor2);
      Gate *and2 = new Gate(Gate::tAND, xor1, carry_in);
      install_gate(and2);
      Gate *carry_out = new Gate(Gate::tOR, and2, and1);
      install_gate(carry_out);
      carry_in = carry_out;
    }
  } else if(i2 != subtrahend->rend()) {
    /* repeat the sign bit of minuend */
    while(i2 != subtrahend->rend()) {
      subtrahend_bit = *i2; i2++;
      /*
       * Add full adder, the subtrahend bit is inversed
       */
      inversed_subtrahend_bit = new Gate(Gate::tNOT, subtrahend_bit);
      install_gate(inversed_subtrahend_bit);
      Gate *xor1 = new Gate(Gate::tODD, minuend_bit, inversed_subtrahend_bit);
      install_gate(xor1);
      Gate *and1 = new Gate(Gate::tAND, minuend_bit, inversed_subtrahend_bit);
      install_gate(and1);
      Gate *xor2 = new Gate(Gate::tODD, xor1, carry_in);
      install_gate(xor2);
      result->push_front(xor2);
      Gate *and2 = new Gate(Gate::tAND, xor1, carry_in);
      install_gate(and2);
      Gate *carry_out = new Gate(Gate::tOR, and2, and1);
      install_gate(carry_out);
      carry_in = carry_out;
    }
  }
  /*
   * One more round with the sign bits, carry out is discarded
   */
  Gate *xor1 = new Gate(Gate::tODD, minuend_bit, inversed_subtrahend_bit);
  install_gate(xor1);
  Gate *and1 = new Gate(Gate::tAND, minuend_bit, inversed_subtrahend_bit);
  install_gate(and1);
  Gate *xor2 = new Gate(Gate::tODD, xor1, carry_in);
  install_gate(xor2);
  result->push_front(xor2);
   
  return result;
}









