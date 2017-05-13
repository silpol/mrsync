/*  
   Copyright (C)  2008 Renaissance Technologies Corp. 
                  main developer: HP Wei <hp@rentec.com>
   Copyright (C)  2006 Renaissance Technologies Corp. 
                  main developer: HP Wei <hp@rentec.com>
      This file contains all the file-operations for multicatcher.
      20060404: 
         found a undetected omission in open_file().
         AFter lseek(), we should write a dummy byte so that
         multicatcher.zzz has the right file size to start with.
         Otherwise, it will grow as syncing progresses.

	 Port the code to deal with Large_files.
         esp in write_page(),
         lseek(fout, (off_t)(page-1)*(off_t)PAGE_SIZE, SEEK_SET)
      200603:
         Remove the meta-data operation.
         Each file's info (stat) is transfered to targets during
         the OPEN_FILE_CMD. extract_file_info() in this file
         is to get that stat info for the current entry(file).
       
   Copyright (C)  2005 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
      Previously, memory mapped file was used for file IO
      but later was changed to simple open() and lseek(), write().  
      This was also echoed in a patch by Clint Byrum <clint@careercast.com>.

   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
   This file was originally called wish_list.c
   This file was modified in 2001 and later from files in the program 
   multicaster copyrighted by Aaron Hillegass as found at 
   <http://sourceforge.net/projects/multicaster/>

   Copyright (C)  2000 Aaron Hillegass <aaron@classmax.com>

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

#include <libgen.h>
#include "main.h"

extern int verbose;
extern int machineID;

char              * baseDir = NULL;
char              * missingPages=NULL;  /* starting address of the array of flags */
int                 fout;               /* file discriptor for output file */

int toRmFirst = FALSE;  /* remove existing file first and then sync */
/* off_t total_bytes_written;*/
unsigned int nPages;
int had_done_zero_page;
int current_file_id = 0;  /* 1, 2, 3 ... or -1, -2 ... for backup */
int backup;           /* flag for a file that needs backup when deletion */
char *backup_suffix = NULL;
char my_backup_suffix[] = "_mcast_bakmmddhhmm";

/* file stat_info which is transmitted from master */
mode_t  stat_mode;
nlink_t stat_nlink;
uid_t   stat_uid;
gid_t   stat_gid;
off_t   stat_size;
time_t  stat_atime;
time_t  stat_mtime;

char filename[PATH_MAX];
char linktar[PATH_MAX];
char fullpath[PATH_MAX];
char tmp_suffix[L_tmpnam];

void default_suffix()  
{
  time_t t;
  struct tm tm;
  time(&t);
  localtime_r(&t, &tm);
  sprintf(my_backup_suffix, "_mcast_bak%02d%02d%02d%02d", 
	  tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min);
  backup_suffix = my_backup_suffix;
  return;
}

void get_tmp_suffix()
{
  /* this is called once in each multicatcher */
  char tmp[L_tmpnam];
  tmpnam_r(&tmp[0]);
  strcpy(tmp_suffix, basename(tmp));
}

int make_backup() 
{
  char fnamebak[PATH_MAX];
  if (strlen(fullpath) + strlen(backup_suffix) > (PATH_MAX-1)) {
    fprintf(stderr, "backup filename too long\n");
    return FAIL;
  };

  if (!backup) return SUCCESS; /* if not match with pattern, skip the backup */
 
  sprintf(fnamebak, "%s%s", fullpath, backup_suffix);
  /* 
     The backup scheme is as follows.
        ln file file.bak (mv file file.bak would cause file to non-exist for a short while)
        mv file.new file
  */
  if (link(fullpath, fnamebak) != 0) {
    if (errno != ENOENT && errno != EINVAL) {
      fprintf(stderr,"hardlink %s => %s : %s\n",fullpath, fnamebak, strerror(errno));
      return FAIL;
    }
  } 
  if (verbose >= 2) {
    fprintf(stderr, "backed up %s to %s\n",fullpath, fnamebak);
  }
  return SUCCESS;
}

void get_full_path(char * dest, char * sub_path)
{
  /* prepend the sub_path with baseDir --> dest */
  strcpy(dest, baseDir);
  strcat(dest, "/");
  strcat(dest, sub_path);
}

void get_tmp_file(char * tmp)
{
  /*char  *fncopy;*/
  /* ******* change to filename_mcast.fileabc%$&? */
  /* fncopy = strdup(filename);   dirname change the string content */
  strcpy(tmp, baseDir);
  strcat(tmp, "/");
  strcat(tmp, filename);
  strcat(tmp, "_");
  strcat(tmp, TMP_FILE);
  strcat(tmp, tmp_suffix);
  /* free(fncopy); */
}

int my_unlink(const char *fn)
{ 
  if (verbose>=2)
    fprintf(stderr, "deleting file: %s\n", fn);
  if (unlink(fn) != 0) {
    if (errno==ENOENT) { 
      return SUCCESS; 
    } else { 
      /* NOTE: unlink() could not remove files which do not have w permission!*/
      /* resort to shell command */
      char cmd[PATH_MAX];
      sprintf(cmd, "rm -f %s", fn);
      if (system(cmd)!=0) {    
	fprintf(stderr, "'rm -f' fails for %s\n", fn);
	return FAIL;
      }
    }
  }
  return SUCCESS;
}

int my_touch(const char*fn)
{
  int fo;
  if( (fo = open(fn,  O_RDWR | O_CREAT | O_TRUNC, 
		 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0) {
    perror(fn);
    return FAIL;
  }

  /* set the size of the output file */
  if(lseek(fo, 0, SEEK_SET) == -1) {
    fprintf(stderr, "cannot seek for %s\n", fn);
    perror(fn);
    return FAIL;
  }  
  close(fo);
  return SUCCESS;
}

int extract_file_info(char * buf, int n_file, unsigned int n_pages)
{
  /*
     Along with OPEN_FILE_CMD, the data area in rec_buf contains
     (stat_ascii)\0(filename)\0(if_is_link linktar_path)\0
     where the stat_string contains the buf in 
     sprintf(buf, "%lu %lu %lu %lu %lu %lu %lu", st.st_mode, st.st_nlink,
	  st.st_uid, st.st_gid, st.st_size, st.st_atime, st.st_mtime);
  */
  char * pc = &buf[0];

  #ifdef _LARGEFILE_SOURCE
  if (sscanf(pc, "%u %u %u %u %llu %lu %lu %d", &stat_mode, &stat_nlink,
	     &stat_uid, &stat_gid, &stat_size, &stat_atime, &stat_mtime, 
	     &toRmFirst) != 8)
    return FAIL;
  #else
  if (sscanf(pc, "%u %u %u %u %lu %lu %lu %d", &stat_mode, &stat_nlink,
	     &stat_uid, &stat_gid, &stat_size, &stat_atime, &stat_mtime,
	     &toRmFirst) != 8)
    return FAIL;
  #endif

  /* fprintf(stderr, "size= %llu\n", stat_size); *********/

  pc += (strlen(pc) + 1);
  strcpy(filename, pc);
  get_full_path(fullpath, filename);
  
  linktar[0] = '\0';
  if (S_ISLNK(stat_mode) || stat_nlink > 1) { /* if it is a softlink or hardlink */
    pc += (strlen(pc) +1);
    strcpy(linktar, pc);
  }

  nPages = n_pages;
  current_file_id  = n_file;
  backup = (current_file_id < 0);
  had_done_zero_page = FAIL;
  /*total_bytes_written = 0;*/
  return SUCCESS;
}

int open_file()
{
  int i;

  /* 
     sometimes for disk space reason, it is necessary 
     to first remove the file and sync.
     If toReplace is true, the backup option should be off.
  */
  if (toRmFirst) {
    if (!my_unlink(fullpath)) {
      fprintf(stderr, "Replacing ");
      perror(filename);
      return FAIL;
    }
  }

  /* fprintf(stderr, "%d %d %d\n", stat_mode, stat_nlink, stat_size); *********/
  if (S_ISREG(stat_mode) && stat_nlink == 1) {
    /* if it is a regular file and not a hardlink */ 
    char tmpFile[PATH_MAX];
    get_tmp_file(tmpFile);
    
    my_unlink(tmpFile);   /* make sure it's not there from previous runs */
    
    if( (fout = open(tmpFile,  O_RDWR | O_CREAT | O_TRUNC, 
		     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0) {
      fprintf(stderr, "cannot open() %s for writing. \n", tmpFile);
      perror(tmpFile);
      return FAIL;
    }

    /* set the size of the output file (see Steve's book on page 411) */
    if(lseek(fout, stat_size - 1 , SEEK_SET) == -1) {
      #ifdef _LARGEFILE_SOURCE
      fprintf(stderr, "lseek() error for %s with size = %llu\n", tmpFile, stat_size);
      #else
      fprintf(stderr, "lseek() error for %s with size = %u\n", tmpFile, (unsigned int)stat_size);
      #endif
      perror(tmpFile);
      close(fout);     
      return FAIL;
    }
    if (write(fout, "", 1) != 1) {
      #ifdef _LARGEFILE_SOURCE
      fprintf(stderr, "write() error for %s with size = %llu\n", tmpFile, stat_size);
      #else
      fprintf(stderr, "write() error for %s with size = %u\n", tmpFile, (unsigned int)stat_size);
      #endif
      perror(tmpFile);
      close(fout);      
      return FAIL;      
    }
  }
  
  /* init missingPages flags */
  if (!missingPages) free(missingPages);
  missingPages =  malloc(sizeof(char) * nPages);  
  for(i=0; i < nPages; ++i) missingPages[i] = MISSING;

  if (verbose>=2) fprintf(stderr, "Ready to receive file = %s\n", filename);
  return SUCCESS;
}

int close_file()
{ 
  /* delete missingPages flags which was malloc-ed in open_file() */
  free(missingPages);
  missingPages = NULL;

  if (S_ISREG(stat_mode) && stat_nlink == 1) {
    /* if it is a regular file and not a hardlink */
    char tmpFile[PATH_MAX];
    struct stat stat;
    
    if ((fout !=  -1) && (close(fout) != 0)) {
      #ifdef _LARGEFILE_SOURCE
      fprintf(stderr, "ERROR: Cannot close() tmp for -- %s size= %llu \n",
	      filename, stat_size);
      #else
      fprintf(stderr, "ERROR: Cannot close() tmp for -- %s size= %u \n", 		
	      filename, (unsigned int)stat_size);
      #endif
      perror("close");
      return FAIL;
    }
    fout = -1;  /* if the following fails, the reentry wont do munmap() */

    /* the real work */
    /* get_full_path(oldFile, filename); **** unnecessary fullpath is the oldFile */
  
    get_tmp_file(tmpFile);

    /* 8/14/2002
       If there was a hardware (disk IO) problem       
       the sync should not proceed.
       ** Add the following checking.
    */
    if (lstat(tmpFile, &stat)<0) {
      perror("ERROR: close_file() cannot lstat the tmp file\n");
      return FAIL;
    }
    
    if (backup && !make_backup(fullpath)) { /* make a hardlink oldFile => backup_file */
      fprintf(stderr, "fail to make backup for %s\n", fullpath);
      return FAIL;
    }
    if (rename(tmpFile, fullpath)<0) {
      perror("ERROR: close_file():rename() \n");
      return FAIL;
    }

    /* 20071016 in rare occasion, the written file has not the right size */
    if (lstat(fullpath, &stat)<0) {
      fprintf(stderr, "ERROR: close_file() cannot lstat %s\n", fullpath);
      return FAIL;
    }
    if (stat_size != stat.st_size) {
      fprintf(stderr, "ERROR: close_file() filesize != incoming-size\n");
      return FAIL;
    }    
  }

  /* for debug
  if (verbose>=2) fprintf(stderr, "total bytes written %llu for file %s\n", 
			  total_bytes_written, filename);
  */

  return SUCCESS;
}

int rm_tmp_file()
{ 
  char tmpFile[PATH_MAX];

  if ((fout !=  -1) && (close(fout) != 0)) {
      #ifdef _LARGEFILE_SOURCE
      fprintf(stderr, "ERROR: Cannot close() tmp for -- %s size= %llu \n",
	      filename, stat_size);
      #else
      fprintf(stderr, "ERROR: Cannot close() tmp for -- %s size= %u \n", 		
	      filename, (unsigned int)stat_size);
      #endif
      perror("rm_tmp");
      return FAIL;
    }
  fout = -1;
  
  get_tmp_file(tmpFile);

  return (my_unlink(tmpFile));
} 

int nPages_for_file()
{ 
  return nPages;
};

/* return total number of missing pages */
int get_missing_pages()
{
  int i, result=0;

  for(i=0; i < nPages; ++i) 
    if ((missingPages[i]) == MISSING) ++result;
  return result;
}

int is_missing(int index)
{
  return (missingPages[index] == MISSING) ?  TRUE : FALSE;
}

void page_received(int index)
{
  missingPages[index] = RECEIVED;
}

/* 
   write() in write_page() may block forever.
   This function is to check if write() is ready.
*/
int writable(int fd)
{
  struct timeval write_tv;
  fd_set   wset;
  FD_ZERO(&wset);
  FD_SET(fd, &wset);

  write_tv.tv_sec = WRITE_WAIT_SEC;
  write_tv.tv_usec = WRITE_WAIT_USEC;
  return (select(fd + 1, NULL, &wset, NULL, &write_tv)==1);
}

void write_page(int page, char *data_ptr, int bytes)
{
  /* page = page number starting with 1 */
  if (page < 1 || page > nPages) return;

  /* Do we need to write this page? */
  if (is_missing(page-1)){
    if (!writable(fout)) return;

    /* Write the data */ 
    if (lseek(fout, (off_t)(page-1)*(off_t)PAGE_SIZE, SEEK_SET)<0) {
      if (verbose>=1) {
	fprintf(stderr, "ERROR: write_page():lseek() at page %d for %s\n", 
		page, filename);
	perror("ERROR");
      }
      return;
    }
    if (write(fout, data_ptr, bytes)<0) {
      /* write IO error !!! */
      perror("ERROR");
      fprintf(stderr, "write_page():write() error: at page %d for %s\n", page, filename);
      return;
    }
    
    /* Mark the page as received in our wish list */
    page_received(page-1);
    /*total_bytes_written += bytes;*/
  } else {
    /* If we don't need to write it, just return */
    if (verbose >=2) {
      fprintf(stderr, "Already have page %d for %d:Ignoring\n", page, current_file_id);
    }
  } 
  return;
}  

/* For files whose size is 0 */
int touch_file()
{
  if (verbose >=2) 
    fprintf(stderr, "touching file: %s\n", fullpath);

  my_unlink(fullpath);
  return my_touch(fullpath);
  /* system() 
    VERY time in-efficient 
    char cmd[PATH_MAX];
    sprintf(cmd, "touch %s", fullpath);
    return (system(cmd)==0);
  */
}

int delete_file(int to_check_dir_type)
{
  struct stat st;
  char fp[PATH_MAX];
  int trailing_slash;
  int type_checking;

  strcpy(fp, fullpath);

  /* remove trailing slash if any -- for deletion-'type' checking */
  if (to_check_dir_type) {
    char *pc;
    type_checking = TRUE;    
    pc = &fp[0] + strlen(fp) - 1;
    if (*pc=='/') {
      *pc = '\0';
      trailing_slash = TRUE;
    } else {
      trailing_slash = FALSE;
    };
  } else {
    type_checking = FALSE;
  }

  if(lstat(fp, &st) < 0) {
    /* already gone ? */
    return SUCCESS;
  }
  if (S_ISREG(st.st_mode)  || S_ISLNK(st.st_mode)) { /* delete a file or link */
    if (verbose>=2)
      fprintf(stderr, "deleting file: %s\n", fp);
    
    if (type_checking && trailing_slash) { /* intended to remove a directory when it is not */
      return FAIL;
    }

    if (backup && S_ISREG(st.st_mode) && !make_backup(fp)) {/* backup regular file */
      return FAIL; /* failed to make_backup */
    } 
    return (my_unlink(fp));
  } else  if (S_ISDIR(st.st_mode)) { /* remove a directory */
    char cmd[PATH_MAX];
    if (verbose>=2)
      fprintf(stderr, "deleting directory: %s\n", fp);

    if (type_checking && (!trailing_slash)) { /* intended to remove a non-dir when it is directory */
      return FAIL;
    }

    sprintf(cmd, "rm -rf %s", fp); /* remove everything in dir, watch out for this */
    return (system(cmd)==0);
  }
  /* not file, link, directory */
  fprintf(stderr, "unrecognized file_mode for %s\n", fp);
  return FAIL;
}

/* send complaints to the master for missing data */
int ask_for_missing_page()
{
  int i, n=0, total=0;

  /* 
     Send missing page indexes if any
  */ 
  init_fill_ptr();
  for(i=0; i < nPages; ++i) {
    if (missingPages[i] == MISSING ) {
      ++n;
      ++total;
      if (n > MAX_NPAGE) {
	/* send previous missing page-indexes */
	send_complaint(MISSING_PAGE, machineID, MAX_NPAGE, current_file_id);
	init_fill_ptr();
	n = 1; 
      }
      /* fill in one page index */
      fill_in_int(i+1); /* origin = 1 */
    }
  }
  /* send the rest of missing pages complaint */
  if (n>0) send_complaint(MISSING_PAGE, machineID, n, current_file_id);

  return total;  /* there are missing pages */
}

void missing_page_stat()
{
  int i, n=0, sum=0, last=-1;

  for(i=0; i < nPages; ++i) {
    if (missingPages[i] == MISSING ) {
      ++n;
      if (last<0) {
	sum += i;
      } else {
	sum += (i - last);
      }
      last = i;
    }
  }
  if (n>0) {
    double a = sum;
    double b = n;
    fprintf(stderr, "file= %d miss= %d out-of %d avg(delta_index) = %f\n", 
	    current_file_id, n, nPages, a/b);
  }
}

void my_perror(char * msg)
{
  char fn[PATH_MAX];
  sprintf(fn, "%s - %s", fullpath, msg);
  perror(fn);
}

int set_owner_perm_times()
{
  int state = SUCCESS;

  /* set owner */
  if (lchown(fullpath, stat_uid, stat_gid)!=0) {
    my_perror("chown");
    state = FAIL;
  }

  /* 
     set time and permission.
     Don't try to set the time and permission on a link 
  */
  if (!S_ISLNK(stat_mode)) {
    struct utimbuf times; 
    if (chmod(fullpath, stat_mode)!=0) {
      my_perror("chmod");
      state = FAIL;    
    }

    times.actime = stat_atime;
    times.modtime = stat_mtime;
    if (utime(fullpath, &times)!=0) {
      my_perror("utime");
      state = FAIL;
    }
  }
  return state;
}

int update_directory()
{
  struct stat st;
  int exists;
  char fp[PATH_MAX], *pc;

  if (verbose>=2)
    fprintf(stderr, "Updating dir: %s\n", fullpath);

  /* if fullpath is a softlink that points to a dir,
     and it has a trailing '/',
     lstat() will view it as a directory ! 
     So, we remove the trailing '/' before lstat() */
  strcpy(fp, fullpath);
  pc = &fp[0] + strlen(fp) - 1;
  if (*pc=='/') *pc = '\0';

  if(lstat(fp, &st) < 0) {
    switch(errno) {
    case ENOENT:
      exists = FALSE;
      break;
    default:
      my_perror("lstat");
      return FAIL;
    }
  } else {
    exists = TRUE;
  }

  if (!exists) {
    /* There's nothing there, so create dir */
    if (mkdir(fp, stat_mode) < 0){
      my_perror("mkdir");
      return FAIL;
    }
    return SUCCESS;
  } else if (!S_ISDIR(st.st_mode)) {
    /* If not a directory delete what is there */
    if (unlink(fp)!=0){
      my_perror("unlink");
      return FAIL;
    }
    if (verbose>=2)
      fprintf(stderr, "Deleted file %s to replace with directory\n", fp);
    if (mkdir(fp, stat_mode) < 0){
      my_perror("mkdir");
      return FAIL;
    }
    return SUCCESS;
  } else {
    /* If dir exists,  just chmod */
    chmod(fp, stat_mode);
    /*** 20070410: changing mtime of a dir can cause NFS to confuse
	 See http://lists.samba.org/archive/rsync/2004-May/009439.html
	 So, I comment it out in the following 

	 struct utimbuf times;
	 times.actime = stat_atime;
	 times.modtime = stat_mtime;
    ***/
    /*
      if (utime(fullpath, &times) < 0) {
      my_perror("utime");
      }
    */
    return SUCCESS;
  }
}

int update_directory0()
{
  DIR *d;

  struct utimbuf times;
  times.actime = stat_atime;
  times.modtime = stat_mtime;

  if (verbose>=2)
    fprintf(stderr, "Creating dir: %s\n", fullpath);

  d = opendir(fullpath);  
  if (d == NULL) {
    switch(errno) {
    case ENOTDIR:
      /* If not a directory delete what is there */
      if (unlink(fullpath)!=0){
	my_perror("unlink");
	return FAIL;
      }
      if (verbose>=2)
	fprintf(stderr, "Deleted file %s to replace with directory\n", fullpath);
      /* Fall through to ENOENT */
    case ENOENT:
      /* There's nothing there, so create dir */
      if (mkdir(fullpath, stat_mode) < 0){
	my_perror("mkdir");
	return FAIL;
      }
      return SUCCESS;
    default:
      my_perror("opendir");
      return FAIL;
    } 
  } else {
    /* If dir exists,  just chmod */
    closedir(d);
    chmod(fullpath, stat_mode);
    /*** 20070410: changing mtime of a dir can cause NFS to confuse
	 See http://lists.samba.org/archive/rsync/2004-May/009439.html
	 So, I comment it out in the following ***/
    /*
    if (utime(fullpath, &times) < 0) {
      my_perror("utime");
    }
    */
    return SUCCESS;
  }
}

int check_zero_page_entry()
{
  /* when total_pages = 0, the entry can be 
       an empty file
       softlink (hardlink),
       a directory
  */
  if (had_done_zero_page) return SUCCESS;  /* to avoid doing it again */

  if (S_ISDIR(stat_mode)) { /* a directory */
    if (!update_directory()) {
      had_done_zero_page = FAIL;
      return FAIL;
    }
  } else if (S_ISLNK(stat_mode)) { /* Is it a softlink? */
    if (verbose>=2)
      fprintf(stderr, "Making softlink: %s -> %s\n", fullpath, linktar);
    delete_file(FALSE);  /* remove the old one at fullpath */
    if (symlink(linktar, fullpath) < 0) {
      my_perror("symlink");
      had_done_zero_page = FAIL;
      return FAIL;
    }
  } else if (stat_nlink > 1) { /* hardlink */
    char fn[PATH_MAX];
    get_full_path(fn, linktar); /* linktar is a relative path from synclist */
    if (verbose>=2) 
      fprintf(stderr, "Making a hardlink: %s => %s\n", fullpath, fn);
    my_unlink(fullpath);  /* remove the old one */
    if (link(fn, fullpath)!=0) {
      my_perror("link");
      had_done_zero_page = FAIL;
      return FAIL;
    }
  } else {
    /* it must be a regular file */
    
    if (!touch_file()) {
      had_done_zero_page = FAIL;
      return FAIL;
    } else {
      set_owner_perm_times();
    }
  }
  had_done_zero_page = SUCCESS;
  return SUCCESS;
}

