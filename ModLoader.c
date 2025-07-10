/*
** ModLoader.c
** Loads custom DLL libraries to modify TA tools
*/

#include <ctype.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#error _WIN32 not defined
#endif

#define CSV_FILE_MUST_EXIST 0
#define VERBOSE_INFO 1

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define streq(s1, s2) (_stricmp(s1, s2) == 0)

/*
SYSTEM ERROR HANDLING
*/

/* fatal error handler: print user and system error message and exit */
static void win_error (const char *fmt, ...) {
  va_list ap;
  DWORD error = GetLastError();
  LPSTR msg = NULL;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                 FORMAT_MESSAGE_FROM_SYSTEM |
                 FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL,
                 error,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPSTR)&msg,
                 0,
                 NULL);
  fprintf(stderr, "ERROR: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, ": %s\n", (char *)msg);
  LocalFree(msg);
  ExitProcess(1);
}

static void c_error (const char *fmt, ...) {
  va_list ap;
  const char *serr = strerror(errno);
  va_start(ap, fmt);
  fprintf(stderr, "ERROR: ");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, ": %s", serr);
  exit(EXIT_FAILURE);
}

static void printinfo (const char *fmt, ...) {
  if (VERBOSE_INFO) {
    va_list ap;
    fprintf(stderr, "MODLOADER: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
  }
}

/*
TA_TOOLS CONFIG FILE LANGUAGE
CSV FILE assigns DLL files to programs:
<program>,<DLL>

example:
linker,linker_lua.dll
ape,C:/path/to/dll
*,tools_mod.dll
The above line loads the dll for any program
*/

/* open file NAME in current directory or TA_TOOLS_PATH/bin */
static FILE *openconfigfile (const char *name) {
  FILE *f;
  /* first try current directory */
  f = fopen(name, "r");
  if (f == NULL) {
    /* then try TA_TOOLS_PATH */
    const char *toolspath = getenv("TA_TOOLS_PATH");
    if (toolspath != NULL) {
      char *buf = malloc(strlen(toolspath) + sizeof("bin/") + strlen(name)+1);
      if (buf == NULL)
        c_error("malloc failed");
      strcpy(buf, toolspath);
      strcat(buf, "bin/");
      strcat(buf, name);
      /* try to open again */
      f = fopen(buf, "r");
      if (CSV_FILE_MUST_EXIST && f == NULL) {
        c_error("cannot open '%s'", buf);
      }
      printinfo("opened '%s'", buf);
      free(buf);
    }
  }
  else
    printinfo("opened '%s'", name);
  return f;
}

#define MAX_LIBS 64

static char *libs [MAX_LIBS];
static int nlibs;

typedef struct {
  const char *progname;
  const char *file;
  int line;
} ParseInfo;

/* input error */
static void in_error (const ParseInfo *pi, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "%s: line %d: ", pi->file, pi->line);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

static void strip_space (char **start, char **end) {
  while (isspace(**start))
    (*start)++;
  while (isspace((*end)[-1]))
    (*end)--;
}

static int checkmatch (ParseInfo *pi, char *start, char *end) {
  int c, matched;
  strip_space(&start, &end);
  c = *end;
  *end = 0;
  printinfo("line %d: '%s' provided", pi->line, start);
  /* "*" matches with any progname */
  matched = streq(start, "*") || streq(start, pi->progname);
  if (matched)
    printinfo("line %d: found matching argument: '%s'", pi->line, start);
  *end = c;
  return matched;
}

static void loadconfigline (ParseInfo *pi, const char *line) {
  int ll;  /* line length */
  char buf [1024];
  /* format: '<program>|<program>|...,<DLL TO LOAD>' */
  char *nextcol, *q, *p = strchr(line, ',');
  if (p == NULL) return;
  printinfo("");
  printinfo("line %d: '%s'", pi->line, line);
  nextcol = p + 1;
  ll = p - line;
  strncpy(buf, line, ll);
  /* get program names delimited by '|' to see if they match PROGNAME */
  for (p = buf; (q = strchr(p, '|')) != NULL; p = q + 1)
    if (checkmatch(pi, p, q)) goto match;
  /* check final progname in column 0 */
  if (!checkmatch(pi, buf, buf + ll))
    return;
  match:
  /* matched progname: get the DLL in the next CSV column */
  p = strchr(nextcol, ',');
  p = p ? strncpy(buf, nextcol, p - nextcol) : nextcol;
  q = p + strlen(p);
  if (nlibs + 1 >= MAX_LIBS)
    in_error(pi, "too many libraries to load (exceeds %d)", MAX_LIBS);
  strip_space(&p, &q);
  *q = 0;
  libs[nlibs] = strdup(p);
  if (libs[nlibs] == NULL)
    c_error("strdup failed");
  printinfo("line %d: dll '%s' added to list", pi->line, libs[nlibs]);
  nlibs++;
}

static void loadconfig (const char *progname, const char *filename) {
  ParseInfo pi;
  char line [1024];  /* buffer for lines */
  FILE *f = openconfigfile(filename);
  int c, lc = 0;
  if (f == NULL) return;
  pi.progname = progname;
  pi.file = filename;
  pi.line = 1;
  for (;;) {
    c = getc(f);
    if (c == '\n' || c == EOF) {
      line[lc] = '\0';
      loadconfigline(&pi, line);
      pi.line++;
      if (c == EOF) break;
      lc = 0;
      continue;
    }
    if (lc + 1 >= (int)sizeof(line))
      in_error(&pi, "line too long (exceeds %d characters)",(int)sizeof(line));
    line[lc++] = c;
  }
  fclose(f);
}

static void loadlib (const char *progname, const char *libname) {
  void (*init_fn) (void);
  HMODULE h = LoadLibraryA(libname);
  if (h == NULL)
    win_error("Cannot load library '%s'", libname);
  fprintf(stderr,
          "MODLOADER: %s: Successfully loaded '%s'\n", progname, libname);
  init_fn = (void (*) (void))GetProcAddress(h, "init");
  if (init_fn) {
    printinfo("calling init() for '%s'", libname);
    (*init_fn)();
  }
}

static void loadlibs (const char *progname) {
  int i;
  for (i = 0; i < nlibs; i++) {
    printinfo("loading DLL '%s'", libs[i]);
    loadlib(progname, libs[i]);
    free(libs[i]);
  }
}

void ModLoader_Load (const char *progname) {
  loadconfig(progname, "ta_tools_config.csv");
  loadlibs(progname);
}
