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

#ifndef GATE_HH
#define GATE_HH

class Gate;
class ChildAssoc;

#include <list>
#include <vector>
#include "defs.hh"
#include "bc.hh"
#include "gatehash.hh"
#include "handle.hh"

/**
 * \brief A gate in a circuit.
 */
class Gate
{
  friend class BC;
  friend class ChildAssoc;

public:
  /** The possible gate types. */
  typedef enum {tEQUIV = 0, tOR, tAND, tEVEN, tODD, tITE,
                tNOT, tTRUE, tFALSE, tVAR, tTHRESHOLD, tATLEAST,
                tREF, tUNDEF, tDELETED, tNOFTYPES} Type;

private:
  /** The possible gate types as char strings. */
  static const char * const typeNames[tNOFTYPES];
  
  /** Initialize the fields of the gate. */
  void init();

  /**
   * Is the order of the children irrelevant?
   */
  bool is_commutative() const {return(type != tITE); }

  void count_child_info(unsigned int& nof_true,
			unsigned int& nof_false,
			unsigned int& nof_undet) const;

  void remove_determined_children(BC* const bc);

  void add_in_pstack(BC * const bc);
  void add_parents_in_pstack(BC * const bc);
  void add_children_in_pstack(BC * const bc);
  /** It is an error to call this function when the gate already has
   * the value !value */
  void transform_into_constant(BC* const bc, const bool value);
  bool remove_duplicate_and_g_not_g_children(BC* const bc,
					     const bool duplicates,
					     const bool g_not_g);
  bool remove_parity_duplicate_and_g_not_g_children(BC* const bc,
						    const bool duplicates,
						    const bool g_not_g);
  bool remove_cardinality_g_not_g(BC *);

  /*
   * Returns false if the current truth assignment is not consistent
   * (implying that the circuit is satisfiable)
   */
  bool check_consistency();

  /*
   * WARNING: use temp fields
   */
  unsigned int compute_min_height();
  unsigned int compute_max_height();


public:
  /** The type of the gate */
  Type type;

  /** The position in the "index_to_gate" array in the owning BC */
  unsigned int index;

  Handle *handles;

  /* Returns null if no name is found */
  const char* get_first_name() const;

  /**
   * Print the names of the gate
   * @param fp    the file stream
   * @param sep   the separator string printed between the names
   */
  void print_name_list(FILE * const fp, const char * const sep = ",") const;

  /** Print the comma-separated list of children in BC1.0 format.
   * A helper for BC::print(FILE * const fp). */
  void print_child_list(FILE* const fp) const;

  ChildAssoc* children;
  unsigned int _nof_children;
  ChildAssoc* parents;

  /** The lower bound for true children in THRESHOLD and ATLEAST gates. */
  unsigned int tmin;
  /** The upper bound for true children in THRESHOLD gates. */
  unsigned int tmax;

  Gate *next, *pstack_next;

  /* Flags for the value of the gate */
  bool determined;
  bool value;

  /* Polarity flags for the monotone variable rule */
  bool mir_pos, mir_neg;

  bool in_pstack;

  int temp;

  /** Create a gate with no children. */
  Gate(const Type);
  /** Create a gate with one child; \a type should be either NOT or REF. */
  Gate(const Type type, Gate* const child);
  /** Create a gate with two children;
   * \a type should be OR, AND, ODD, EVEN, EQUIV, THRESHOLD or ATLEAST. */
  Gate(const Type type, Gate* const child1, Gate* const child2);
  /** Create a gate with three children; \a type should be ITE. */
  Gate(const Type type, Gate* const if_gate,
       Gate* const then_gate, Gate* const else_gate);
  /** Create a gate with a list of children;
   * \a type should be OR, AND, ODD, EVEN, EQUIV, THRESHOLD or ATLEAST. */
  Gate(const Type t, const std::list<Gate*>* const children);
  /** Destroy the gate and associations/handles referencing it. */
  ~Gate();

  bool share(BC * const bc, GateHash * const ht, Gate ** const cache);
  bool cnf_normalize(BC* const bc);


  void cnf_get_clauses(std::list<std::vector<int> *> &clauses,
		       const bool notless);
  void cnf_get_clauses_polarity(std::list<std::vector<int> *> &clauses,
				const bool notless);

  /** Get clauses and xor-clauses in the xcnf-translation. */
  void xcnf_get_clauses(std::list<std::vector<int> *> &cnf_clauses,
			std::list<std::vector<int> *> &xor_clauses,
			const bool notless);
  /** Get clauses and xor-clauses in the xcnf-translation with polarity. */
  void xcnf_get_clauses_polarity(std::list<std::vector<int> *> &cnf_clauses,
				 std::list<std::vector<int> *> &xor_clauses,
				 const bool notless);

  bool edimacs_normalize(BC* const bc);
  void edimacs_print(FILE* const fp, const bool notless);
  void edimacs_print_children(FILE* const fp, const bool notless);

  /**
   * Write the gate in the ISCAS89 format.
   * The circuit must have been normalized with BC::cnf_normalize()
   * before calling this.
   * @param fp  the output file stream
   */
  void write_iscas89(FILE* const fp) const;
  /** A helper for write_iscas89() */
  void write_iscas89_children(FILE* const fp) const;
  /** A helper for write_iscas89() */
  void write_iscas89_name(FILE* const fp, const bool positive = true) const;
  /** A helper for write_iscas89() */
  void write_iscas89_map(FILE* const fp) const;


  /** Get a structural hash value for the gate. */
  unsigned int hash_value() const;

  /** Compare if two gates are of the same type and have the same children. */
  int comp(const Gate* const other) const;

  /** Sort the children list of the gate according to the child indices.
   *  Does nothing for gates of non-commutative type (e.g. ITE). */
  void sort_children();

  /** Count how many parents the gate has.
   * Time requirement: O(N), where N is the number of parents */
  unsigned int count_parents() const;
  /** Return true if the gate has any parents */
  bool has_parents() const;
  /** Return true if the gate has at least two parents */
  bool has_many_parents() const;

  /** Count how many children the gate has.
   * Time requirement: O(N), where N is the number of children */
  unsigned int count_children() const;

  /** Get the first child */
  Gate* first_child() const;
  /** Get the number of children of this gate. */
  unsigned int nof_children() const {return _nof_children; }

  /** Add a new child association for the gate.
   *  The child is added to the >front< of the child list. */
  void add_child(Gate* const child);

  /** Remove all child associations of the gate. */
  void remove_all_children();

  /** Check whether the circuit has cycles. */
  int test_acyclicity(std::list<const char*> &cycle);

  /** Mark the cone of influence (i.e. descendants) of the gate.
   * Assigns each gate in the cone with a unique number.
   * Assumes that the temp fields of gates have been initialized to -1. */
  void mark_coi(int& counter);

  /** Simplify the gate if possible.
   * @return false if an inconsistency was found (implying unsatisfiability
   *               of the circuit) */
  bool simplify(BC* const bc, const SimplifyOptions& opts);



  /**
   * Evaluate the value of the gate and,
   * if necessary, recursively its children.
   * @return false if the value cannot be evaluated
   */
  bool evaluate();

  /**
   * Returns true iff the value of the gate is determined and
   * justified by the values of children.
   */
  bool is_justified();

  /*
   * Progates the polarity information needed in the monotone variable rule.
   */
  void mir_propagate_polarity(bool polarity);
};


/** \brief An association between two gates, parent and child. */
class ChildAssoc {
public: 
  Gate* parent;
  Gate* child;

  ChildAssoc* prev_child;
  ChildAssoc* next_child;
  ChildAssoc* prev_parent;
  ChildAssoc* next_parent;

  /** Create a new association between \a parent and \a child. */
  ChildAssoc(Gate* const parent, Gate* const child);
  /** Destroy the association. */
  ~ChildAssoc();
  /** Change the current parent of the association to \a new_child. */
  void change_child(Gate* const new_child);
  /** Change the current parent of the association to \a new_parent.
   * Both the current and the new parent must be commutative gates. */
  void change_parent(Gate* const new_parent);

private:
  /* Some helper methods */
  void link_parent(Gate* const parent);
  void link_child(Gate* const child);
  void unlink_parent();
  void unlink_child();
};


inline Gate*
Gate::first_child() const
{
  DEBUG_ASSERT(children);
  DEBUG_ASSERT(children->child != this);
  return children->child;
}

inline bool
Gate::has_many_parents() const
{
  return(parents and parents->next_parent);
}

inline bool
Gate::has_parents() const
{
  return(parents);
}


#endif
