/*
 Copyright (C) 2005 Tommi A. Junttila
 
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
#include <cctype>
#include <vector>
#include "defs.hh"

using namespace std;

const char *default_program_name = "edimacs2bc";

//const char *VERSION = "version 0.1 (2005-03-09)";

const char *infilename = 0;
FILE *infile = stdin;

const char *outfilename = 0;
FILE *outfile = stdout;

static void usage(FILE * const fp, const char *argv0)
{
  const char *program_name;
  
  program_name = rindex(argv0, '/');
  
  if(program_name) program_name++;
  else program_name = argv0;
  
  if(!*program_name) program_name = default_program_name;
  fprintf(fp, "edimacs2bc, %s\n", BCPACKAGE_VERSION);
  fprintf(fp, "Copyright 2005 Tommi Junttila\n");
  fprintf(fp,
"%s <options> [<circuit file>] [<edimacs file>]\n"
"\n"
"  -v              switch verbose mode on\n"
"  <circuit file>  input edimacs file (if not specified stdin is used)\n"
"  <edimacs file>  output circuit file (if not specified stdout is used)\n"
          ,program_name);
}


static void parse_options(const int argc, const char ** argv)
{
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-verbose") == 0)
      verbose = true;
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


static int line_num = 1;

static int read_int(FILE * const fp)
{
  int n = 0;
  int sign = 1;
  int c;
  c = getc(fp);
  while(isspace(c) && c != EOF)
    {
      if(c == '\n') line_num++;
      c = getc(fp);
    }
  if(c == '-')
    {
      sign = -1;
      c = getc(fp);
    }
  if(!isdigit(c))
    {
      fprintf(stderr, "Error at line %u\n", line_num);
      exit(1);
    }
  while(isdigit(c))
    {
      n = n * 10;
      n = n + (c - '0');
      c = getc(fp);
    }
  ungetc(c, fp);
  return n * sign;
}



int main(const int argc, const char **argv)
{
  unsigned int nof_vars = 0;
  vector<int> parameters;
  vector<int> IOs;

  verbstr = stdout;

  parse_options(argc, argv);
  
  if(verbose) {
    fprintf(verbstr, "parsing from %s\n", infilename?infilename:"stdin");
    fflush(verbstr); }

  while(true)
    {
      int c = getc(infile);

      if(c == 'c')
	{
	  /* Comment line */
	  while(c != '\n' && c != EOF)
	    c = getc(infile);
	  if(c == '\n') line_num++;
	  continue;
	}
      if(c == 'p')
	{
	  /* Problem definition line */
	  if(nof_vars > 0)
	    {
	      fprintf(stderr, "Error at line %u\n", line_num);
	      exit(1);
	    }
	  if(fscanf(infile, " noncnf %u\n", &nof_vars) != 1)
	    {
	      fprintf(stderr, "Error at line %u\n", line_num);
	      exit(1);
	    }
	  if(nof_vars == 0)
	    {
	      fprintf(stderr, "Error at line %u\n", line_num);
	      exit(1);
	    }
	  break;
	}
      fprintf(stderr, "Error at line %u\n", line_num);
      exit(1);
    }
  
  fprintf(outfile, "BC1.0\n");

  /*
   * Read gate definitions
   */
  while(true)
    {
      /* Read leading spaces and detect EOF */
      int c = getc(infile);
      while(isspace(c) && c != EOF)
	{
	  if(c == '\n') line_num++;
	  c = getc(infile);
	}
      if(c == EOF)
	break;
      ungetc(c, infile);

      const int type = read_int(infile);
      /*fprintf(stderr, "type = %d\n", type);*/
      const int nof_parameters = read_int(infile);
      /*fprintf(stderr, "nof parameters = %d\n", nof_parameters);*/

      parameters.clear();
      for(int p = 0; p < nof_parameters; p++)
	parameters.push_back(read_int(infile));

      IOs.clear();
      int IO = read_int(infile);
      while(IO != 0)
	{
	  IOs.push_back(IO);
	  IO = read_int(infile);
	}

      switch(type)
	{
	case 1:
	  {
	    /* FALSE */
	    assert(nof_parameters == -1);
	    assert(IOs.size() == 1);
	    fprintf(outfile, "g%d := %sFALSE\n",abs(IOs[0]),(IOs[0]>0)?"":"~");
	    break;
	  }
	case 2:
	  {
	    /* TRUE */
	    assert(nof_parameters == -1);
	    assert(IOs.size() == 1);
	    fprintf(outfile, "g%d := %sTRUE\n",abs(IOs[0]),(IOs[0]>0)?"":"~");
	    break;
	  }
	case 3:
	  {
	    /* NOT */
	    assert(nof_parameters == -1);
	    assert(IOs.size() == 2);
	    fprintf(outfile, "g%d := %sNOT(", abs(IOs[0]), (IOs[0]>0)?"":"~");
	    fprintf(outfile, "%sg%d", (IOs[1]>0)?"":"~", abs(IOs[1]));
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 4:
	  {
	    /* AND */
	    assert(nof_parameters == -1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %sAND(", abs(IOs[0]), (IOs[0]>0)?"":"~");
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 5:
	  {
	    /* NAND */
	    assert(nof_parameters == -1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %s~AND(", abs(IOs[0]), (IOs[0]>0)?"":"~");
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 6:
	  {
	    /* OR */
	    assert(nof_parameters == -1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %sOR(", abs(IOs[0]), (IOs[0]>0)?"":"~");
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 7:
	  {
	    /* NOR */
	    assert(nof_parameters == -1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %s~OR(", abs(IOs[0]), (IOs[0]>0)?"":"~");
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 8:
	  {
	    /* XOR */
	    assert(nof_parameters == -1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %sODD(", abs(IOs[0]), (IOs[0]>0)?"":"~");
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 9:
	  {
	    /* XNOR */
	    assert(nof_parameters == -1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %sEVEN(", abs(IOs[0]), (IOs[0]>0)?"":"~");
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 10:
	  {
	    /* IMPLIES */
	    assert(nof_parameters == -1);
	    assert(IOs.size() == 3);
	    fprintf(outfile, "g%d := %sIMPLY(",abs(IOs[0]),(IOs[0]>0)?"":"~");
	    fprintf(outfile, "%sg%d", (IOs[1]>0)?"":"~", abs(IOs[1]));
	    fprintf(outfile, ",");
	    fprintf(outfile, "%sg%d", (IOs[2]>0)?"":"~", abs(IOs[2]));
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 11:
	  {
	    /* IFF */
	    assert(nof_parameters == -1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %sEQUIV(",abs(IOs[0]),(IOs[0]>0)?"":"~");
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 12:
	  {
	    /* ITE */
	    assert(nof_parameters == -1);
	    assert(IOs.size() == 4);
	    fprintf(outfile, "g%d := %sITE(",abs(IOs[0]),(IOs[0]>0)?"":"~");
	    fprintf(outfile, "%sg%d", (IOs[1]>0)?"":"~", abs(IOs[1]));
	    fprintf(outfile, ",");
	    fprintf(outfile, "%sg%d", (IOs[2]>0)?"":"~", abs(IOs[2]));
	    fprintf(outfile, ",");
	    fprintf(outfile, "%sg%d", (IOs[3]>0)?"":"~", abs(IOs[3]));
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 13:
	  {
	    /* ATLEAST */
	    assert(nof_parameters == 1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %s[%d,%lu](",
		    abs(IOs[0]), (IOs[0]>0)?"":"~",
		    parameters[0], (long unsigned int)IOs.size()-1);
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 14:
	  {
	    /* ATMOST */
	    assert(nof_parameters == 1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %s[0,%d](",
		    abs(IOs[0]), (IOs[0]>0)?"":"~",
		    parameters[0]);
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	case 15:
	  {
	    /* COUNT */
	    assert(nof_parameters == 1);
	    assert(IOs.size() >= 1);
	    fprintf(outfile, "g%d := %s[%d,%d](",
		    abs(IOs[0]), (IOs[0]>0)?"":"~",
		    parameters[0], parameters[0]);
	    const char *sep = "";
	    for(unsigned int i = 1; i < IOs.size(); i++)
	      {
		fprintf(outfile, "%s", sep); sep = ",";
		fprintf(outfile, "%sg%d", (IOs[i]>0)?"":"~", abs(IOs[i]));
	    }
	    fprintf(outfile, ");\n");
	    break;
	  }
	default:
	  fprintf(stderr, "Error at line %u: unknown gate type %d\n",
		  line_num, type);
	  exit(1);
	}
    }


  fprintf(outfile, "ASSIGN g%u;\n", nof_vars);

  if(infilename) fclose(infile);

  return 0;
}


