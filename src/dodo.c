#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#include <sys/time.h>
#include <pwd.h>

/******************************************************************************\
|                                   Helpers                                    |
\******************************************************************************/
struct timeval tv_now     = {0};  // hold timevalified time
char           ts_now[21] = {0};  // hold stringified unix time
time_t         tt_now     = {0};  // hold regular unix time
struct tm      tm_now     = {0};  // hold broken-down time of now

void time_init() {
  // Slightly inaccurate, since the times won't be identical between the first two calls
  tt_now = time(NULL);
  gettimeofday(&tv_now, NULL);
  sprintf(ts_now, "%lu", tt_now);
  strptime(ts_now, "%s", &tm_now);
}

static inline int days_diff(time_t a, time_t b) {
  return (a>b ? a-b : b-a)/86400;
}

static inline char hasterm(char* path) {return '/' == path[strlen(path)-1];}


/******************************************************************************\
|                              VT-100 Formatters                               |
\******************************************************************************/
#define FG(x) "\33[3"#x"m"
#define BG(x) "\33[4"#x"m"
#define AT(x) "\33[" #x"m"
#define CLS   "\33[0m\33[39m\33[49m"
char*F[]={FG(0),FG(1),FG(2),FG(3),FG(4),FG(5),FG(6),FG(7),FG(8),FG(9)};
char*B[]={BG(0),BG(1),BG(2),BG(3),BG(4),BG(5),BG(6),BG(7),BG(8),BG(9)};
char*A[]={AT(0),
  AT(1),
  AT(2),
  AT(1) AT(2),
  AT(4),
  AT(4) AT(1),
  AT(4) AT(2),
  AT(4) AT(1) AT(2),
  AT(5),
  AT(5) AT(1),
  AT(5) AT(2),
  AT(5) AT(1) AT(2),
  AT(5) AT(4),
  AT(5) AT(4) AT(1),
  AT(5) AT(4) AT(2),
  AT(5) AT(4) AT(1) AT(2)};

typedef enum sp_type {
  SP_DONE = 1<<0,
  SP_HIPR = 1<<1,
  SP_STAR = 1<<2,
  SP_LOVE = 1<<3,
  SP_NOTE = 1<<4
} sp_type;

static char sp_todo[] = FG(5)"□"FG(9);
static char sp_done[] = FG(2)"✓"FG(9);
static char sp_hipr[] = FG(6)"!"FG(9);
static char sp_star[] = FG(3)"★"FG(9);
static char sp_love[] = FG(1)"♡"FG(9);
static char sp_note[] = FG(4)"•"FG(9);


/******************************************************************************\
|                                DDNode, DDList                                |
\******************************************************************************/
#define MAX_INDENT 16
#define MAX_MODS   5

char* DODO_ROOT  = NULL;
char* DODO_STASH = NULL;
typedef struct DDNode {
  char            done;
  char            type;
  time_t          dt;
  char            mods[MAX_MODS+1];
  char*           desc;
} DDNode;

typedef struct DDList {
  unsigned int   n;
  unsigned char* depth; // nesting
  DDNode*        nodes;
  char*          name;
} DDList;

size_t DDListLen(DDList* dl) {
  size_t n = 0;
  for(int i=0; i<dl->n; i++)
    n += strlen(dl->nodes[i].desc) + 8 + 1;  // add 1 for terminating newline/null
  return n;
}

char* DDListGetMakeRoot() {
  static char dodo_subdir[] = ".dodo/";
  char *rootd = NULL, *buf = NULL;
  DIR* dir;
  struct stat sa = {0};
  if((buf=getenv("DODO_ROOT"))) {

    // Reject if not absolute path
    if('/' != *buf) {
      printf("<WRN> $DODO_ROOT contains a relative path %s, using default.\n", buf);

    // Reject if not a directory (equivalently, if doesn't exist and can't be made)
    } else if(!(mkdir(buf, 0700), (dir=opendir(buf)),closedir(dir),dir)) {
      printf("<WRN> $DODO_ROOT is not a directory (%s), using default.\n", buf);

    // Else, we're good.
    } else {
      rootd = strdup(buf);
      DODO_ROOT = rootd;
      return rootd;
    }
  }

  // Use the default directory
  if(!(buf=getenv("HOME")) || '/' != *buf) {
    struct passwd *pw = getpwuid(getuid());
    buf = pw->pw_dir;
    if('/' != *buf) {
      printf("<ERR> Cannot find suitable default directory.  Exiting.\n");
      return NULL;
    }
  }
  rootd = calloc(1, strlen(buf) + 1 + strlen(dodo_subdir) + 1);
  strcpy(rootd, buf);
  if(!hasterm(rootd)) strcat(rootd, "/");
  strcat(rootd, dodo_subdir);

  if(!(mkdir(rootd, 0700), (dir=opendir(rootd)),closedir(dir),dir)) {
    printf("<ERR> Could not use or create the default directory, %s.  Exiting.\n", rootd);
    free(rootd);
    return NULL;
  }

  DODO_ROOT = rootd;
  return rootd;
}

char* DDListGetMakeStash() {
  static char dodo_stash[] = "stash/";
  char *stashd;
  DIR* dir;
  if(!DODO_ROOT) {
    if(!DDListGetMakeRoot()) return NULL;
  }
  stashd = calloc(1,strlen(DODO_ROOT) + 1 + strlen(dodo_stash) + 1);
  strcpy(stashd, DODO_ROOT);
  if(!hasterm(DODO_ROOT)) strcat(DODO_ROOT, "/");
  strcat(stashd, dodo_stash);

  // Check to make sure it exists
  if(!(mkdir(stashd, 0700), (dir=opendir(stashd)),closedir(dir),dir)) {
    printf("<ERR> Could not create or open stash directory (%s)\n", stashd);
    free(stashd);
    return NULL;
  }

  DODO_STASH = stashd;
  return stashd;
}

void DDListToFD(DDList* ddl, int fd) {
  struct tm tm_dt = {0};
  char _dt[32] = {0}; char* dt = _dt;
  for(int i=0; i<ddl->n; i++) {
    // this date stuff is dumb!
    sprintf(dt, "%ld", ddl->nodes[i].dt);
    strptime(dt,     "%s",         &tm_dt);
    strftime(dt, 32, "%y%m%d%H%M", &tm_dt);
    dprintf(fd, "%*s%s {%s,%s} %s\n", 2*ddl->depth[i], "", "*", dt, ddl->nodes[i].mods, ddl->nodes[i].desc);
  }
}

size_t DDListToFile(DDList* ddl, char* name) {
  static char dodo_temp[] = "DODO_TEMP_FILE_PLEASE_IGNORE";
  int fd = -1, l=0;
  size_t sz_pl = 0;
  char *fpath = NULL, *ftemp = NULL;
  unsigned char *payload = NULL, *p = NULL;
  if(!DDListGetMakeStash()) return 0;
  fpath = calloc(1, strlen(DODO_STASH) + 1 + strlen(name) + 1);
  strcpy(fpath, DODO_STASH);
  if(!hasterm(DODO_STASH)) strcat(fpath, "/");
  strcat(fpath, name);

  // We have data to serialize, but we don't want to step on anyone who might
  // be reading the data.  We'll unlink then reopen, but we want to avoid the
  // situation where we have permissions to unlink, but not re-create.
  // We'll create and delete a canary file first, which presumes that the mode
  // of the stash directory isn't changing within this window...
  ftemp = calloc(1,strlen(DODO_STASH) + 1 + strlen(dodo_temp) + 1);
  strcpy(ftemp, DODO_STASH);
  if(!hasterm(DODO_STASH)) strcat(ftemp, "/");
  strcat(ftemp, dodo_temp);
  if(-1==( (fd = open(ftemp, O_CREAT, S_IRUSR | S_IWUSR)),close(fd),fd) ) {
    printf("<ERR> I cannot create the stash for %s.\n", fpath);
    free(ftemp);
    free(fpath);
    return 0;
  }

  unlink(ftemp);
  unlink(fpath);
  if(-1==(fd = open(fpath, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR))) {
    free(payload);
    printf("<ERR> I cannot open the stash for %s.\n", fpath);
    return 0;
  }

  DDListToFD(ddl, fd);
  close(fd);
  return sz_pl;
}

DDList* FileToDDList(char* name) {
  static char T[256] = {0};T['*']=T['-']=T['+']=T['.']=T['>']=T['<']=T['#']=T[' ']=T['\t']=1;
  DDList* ddl;
  char* fpath = NULL;
  unsigned char *pi = NULL, *pf = NULL;
  int fd = -1;
  struct stat sa = {0};
  time_init();
  if(!DDListGetMakeStash()) return NULL;

  fpath = calloc(1, strlen(DODO_STASH) + 1 + strlen(name) + 1);
  strcpy(fpath, DODO_STASH);
  if(!hasterm(DODO_STASH)) strcat(fpath, "/");
  strcat(fpath, name);
  if(stat(fpath, &sa)) {
    printf("<ERR> Problem stat()ing the stash for %s.\n", fpath);
    return NULL;
  }
  if(-1==(fd = open(fpath, 0, S_IRUSR | S_IWUSR))) {
    printf("<ERR> Problem opening the stash for %s.\n", fpath);
    return NULL;
  }
  pi = mmap(NULL, sa.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  pf = &pi[sa.st_size-1];
  close(fd);

  // Do a first pass to figure out how many nodes there are.
  unsigned char *p1 = pi, *p2 = pi;
  unsigned int nl_num = 0;
  while(p1 <= pf)
    nl_num += '\n' == *p1++;
  p1=pi;

  // Process line-by-line
  ddl        = calloc(1,sizeof(DDList));
  ddl->nodes = calloc(nl_num, sizeof(DDNode));
  ddl->depth = calloc(nl_num, sizeof(unsigned char));
  ddl->name  = strdup(name);
  unsigned int indent[MAX_INDENT] = {0};  // ancestor indent amounts
  int id       = 0;   // current indentation level
  int il       = -1;  // current line number (inc before, so negative)

  while(++il,p1 < pf) {
    int d = 0;
    while(isspace(*p1) && p1<=pf) {p1++;d++;} // skip spaces
    if(T[*p1]) while(T[*p1] && p1<=pf) p1++;  // bullet marks front of line
    p2 = p1;                                  // catch p2 up to p1
    ddl->nodes[il].dt = tt_now;               // set default time

    // Handle indentation
    // The indentation rule is as follows:
    // * inward indentations, no matter the depth, are worth 1
    // * outward indentations are matched as far as possible
    if(!il) indent[id] = d; // First line is always a top-level node
    else if(d>indent[id]) {indent[id+=id<MAX_INDENT]=d;}  // Only indent once inward
    else {
      while(d<indent[id] && id>0) indent[id--]=0; // handle non-root by backtracking
      if(d<indent[id]) indent[id]=d;              // handle the root case
    }
    ddl->depth[il] = id;

    // Check to see if line has metadata (only if matching {}
    char *meta = NULL, *mb = NULL;
    if('{' == *p1) {
      while('}' != *p2 && '\n' != *p2 && p2<=pf) p2++;  // until }, EOR, EOF
      if('}' == *p2) meta = p1+1;                       // proceed to get meta
    }
    if(meta) {
      // Process time
      struct tm tmt = {0};
      if(mb = strptime(meta, "%y%m%d%H%M", &tmt)) {
        meta=mb;
        ddl->nodes[il].dt = mktime(&tmt);
      }

      // Process flags
      memset(ddl->nodes[il].mods, 0, MAX_MODS+1);
      if(',' == *meta) meta++;
      for(int im=0; '}' !=*meta && im<MAX_MODS;im++) {
        if('$' == *meta) ddl->nodes[il].done = 1;
        if('?' == *meta) ddl->nodes[il].type = 1;
        ddl->nodes[il].mods[im] = *meta++;
      }
      p1=++p2;  // skip metadata for payload
      while(isspace(*p1)) p1++;                 // skip any pre-payload spaces
    }
    p2=p1;

    // The rest of the line is the payload
    while('\n' != *p2) p2++;
    ddl->nodes[il].desc = calloc(1+p2-p1,1);
    memcpy(ddl->nodes[il].desc, p1, p2-p1);
    p1=++p2; // next line
  }

  ddl->n = il;
  munmap(pi, sa.st_size);
  return ddl;
}


/******************************************************************************\
|                        DDNode, DDList Print Functions                        |
\******************************************************************************/
void printn(DDNode* dn, int depth) {
  char bg=9, fg=9, sp=0; int  at=0;
  for(int i=0; i<MAX_MODS; i++) {
    switch(dn->mods[i]) {
      // TODO this is dumb
      // lowercase => fg
      case 'k': fg = 0;        break;
      case 'r': fg = 1;        break;
      case 'g': fg = 2;        break;
      case 'y': fg = 3;        break;
      case 'b': fg = 4;        break;
      case 'm': fg = 5;        break;
      case 'c': fg = 6;        break;
      case 'w': fg = 7;        break;
      // uppercase => bg
      case 'K': bg = 0;        break;
      case 'R': bg = 1;        break;
      case 'G': bg = 2;        break;
      case 'Y': bg = 3;        break;
      case 'B': bg = 4;        break;
      case 'M': bg = 5;        break;
      case 'C': bg = 6;        break;
      case 'W': bg = 7;        break;
      // numerics => display attributes
      case '0': at  = 0;       break;
      case '1': at |= 1<<0;    break;
      case '2': at |= 1<<1;    break;
      case '4': at |= 1<<2;    break;
      case '5': at |= 1<<3;    break;
      case '7': at |= 1<<4;    break;
      // others => special decorators
      case '$': sp |= SP_DONE; break;  // done
      case '!': sp |= SP_HIPR; break;  // hipri
      case '#': sp |= SP_STAR; break;  // star
      case '@': sp |= SP_LOVE; break;  // heart
      case '?': sp |= SP_NOTE; break;  // note (not a todo)
    }
  }

  char* front = SP_NOTE & sp ? sp_note :
                SP_DONE & sp ? sp_done :
                SP_HIPR & sp ? sp_hipr :
                SP_STAR & sp ? sp_star :
                SP_LOVE & sp ? sp_love : sp_todo;
  char _mods[32] = {0}; char* mods = _mods;
  strcat(mods, A[at]); strcat(mods, F[fg]); strcat(mods, B[bg]);

  printf("%*s%s %s%s"CLS" "AT(2)"%dd"CLS"\n", depth, "", front, mods, dn->desc, days_diff(tt_now, dn->dt));
}

void printd(DDList* ddl) {
  int n_todo=0, n_done=0;
  for(int i=0; i<ddl->n; i++) {
    n_todo += 0==ddl->nodes[i].type;
    n_done += 1==ddl->nodes[i].done;
  }

  printf("\33[4m%s\33[0m\t[%d/%d]\n\n", ddl->name, n_done, n_todo);

  for(int i=0; i<ddl->n; i++)
    printn(&ddl->nodes[i], 1+2*ddl->depth[i]);
}


/******************************************************************************\
|                                  Entrypoint                                  |
\******************************************************************************/
int main(int argc, char *argv[]) {
  if(argc < 2) return -1;
  DDList* ddl = FileToDDList(argv[1]);
  printd(ddl);
  DDListToFile(ddl, argv[1]);
  return 0;
}
