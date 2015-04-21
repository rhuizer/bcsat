/*
 Copyright (C) 2004-2008 Tommi A. Junttila
 
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

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <list>
#include "defs.hh"
#include "bc.hh"

#if !defined(BC_HAS_ZCHAFF)
/*
 *
 * No ZChaff, a call will result in a terminating internal error.
 *
 */
int BC::zchaff_solve(const bool perform_simplifications,
		     const bool polarity_cnf,
		     const bool notless,
		     const bool input_cuts_only,
		     const bool permute_cnf,
		     const unsigned int permute_cnf_seed)
{
  internal_error("no ZChaff included");
  exit(1);
}



#else
/*
 *
 * ZChaff is included
 *
 */
/* ZChaff include files */
#include "SAT.h"

static void
print_zchaff_statistics(SAT_Manager mng, FILE* const fp)
{
  fprintf(fp, "Max Decision Level\t\t\t%d\n", SAT_MaxDLevel(mng));
  fprintf(fp, "Num. of Decisions\t\t\t%d\n", SAT_NumDecisions(mng));
  fprintf(fp, "Original Num Clauses\t\t\t%d\n", SAT_InitNumClauses(mng));
  fprintf(fp, "Original Num Literals\t\t\t%d\n", SAT_InitNumLiterals(mng));
  fprintf(fp, "Added Conflict Clauses\t\t\t%d\n",
          SAT_NumAddedClauses(mng)- SAT_InitNumClauses(mng));
  fprintf(fp, "Added Conflict Literals\t\t\t%lld\n",
          SAT_NumAddedLiterals(mng) - SAT_InitNumLiterals(mng));
  fprintf(fp, "Deleted Unrelevant clause\t\t%d\n", SAT_NumDeletedClauses(mng));
  fprintf(fp, "Deleted Unrelevant literals\t\t%lld\n",
          SAT_NumDeletedLiterals(mng));
  fprintf(fp, "Number of Implication\t\t\t%lld\n", SAT_NumImplications(mng));
  //other statistics comes here
  //fprintf(fp, "Total Run Time\t\t\t\t%d\n\n", SAT_GetCPUTime(mng));
}



int
BC::zchaff_solve(const bool perform_simplifications,
		 const bool polarity_cnf,
		 const bool notless,
		 const bool input_cuts_only,
		 const bool permute_cnf,
		 const unsigned int permute_cnf_seed)
{
  int result;
  int max_var_num;
  unsigned int max_clause_length;
  unsigned int nof_clauses = 0;
  SAT_Manager mng = 0;

  SimplifyOptions simplify_opts;

  if(perform_simplifications)
    {
      if(!simplify(simplify_opts))
	return 0;
    }
  else
    {
      if(!share())
	return 0;
    }
  
  
  if(!cnf_normalize())
    return 0;
  
  if(perform_simplifications)
    {
      simplify_opts.preserve_cnf_normalized_form = true;
      if(!simplify(simplify_opts))
	return 0;
    } 
  else
    {
      if(!share())
	return 0;
    }
    
  
  /*
   * Print some statistics
   */
  if(verbose)
    {
      unsigned int max_min_height, max_max_height;
      compute_stats(max_min_height, max_max_height);
      fprintf(verbstr, "The max-min height of the circuit is %u\n",
              max_min_height);
      fprintf(verbstr, "The max-max height of the circuit is %u\n",
              max_max_height);
      fflush(verbstr);
    }


  /*
   * Find the relevant gates and number them in temp field
   */
  reset_temp_fields(-1);
  int nof_relevant_gates = 0;
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      assert(gate->type != Gate::tTRUE ||
	     (gate->determined && gate->value == true));
      assert(gate->type != Gate::tFALSE ||
	     (gate->determined && gate->value == false));
      if(gate->determined and !gate->is_justified())
	gate->mark_coi(nof_relevant_gates);
    }
  if(verbose) {
    fprintf(verbstr, "The circuit has %d relevant gates\n",
	    nof_relevant_gates);
    fflush(verbstr);
  }
  if(nof_relevant_gates == 0)
    {
      goto sat_exit;
    }
  
  
  /*
   * Renumber the gates in temp-fields and compute
   * (i) maximum clause length and
   * (ii) the number of relevant input gates
   */
  {
    max_clause_length = 2;
    int gate_num = 1;
    int nof_relevant_input_gates = 0;
    for(Gate *gate = first_gate; gate; gate = gate->next)
      {
        if(gate->temp == -1) {
          /* Not relevant */
          continue;
        }
        if(notless && gate->type == Gate::tNOT) {
          /* NOT-less translation */
          assert(!gate->determined);
          assert(gate->children->child->type != Gate::tNOT);
          gate->temp = -1;
        }
        else {
          gate->temp = gate_num++;
        }

        /* Compute max clause length */
        if(gate->count_children() + 1 > max_clause_length)
          max_clause_length = gate->count_children() + 1;

        if(gate->type == Gate::tVAR)
          nof_relevant_input_gates++;
      }
    max_var_num = gate_num;

    if(verbose) {
      fprintf(verbstr, "The circuit has %d relevant input gates\n",
              nof_relevant_input_gates);
      fflush(verbstr);
    }
  }


  /*
   * Permute the CNF variable indices if required
   */
  if(permute_cnf)
    {
      if(verbose)
	{
	  fprintf(verbstr, "Permuting the CNF variables\n"); fflush(verbstr);
	}
      unsigned int * const perm = my_perm(max_var_num-1, permute_cnf_seed);
      for(Gate* gate = first_gate; gate; gate = gate->next)
	{
	  assert(gate->temp <= max_var_num-1);
	  if(gate->temp <= 0)
	    continue;
	  gate->temp = perm[gate->temp];
	  assert(gate->temp > 0 && gate->temp <= max_var_num-1);
	}
      free(perm);
      if(verbose)
	{
	  fprintf(verbstr, "done\n"); fflush(verbstr);
	}
    }


  /*
   * Init ZChaff
   */
  mng = SAT_InitManager();
  SAT_SetNumVariables(mng, max_var_num);
  
  if(polarity_cnf)
    mir_compute_polarity_information();

  /*
   * Build the CNF
   */
  {
    int* clause = (int *)malloc((max_clause_length + 1) * sizeof(int));
    std::list<std::vector<int> *> clauses;
    int* duplicate_array = (int*)calloc(max_var_num, sizeof(int));
    for(Gate* gate = first_gate; gate; gate = gate->next)
      {
        if(gate->temp == -1) {
          /* Not relevant */
          continue;
        }
        /*
         * Get clauses
         */
	if(polarity_cnf)
	  gate->cnf_get_clauses_polarity(clauses, notless);
	else
	  gate->cnf_get_clauses(clauses, notless);

        /*
         * Add clauses to ZChaff
         */
        while(!clauses.empty()) {
          std::vector<int> *cl = clauses.back();
          clauses.pop_back();
          assert(cl->size() <= max_clause_length);
          /* Transform clause into ZChaff form */
          int i = 0;
          for(std::vector<int>::iterator li = cl->begin(); li != cl->end(); li++) {
            int lit = *li;
            assert(lit != 0 && abs(lit) <= max_var_num);
            if(lit > 0)
              clause[i] = lit * 2;
            else
              clause[i] = (lit * -2) + 1;
            i++;
          }
          clause[i] = 0;
	  /* ZChaff allows no multiple occurrences of variables in a clause */
          int r = 0, w = 0;
          while(clause[r] > 0) {
            const int var = clause[r] >> 1;
            const bool sign = ((clause[r] & 0x01) == 0);
            if(duplicate_array[var] != 0) {
              const bool already_occurred_sign = (duplicate_array[var] == 2);
              if(already_occurred_sign == sign) {
                /* A duplicate literal */
                r++;
                continue;
              }
              /* Clause is tautological */
              clause[0] = 0;
              break;
            }
            /* The variable has not occurred before in the clause */
            duplicate_array[var] = sign?2:1;
            clause[w++] = clause[r++];
          }
          clause[w] = 0;
          /* Reset duplicate_array */
          for(std::vector<int>::iterator li=cl->begin(); li!=cl->end(); li++) {
            int var = abs(*li);
            duplicate_array[var] = 0;
          }
          /* Add clause to ZChaff */
          if(clause[0] != 0) {
            SAT_AddClause(mng, clause, w);
            nof_clauses++;
          }
          delete cl;
        }
	/*
         * Add unit clauses for constrained gates
         */
        if(gate->determined)
	  {
	    clause[0] = gate->value?gate->temp * 2:gate->temp * 2 + 1;
	    clause[1] = 0;
	    SAT_AddClause(mng, clause, 1);
	    nof_clauses++;
	  }
	else
	  {
	    /* The following cases should really not happen... */
	    if(gate->type == Gate::tTRUE)
	      {
		clause[0] = gate->temp * 2;
		clause[1] = 0;
		SAT_AddClause(mng, clause, 1);
		nof_clauses++;
	      }
	    else if(gate->type == Gate::tFALSE)
	      {
		clause[0] = gate->temp * 2 + 1;
		clause[1] = 0;
		SAT_AddClause(mng, clause, 1);
		nof_clauses++;
	      }
	  }
      }
    free(clause);
    free(duplicate_array);
  }

 if(verbose)
   {
     fprintf(verbstr, "The cnf has %d variables and %d clauses\n",
	     max_var_num, nof_clauses);
     fflush(verbstr);
   }

  
  /*
   * Mark branchable variables
   */
  if(input_cuts_only)
    {
      for(Gate* gate = first_gate; gate; gate = gate->next)
        {
	  DEBUG_ASSERT(gate->temp == -1 || gate->temp > 0);
	  /* Not relevant? */
	  if(gate->temp == -1)
	    continue;
	  /* An input or constant gate? */
          if(gate->type == Gate::tVAR ||
	     gate->type == Gate::tFALSE ||
	     gate->type == Gate::tTRUE)
	    {
	      SAT_EnableVarBranch(mng, gate->temp);
	    }
	  else
	    {
	      /* Disable branching on this gate */
	      SAT_DisableVarBranch(mng, gate->temp);
	    }
	}
    }


  /*
   * Execute zchaff
   */
  if(verbose)
    {
      fprintf(verbstr, "Executing zchaff...\n");
      fflush(verbstr);
    }
  result = SAT_Solve(mng);
  
  if(verbose) {
    print_zchaff_statistics(mng, verbstr);
    fflush(verbstr);
  }

  
  if(result == UNSATISFIABLE)
    {
      SAT_ReleaseManager(mng);
      return 0;
    }
  if(result == UNDETERMINED)
    {
      SAT_ReleaseManager(mng);
      return 2;
    }
  if(result == TIME_OUT)
    {
      SAT_ReleaseManager(mng);
      return 3;
    }
  if(result == MEM_OUT)
    {
      SAT_ReleaseManager(mng);
      return 4;
    }
  if(result == ABORTED)
    {
      SAT_ReleaseManager(mng);
      return 5;
    }
  assert(result == SATISFIABLE);

 sat_exit:
  if(mng) {
    /*
     * Move ZChaff truth assignment on input vars to the circuit
     */
    for(Gate* gate = first_gate; gate; gate = gate->next)
      {
	if(gate->temp <= 0)
	  continue;
	if(gate->type != Gate::tVAR)
	  continue;
	int val = SAT_GetVarAsgnment(mng, gate->temp);
	assert(val == 0 || val == 1);
	bool zchaff_value = (val == 1);
	if(gate->determined)
	  {
	    if(gate->value != zchaff_value)
	      {
		fprintf(stderr, "Solution is inconsistent\n");
		exit(-1);
	      }
	  }
	else
	  {
	    gate->determined = true;
	    gate->value = zchaff_value;
	  }
      }
    SAT_ReleaseManager(mng); mng = 0;
  }

  /*
   * Assign irrelevant input gates to arbitrary values
   */
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      if(gate->type == Gate::tVAR && !gate->determined)
	{
	  assert(gate->temp == -1);
	  gate->determined = true;
	  gate->value = false;
	}
    }
  
  /*
   * Evaluate rest of the irrelevant gates
   */
  for(Gate *gate = first_gate; gate; gate = gate->next)
    {
      if(!gate->determined)
	{
	  bool evaluation_ok = gate->evaluate();
	  if(!evaluation_ok)
	    internal_error("%s:%u: Evaluation error",__FILE__,__LINE__);
	  DEBUG_ASSERT(gate->determined);
	}
    }

  /*
   * Check consistency
   */
  if(!check_consistency())
    {
      internal_error("%s:%u: Consistency check failed",__FILE__,__LINE__);
      exit(-1);
    }

  return 1;
}


#endif //BC_HAS_ZCHAFF
