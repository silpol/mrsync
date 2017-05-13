/*  
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
   Copyright (C)  2005 Renaissance Technologies Corp.
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

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

void strip(char * str);

/* place to hold the array of string */
char **    machine_names = NULL;  /* array of (char*) */
int nTargets=0;

void get_machine_names(char * filename)
{
  FILE *fd;
  char line[PATH_MAX];
  int count=0;

  if ((fd = fopen(filename, "r")) == NULL) {
    fprintf(stderr, "Cannot open file -- %s \n", filename);
    return;
  }
  while (fgets(line, PATH_MAX, fd) != NULL) {
    strip(line);
    if (strlen(line) != 0) ++count;
  }
  if (count == 0) {
    fclose(fd); 
    fprintf(stderr, "No machine names in the file = %s\n", filename);
    return;
  }

  nTargets = count;

  rewind(fd);
  machine_names = malloc(nTargets * sizeof(void*));

  line[0] = '\0';
  count = 0;
  while(fgets(line, PATH_MAX, fd) != NULL) {
    strip(line);
    if (strlen(line)==0) continue;    
    machine_names[count] = (char*)strdup(line);
    line[0] = '\0';
    ++count;
  }
  fclose(fd);
}

char * id2name(int id)
{
  return (machine_names) ? machine_names[id] : "";
}
