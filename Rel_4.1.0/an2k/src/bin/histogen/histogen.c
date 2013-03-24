/*******************************************************************************

License: 
This software and/or related materials was developed at the National Institute
of Standards and Technology (NIST) by employees of the Federal Government
in the course of their official duties. Pursuant to title 17 Section 105
of the United States Code, this software is not subject to copyright
protection and is in the public domain. 

This software and/or related materials have been determined to be not subject
to the EAR (see Part 734.3 of the EAR for exact details) because it is
a publicly available technology and software, and is freely distributed
to any interested party with no licensing requirements.  Therefore, it is 
permissible to distribute this software as a free download from the internet.

Disclaimer: 
This software and/or related materials was developed to promote biometric
standards and biometric technology testing for the Federal Government
in accordance with the USA PATRIOT Act and the Enhanced Border Security
and Visa Entry Reform Act. Specific hardware and software products identified
in this software were used in order to perform the software development.
In no case does such identification imply recommendation or endorsement
by the National Institute of Standards and Technology, nor does it imply that
the products and equipment identified are necessarily the best available
for the purpose.

This software and/or related materials are provided "AS-IS" without warranty
of any kind including NO WARRANTY OF PERFORMANCE, MERCHANTABILITY,
NO WARRANTY OF NON-INFRINGEMENT OF ANY 3RD PARTY INTELLECTUAL PROPERTY
or FITNESS FOR A PARTICULAR PURPOSE or for any purpose whatsoever, for the
licensed product, however used. In no event shall NIST be liable for any
damages and/or costs, including but not limited to incidental or consequential
damages of any kind, including economic damage or injury to property and lost
profits, regardless of whether NIST shall be advised, have reason to know,
or in fact shall know of the possibility.

By using this software, you agree to bear all risk relating to quality,
use and performance of the software and/or related materials.  You agree
to hold the Government harmless from any claim arising from your use
of the software.

*******************************************************************************/

/******************************************************************************

      PACKAGE: ANSI/NIST 2007 Standard Reference Implementation

      FILE:    HISTOGEN.C

      AUTHOR:  Bruce Bandini

      DATE:    05/18/2010

#cat: histogen - Parses the text file of an ANSI/NIST 2007 file that is
#cat             generated by an2k2txt and builds a histogram of Field Numbers.
#cat             For example of Field Number, see document NIST Special Publi-
#cat             cation 500-271 ANSI/NIST-ITL 1-2007, Table 8 on page 24.
#cat             The text file containing the metadata is read line by line
#cat             until EOF.  The Field Number is parsed and stored in an array.
#cat             If the Field Number does not yet exist in the array, it is
#cat             added and its count is set to 1.  For each subsequent "find"
#cat             of an existing Field Number, its count is incremented by 1.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <getopt.h>

#include "histogen.h"
#include "version.h"

extern char *dirname(const char *);
extern unsigned gl_num_invalid_ext;
extern unsigned gl_num_invalid_ansi_nist;
extern unsigned node_count;

void procargs(int, char **);
void procdir_entries();
void build_file_paths(char **);

#ifdef __MSYS__
void build_dos_file_paths(FILE * file, char *tmp_path);
#endif

void init();
void help();
void usage();

char *program;
char *filemask;
char *tmp_path;
char *p_dirpath;
char *ptemp;

struct stat st;

short gl_opt_flags[NUM_OPTIONS];
unsigned gl_num_paths;
unsigned gl_total_field_nums;
unsigned gl_num_valid;
unsigned gl_num_invalid_ansi_nist;
unsigned gl_num_invalid_ext;

FILE *fp_histo_log;

char histo_log_fname[FILESYS_PATH_LEN];
char **dynarr_path_ptrs;
int  alloc_block;
const unsigned BLOCK_SIZE = ALLOC_BLOCK_SIZE;

/******************************************************************************/
int main(int argc, char *argv[])
{
  procargs(argc, argv);
  init();
  initialize_linked_list();
  procdir_entries();
  exit(0);
}

/******************************************************************************/
/* Process the command line arguments. */
void procargs(int argc, char **argv)
{
  int c;
  long file_size;
  char *options = "ifnp";
  FILE *file;
  char cmd[CMD_LEN];
  char dir_log[FILESYS_PATH_LEN];
  char tmpdir[FILESYS_PATH_LEN];
  char *p_tmpdir;
  p_tmpdir = &tmpdir[0];

  if ((argc == 2) && (str_eq(argv[1], "-version"))) {
    getVersion();
    exit(0);
  }

  if ((argc == 2) && (str_eq(argv[1], "-help"))) {
    program = strrchr(*argv,'/');
    if (program == (char *) NULL)
      program = *argv;
    else
      program++;

    help();
    usage();
  }

  if (argc < 2)
  {
    program = strrchr(*argv,'/');
    if (program == (char *) NULL)
      program = *argv;
    else
      program++;

    usage();
  }

  while ((c = getopt (argc, argv, options)) != -1) {
    switch (c)
      {
      case 'i':
	gl_opt_flags[0] = INCLUDE_INVALID_FILES;
	break;

      case 'f':
	gl_opt_flags[1] = INCLUDE_FIELD_SEPARATORS;
	break;

      case 'n':
	gl_opt_flags[2] = INCLUDE_NEWLINE_CHARS;
	break;

      case 'p':
	gl_opt_flags[3] = INCLUDE_SPACE_CHARS;
	break;

      case '?':
	if (isprint (optopt))
	  fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	else
	  fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);

	exit(0);

      default:
	abort();
      }
  }

  program = strrchr(*argv,'/');
  if (program == (char *) NULL)
    program = *argv;

  /* process if first non-option is null */
  tmp_path = argv[optind++];
  if(tmp_path == NULL) {
    printf("No <TEMP DIR PATH>\n");
    usage();
  }

  /* check for existence of temp dir */
  if(stat(tmp_path, &st) != 0) {
    printf("<TEMP DIR PATH> does not exist: '%s'\n", tmp_path);
    usage();
  }

  /* process if second non-option is null */
  filemask = argv[optind];
  if(filemask == NULL) {
    printf("No <FILES DIR PATH/filemask>\n");
    usage();
  }

  strcpy(p_tmpdir, filemask);
  p_dirpath = dirname(p_tmpdir);

  if(stat(p_dirpath, &st) != 0) {
    printf("<FILES DIR PATH> does not exist: '%s'\n", p_dirpath);
    usage();
  }

  /* get the paths, store dynamically */
  gl_num_paths = 0;
  alloc_block = BLOCK_SIZE;

  dynarr_path_ptrs = malloc(alloc_block * sizeof(char *));

  if (dynarr_path_ptrs == NULL) {
    fprintf(stderr, "ERROR: path pointers malloc failed\n");
    exit(0);
  }

  /* search for mask chars in <FILES DIR PATH>; if exist, then run
     DIR cmd (windows) to get list of files to process */
#ifdef __MSYS__
  /* check for wildcard char in string */
  if((strrchr(filemask, '*')==NULL) &&
     (strrchr(filemask, '?')==NULL)) {

    /* no wildcard found */
    build_file_paths(argv);
  }
  else {
    /* wildcard found */
    strcpy(dir_log, tmp_path);
    strcat(dir_log, "/dir.log");

    /* try dir */
    sprintf(cmd, "dir /B \"%s\" > \"%s\"", filemask, dir_log);

/*     printf("%s\n", cmd); */

    system(cmd);

    /* check dir.log file */
    file = fopen(dir_log, "r");
    file_size = 0;
    file_size = filelength(fileno(file));
    if (file_size > 0) {
      build_dos_file_paths(file, p_dirpath);
      fclose(file);
    }
    else {
      printf("<FILES DIR PATH/filemask> is not valid!\n");
      fclose(file);
      remove(dir_log);
      usage();
    }
    remove(dir_log);
  }
#else
  build_file_paths(argv);
#endif

  chdir(tmp_path);
  strcat(tmp_path, "/");
}

/******************************************************************************/
/* Process each PATH to ANSI/NIST file. */
void procdir_entries()
{
  int i;

  /* process each file in the paths array */
  fprintf(fp_histo_log, "ANSI/NIST files in set:\n");
  for(i=0; i<gl_num_paths; i++) {
    process_file(dynarr_path_ptrs[i]);
    free (dynarr_path_ptrs[i]);
  }

  /* when complete, write the histogram to the output log */
  fprintf(fp_histo_log, "\n\n");
  output_linked_list(fp_histo_log);

  fprintf(fp_histo_log, "\nTOTAL field nums: '%d'\n\n", node_count);
  fprintf(fp_histo_log,   "TOTAL valid an2/sub files  :  '%d'\n", gl_num_valid);

  if(gl_opt_flags[0] == INCLUDE_INVALID_FILES) {
    fprintf(fp_histo_log, "TOTAL invalid an2/sub files:  '%d'\n", gl_num_invalid_ansi_nist);
    fprintf(fp_histo_log, "TOTAL invalid extensions   :  '%d'\n", gl_num_invalid_ext);
  }

  fclose(fp_histo_log);
  free (dynarr_path_ptrs);
}

/******************************************************************************/
void build_file_paths(char **argv)
{
  int slen;
  FILE *file;

  while(filemask != NULL) {
    /* throw out any DIR included in filemask=* */
    stat(filemask, &st);
    if (S_ISDIR(st.st_mode)) {
      filemask = argv[++optind];
      continue;
    }

    slen = strlen(filemask);
    ptemp = (char *)malloc(slen+1);

    if (ptemp == NULL) {
      fprintf(stderr, "ERROR: filemask malloc failed\n");
      exit(0);
    }

    strncpy(ptemp, filemask, slen+1);

    if (gl_num_paths == alloc_block) {
      alloc_block += BLOCK_SIZE;
      dynarr_path_ptrs = (char **)realloc(dynarr_path_ptrs, alloc_block * sizeof(char *));
      if (dynarr_path_ptrs == NULL) {
        fprintf(stderr, "ERROR: path pointers realloc failed\n");
        exit(0);
      }
    }

    dynarr_path_ptrs[gl_num_paths] = ptemp;
    gl_num_paths++;

    filemask = argv[++optind];
  }

  /* verify that the <FILES DIR PATH> is correct by checking for the existence
     of the first file in the filemask set */
  file = NULL;
  file = fopen(dynarr_path_ptrs[0], "r");
  if (file != NULL) {
    fclose(file);
  }
  else {
    printf("<FILES DIR PATH/filemask> is not valid!\n");
    usage();
  }
}

#ifdef __MSYS__

/******************************************************************************/
void build_dos_file_paths(FILE * file, char *p_dirpath)
{
  int  slen;
  char line[FILESYS_PATH_LEN];
  char fname[FILESYS_PATH_LEN];
  char *ptemp;

  strcpy(fname, p_dirpath);
  strcat(fname, "/");

  /* get the paths, store dynamically */
  gl_num_paths = 0;
  alloc_block = BLOCK_SIZE;
  dynarr_path_ptrs = malloc(alloc_block * sizeof(char *));
  if (dynarr_path_ptrs == NULL) {
    fprintf(stderr, "ERROR: path pointers malloc failed\n");
    exit(0);
  }

  /* read file line by line */
  while (fgets (line, sizeof line, file) != NULL) {
    /* concat the filename from the dir.log to path */
    strcat(fname, line);

    /* strip trailing '\n' if it exists */
    slen = strlen(fname)-1;
    if(fname[slen] == '\n')
      fname[slen] = '\0';

    /* throw out any DIR included from filemask=* */
    stat(fname, &st);

    if (S_ISDIR(st.st_mode)) {
      strcpy(fname, p_dirpath);
      strcat(fname, "/");
      continue;
    }

    slen = strlen(fname);
    ptemp = (char *)malloc(slen+1);

    if (ptemp == NULL) {
      fprintf(stderr, "ERROR: filemask malloc failed\n");
      exit(0);
    }

    strncpy(ptemp, fname, slen+1);

    if (gl_num_paths == alloc_block) {
      alloc_block += BLOCK_SIZE;
      dynarr_path_ptrs = (char **)realloc(dynarr_path_ptrs, alloc_block * sizeof(char *));
      if (dynarr_path_ptrs == NULL) {
        fprintf(stderr, "ERROR: path pointers realloc failed\n");
        exit(0);
      }
    }

    strcpy(fname, p_dirpath);
    strcat(fname, "/");

    dynarr_path_ptrs[gl_num_paths] = ptemp;
    gl_num_paths++;
  }
}
#endif

/******************************************************************************/
void init()
{
  strcpy(histo_log_fname, p_dirpath);
  strcat(histo_log_fname, "/");
  strcat(histo_log_fname, HISTOGEN_LOG_FNAME);

  /* Open the histogram output file */
  if(!(fp_histo_log=fopen(histo_log_fname,"w"))!=0) {
    fprintf(stderr, "ERROR: cannot open file '%s'\n", histo_log_fname);
    exit(0);
  }

  gl_num_invalid_ansi_nist = 0;
  gl_num_invalid_ext = 0;
  gl_num_valid = 0;
}

/******************************************************************************
Print help message.
*******************************************************************************/
void help()
{
  static char help_msg[] = "\
This application processes one or more ANSI/NIST 2007 files and builds\n\
a histogram of the number of times each field and sub-field appears\n\
within that set of ANSI/NIST files.\n\
For examples of the various field types, see document NIST Special\n\
Publication 500-271 ANSI/NIST-ITL 1-2007.\n\
This application depends on the an2k2txt executable as a prerequisite.\n\
Please ensure that an2k2txt is in your environment PATH.\n";

  (void) fprintf(stderr, help_msg, program);
}

/******************************************************************************
Print usage message and exit.
*******************************************************************************/
void usage()
{
  static char usage_msg[] = "\
  Usage:\n\
  %s [options] <TEMP DIR PATH> <FILES DIR PATH/filemask>\n\
     -i     include invalid files in output log\n\
     -f     include Field Numbers followed by field separator\n\
     -n     include Field Numbers followed by newline char\n\
     -p     include Field Numbers followed by space char\n\n\
   The generated histogram text file, histogen.log, is saved into the\n\
   <FILES DIR PATH> dir.  <FILES DIR PATH> must be absolute.\n\n\
   <TEMP DIR PATH> is a directory where each ANSI/NIST file is copied\n\
   to be processed.  All files are deleted after processing such that\n\
   this dir is empty upon completion.  <TEMP DIR PATH> must be absolute.\n\n\
   EX: histogen -ifnp /d/tmp1 /d/fp_files/*.sub\n\
   EX: histogen -ifnp /d/tmp1 /d/fp_files/file_??.an2\n\
   EX: histogen -i c:\\tmp1 \"c:\\fp_files\\disc 1\"\\*\n";

  (void) fprintf(stderr, usage_msg, program);
  exit(1);
}
