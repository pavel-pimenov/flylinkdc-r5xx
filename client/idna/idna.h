#ifndef _IDNA_H
#define _IDNA_H

#include <stdlib.h>
#include <windows.h>
#include <winsock.h>

#ifndef MAX_HOST_LEN
#define MAX_HOST_LEN  256
#endif

#ifndef MAX_HOST_LABELS
#define MAX_HOST_LABELS  8
#endif

//#define IDNA_DEBUG_ENABLED


#if defined(_MSC_VER) && !defined(__cplusplus)
  #define MS_CDECL _cdecl  /* var-arg functions cannot be _fastcall */
#else
  #define MS_CDECL
#endif

#ifdef __cplusplus
extern "C" {
#endif
#ifdef IDNA_DEBUG_ENABLED
extern int _idna_debug, _idna_errno, _idna_winnls_errno;

extern int (MS_CDECL *_idna_printf) (const char *fmt, ...)
  #if defined(__GNUC__) && (__GNUC__ >= 3)
    __attribute__((format(printf,1,2)))
  #endif
  ;

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

  typedef int (*MS_CDECL IDNA_debug_func) (const char *fmt, ...);
  /* [-] IRainman: óæñ.
  class CIDNA_convert
  {
    public:
      CIDNA_convert (WORD code_page = 0)
      {
        IDNA_init (code_page);
        m_buf[0] = '\0';
      }

      inline const char *convert_to_ACE (const char *name, int *error = NULL)
      {
	strncpy_s (m_buf, sizeof(m_buf)-1, name, MAX_COPY_SIZE);
	m_buf [sizeof(m_buf)-1] = '\0';

        size_t size = sizeof (m_buf);
        BOOL   rc   = IDNA_convert_to_ACE (m_buf, &size);

        if (error)
           *error = _idna_errno;
        return (rc ? m_buf : NULL);
      }

      inline const char *convert_from_ACE (const char *name, int *error = NULL)
      {
	strncpy_s (m_buf, sizeof(m_buf)-1, name, MAX_COPY_SIZE);
	m_buf [sizeof(m_buf)-1] = '\0';

        size_t size = strlen (m_buf);
        BOOL   rc   = IDNA_convert_from_ACE (m_buf, &size);

        if (error)
           *error = _idna_errno;
        return (rc ? m_buf : NULL);
      }

      inline BOOL is_ACE (void)
      {
        return (strstr(m_buf,"xn--") ? TRUE : FALSE);
      }

      inline void set_debug (int level, IDNA_debug_func func)
      {
	_idna_debug = level;
 	_idna_printf = func;
      }

    private:
      char m_buf [2*MAX_HOST_LEN];  // A conservative estimate
  };
  
  class CIDNA_resolver : public CIDNA_convert
  {
    public:
      CIDNA_resolver (WORD code_page = 0) : CIDNA_convert (code_page)  {}
      struct hostent *gethostbyname (const char *name);
      struct hostent *gethostbyaddress (const char *addr_name, int size, int af);
  };
  */
}
#endif  /* __cplusplus */
#endif  /* _IDNA_H */

