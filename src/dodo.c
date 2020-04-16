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
#include <getopt.h>


/******************************************************************************\
|                                   Version                                    |
\******************************************************************************/
#define DD_MAJOR 0
#define DD_MINOR 5
static int dd_major = DD_MAJOR, dd_minor = DD_MINOR;


/******************************************************************************\
|                                   Globals                                    |
\******************************************************************************/
#define MAX_INDENT 16
#define MAX_MODS   5

char* DODO_ROOT  = NULL;
char* DODO_STASH = NULL;


/******************************************************************************\
|                                   Helpers                                    |
\******************************************************************************/
time_t         tt_now     = {0};  // hold regular unix time
void time_init() {tt_now = time(NULL);}

static inline int days_diff(time_t a, time_t b) {
  return (a>b ? a-b : b-a)/86400;
}

static inline char hasterm(char* path) {return '/' == path[strlen(path)-1];}

void print_help() {
  static char help[] = ""
"Usage: dodo [OPTION]... LIST\n"
"Display a todo list\n"
"Example: dodo foo\n"
"\n"
"Miscellaneous:\n"
"  -v, --version         display version information and exit\n"
"  -h, --help            display this help and exit\n"
"  -q, --quiet           do not display output\n"
"  -l, --list            display the list of valid todos and exit\n"
"\n"
"Display:\n"
"  -E, --noextra         do not show tagged items (.e.g., heart, star)\n"
"  -N, --nonote          do not show notes\n"
"  -T, --notodo          do not show uncompleted todo items\n"
"  -K, --nokids          do not show children\n"
"  -A, --plaintext       do not print with colors or other attributes\n"
"  -L, --literal         print the results using dodo's native format\n"
"\n"
"Input/Output:\n"
"  -s, --sort            asks the user questions in order to sort entries\n"
"  -o, --output          write to the specified location instead of the stash\n"
"                        given by LIST.  Ignored if -d also given.\n"
"  -d, --dry_run         do not save output (even if specified)\n"
"  -e, --edit            open the specified stash with $EDITOR, $DODO_EDITOR\n"
"\n"
"Environment Variables\n"
"dodo inspects the DODO_ROOT environment variable to look for configuration\n"
"files and data, such as the stashes (saved todo lists which can be tracked by\n"
"name).  This directory must exist, be writable by the current user, and be\n"
"represented as an absolute path."
"\n\n"
"Todo Lists\n"
"A todo list is a newline-delimited file whose entries are indented lines\n"
"beginning with an optional bullet (*, -, +, etc).  By default, dodo will look\n"
"in its stash directory (the .dodo/stash subdirectory of the current user's home)\n"
"for todo lists, unless the LIST is contains a slash--eg foo/mylist for a\n"
"relative path or ./mylist for a file in the local directory.  Upon reading a\n"
"list, dodo will look for optional line-level metadata, such as an entry\n"
"timestamp, and display the list according to the specified display parameters.\n"
"It will then overwrite the list with the additional metadata.\n"
"\n"
"For example, the following list:"
"  * write useful todo documentation\n"
"    * add some subtasks\n"
"    * think of a witty joke\n"
"  * send holiday thank-you cards\n"
"\n"
"will be overwritten by:\n"
"  * {2001062201,} write useful todo documentation\n"
"    * {2001062201,} add some subtasks\n"
"    * {2001062201,} think of a witty joke\n"
"  * {2001062201,} send holiday thank-you cards\n"
"\n"
"The text in the curly brackets indicate the entry date in YYMMDDhhmm format,\n"
"followed by modifiers that alter the display or behavior of the item.\n"
"\n"
"The modifiers are:\n"
"krgybmcw - black, red, green, yellow, blue, magenta, cyan, white text\n"
"KRGYBMCW - same colors, but the background\n"
"12457 - bright, dim, underscore, blink, reverse\n"
"!@# - show a !, heart, or star\n"
"? - item is a note\n"
"$ - item is complete"
"\n\n"
"dodo supports input lists streamed from stdin.  In that case, the LIST gives the\n"
"output location (equivalently, -o can be used).  If no LIST or -o is given,\n"
"dodo will merely print the results to stdout.\n";
  printf("%s\n",help);

}

void print_version() { printf("dodo %d.%d\n", dd_major, dd_minor); }

char* DDGetMakeRoot() {
  static char dodo_subdir[] = ".dodo/";
  char *rootd = NULL, *buf = NULL;
  DIR* dir;

  // Check whether the user defined the DODO_ROOT env var
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

char* DDGetMakeStash() {
  static char dodo_stash[] = "stash/";
  char *stashd;
  DIR* dir;
  if(!DODO_ROOT) {
    if(!DDGetMakeRoot()) return NULL;
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

char edit_file(char* arg) {
  char* dodo_editor = getenv("DODO_EDITOR") ? getenv("DODO_EDITOR") : getenv("EDITOR");
  char *filename, *cmd;
  if(!dodo_editor) {
    printf("<ERR> No editor specified in EDITOR or DODO_EDITOR.\n");
    return -1;
  }

  // Build the filename
  if(strchr(arg, '/')) {   // if it's a path, use the exact file
    filename = arg;
  } else {                 // if not, use the name as a base
    if(!DDGetMakeStash()) return -1;  // Maybe an error later?
    filename = calloc(1, strlen(DODO_STASH) + 1 + strlen(arg) + 1 );
    strcpy(filename, DODO_STASH);
    if(!hasterm(DODO_STASH)) strcat(filename, "/");
    strcat(filename, arg);
  }

  // Build the command string
  cmd = calloc(1,strlen(dodo_editor) + 1 + strlen(filename) + 1);
  strcpy(cmd, dodo_editor);
  strcat(cmd, " ");
  strcat(cmd, filename);
  system(cmd);
  if(!strchr(arg, '/')) free(filename);
  return 0;
}

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
  SP_TODO  = 0,
  SP_DONE  = 1<<0,
  SP_HIPR  = 1<<1,
  SP_STAR  = 1<<2,
  SP_LOVE  = 1<<3,
  SP_NOTE  = 1<<4,
  SP_EXTRA = (SP_HIPR | SP_STAR | SP_LOVE)
} sp_type;

static char sp_todo[] = FG(5)"□"FG(9);
static char sp_done[] = FG(2)"✓"FG(9);
static char sp_hipr[] = FG(6)"!"FG(9);
static char sp_star[] = FG(3)"★"FG(9);
static char sp_love[] = FG(1)"♡"FG(9);
static char sp_note[] = FG(4)"•"FG(9);


/******************************************************************************\
|                              Preference Globals                              |
\******************************************************************************/
static char DD_show_extra    = 1;
static char DD_show_note     = 1;
static char DD_show_todo     = 1;
static char DD_show_done     = 1;
static char DD_show_children = 1;
static char DD_plaintext     = 0;
static char DD_quiet         = 0;
static char DD_literal       = 0;
static char DD_edit          = 0;


/******************************************************************************\
|                                DDNode, DDList                                |
\******************************************************************************/
typedef struct DDNode {
  char            done;
  char            type;
  time_t          dt;
  char            mods[MAX_MODS+1];
  char*           desc;
  struct DDNode** nodes;
  unsigned int    n;
} DDNode;

typedef struct DDList {
  char*          name;
  DDNode         root;
} DDList;

int DDNodeAddNode(DDNode* p, DDNode* c) {
  p->nodes = realloc(p->nodes, (p->n+1)*sizeof(DDNode*));
  p->nodes[p->n] = c;
  return ++p->n;
}

void DDNodeToFD(DDNode* node, int depth, int fd) {
  struct tm tm_dt = {0};
  char _dt[32] = {0}; char* dt = _dt;
  sprintf(dt,  "%ld", node->dt);
  strptime(dt, "%s",  &tm_dt);
  strftime(dt, 32, "%y%m%d%H%M", &tm_dt);
  dprintf(fd, "%*s%s {%s,%s} %s\n", 2*depth, "", "*", dt, node->mods, node->desc);
  for(int i=0; i<node->n; i++)
    DDNodeToFD(node->nodes[i], depth+1, fd);
}

char DDListToFile(DDList* ddl, char* arg) {
  int fd = -1, rc = 0;
  char *name = NULL, *fpath = NULL, *ftemp = NULL;

  if(strchr(arg, '/')) {   // if it's a path, use the exact file
    name = strrchr(arg, '/')+1;
    fpath = name;
    ftemp = calloc(1, strlen(fpath) + 1 + 6 + 1);
  } else {                 // if not, use the name as a base
    name = arg;
    if(!DDGetMakeStash()) return 0;
    fpath = calloc(1, strlen(DODO_STASH) + 1 + strlen(name) + 1 );
    ftemp = calloc(1, strlen(DODO_STASH) + 1 + strlen(name) + 1 + 6 + 1);

    // Build the directory tree for the stash
    strcpy(fpath, DODO_STASH);
    if(!hasterm(DODO_STASH)) strcat(fpath, "/");
    strcat(fpath, name);
  }

  // Build the temp filename based on stash path
  strcpy(ftemp, fpath);
  strcat(ftemp, ".XXXXXX");

  // Open the temp file
  if(-1==((fd=mkstemp(ftemp)))) {
    printf("<ERR> I cannot create the stash for %s.\n", fpath);
    if(strchr(arg, '/')) free(fpath);
    free(ftemp);
    rc = -1;
    goto DDLTOF_CLEANUP;
  }

  // Populate new temp file
  for(int i=0; i<ddl->root.n; i++)
    DDNodeToFD(ddl->root.nodes[i], 0, fd);

  // Rename to the desired path and we're done
  if(-1==(rename(ftemp, fpath))) {
    printf("<ERR> I cannot finalize the stash for %s.\n", fpath);
    if(strchr(arg, '/')) free(fpath);
    free(ftemp);
    rc = -1;
  }

DDLTOF_CLEANUP:
  close(fd);
  unlink(ftemp);
  if(strchr(arg, '/')) free(fpath);
  free(ftemp);
  return rc;
}

void CharToDDList(DDList* ddl, unsigned char* pi, unsigned char* pf) {
  static char T[256] = {0};T['*']=T['-']=T['+']=T['.']=T['>']=T['<']=T['#']=T[' ']=T['\t']=1;
  time_init();

  // Do a first pass to figure out how many nodes there are.
  unsigned char *p1 = pi, *p2 = pi;
  unsigned int nl_num = 0;
  while(p1 <= pf)
    nl_num += '\n' == *p1++;
  p1=pi;

  // Process line-by-line
  unsigned int indent[MAX_INDENT]    = {0};  // ancestor indent amounts
  DDNode*      parents[MAX_INDENT+1] = {0};  parents[0] = &ddl->root;
  int id       = 0;   // current indentation level
  int il       = -1;  // current line number (inc before, so negative)

  while(++il,p1 < pf) {
    DDNode* node = calloc(1,sizeof(DDNode));
    int d = 0;
    while(isspace(*p1) && p1<=pf) {p1++;d++;} // skip spaces
    if(T[*p1]) while(T[*p1] && p1<=pf) p1++;  // bullet marks front of line
    p2 = p1;                                  // catch p2 up to p1
    node->dt = tt_now;                        // set default time

    // Handle indentation
    // The indentation rule is as follows:
    // * inward indentations, no matter the depth, are worth 1
    // * outward indentations are matched as far as possible
    if(!il) indent[id] = d; // First line is always a top-level node
    else if(d>indent[id]) {indent[id+=id<MAX_INDENT]=d;} // Only indent once inward
    else {
      while(d<indent[id] && id>0) indent[id--]=0; // handle non-root by backtracking
      if(d<indent[id]) indent[id]=d;              // handle the root case
    }
    parents[id+1] = node;                         // I am my child's parent
    DDNodeAddNode(parents[id], node);

    // Check to see if line has metadata (only if matching {}
    unsigned char *meta = NULL, *mb = NULL;
    if('{' == *p1) {
      while('}' != *p2 && '\n' != *p2 && p2<=pf) p2++;  // until }, EOR, EOF
      if('}' == *p2) meta = p1+1;                       // proceed to get meta
    }
    if(meta) {
      // Process time
      struct tm tmt = {0};
      if((mb = (unsigned char*)strptime((char*)meta, "%y%m%d%H%M", &tmt))) {
        meta=mb;
        node->dt = mktime(&tmt);
      }

      // Process flags
      if(',' == *meta) meta++;
      for(int im=0; '}' !=*meta && im<MAX_MODS;im++) {
        if('$' == *meta) node->done = 1;
        if('?' == *meta) node->type = 1;
        node->mods[im] = *meta++;
      }
      p1=++p2;  // skip metadata for payload
      while(isspace(*p1)) p1++;                 // skip any pre-payload spaces
    }
    p2=p1;

    // The rest of the line is the payload
    while('\n' != *p2) p2++;
    node->desc = calloc(1+p2-p1,1);
    memcpy(node->desc, p1, p2-p1);
    p1=++p2; // next line
  }
}

DDList* FileToDDList(char* arg) {
  char *fpath = NULL, *name = NULL;
  unsigned char *pi = NULL, *pf = NULL;
  int fd = -1;
  struct stat sa = {0};

  if(strchr(arg, '/')) {     // it's a path, so open the exact file
    fpath = arg;
    name = strrchr(arg, '/')+1;
  } else {                   // it's a name, so refer to the stash
    name = arg;
    if(!DDGetMakeStash()) return NULL;
    fpath = calloc(1, strlen(DODO_STASH) + 1 + strlen(name) + 1);
    strcpy(fpath, DODO_STASH);
    if(!hasterm(DODO_STASH)) strcat(fpath, "/");
    strcat(fpath, name);
  }

  if(stat(fpath, &sa)) {
    printf("<ERR> Problem stat()ing the stash for %s.\n", fpath);
    if(strchr(arg, '/')) free(fpath);
    return NULL;
  }
  if(-1==(fd = open(fpath, 0, S_IRUSR | S_IWUSR))) {
    printf("<ERR> Problem opening the stash for %s.\n", fpath);
    if(strchr(arg, '/')) free(fpath);
    return NULL;
  }
  if(!strchr(arg, '/')) free(fpath);
  pi = mmap(NULL, sa.st_size, PROT_READ, MAP_PRIVATE, fd, 0); // map private -> CoW per page?
  pf = &pi[sa.st_size-1];
  close(fd);

  DDList* ddl = calloc(1,sizeof(DDList));
  CharToDDList(ddl, pi, pf);
  ddl->name  = strdup(name);
  munmap(pi, sa.st_size);
  return ddl;
}

DDList* StdinToDDList(char* name) {
  // TODO optimize the reader so that it can be chunked
  DDList* ddl = calloc(1,sizeof(DDList));
  size_t sz_buf = 4*4096, n=0, m=0;
  unsigned char* pi = calloc(1,sz_buf), *pf = pi;

  while(0<(n=read(0, pi+m, sz_buf-n))) {
    sz_buf *= 2;
    m += n;
    pi = realloc(pi, sz_buf);
    pf = m + pi;
  }

  CharToDDList(ddl, pi, pf);
  ddl->name  = strdup(name);
  free(pi);
  return ddl;
}


/******************************************************************************\
|                        DDNode, DDList Print Functions                        |
\******************************************************************************/
void DDListToStdout(DDList* ddl) {
  for(int i=0; i<ddl->root.n; i++)
    DDNodeToFD(ddl->root.nodes[i], 0, 1);
}

void DDNodeToTextPrint(DDNode* dn, int depth) {
  static char fcord[] = "krgybmcw";
  static char bcord[] = "KRGYBMCW";
  static char acord[] = "012457}}";
  static char scord[] = "$!#@?}}}"; static int sval[] = {SP_DONE, SP_HIPR, SP_STAR, SP_LOVE, SP_NOTE};
  char bg=9, fg=9, sp=0; int at=0,j=0,i=0;
  for(i=0; i<MAX_MODS; i++) {
    for(j=0; j<8; j++) {
      if(fcord[j] == dn->mods[i]) fg=j;
      if(bcord[j] == dn->mods[i]) bg=j;
      if(acord[j] == dn->mods[i]) at|=1<<j;
      if(scord[j] == dn->mods[i]) sp|=sval[j];
    }
  }

  if(!DD_show_note  && (SP_NOTE & sp)) return;
  if(!DD_show_done  && (SP_DONE & sp)) return;
  if(!DD_show_todo  && SP_TODO == sp)  return;
  if(!DD_show_extra && ((SP_HIPR | SP_STAR | SP_LOVE) & sp)) return;
  char* front = SP_NOTE & sp ? sp_note :
                SP_DONE & sp ? sp_done :
                SP_HIPR & sp ? sp_hipr :
                SP_STAR & sp ? sp_star :
                SP_LOVE & sp ? sp_love : sp_todo;
  char _mods[32] = {0}; char* mods = _mods;
  strcat(mods, A[at]); strcat(mods, F[fg]); strcat(mods, B[bg]);

  if(DD_plaintext)
    printf("%*s %s %dd\n", depth, "", dn->desc, days_diff(tt_now, dn->dt));
  else
    printf("%*s%s %s%s"CLS" "AT(2)"%dd"CLS"\n", depth, "", front, mods, dn->desc, days_diff(tt_now, dn->dt));

  if(DD_show_children)
    for(int i=0; i<dn->n; i++)
      DDNodeToTextPrint(dn->nodes[i], depth+2);
}

void DDListPrint(DDList* ddl) {
  int n_todo=0, n_done=0;
  for(int i=0; i<ddl->root.n; i++) {
    n_todo += 0==ddl->root.nodes[i]->type;
    n_done += 1==ddl->root.nodes[i]->done;
  }

  printf(" \33[4m%s\33[0m\t[%d/%d]\n\n", ddl->name, n_done, n_todo);
  for(int i=0; i<ddl->root.n; i++)
    DDNodeToTextPrint(ddl->root.nodes[i], 1);
}

int DDNodeUserCompare(const void* _a, const void* _b) {
  const DDNode *a = *(DDNode**)_a, *b = *(DDNode**)_b;
  char c = 0;
  if(a->done) return 1;       // Skip if marked done
  printf("a) %s\nb) %s\nx) either\n",a->desc,b->desc);
  while(c!='a' && c!='b' && c!='x')
    c = (-1==read(STDIN_FILENO, &c, 1)) ? 0 : tolower(c);
  return 'x' == c ? 0 :
         'a' == c ? -1 : 1;
}

void DDListSort(DDList* ddl) {
  void qsort(void *base, size_t nmemb, size_t size,
                 int (*compar)(const void *, const void *));
  printf("Select the item which should be done first.\n");
  qsort((void*)ddl->root.nodes, ddl->root.n, sizeof(DDNode*), DDNodeUserCompare);
}

char print_summary() {
  char* stash = DDGetMakeStash();
  DIR* dir = opendir(stash);
  struct dirent* d;
  int n_done=0, n_todo=0;
  DDList* ddl;
  if(!dir) return -1;
  printf("Lists:\n");
  while((d=readdir(dir))) {
    if(!strcmp(".",d->d_name) || !strcmp("..",d->d_name)) continue;
    printf("%s", d->d_name);
    if(!(ddl=FileToDDList(d->d_name))) {
      printf(": <ERR> could not read\n");
    } else {
      n_done = n_todo = 0;
      for(int i=0; i<ddl->root.n; i++) {
        n_todo += 0==ddl->root.nodes[i]->type;
        n_done += 1==ddl->root.nodes[i]->done;
      }
      free(ddl);
      printf(" [%d/%d]\n", n_done, n_todo);
    }
  }
  return 0;
}


/******************************************************************************\
|                                  Entrypoint                                  |
\******************************************************************************/
typedef enum DDPost {
  DDP_SORT   = 1<<0,
  DDP_DRY    = 1<<1,
  DDP_EXPORT = 1<<2,
} DDPost;

int main(int argc, char *argv[]) {
  int c = 0, oi = 1, rc = 0;
  unsigned int post = 0;
  char *oarg = NULL;
  DDList* ddl;
  struct stat sa = {0};
  static struct option lopts[] = {
    {"output", required_argument, 0, 'o'},
    {"help",         no_argument, 0, 'h'},
    {"version",      no_argument, 0, 'v'},
    {"list",         no_argument, 0, 'l'},
    {"quiet",        no_argument, 0, 'q'},
    {"sort",         no_argument, 0, 's'},
    {"dry_run",      no_argument, 0, 'd'},
    {"noextra",      no_argument, 0, 'E'},
    {"nonote",       no_argument, 0, 'N'},
    {"nodone",       no_argument, 0, 'D'},
    {"notodo",       no_argument, 0, 'T'},
    {"nokids",       no_argument, 0, 'K'},
    {"noattr",       no_argument, 0, 'A'},
    {"literal",      no_argument, 0, 'L'},
    {0}};

  while(-1!=(c=getopt_long(argc, argv, "+i:o:hvqsdleENDTKAL", lopts, &oi))) {
    switch(c) {
    case 'o': oarg = optarg;              break;
    case 'h': print_help();               return 0;
    case 'v': print_version();            return 0;
    case 'l': print_summary();            return 0;
    case 'q': DD_quiet = 1;               break;
    case 's': post |= DDP_SORT;           break;
    case 'd': post |= DDP_DRY;            break;
    case 'e': DD_edit = 1;                break;
    case 'E': DD_show_extra= 0;           break;
    case 'N': DD_show_note = 0;           break;
    case 'D': DD_show_done = 0;           break;
    case 'T': DD_show_todo = 0;           break;
    case 'K': DD_show_children = 0;       break;
    case 'A': DD_plaintext = 1;           break;
    case 'L': DD_literal= 1;              break;
    default: printf("Unrecognized option %s\n",argv[oi]); return -1;
    }
  }

  // Handle input.  If we get something on stdin and we have no name and no
  // output, then it's effectively a dry run.
  fstat(0, &sa);
  if(sa.st_mode & S_IFIFO) {
    if(!(ddl = StdinToDDList("New Todo")))  return -1;
    if(optind == argc && !oarg) post|=DDP_DRY;
  } else if(1==argc) {
    return print_summary();
  } else {
    if(DD_edit) edit_file(argv[argc-1]);
    if(!(ddl = FileToDDList(argv[argc-1]))) return -1;
  }

  if(post & DDP_SORT)       DDListSort(ddl);
  if(!DD_quiet)             DD_literal ? DDListToStdout(ddl) : DDListPrint(ddl);
  if(!(post & DDP_DRY))  rc=DDListToFile(ddl, oarg ? oarg : argv[argc-1]);
  return rc;
}
