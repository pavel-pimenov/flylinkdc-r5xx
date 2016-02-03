
#pragma once

#ifndef _IDNA_H
#define _IDNA_H

#ifndef MAX_HOST_LEN
#define MAX_HOST_LEN  256
#endif

#ifndef MAX_HOST_LABELS
#define MAX_HOST_LABELS  8
#endif

#define IDNA_DEBUG_ENABLED

#if defined(_MSC_VER) && !defined(__cplusplus)
  #define MS_CDECL _cdecl  /* var-arg functions cannot be _fastcall */
#else
  #define MS_CDECL
#endif

#ifdef __cplusplus
extern "C" {
#endif
#ifdef IDNA_DEBUG_ENABLED
extern int  _idna_errno, _idna_winnls_errno;

enum IDNA_errors {
     IDNAERR_OK = 0,
     IDNAERR_NOT_INIT,
     IDNAERR_PUNYCODE_BASE,
     IDNAERR_PUNYCODE_BAD_INPUT,
     IDNAERR_PUNYCODE_BIG_OUTBUF,
     IDNAERR_PUNYCODE_OVERFLOW,
     IDNAERR_PUNY_ENCODE,
     IDNAERR_ILL_CODEPAGE,
     IDNAERR_WINNLS               /* specific error in _idna_winnls_errno */
   };

const char *IDNA_strerror (int err);
#endif // IDNA_DEBUG_ENABLED

BOOL IDNA_init (WORD code_page);

BOOL IDNA_convert_to_ACE   (char *name, size_t *size);
BOOL IDNA_convert_from_ACE (char *name, size_t *size);

#define MAX_COPY_SIZE 256

#ifdef __cplusplus

}
#endif  /* __cplusplus */
#endif  /* _IDNA_H */

