/*  
   Copyright (C)  2008 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
*/

#include "main.h"
#include <regex.h>   /* use POSIX in order to be portable to linux */

extern int verbose;
int backup = FALSE;
char *pattern_baseDir = NULL;

/* --------- for syncing all files with/without backup. */
int nPattern=0;   /* number of regular expression for backup files */
regex_t ** fPatterns;/* array of pointers to regular expression */

int set_nPattern(char * fpat_file) 
{
  FILE *fd;
  char pat[PATH_MAX]; 
  int count = 0;
  if ((fd = fopen(fpat_file, "r"))==NULL) {
    fprintf(stderr, "Cannot open file -- %s\n", fpat_file);
    return FAIL;
  }
  while (!feof(fd)) {
    if (fscanf(fd, "%s", pat) == 1) {
      ++count;
    }
  }
  nPattern = count;
  fclose(fd);
  return SUCCESS;
}
 
int read_backup_pattern(char * fpat_file)
{
  FILE *fd;
  char pat[PATH_MAX]; 
  int i = 0;

  if (!set_nPattern(fpat_file)) return FAIL;

  fPatterns = malloc(sizeof(void *)*nPattern);
  for(i = 0; i< nPattern; ++i) {
    fPatterns[i] = (regex_t *) malloc(sizeof(regex_t));
  }

  fd = fopen(fpat_file, "r");

  /* 
     if we don't prepend ^,
     then pattern 'file' intended for files under srcBase
     will select files with pattern = subdir/file.* 
     which is not our intention.
  */
  i = 0;
  while (!feof(fd)) {
    if (fscanf(fd, "%s", pat) == 1) {
      char fullpat[PATH_MAX];
      sprintf(fullpat, "^%s", pat);

      regcomp(fPatterns[i], fullpat, REG_EXTENDED|REG_NOSUB);
      ++i;
    }
  };
  fclose(fd);
  return SUCCESS;
}

int needBackup(char * filename) /* fullpath */
{
  /* we reach this point when the backup flag is true
        if no_pattern -> backup all of them
        if pattern 
           if match   -> backup
              nomatch -> no-backup
  */
  int i;
  char *p;
  if (!backup) return FALSE;
  if (nPattern==0) return TRUE;

  p = filename + strlen(pattern_baseDir) + 1; /* +1 to get pass the / after basedir */
  /* fprintf(stderr, "nPattern = %d file= %s\n", nPattern, p); ********/
  for(i=0; i<nPattern; ++i) {
    if (regexec(fPatterns[i], p, (size_t)0, NULL, 0)!=0) 
      continue; /* no match */    
    return TRUE;
  }
  return FALSE;
}

