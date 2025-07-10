/*
** RemoteLogger.c
** Implementation for RemoteLogger.dll
*/

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "RemoteLogger.h"

extern void ModLoader_Load (const char *progname);

/* called by host programs in their main function */
void RemoteLogger_Start(const char *program, const char *version,
                        const char *buildmachine, const char *buildtype)
{
  (void)version; (void)buildmachine; (void)buildtype;
  ModLoader_Load(program);
}

void RemoteLogger_Stop(void)
{
}

#ifdef REMOTE_LOGGER_HAVE_LOG
void RemoteLogger_Log(const char *event, const char *str)
{
  (void)event; (void)str;
}

void RemoteLogger_Logv(const char *event, const char *fmt, ...)
{
  (void)event; (void)fmt;
}
#endif /* REMOTE_LOGGER_HAVE_LOG */
