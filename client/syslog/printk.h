
#pragma once

#ifdef FLYLINKDC_USE_SYSLOG

#ifndef __PRINTK_H
#define __PRINTK_H

#include <stdio.h>
#include <stdarg.h>
#include <process.h>


#define NAMESPACE(x)  syslog_ ## x

#define snprintk      NAMESPACE (snprintk)
#define vsnprintk     NAMESPACE (vsnprintk)
#define time_now      NAMESPACE (time_now)

extern int vsnprintk   (char *buf, int len, const char *fmt, va_list ap);

extern const char *time_now (void);

extern int snprintk (char *buf, int len, const char *fmt, ...)
#if defined(__GNUC__)
  __attribute__((format(printf,3,4)))
#endif
;

#endif

#endif  /* __PRINTK_H */

