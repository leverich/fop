#define _XOPEN_SOURCE 600
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <dirent.h>
#include <string.h>

void usage() {
  printf("Usage: fop [-a|--advice <advice|#>] [-d|--dump] [-s|--sync]\n"
         "           [-v|--verbose] [-L|--links] [-R|--recursive]\n"
         "           <file> ..\n"
         "\n"
         "  -d is equivalent to -a POSIX_FADV_DONTNEED.\n"
         "  See posix_fadvise(2) for advice names and numbers.\n"
         );
  exit(0);
}

int error            = 0;

int opt_sync         = 0;
int opt_advice       = 0;
int opt_follow_links = 0;
int opt_recursive    = 0;
int opt_verbose      = 0;


#define VERBOSE(a...) do { if (opt_verbose) fprintf(stderr, a); } while(0)

#define ABORT_FOP() do { error = 1; depth--; return; } while(0)
void fop(char *filename) {
  int fd;
  static int depth = 0;
  struct stat statbuf;
  static char parent[1024] = "";

  depth++;

  VERBOSE("fop(%s%s): ", parent, filename);

  if (depth > 100) {
    fprintf(stderr, "fop() recursed over 100 deep; loop detected? ABORTING\n");
    exit(-1);
    //    ABORT_FOP();
  }

  if (lstat(filename, &statbuf) < 0) {
    perror("stat() failed");
    ABORT_FOP();
  }

  if (S_ISLNK(statbuf.st_mode)) {
    char path[1024];
    int cnt;

    if (opt_follow_links) {
      if ((cnt = readlink(filename, path, 1023)) < 0) {
        perror("readlink() failed");
        ABORT_FOP();
      }

      path[cnt] = '\0';
      VERBOSE("following link to %s\n", path);

      if (strcmp(path, ".") && strcmp(path, "..")) {
        fop(path);
      }

      depth--;
      return;
    } else {
      VERBOSE("not following link");
    }
  } else if (S_ISDIR(statbuf.st_mode)) {
    // open directory, fop() contents
    DIR *dd;
    struct dirent *de;
    char cwd[256];
    char oldparent[1024];

    if (opt_recursive) {
      if ((dd = opendir(filename)) < 0) {
        perror("opendir() failed");
        ABORT_FOP();
      }

      if ((getcwd(cwd, 256)) == 0) {
        perror("getcwd() failed");
        closedir(dd);
        ABORT_FOP();
      }

      strcpy(oldparent, parent);
      strncat(parent, filename, 1024 - strlen(parent));
      strncat(parent, "/", 1024 - strlen(parent));

      if (chdir(filename) < 0) {
        perror("chdir() failed");
        closedir(dd);
        ABORT_FOP();
      }

      VERBOSE("recursing into directory\n");

      while (de = readdir(dd)) {
        if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
          fop(de->d_name);
        }
      }

      if (chdir(cwd) < 0) {
        perror("chdir(old_cwd) failed");
        closedir(dd);
        ABORT_FOP();
      }

      strcpy(parent, oldparent);

      closedir(dd);

      depth--;
      return;
    } else {
      VERBOSE("not recursing into directory");
    }
  } else if (S_ISREG(statbuf.st_mode)) {
    // regular file or followed link, do your thing

    if ((fd = open(filename, O_RDONLY)) < 0) {
      perror("open() failed");
      ABORT_FOP();
    }

    if (opt_sync) VERBOSE("fdatasync ");
    if (opt_sync && (fdatasync(fd) < 0)) {
      perror("fdatasync() failed");
      ABORT_FOP();
    }

    if (opt_advice) VERBOSE("fadvise(%d) ", opt_advice);
    if (opt_advice && (posix_fadvise(fd, 0, 0, opt_advice) < 0)) {
      perror("posix_fadvise() failed");
      ABORT_FOP();
    }

    close(fd);
  } // directory and !opt_recursive or islink and !opt_follow_links

  VERBOSE("\n");

  depth--;
}

int main(int argc, char *argv[]) {
  int c;
  int fd;

  while (1) {
    static struct option long_options[] = {
      {"advice", 1, 0, 'a'},
      {"dump", 0, 0, 'd'},
      {"sync", 0, 0, 's'},
      {"verbose", 0, 0, 'v'},
      {"links", 0, 0, 'L'},
      {"recursive", 0, 0, 'R'},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, "a:dsvLR", long_options, NULL);
    if (c == -1) break;

    switch (c) {
    case 'a':
      fprintf(stderr, "-a not implemented :-(\n");
      exit(-1);
      break;

    case 'd':
      opt_advice = POSIX_FADV_DONTNEED;
      break;

    case 's':
      opt_sync = 1;
      break;

    case 'v':
      opt_verbose = 1;
      break;

    case 'L':
      opt_follow_links = 1;
      break;
      
    case 'R':
      opt_recursive = 1;
      break;

    default:
      usage();
      break;
    }
  }

  argv += optind;
  argc -= optind;

  if (argc == 0) usage();

  if (!opt_advice && !opt_sync) {
    fprintf(stderr, "No actions have been specified.\n\n");
    usage();
  }

  while (argc--) {
    fop(argv[0]);
    argv++;
  }

  return error ? -1 : 0;
}
