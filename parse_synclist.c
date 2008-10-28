/*  
   Copyright (C)  2006-2008 Renaissance Technologies Corp.
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
#include <stdlib.h>
#include <limits.h> /* to define PATH_MAX */

/* 
   Get the next to-be-synced file from synclist which is the output of
   parseRsyncList.C 
   The latter in turn parses the output from rsync.
   In other words, we use rsync in dry-run mode to get the
   files that need to be synced.

   to port to large_file environment: use off_t for size
*/

extern int verbose;

char basedir[PATH_MAX]; /* baseDir for the syncing */
FILE *fd;
struct stat st;  /* stat for the current file */

unsigned int nEntries;
int cur_entry;            /* file id -- <0 if it needs backup */
unsigned int nPages;
unsigned int last_bytes;  /* number of bytes for the last page */
int toRmFirst = FALSE;    /* flag rm existing file and then sync */
int file_changed = FALSE; /* flag to indicate if file has been changed during syncing */

unsigned int total_pages;
off_t total_bytes;

int isDelete, isHardlink;
char filename[PATH_MAX];
char fullname[PATH_MAX];
char linktar[PATH_MAX];

int init_synclist(char * synclist_path, char *bdir)
{
  char line[PATH_MAX];
  nEntries = 0;
  strcpy(basedir, bdir);

  if ((fd = fopen(synclist_path, "r")) == NULL) {
    fprintf(stderr, "Cannot open synclist file = %s \n", synclist_path);
    return FAIL;
  }

  while (fgets(line, PATH_MAX, fd) != NULL) nEntries++;
  if (nEntries == 0) { 
    fclose(fd); 
    fprintf(stderr, "Empty entires in synclist file = %s\n", synclist_path);
    return SUCCESS; /* OK, nothing to sync */
  } 
  rewind(fd);

  cur_entry = 0;
  total_pages = 0; 
  total_bytes = 0;
  
  return SUCCESS;
}  

/* 
   pages_for_file calculates the number (int) of pages for the current file
   and returns that number in an (int) type.  
   So, max_pages = 2**31 = 2147483648
   which corresponds to a file_size of 2**31 * 64512 = 1.38e14
   [ general limit = (1<<(sizeof(int)*8)) * PAGE_SIZE ]
   At that time, the type of page_number needs to be upgraded :)

   to_delete   -> -1
   normal_file -> number_of_pages
   softlink    -> 0
   hardlink    -> 0
   directory   -> 0
*/
int pages_for_file()
{  
  if (isDelete) {                              /* to be deleted directory or file */
    return TO_DELETE;
  }

  if (S_ISREG(st.st_mode)){
    int n;
    if (st.st_nlink > 1) return 0;              /* hardlink file */

    n = (int)((st.st_size)/(off_t)PAGE_SIZE);   /* regular file */
    if ((st.st_size)%((off_t)PAGE_SIZE) == 0) {
      last_bytes = (unsigned int)(PAGE_SIZE);
      return n;
    } else {      
      last_bytes = (unsigned int)(st.st_size - (off_t)n * (off_t)PAGE_SIZE);
      return n+1;
    }
    /*return ((st.st_size)%((off_t)PAGE_SIZE) == 0) ? n : n+1 ;*/
  }
  if (S_ISLNK(st.st_mode)){
    return 0;                                   /* softlink */
  }
  return 0;                                     /* directory */
}

off_t bytes_for_file()
{
  return st.st_size;
}

unsigned int get_nPages()  /* for this file */
{
  return nPages;
}

void strip(char * str)
{
  /* remove trailing \n and spaces */
  char *pc = &str[strlen(str)-1];
  while (*pc == ' ' || *pc == '\n') { 
    *(pc--) = '\0';
  }
}

int same_stat_for_file()
{
  /* check if current stat is same as that when get_next_entry is called */
  struct stat st1;

  if(lstat(fullname, &st1) < 0) {
    if (verbose >=1) perror(fullname);
    return FAIL;
  }
  
  if (st1.st_size != st.st_size || st1.st_mode != st.st_mode || 
      st1.st_mtime != st.st_mtime) {
    return FAIL; /* the file has changed */
  }
  return SUCCESS;

}

int is_hardlink_line(char * line)
{
  /* when line is in the form of 
     string1 string2
     it can be either a filename (string1 string2)
     or a hardlink string1 => string2
  */
  struct stat st;
  char fn[PATH_MAX];
  strcpy(fn, basedir);
  strcat(fn, "/");
  strcat(fn, line);

  /* if the whole line is not a file, then we are dealing with hardlink case */
  return (lstat(fn, &st) < 0);
  /* cannot deal with the situation 
        str1  is a file
        str2  is a hardlink to str1
        str1 str2   is a file
     AND if we need to sync 
        str1 and 'str1 str2' at the same time.
     But this situation is very rare. */
}

int get_next_entry(int current_file_id)
{
  char *c;
  char line[PATH_MAX];

  /* inside this function, cur_entry is set to be positve to facilitate processing */
  cur_entry = current_file_id; /* from main loop's index, starting with 1 */
  
  isDelete = FALSE;
  isHardlink = FALSE;

  fgets(line, PATH_MAX, fd);
  strip(line);

  if (current_file_id == nEntries) {
    fclose(fd); /* close the synclist file */
    if (verbose>=2) fprintf(stderr, "no more entry in synclist.\n");
  }

  if (verbose>=2) {
    fprintf(stderr, "Got current entry = %d (total= %d)\n", cur_entry, nEntries);
    fprintf(stderr, "%s\n", line);
  }

  strcpy(fullname, basedir);
  strcat(fullname, "/");
  if (strncmp(line, "deleting ", 9)==0) {
    isDelete = TRUE;
    nPages = -1;
    strcat(fullname, &line[9]);
    strcpy(filename, &line[9]);
    if (needBackup(fullname)) cur_entry = -cur_entry;
    return SUCCESS;
  } else if ((c = strchr(line, ' '))!=NULL && is_hardlink_line(line)) {
    /* is it a hardlink -- two filenames separated by a space */
    char fn[PATH_MAX];
    isHardlink = TRUE;
    strncpy(fn, line, (c - line));
    fn[c-line] = '\0';
    strcat(fullname, fn);
    strcpy(filename, fn);
    strcpy(linktar, c+1);    
  } else {    
    /* normal, single entry */
    strcat(fullname, line);
    strcpy(filename, line);
  }

  /* update stat */
  if(lstat(fullname, &st) < 0) {
    if (verbose >=1) perror(fullname);
    return FAIL;
  }

  if (S_ISLNK(st.st_mode)) {
    int linklen;
    linklen = readlink(fullname, linktar, PATH_MAX);
    /* readlink doesn't null-terminate the string */
    *(linktar + linklen) = '\0';
  } else if (st.st_nlink>1 && !isHardlink) {
    /* this is the target file that others (hard)link to.
       treat it like a normal file */
    st.st_nlink = 1;
  }

  nPages = pages_for_file();
  if (nPages > 0) { /* for regular files */
    total_pages += nPages;
    total_bytes += st.st_size;    
  }

  if (needBackup(fullname)) cur_entry = -cur_entry;

  file_changed = FALSE;

  return SUCCESS;
}

void adjust_totals()
{
  if (nPages > 0) {
    total_pages -= nPages;
    total_bytes -= st.st_size;
  }
}

/* some accessors */
unsigned int total_entries() { return nEntries; }

int current_entry() { return cur_entry; }

int is_softlink() { return S_ISLNK(st.st_mode); }

int is_hardlink() { return isHardlink; }

int is_directory() { return S_ISDIR(st.st_mode); }

char * getFilename() { /* relative to basedir */ return &filename[0]; }

char * getFullname() { return &fullname[0]; }

/* 
   The following three fx are used to fill file_info into the send_buffer.
   They return the number of bytes being written into the buf, including the \0 byte.
*/
unsigned int fill_in_stat(char *buf)
{
  /* load into buf area the stat info in ascii format */
  if (isDelete) 
    sprintf(buf, "0 0 0 0 0 0 0 0");
  else {
    #ifdef _LARGEFILE_SOURCE
    sprintf(buf, "%u %u %u %u %llu %lu %lu %d", st.st_mode, st.st_nlink,
	    st.st_uid, st.st_gid, st.st_size, st.st_atime, st.st_mtime,
	    toRmFirst);
    #else
    sprintf(buf, "%u %u %u %u %lu %lu %lu %d", st.st_mode, st.st_nlink,
	    st.st_uid, st.st_gid, st.st_size, st.st_atime, st.st_mtime,
	    toRmFirst);
    #endif
  }
  
  return strlen(buf)+1;
}

unsigned int fill_in_filename(char * buf)
{
  strcpy(buf, filename);
  return strlen(buf)+1;
}

unsigned int fill_in_linktar(char *buf)
{
  strcpy(buf, linktar);
  return strlen(buf)+1;
}


