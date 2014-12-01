/*
 *  Formatted printf style routines for syslog() etc.
 *
 *  Copyright (c) 1997-2002 Gisle Vanem <giva@bgnett.no>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. All advertising materials mentioning features or use of this software
 *     must display the following acknowledgement:
 *       This product includes software developed by Gisle Vanem
 *       Bergen, Norway.
 *
 *  THIS SOFTWARE IS PROVIDED BY ME (Gisle Vanem) AND CONTRIBUTORS ``AS IS''
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL I OR CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>

#include "printk.h"

static char  hex_chars[] = "0123456789abcdef";

static const char *str_signal (int sig);
static const char *str_error (DWORD err, BOOL sys_err);

/*
 * snprintk - format a message into a buffer.  Like sprintf except we
 * also specify the length of the output buffer, and we handle
 * %m (errno message), %M (GetLastError() message), %t (current time).
 * %S signal and %I (IP address network order) formats.
 * Returns the number of chars put into buf.
 *
 * NB! Doesn't do floating-point formats and long modifiers.
 *     (e.g. "%lu").
 */
int snprintk (char *buf, int buflen, const char *fmt, ...)
{
  int     len;
  va_list args;
  va_start (args, fmt);

  len = vsnprintk (buf, buflen, fmt, args);
  va_end (args);
  return (len);
}


/*
 * Return current local date/time.
 */
const char *time_now (void)
{
  static char buf[50];
  SYSTEMTIME now;

  memset (&now, 0, sizeof(now));
  GetLocalTime (&now);
  _snprintf (buf, sizeof(buf)-1, "%04d-%02d-%02d %02d:%02d:%02d",
             now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
  return (buf);
}

static char *rip (char *s)
{
  char *p;

  if ((p = strrchr(s,'\n')) != NULL) *p = '\0';
  if ((p = strrchr(s,'\r')) != NULL) *p = '\0';
  return (s);
}

/*
 * Like snprintk(), but takes a va_list instead of a list of args.
 */
#define OUTCHAR(c)  (buflen > 0 ? (--buflen, *buf++ = (c)) : 0)

int vsnprintk (char *buf, int buflen, const char *fmt, va_list args)
{
  struct in_addr ia;
  int    c, i, n, width, prec, fillch;
  int    base, len, neg, quoted, upper;
  DWORD  val = 0L;
  BYTE  *p;
  char  *str, *f, *buf0 = buf, *_fmt = (char*)fmt;
  char   num[32];

  if (--buflen < 0)
     return (-1);

  while (buflen > 0)
  {
    for (f = _fmt; *f != '%' && *f != 0; ++f)
        ;
    if (f > _fmt)
    {
      len = f - _fmt;
      if (len > buflen)
          len = buflen;
      memcpy (buf, _fmt, len);
      buf    += len;
      buflen -= len;
      _fmt = f;
    }
    if (*_fmt == 0)
       break;
    c = *++_fmt;
    width = prec = 0;
    fillch = ' ';
    if (c == '0')
    {
      fillch = '0';
      c = *++_fmt;
    }
    if (c == '*')
    {
      width = va_arg (args, int);
      c = *++_fmt;
    }
    else
      while (isdigit(c))
      {
        width = width * 10 + c - '0';
        c = *++_fmt;
      }

    if (c == '.')
    {
      c = *++_fmt;
      if (c == '*')
      {
        prec = va_arg (args, int);
        c = *++_fmt;
      }
      else
      {
        while (isdigit(c))
        {
          prec = prec * 10 + c - '0';
          c = *++_fmt;
        }
      }
    }
    str   = NULL;
    base  = 0;
    neg   = 0;
    upper = 0;
    ++_fmt;

    switch (c)
    {
      case 'd':
           i = va_arg (args, int);
           if (i < 0)
           {
             neg = 1;
             val = -i;
           }
           else
             val = i;
           base = 10;
           break;

      case 'u':
           val  = va_arg (args, unsigned);
           base = 10;
           break;

      case 'o':
           val  = va_arg (args, unsigned int);
           base = 8;
           break;

      case 'X':
           upper = 1;
           /* fall through */
      case 'x':
           val  = va_arg (args, unsigned int);
           base = 16;
           break;

      case 'p':
           val   = (DWORD)va_arg (args,void*);
           base  = 16;
           neg   = 2;
           upper = 1;
           break;

      case 's':
           str = va_arg (args, char*);
           break;

      case 'S':
           str = (char*) str_signal (va_arg(args, int));
           break;

      case 'c':
           num[0] = va_arg (args, int);
           num[1] = 0;
           str = num;
           break;

      case 'm':
           str = (char*) str_error (errno, FALSE);
           break;

      case 'M':
           str = (char*) str_error (GetLastError(), TRUE);
           break;

      case 'I':
           ia.s_addr = va_arg (args, DWORD);
           str = inet_ntoa (ia);
           break;

      case 't':
           str = (char*) time_now();
           break;

      case 'v':                /* "visible" string */
      case 'q':                /* quoted string */
           quoted = c == 'q';
           p = va_arg (args, BYTE*);
           if (fillch == '0' && prec > 0)
              n = prec;
           else
           {
             n = strlen ((char*)p);
             if (prec > 0 && prec < n)
                n = prec;
           }
           while (n > 0 && buflen > 0)
           {
             c = *p++;
             --n;
             if (!quoted && c >= 0x80)
             {
               OUTCHAR ('M');
               OUTCHAR ('-');
               c -= 0x80;
             }
             if (quoted && (c == '"' || c == '\\'))
                OUTCHAR ('\\');
             if (c < 0x20 || (0x7F <= c && c < 0xA0))
             {
               if (quoted)
               {
                 OUTCHAR ('\\');
                 switch (c)
                 {
                   case '\t': OUTCHAR ('t');   break;
                   case '\n': OUTCHAR ('n');   break;
                   case '\b': OUTCHAR ('b');   break;
                   case '\f': OUTCHAR ('f');   break;
                   default  : OUTCHAR ('x');
                              OUTCHAR (hex_chars[c >> 4]);
                              OUTCHAR (hex_chars[c & 0xf]);
                 }
               }
               else
               {
                 if (c == '\t')
                    OUTCHAR (c);
                 else
                 {
                   OUTCHAR ('^');
                   OUTCHAR (c ^ 0x40);
                 }
               }
             }
             else
               OUTCHAR (c);
           }
           continue;

      default:
           *buf++ = '%';
           if (c != '%')
              --_fmt;        /* so %z outputs %z etc. */
           --buflen;
           continue;
    }

    if (base)
    {
      str = num + sizeof(num);
      *--str = 0;
      while (str > num + neg)
      {
        *--str = upper ? toupper(hex_chars[val % base]) : hex_chars [val % base];
        val = val / base;
        if (--prec <= 0 && val == 0)
           break;
      }
      switch (neg)
      {
        case 1: *--str = '-';
                break;
        case 2: *--str = 'x';
                *--str = '0';
                break;
      }
      len = num + sizeof(num) - 1 - str;
    }
    else
    {
      len = strlen (str);
      if (prec > 0 && len > prec)
         len = prec;
    }
    if (width > 0)
    {
      if (width > buflen)
          width = buflen;
      if ((n = width - len) > 0)
      {
        buflen -= n;
        for (; n > 0; --n)
            *buf++ = fillch;
      }
    }
    if (len > buflen)
        len = buflen;
    memcpy (buf, str, len);
    buf += len;
    buflen -= len;
  }
  *buf = '\0';
  return (buf - buf0);
}


/*
 * Return name for signal 'sig'
 */
#ifdef __HIGHC__
#undef SIGABRT   /* = SIGIOT */
#endif

static const char *str_signal (int sig)
{
  static char buf[20];

  switch (sig)
  {
    case 0:
         return ("None");
#ifdef SIGINT
    case SIGINT:
         return ("SIGINT");
#endif
#ifdef SIGABRT
    case SIGABRT:
         return ("SIGABRT");
#endif
#ifdef SIGFPE
    case SIGFPE:
         return ("SIGFPE");
#endif
#ifdef SIGILL
    case SIGILL:
         return ("SIGILL");
#endif
#ifdef SIGSEGV
    case SIGSEGV:
         return ("SIGSEGV");
#endif
#ifdef SIGTERM
    case SIGTERM:
         return ("SIGTERM");
#endif
#ifdef SIGALRM
    case SIGALRM:
         return ("SIGALRM");
#endif
#ifdef SIGHUP
    case SIGHUP:
         return ("SIGHUP");
#endif
#ifdef SIGKILL
    case SIGKILL:
         return ("SIGKILL");
#endif
#ifdef SIGPIPE
    case SIGPIPE:
         return ("SIGPIPE");
#endif
#ifdef SIGQUIT
    case SIGQUIT:
         return ("SIGQUIT");
#endif
#ifdef SIGUSR1
    case SIGUSR1:
         return ("SIGUSR1");
#endif
#ifdef SIGUSR2
    case SIGUSR2:
         return ("SIGUSR2");
#endif
#ifdef SIGUSR3
    case SIGUSR3:
         return ("SIGUSR3");
#endif
#ifdef SIGNOFP
    case SIGNOFP:
         return ("SIGNOFP");
#endif
#ifdef SIGTRAP
    case SIGTRAP:
         return ("SIGTRAP");
#endif
#ifdef SIGTIMR
    case SIGTIMR:
         return ("SIGTIMR");
#endif
#ifdef SIGPROF
    case SIGPROF:
         return ("SIGPROF");
#endif
#ifdef SIGSTAK
    case SIGSTAK:
         return ("SIGSTAK");
#endif
#ifdef SIGBRK
    case SIGBRK:
         return ("SIGBRK");
#endif
#ifdef SIGBUS
    case SIGBUS:
         return ("SIGBUS");
#endif
#ifdef SIGIOT
    case SIGIOT:
         return ("SIGIOT");
#endif
#ifdef SIGEMT
    case SIGEMT:
         return ("SIGEMT");
#endif
#ifdef SIGSYS
    case SIGSYS:
         return ("SIGSYS");
#endif
#ifdef SIGCHLD
    case SIGCHLD:
         return ("SIGCHLD");
#endif
#ifdef SIGPWR
    case SIGPWR:
         return ("SIGPWR");
#endif
#ifdef SIGWINCH
    case SIGWINCH:
         return ("SIGWINCH");
#endif
#ifdef SIGPOLL
    case SIGPOLL:
         return ("SIGPOLL");
#endif
#ifdef SIGCONT
    case SIGCONT:
         return ("SIGCONT");
#endif
#ifdef SIGSTOP
    case SIGSTOP:
         return ("SIGSTOP");
#endif
#ifdef SIGTSTP
    case SIGTSTP:
         return ("SIGTSTP");
#endif
#ifdef SIGTTIN
    case SIGTTIN:
         return ("SIGTTIN");
#endif
#ifdef SIGTTOU
    case SIGTTOU:
         return ("SIGTTOU");
#endif
#ifdef SIGURG
    case SIGURG:
         return ("SIGURG");
#endif
#ifdef SIGLOST
    case SIGLOST:
         return ("SIGLOST");
#endif
#ifdef SIGDIL
    case SIGDIL:
         return ("SIGDIL");
#endif
#ifdef SIGXCPU
    case SIGXCPU:
         return ("SIGXCPU");
#endif
#ifdef SIGXFSZ
    case SIGXFSZ:
         return ("SIGXFSZ");
#endif
  }
  strcpy (buf, "Unknown ");
  itoa (sig, buf+8, 10);
  return (buf);
}

/*
 * Return string for errno or GetLastError().
 */
static const char *str_error (DWORD err, BOOL sys_err)
{
  static char buf[512];
  DWORD  lang = LANG_NEUTRAL; /* MAKELANGID (LANG_ENGLISH,SUBLANG_ENGLISH_US); */

  if (err < sys_nerr && !sys_err)
  {
    strncpy (buf, strerror(err), sizeof(buf)-1);
    buf [sizeof(buf)-1] = '\0';
    return rip (buf);
  }

  if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, lang,
                      buf, sizeof(buf)-1, NULL))
     sprintf (buf, "Unknown error 0x%08lX", err);
  return rip (buf);
}

