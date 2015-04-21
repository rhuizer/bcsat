/*
 Copyright (C) 2005-2009 Tommi Junttila
 
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
#include "defs.hh"
#include "bc.hh"

const char *default_program_name = "bc2edimacs";

const char *infilename = 0;
FILE *infile = stdin;

const char *outfilename = 0;
FILE *outfile = stdout;

static bool opt_notless = true;
static bool opt_perform_simplifications = true;
static bool opt_preserve_all_solutions = false;

static void
usage(FILE* const fp, const char* argv0)
{
  const char *program_name;
  
  program_name = rindex(argv0, '/');
  
  if(program_name) program_name++;
  else program_name = argv0;
  
  if(!*program_name) program_name = default_program_name;
  fprintf(fp, "bc2edimacs, %s\n", BCPACKAGE_VERSION);
  fprintf(fp, "Copyright 2005-2009 Tommi Junttila\n");
  fprintf(fp,
"%s <options> [<circuit file>] [<edimacs file>]\n"
"\n"
"  -all            preserve all solutions (default: preserve satisfiability)\n"
"  -nosimplify     do not perform simplifications\n"
"  -nots           perform an unoptimized CNF-translation with NOT-gates\n"
"  -v              switch verbose mode on\n"
"  <circuit file>  input circuit file (if not specified stdin is used)\n"
"  <edimacs file>  output edimacsfile (if not specified stdout is used)\n"
          ,program_name);
}


static void
parse_options(const int argc, const char** argv)
{
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-verbose") == 0)
      verbose = true;
    else if(strcmp(argv[i], "-all") == 0)
      opt_preserve_all_solutions = true;
    else if(strcmp(argv[i], "-nosimplify") == 0)
      opt_perform_simplifications = false;
    else if(strcmp(argv[i], "-nots") == 0)
      opt_notless = false;
    else if(argv[i][0] == '-') {
      fprintf(stderr, "unknown command line argument `%s'\n", argv[i]);
      usage(stderr, argv[0]);
      exit(1);
    }
    else {
      if(infile != stdin) {
	if(outfile != stdout) {
	  fprintf(stderr, "too many file arguments\n");
	  usage(stderr, argv[0]);
	  exit(1);
	}
	outfilename = argv[i];
	outfile = fopen(argv[i], "w");
	if(!outfile) {
	  fprintf(stderr, "cannot open `%s' for output\n", argv[i]);
	  exit(1); }
      }
      else {
	infilename = argv[i];
	infile = fopen(argv[i], "r");
	if(!infile) {
	  fprintf(stderr, "cannot open `%s' for input\n", argv[i]);
	  exit(1); }
      }
    }
  }
}

int
main(const int argc, const char** argv)
{
  BC* circuit = 0;

  verbstr = stdout;

  parse_options(argc, argv);
  
  if(verbose) {
    fprintf(verbstr, "parsing from %s\n", infilename?infilename:"stdin");
    fflush(verbstr); }
  
  circuit = BC::parse_circuit(infile);
  if(circuit == 0)
    exit(1);
    
  if(infilename) fclose(infile);

  if(verbose) {
    fprintf(verbstr, "The circuit has %d gates\n", circuit->count_gates());
    fflush(verbstr); }
  

  if(verbose)
    {
      /*
       * Print input gates
       */
      fprintf(verbstr, "The input gates are:");
      circuit->print_input_gate_names(verbstr, " ");
      fprintf(verbstr, "\n");
    }
  
  /*
   * Mark values of assigned gates
   */
  while(!circuit->assigned_to_true.empty())
    {
      Gate* const gate = circuit->assigned_to_true.front();
      circuit->assigned_to_true.pop_front();
      if(!circuit->force_true(gate))
	goto unsat_exit;
    }
  while(!circuit->assigned_to_false.empty())
    {
      Gate* const gate = circuit->assigned_to_false.front();
      circuit->assigned_to_false.pop_front();
      if(!circuit->force_false(gate))
	goto unsat_exit;
    }


  /*
   * Remove all gate names beginning with the underscore character _
   */
  circuit->remove_underscore_names();

  /*
   * Set flags for simplifications
   */
  circuit->preserve_all_solutions = opt_preserve_all_solutions;

  /*
   * Simplify or at least share structure
   */
  if(opt_perform_simplifications)
    {
      SimplifyOptions opts;
      if(!circuit->simplify(opts))
	goto unsat_exit;
    }
  else
    {
      if(!circuit->share())
	goto unsat_exit;
    }




  /*
   * Print some header information
   */
  fprintf(outfile, "\
c This is a SAT formula in the EDIMACS format,\n\
c produced with the bc2edimacs translator by Tommi Junttila;\n\
c see http://users.ics.aalto.fi/tjunttil/circuits/index.html\n\
");

  /*
   * Translate to cnf
   */
  circuit->to_edimacs(outfile, opt_notless, opt_perform_simplifications);
  
  return 0;

 unsat_exit:
  /*
   * Print a small unsatisfiable formula
   */
  fprintf(outfile, "\
c This is a SAT formula in the EDIMACS format,\n\
c produced with the bc2edimacs translator by Tommi Junttila;\n\
c see http://users.ics.aalto.fi/tjunttil/circuits/index.html\n\
c The instance was determined unsatisfiable during preprocessing.\n\
");
  fprintf(outfile, "p noncnf 1\n");
  fprintf(outfile, "1 -1 1 0\n");
  return 0;
}


