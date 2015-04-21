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

#ifndef BC_HH
#define BC_HH

class BC;
class SimplifyOptions;

#include <cstdio>
#include <list>
#include <map>
#include "defs.hh"
#include "gate.hh"
#include "handle.hh"
#include "hashset.hh"

/**
 * \brief A Boolean circuit.
 */
class BC
{
  friend class Gate;
  void remove_deleted_gates(unsigned int &return_nof_removed,
			    unsigned int &return_nof_remaining);
  void remove_deleted_gates();

  Gate *pstack;
  bool changed;

  bool contradictory;

  std::vector<Gate*> index_to_gate;
  std::vector<unsigned int> free_gate_indices;



public:
  BC();
  ~BC();

  /**
   * Read the circuit from the file stream \a fp.
   * \param fp  The input file stream.
   * \return    The circuit, or 0 if an error occurred.
   */
  static BC* parse_circuit(FILE* const fp);
  /**
   * Read the circuit from the file \a filename.
   * \param fp  The input file name.
   * \return    The circuit, or 0 if an error occurred.
   */
  static BC* parse_circuit(const char* const filename);

  /** Add an equivalence gate in the circuit.
   * \param  child1   A gate.
   * \param  child2   A gate.
   * \return A gate that evaluates to True iff \a child1 and \a child2
             evaluate to the same value. */
  Gate *new_EQUIV(Gate* const child1, Gate* const child2);

  /** Add an equivalence gate in the circuit.
   * \param  children  A list of gates.
   * \return A gate that evaluates to True iff all \a children
             evaluate to the same value. */
  Gate *new_EQUIV(const std::list<Gate*>* const children);

  /** Add an OR-gate in the circuit.
   * \param  child1   A gate.
   * \param  child2   A gate.
   * \return A gate that evaluates to True iff \a child1, \a child2,
             or both evaluate to True. */
  Gate *new_OR(Gate* const child1, Gate* const child2);

  /** Add an OR-gate in the circuit.
   * \param  children  A list of gates.
   * \return A gate that evaluates to True iff at least one of \a children
             evaluates to True. */
  Gate *new_OR(const std::list<Gate*>* const children);

  /** Add a parity gate in the circuit.
   * \param  c1 A child gate
   * \param  c2 A child gate
   * \return A gate that evaluates to True iff an odd number of {\a c1, \a c2}
             evaluate to True. */
  Gate *new_ODD(Gate* const c1, Gate* const c2);

  /** Add a parity gate in the circuit.
   * \param  children  A list of gates.
   * \return A gate that evaluates to True iff an odd number of \a children
             evaluate to True. */
  Gate *new_ODD(const std::list<Gate*>* const children);

  /** Add a parity gate in the circuit.
   * \param  c1 A child gate
   * \param  c2 A child gate
   * \return A gate that evaluates to True iff an even number of {\a c1, \a c2}
             evaluate to True. */
  Gate *new_EVEN(Gate* const c1, Gate* const c2);

  /** Add a parity gate in the circuit.
   * \param  children  A list of gates.
   * \return A gate that evaluates to True iff an even number of \a children
             evaluate to True. */
  Gate *new_EVEN(const std::list<Gate*>* const children);

  /** Add an AND-gate in the circuit.
   * \param  child1   A gate.
   * \param  child2   A gate.
   * \return A gate that evaluates to True iff both \a child1 and \a child2
             evaluate to True. */
  Gate *new_AND(Gate* const child1, Gate* const child2);

  /** Add an AND-gate in the circuit.
   * \param  children  A list of gates.
   * \return A gate that evaluates to True iff all \a children
             evaluate to True. */
  Gate *new_AND(const std::list<Gate*>* const children);

  /** Add a NOT-gate in the circuit.
   * \param  child   A gate.
   * \return A gate that evaluates to True iff \a child evaluates to False. */
  Gate *new_NOT(Gate* const child);

  Gate *new_REF(Gate* const child);

  /** Add an if-then-else gate in the circuit.
   * \param  i  The "if" gate.
   * \param  t  The "then" gate.
   * \param  e  The "else" gate.
   * \return  A gate that evaluates to (i) \a t if \a i evaluates to True, and
   *          (ii) \a e otherwise. */
  Gate *new_ITE(Gate* const i, Gate* const t, Gate* const e);

  /**
   * Add a cardinality gate in the circuit.
   * \param  tmin      The minimum threshold.
   * \param  tmax      The maximum threshold.
   * \param  children  A list of gates.
   * \return A gate that evaluates to True iff at least \a tmin but at most
   *         \a tmax of \a children evaluate to True. */
  Gate *new_CARDINALITY(const unsigned int tmin,
			const unsigned int tmax,
			const std::list<Gate*>* const children);

  /** Add the constant False gate in the circuit. */
  Gate *new_FALSE();

  /** Add the constant True gate in the circuit. */
  Gate *new_TRUE();

  /** Add a new Boolean variable in the circuit */
  Gate *new_VAR();

  /* For parsers etc only */
  Gate *new_UNDEF();

  /**
   * Force the value of the gate to false.
   * @return true if succesfull, false if the circuit was detected to become
   *         permanently unsatisfiable. */
  bool force_false(Gate* const gate);

  /**
   * Force the value of the Gate \a gate to true.
   * @return true if succesfull, false if the circuit was detected to become
   *         permanently unsatisfiable. */
  bool force_true(Gate* const gate);

  bool is_contradictory() const {return contradictory; }

  /**
   * Find gate by the name
   */
  NameHandle *find_gate(char *name);

  /*
   * Associate a gate with a name.
   * The returned handle points to the gate if the name was not reserved,
   * otherwise it points to the existing gate and name map is not modified.
   */
  NameHandle *insert_gate_name(char *name, Gate *gate);

  /**
   * Print the circuit in the BC1.0 format.
   * @param filename  the output file name
   */
  void print(const char * const filename);

  /**
   * Print the circuit in the BC1.0 format.
   * @param fp   the output file stream
   */
  void print(FILE * const fp);


  /**
   * Print the circuit in the graphviz dotty format.
   *
   * \param filename  The name of the output file.
   */
  void to_dot(const char * const filename) const;

  /**
   * Print the circuit in the graphviz dotty format.
   *
   * \param fp  The output file stream.
   */
  void to_dot(FILE * const fp) const;




  /**
   * Translate the circuit in edimacs and print
   * Note: may transform the circuit structure.
   */
  void to_edimacs(FILE *fp, const bool notless, const bool simplify);

  /**
   * Print the names of the named input gates.
   *
   * \param fp   The output file stream.
   * \param sep  The separator string printed between the names.
   */
  void print_input_gate_names(FILE * const fp, const char *sep = " ");


  /*
   * Solvers
   */
  /*
   * Returns
   *   0 if unsat
   *   1 if sat
   *   2 if undetermined
   *   3 if time out
   *   4 if out of memory
   *   5 if aborted
   * May transform the structure of the circuit
   * The circuit is left in an unclear state at the moment
   */
  int zchaff_solve(const bool perform_simplifications,
		   const bool polarity_cnf,
		   const bool notless,
		   const bool input_cuts_only,
		   const bool permute_cnf,
		   const unsigned int permute_cnf_seed);
  
  /*
   * Returns
   *   0 if unsat
   *   1 if sat
   * May transform the structure of the circuit
   * The circuit is left in an unclear state at the moment
   */
  int minisat_solve(const bool perform_simplifications,
		    const SimplifyOptions& opts,
		    const bool polarity_cnf,
		    const bool notless,
		    const bool input_cuts_only,
		    const bool permute_cnf,
		    const unsigned int permute_cnf_seed
		    );







  /**
   * Add a subcircuit of constant gates that binary encode the constant \a c.
   * E.g. when c=10, returns a list [True,False,True,False].
   */
  std::list<Gate*>* add_unsigned_constant(unsigned int c);

  /**
   * Add a subcircuit that computes how many of the gates in \a args
   * evaluate to True.
   * \param  args  A list of gates.
   * \return       A list of gates that gives (in binary and in the
   *               "most significant bit first" order) how many of
   *               the gates in \a args evaluate to True.
   */
  std::list<Gate*>* add_true_gate_counter(const std::list<Gate *> *args);

  /**
   * Add an unsigned "less than" comparator subcircuit in the circuit.
   * The input argument gate lists are in
   * the "most significant bit first" order.
   *
   * \param x  A gate list representing an unsigned integer
   * \param y  A gate list representing an unsigned integer
   * \return   A gate that evaluates to true iff \a x < \a y.
   */
  Gate* add_unsigned_lt(const std::list<Gate*>* const x,
			const std::list<Gate*>* const y);

  /**
   * Add an unsigned "less or equal" comparator subcircuit in the circuit.
   * The input argument gate lists are in
   * the "most significant bit first" order.
   *
   * \param x  A gate list representing an unsigned integer
   * \param y  A gate list representing an unsigned integer
   * \return   A gate that evaluates to true iff \a x <= \a y
   */
  Gate* add_unsigned_le(const std::list<Gate*>* const x,
			const std::list<Gate*>* const y);

  /**
   * Add an unsigned "greater or equal" comparator subcircuit in the circuit.
   * The input argument gate lists are in
   * the "most significant bit first" order.
   *
   * \param x  A gate list representing an unsigned integer
   * \param y  A gate list representing an unsigned integer
   * \return   A gate that evaluates to true iff \a x >= \a y.
   */
  Gate* add_unsigned_ge(const std::list<Gate*>* const x,
			const std::list<Gate*>* const y);

  /**
   * Add an unsigned "greater than" comparator subcircuit in the circuit.
   * The input argument gate lists are in
   * the "most significant bit first" order.
   *
   * \param x  A gate list representing an unsigned integer
   * \param y  A gate list representing an unsigned integer
   * \return   A gate that evaluates to true iff \a x > \a y.
   */
  Gate* add_unsigned_gt(const std::list<Gate*>* const x,
			const std::list<Gate*>* const y);

  /**
   * Add an unsigned adder subcircuit in the circuit.
   * Input and output argument gate lists are in
   * the "most significant bit first" order.
   *
   * \param augend  The augend.
   * \param addend  The addend.
   * \return        The list of output gates of the adder subcircuit.
   */
  std::list<Gate*>* add_unsigned_adder(const std::list<Gate*>* const augend,
				       const std::list<Gate*>* const addend);

  /**
   * Add a signed adder subcircuit in the circuit.
   * Input and output argument gate lists are in
   * the "most significant bit first" order,
   * the sign bit being the first one.
   *
   * \param augend  The augend.
   * \param addend  The addend.
   * \return        The list of output gates of the adder subcircuit.
   */
  std::list<Gate*>* add_signed_adder(const std::list<Gate*>* const augend,
				     const std::list<Gate*>* const addend);

  /**
   * Add a signed subtractor subcircuit in the circuit.
   * Input and output argument gate lists are in
   * the "most significant bit first" order,
   * the sign bit being the first one.
   *
   * \param minuend     The minuend.
   * \param subtrahend  The subtrahend.
   * \return            The list of output gates of the subtractor subcircuit.
   */
  std::list<Gate*>* add_signed_subtractor(const std::list<Gate*>* const minuend,
					  const std::list<Gate*>* const subtrahend);


  //private:
  /************************************************************************
   *
   * The public interface ends somewhere here...
   *
   ************************************************************************/

  Gate *first_gate;
  std::list<Gate*> assigned_to_true, assigned_to_false;


  //#define BC_USE_STL_MAP
#ifdef BC_USE_STL_MAP
  struct ltstr
  {
    bool operator()(const char* s1, const char* s2) const
    {
      return strcmp(s1, s2) < 0;
    }
  };
  typedef std::map<char *, NameHandle *, ltstr> GateNameMap;
#else
  struct string_hash {
    unsigned int operator()(const char *s) const {
      unsigned int h = 0;
      for( ; *s; s++)
	h = (h * 5) + (unsigned int)*s;
      return h;
    }
  };
  struct string_comp {
    int operator()(const char *s1, const char *s2) const {
      return strcmp(s1, s2);
    }
  };
  typedef HashMap<char *, string_hash, string_comp, NameHandle *> GateNameMap;
#endif
  GateNameMap named_gates;

  /* Remove all names that begin with an underscore _ */
  void remove_underscore_names();

  /* All gates should have a value */
  void print_assignment(FILE* const fp);

  void mir_compute_polarity_information();
  void mir();


  
  unsigned int count_gates();
  void compute_size(unsigned int &nof_gates, unsigned int &nof_edges);
  void compute_stats(unsigned int &max_min_height,
		     unsigned int &max_max_height);

  /**
   * Transform the circuit into a form that can be translated into CNF:
   * Remove double negations and ref-gates,
   * translate threshold gates into normal gates,
   * and explode n-ary equivs, odds and evens.
   */
  bool cnf_normalize();

  /**
   * Transform the circuit into a form that can be translated into edimacs:
   * Remove double negations and ref-gates,
   * and translate threshold gates into appropriate form.
   */
  bool edimacs_normalize();

  /** Check whether the circuit is acyclic. */
  bool test_acyclicity();

  /** Assign the temp fields of all gates to \a value. */
  void reset_temp_fields(const int value = 0);

  /**
   * Share common sub-structures in the circuit.
   * @return false if an inconsistency was found (implying that the circuit
   *               is unsatisfiable. */
  bool share();

  /**
   * Get a total ordering of the gates so that all the parents of a gate
   * precede the gate in the order.
   */
  std::vector<Gate *>* get_top_down_ordering() const;
  /**
   * Get a total ordering of the gates so that all the children of a gate
   * precede the gate in the order.
   */
  std::vector<Gate *>* get_bottom_up_ordering() const;
    
  /**
   * Perform some simplifications in the circuit.
   * @return false if an incosistency is found
   *         (implying that the circuit is unsatisfiable).
   */
  bool simplify(const SimplifyOptions& opts);


  /**
   * The destructive "cone of incluence" reduction; removes all the gates
   * that are not descendants of at least one constrained gate.
   * If there are no constrained gates, the results is an empty circuit.
   * WARNING: gates with names, including input gates, may be deleted!
   */
  void destructive_coi();

  /*
   * Returns false if the current truth assignment is not consistent
   * (implying that the circuit is not satisfiable)
   */
  bool check_consistency();

  /*
   * Flags that are used to guide simplifications
   */
  bool may_transform_input_gates; /* default: true */
  bool preserve_all_solutions; /* default: false */



private:
  void release_gate(Gate* const gate);
  void install_gate(Gate* const gate);

  /** In debug mode, check whether the temp fields of all gates are zero. */
  void debug_check_temp_fields_zero();

  /**
   * Check whether the gate \a gate1 depends on (i.e. is an ancestor of /
   * is in the fan-out cone of) the gate \a gate2.
   * Time requirement: worst-case linear in the size of the circuit.
   */
  bool depends_on(Gate * const gate1, Gate * const gate2);
};


class SimplifyOptions {
public:
  SimplifyOptions() {
    preserve_cnf_normalized_form = false;
    may_transform_input_gates = true;
    constant_folding = true;
    downward_bcp = true;
    remove_duplicate_children = true;
    remove_g_not_g_children = true;
    inline_equivalences = true;
    absorb_children = CHILDABSORB_NONE;
    misc_reductions = true;
    use_coi = true;
  }
  typedef enum {CHILDABSORB_NONE = 0, CHILDABSORB_UNSHARED, CHILDABSORB_ALL} ChildAbsorb;
  bool preserve_cnf_normalized_form;
  bool may_transform_input_gates;
  bool constant_folding;
  bool downward_bcp;
  bool remove_duplicate_children;
  bool remove_g_not_g_children;
  bool inline_equivalences;
  ChildAbsorb absorb_children;
  bool misc_reductions;
  bool use_coi;
};


#endif
