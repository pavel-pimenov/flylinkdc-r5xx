/*
 * Code for enabling lookup of names with non-ASCII letters via
 * ACE and IDNA (Internationalizing Domain Names in Applications)
 * Ref. RFC-3490.
 */

/*  \version 0.1: Mar 19, 2004 :
 *    G. Vanem - Created.
 *
 *  \version 0.2: Mar 29, 2004 :
 *    G. Vanem - Adapted for Windows (MSVC+MingW) and C++.
 * 
 *  \version 0.3: Sep 11, 2012 :
 *    A. Solomin - code cleanup, special for FlylinkDC++.
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "punycode.h"
#include "idna.h"

#define CRITICAL_SECTION_SPIN_COUNT 10000 // [+] IRainman opt.

#pragma warning( disable : 4127 )

#define ARGSUSED(foo)  (void)foo

#ifdef UNICODE
  #define STR_FMT  "%S"
  #define ATOI(s)  _wtoi (s)
#else
  #define STR_FMT  "%s"
  #define ATOI(s)  atoi (s)
#endif

#ifdef IDNA_DEBUG_ENABLED
#define IDNA_DEBUG(lvl, args) \
        do { \
          if (_idna_debug >= lvl && _idna_printf) { \
            (*_idna_printf) ("%s(%u): ", __FILE__, __LINE__); \
            (*_idna_printf) args; \
            if (_idna_printf == printf) \
               fflush (stdout); \
          } \
        } while (0)

int            _idna_winnls_errno = 0;
int            _idna_errno = 0;
int            _idna_debug = 0;
int (MS_CDECL *_idna_printf) (const char *fmt, ...) = printf;
#endif // IDNA_DEBUG_ENABLED

/*
 * The following string is used to convert printable
 * Punycode characters to ASCII:
 */
static const char print_ascii[] = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                                  "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                                  " !\"#$%&'()*+,-./"
                                  "0123456789:;<=>?"
                                  "@ABCDEFGHIJKLMNO"
                                  "PQRSTUVWXYZ[\\]^_"
                                  "`abcdefghijklmno"
                                  "pqrstuvwxyz{|}~\n";

static CRITICAL_SECTION critSection;
static UINT             cur_cp = CP_ACP;

/*
 * Get ANSI/system codepage.
 */
static inline UINT IDNA_GetCodePage (void)
{
#ifdef IDNA_DEBUG_ENABLED
  CPINFOEX CPinfo;
  UINT     CP = 0;

  IDNA_DEBUG (2, ("OEM codepage %u\n", GetOEMCP()));
  CP = GetACP();

  if (GetCPInfoEx(CP, 0, &CPinfo))
     IDNA_DEBUG (2, ("ACP-name " STR_FMT "\n", CPinfo.CodePageName));
  return (CP);
#else
	return GetACP();
#endif
}

/*
 * Callback for EnumSystemCodePages()
 */
static BOOL cp_found = FALSE;
static UINT cp_requested = 0;

static BOOL CALLBACK print_cp_info (LPTSTR cp_str)
{
#ifdef IDNA_DEBUG_ENABLED
  CPINFOEX cp_info;
#endif
  UINT     cp = ATOI (cp_str);

  if(!IsValidCodePage(cp))
  {
#ifdef IDNA_DEBUG_ENABLED
    IDNA_DEBUG (1, ("INVALID CODEPAGE: %u\n", cp));
#endif
    return (TRUE);
  }
  if (cp == cp_requested)
     cp_found = TRUE;
#ifdef IDNA_DEBUG_ENABLED
  IDNA_DEBUG (3, ("CP: %5u, ", cp));

  if (GetCPInfoEx(cp, 0, &cp_info))
       IDNA_DEBUG (3, ("name: " STR_FMT "\n", cp_info.CodePageName));
  else IDNA_DEBUG (3, ("name: <unknown>\n"));
#endif
  return (TRUE);
}

/*
 * Check if given codepage is available
 */
static inline BOOL IDNA_CheckCodePage (UINT cp)
{
  cp_requested = cp;
  cp_found = FALSE;
  EnumSystemCodePages (print_cp_info, CP_INSTALLED);
  return (cp_found);
}

static void IDNA_exit (void)
{
  DeleteCriticalSection (&critSection);
}

/*
 * A safer strncpy()
 */
static inline char *StrLcpy (char *dst, const char *src, size_t len)
{
  assert (src != NULL);
  assert (dst != NULL);
  assert (len > 0);

  size_t src_len = strlen(src);
  if (src_len < len)
  {
     strcpy_s (dst, src_len+1, src);
	 return dst;
  }

  memcpy (dst, src, len);
  dst [len-1] = '\0';
  return (dst);
}

/*
 * Get active codpage and initialise crit-section.
 */
BOOL IDNA_init (WORD cp)
{
  if (cp == 0)
  {
    cp = (WORD)IDNA_GetCodePage();
  }
  else if (!IDNA_CheckCodePage(cp))
  {
#ifdef IDNA_DEBUG_ENABLED
    _idna_errno = IDNAERR_ILL_CODEPAGE;
    _idna_winnls_errno = GetLastError();
    IDNA_DEBUG (0, ("IDNA_init: %s\n", IDNA_strerror(_idna_errno)));
#endif // IDNA_DEBUG_ENABLED
    return (FALSE);
  }

  cur_cp = cp;
#ifdef IDNA_DEBUG_ENABLED
  IDNA_DEBUG (2, ("IDNA_init: Using codepage %u\n", cp));
#endif
  InitializeCriticalSectionAndSpinCount(&critSection, CRITICAL_SECTION_SPIN_COUNT); // [!] IRainman: InitializeCriticalSectionAndSpinCount
  atexit (IDNA_exit);
  return (TRUE);
}
#ifdef IDNA_DEBUG_ENABLED
const char *IDNA_strerror (int err)
{
  static char buf[200];

  switch ((enum IDNA_errors)err)
  {
    case IDNAERR_OK:
         return ("No error");
    case IDNAERR_NOT_INIT:
         return ("Not initialised");
    case IDNAERR_PUNYCODE_BASE:
         return ("No Punycode error");
    case IDNAERR_PUNYCODE_BAD_INPUT:
         return ("Bad Punycode input");
    case IDNAERR_PUNYCODE_BIG_OUTBUF:
         return ("Punycode output buf too small");
    case IDNAERR_PUNYCODE_OVERFLOW:
         return ("Punycode arithmetic overflow");
    case IDNAERR_PUNY_ENCODE:
         return ("Mysterious Punycode encode result");
    case IDNAERR_ILL_CODEPAGE:
         return ("Illegal or no Codepage defined");
    case IDNAERR_WINNLS:
         if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, _idna_winnls_errno,
                            LANG_NEUTRAL, buf, sizeof(buf)-1, NULL))
            return (buf);
  }
  sprintf_s (buf, 200,  "Unknown %d", err);
  return (buf);
}
#endif // IDNA_DEBUG_ENABLED
/*
 * Convert a single ASCII codepoint from active codepage to Unicode.
 */
static BOOL conv_to_unicode (char ch, wchar_t *wc)
{
  int rc = MultiByteToWideChar (cur_cp, 0, (LPCSTR)&ch, 1, wc, 1);

  if (rc == 0)
  {
#ifdef IDNA_DEBUG_ENABLED
    _idna_winnls_errno = GetLastError();
    _idna_errno = IDNAERR_WINNLS;
    IDNA_DEBUG (1, ("conv_to_unicode failed; %s\n", IDNA_strerror(_idna_winnls_errno)));
#endif // IDNA_DEBUG_ENABLED
    return (FALSE);
  }
  return (TRUE);
}

/*
 * Convert a single Unicode codepoint to ASCII in active codepage.
 * Allow 4 byte GB18030 Simplified Chinese to be converted.
 */
static inline BOOL conv_to_ascii (wchar_t wc, char *ch, int *len)
{
  const int rc = WideCharToMultiByte (cur_cp, 0, &wc, 1, (LPSTR)ch, 4, NULL, NULL);

  if (rc == 0)
  {
#ifdef IDNA_DEBUG_ENABLED
    _idna_winnls_errno = GetLastError();
    _idna_errno = IDNAERR_WINNLS;
    IDNA_DEBUG (1, ("conv_to_ascii failed; %s\n", IDNA_strerror(_idna_winnls_errno)));
#endif // IDNA_DEBUG_ENABLED
    return (FALSE);
  }
  *len = rc;
  return (TRUE);
}

/*
 * Split a domain-name into labels (no trailing dots)
 */
static inline char **split_labels (const char *name)
{
  static char  buf [MAX_HOST_LABELS][MAX_HOST_LEN];
  static char *res [MAX_HOST_LABELS+1];
  const  char *p = name;
  int    i;

  for (i = 0; i < MAX_HOST_LABELS && *p; i++)
  {
    const char *dot = strchr (p, '.');

    if (!dot)
    {
      res[i] = StrLcpy (buf[i], p, sizeof(buf[i]));
      i++;
      break;
    }
    res[i] = StrLcpy (buf[i], p, dot-p+1);
    p = ++dot;
  }
  res[i] = NULL;
#ifdef IDNA_DEBUG_ENABLED
  IDNA_DEBUG (3, ("split_labels: `%s', %d labels\n", name, i));
#endif
  return (res);
}

/*
 * Convert a single label to ACE form
 */
static char *convert_to_ACE (const char *name)
{
  static char out_buf [2*MAX_HOST_LEN];  /* A conservative guess */
  DWORD  ucs_input [MAX_HOST_LEN];
  BYTE   ucs_case [MAX_HOST_LEN];
  const  char *p;
  size_t in_len, out_len;
  int    i, c;

  for (i = 0, p = name; *p; i++)
  {
    wchar_t ucs = 0;

    c = *p++;
    if (!conv_to_unicode ((char)c, &ucs))
       goto convert_to_ACE_exit;

    ucs_input[i] = ucs;
    ucs_case[i]  = 0;
#ifdef IDNA_DEBUG_ENABLED
    IDNA_DEBUG (3, ("%c -> u+%04X\n", c, ucs));
#endif
  }
  in_len  = i;
  out_len = sizeof(out_buf);
  const punycode_status status = punycode_encode (in_len, ucs_input, ucs_case, &out_len, out_buf);

  if (status != punycode_success)
  {
#ifdef IDNA_DEBUG_ENABLED
    _idna_errno = IDNAERR_PUNYCODE_BASE + status;
#endif
	goto convert_to_ACE_exit;
  }

  for (i = 0; i < (int)out_len; i++)
  {
    c = out_buf[i];
    if (c < 0 || c > 127)
    {
#ifdef IDNA_DEBUG_ENABLED
      _idna_errno = IDNAERR_PUNY_ENCODE;
      IDNA_DEBUG (1, ("illegal Punycode result: %c (%d)\n", c, c));
#endif
      goto convert_to_ACE_exit;
    }
    if (!print_ascii[c])
    {
#ifdef IDNA_DEBUG_ENABLED
      _idna_errno = IDNAERR_PUNY_ENCODE;
      IDNA_DEBUG (1, ("Punycode not ASCII: %c (%d)\n", c, c));
#endif
      goto convert_to_ACE_exit;
    }
    out_buf[i] = print_ascii[c];
  }
  out_buf[i] = '\0';
#ifdef IDNA_DEBUG_ENABLED
  IDNA_DEBUG (2, ("punycode_encode: status %d, out_len %d, out_buf `%s'\n",
              status, out_len, out_buf));
#endif
  // [-] if (status == punycode_success && i == (int)out_len)   /* encoding and ASCII conversion okay */
     return (out_buf);

convert_to_ACE_exit:
  return (NULL);
}

/*
 * Convert a single ACE encoded label to native encoding
 * u+XXXX is used to signify a lowercase character.
 * U+XXXX is used to signify a uppercase character.
 * Normally only lowercase should be expected here.
 */
static char *convert_from_ACE (const char *name)
{
  static char out_buf [MAX_HOST_LEN];
  DWORD  ucs_output [MAX_HOST_LEN];
  BYTE   ucs_case  [MAX_HOST_LEN];
  size_t ucs_len, i, j;

  memset (&ucs_case, 0, sizeof(ucs_case));
  ucs_len = sizeof(ucs_output);
  const punycode_status status = punycode_decode (strlen(name), name, &ucs_len, ucs_output, ucs_case);

  if (status != punycode_success)
  {
#ifdef IDNA_DEBUG_ENABLED
    _idna_errno = IDNAERR_PUNYCODE_BASE + status;
#endif
	goto convert_from_ACE_exit;
  }

  for (i = j = 0; i < ucs_len && j < sizeof(out_buf)-4; i++)
  {
    wchar_t ucs = (wchar_t)ucs_output[i];
    int     len/* [-] please see conv_to_ascii function = 0*/; //[+] idna.cpp(358): error #12144: "len" is possibly uninitialized
    if (!conv_to_ascii(ucs, out_buf+j, &len))
       goto convert_from_ACE_exit;
#ifdef IDNA_DEBUG_ENABLED
    IDNA_DEBUG (3, ("%c+%04X -> %.*s\n",
                ucs_case[i] ? 'U' : 'u', ucs, len, out_buf+j));
#endif
    j += len;
  }
  out_buf[j] = '\0';
#ifdef IDNA_DEBUG_ENABLED
  IDNA_DEBUG (2, ("punycode_decode: status %d, out_len %d, out_buf `%s'\n",
              status, ucs_len, out_buf));
#endif
  // [-] return (status == punycode_success ? out_buf : NULL);
  return out_buf;

convert_from_ACE_exit:
  return (NULL);
}


/*
 * E.g. convert "www.tromsø.no" to ACE:
 *
 * 1) Convert each label separately. "www", "tromsø" and "no"
 * 2) "tromsø" -> u+0074 u+0072 u+006F u+006D u+0073 u+00F8
 * 3) Pass this through `punycode_encode()' which gives "troms-zua".
 * 4) Repeat for all labels with non-ASCII letters.
 * 5) Prepending "xn--" for each converted label gives "www.xn--troms-zua.no".
 *
 * E.g. 2:
 *   "www.blåbærsyltetøy.no" -> "www.xn--blbrsyltety-y8aO3x.no"
 *
 * Ref. http://www.imc.org/idna/do-idna.cgi
 *      http://www.norid.no/domenenavnbaser/ace/ace_technical.en.html
 */
BOOL IDNA_convert_to_ACE (
          char  *name,   /* IN/OUT: native ASCII/ACE name */
          size_t *size)   /* IN:     length of name buf */
{                         /* OUT:    ACE encoded length */
  const  BYTE *p;
  const  char *ace;
  char  *in_name = name;
  char **labels;
  int    i;
  size_t len = 0;
  BOOL   rc = FALSE;
#ifdef IDNA_USE_STATIC_BUFFERS
  EnterCriticalSection (&critSection);
#endif
  labels = split_labels (name);

  for (i = 0; labels[i]; i++)
  {
    ace = NULL;
    for (p = (const BYTE*)labels[i]; *p; p++)
        if (*p >= 0x80)  /* !! this may not be true for all codepages */
        {
          ace = convert_to_ACE (labels[i]);
          if (!ace)
             goto quit;
          break;
        }

    if (ace)
    {
      if (len + 5 + strlen(ace) > *size)
      {
#ifdef IDNA_DEBUG_ENABLED
        IDNA_DEBUG (1, ("input length exceeded\n"));
#endif
        goto quit;
      }
	  name += sprintf (name, "xn--%s.", ace);
    }
    else  /* pass through unchanged */
    {
      if (len + 1 + strlen(labels[i]) > *size)
      {
#ifdef IDNA_DEBUG_ENABLED
        IDNA_DEBUG (1, ("input length exceeded\n"));
#endif
        goto quit;
      }
      name += sprintf (name, "%s.", labels[i]);
    }
  }
  if (in_name != name)   /* drop trailing '.' */
     name--;
  len = name - in_name;
  *name = '\0';
  *size = len;
#ifdef IDNA_DEBUG_ENABLED
  IDNA_DEBUG (2, ("IDNA_convert_to_ACE: `%s', %d bytes\n", in_name, len));
#endif
  rc = TRUE;

quit:
  LeaveCriticalSection (&critSection);
  return (rc);
}

/*
 * 1) Pass through labels w/o "xn--" prefix unaltered.
 * 2) Strip "xn--" prefix and pass to punycode_decode()
 * 3) Repeat for all labels with "xn--" prefix.
 * 4) Collect Unicode strings and convert to original codepage.
 */
BOOL IDNA_convert_from_ACE (
          char   *name,    /* IN/OUT: ACE/native ASCII name */
          size_t *size)    /* IN:     ACE raw string length */
{                          /* OUT:    ASCII decoded length */
  char  *in_name = name;
  char **labels;
  int    i;
  BOOL   rc = FALSE;

  EnterCriticalSection (&critSection);

  labels  = split_labels (name);

  for (i = 0; labels[i]; i++)
  {
    const char *ascii = NULL;
    const char *label = labels[i];

    if (!strncmp(label,"xn--",4) && label[4])
    {
      ascii = convert_from_ACE (label+4);
      if (!ascii)
         goto quit;
    }
    name += sprintf_s (name, MAX_COPY_SIZE, "%s.", ascii ? ascii : label);
  }
  if (name > in_name)
     name--;
  *name = '\0';
  *size = name - in_name;
  rc = TRUE;

quit:
  LeaveCriticalSection (&critSection);
  return (rc);
}

/*
 * The rest is C++
 */
#if 0
//#ifdef  __cplusplus
struct hostent *CIDNA_resolver::gethostbyname (const char *name)
{
  /* if the name is in the hosts file (or LMHOSTS or a WINS server) in native CP,
   * return that.
   */
  struct hostent *he = ::gethostbyname (name);
  const  char    *ace;

  if (he)
     return (he);

  ace = CIDNA_convert::convert_to_ACE (name);
  if (!ace)
     return (NULL);

  he = ::gethostbyname (ace);
  if (he)
     he->h_aliases[0] = (char*) ace;
  return (he);
}

struct hostent *CIDNA_resolver::gethostbyaddress (const char *addr_name, int size, int af)
{
  struct hostent *he = ::gethostbyaddr (addr_name, size, af);
  const  char    *name;

  if (!he || !he->h_name)
     return (NULL);

  name = CIDNA_convert::convert_from_ACE (he->h_name);
  if (name)
  {
    he->h_aliases[0] = he->h_name;
    he->h_name = (char*) name;
  }
  return (he);
}

/*
 * TODO:
 *
 * There seems to be considerable confusion on what the reentrant versions should
 * look like. Some specifies returning 'int' (Posix?). OpenBSD/NetBSD return
 * 'struct hostent*'. So I leave this as an excercise for the guru reader...
 */
#if 0
int CIDNA_resolver::gethostbyname_r (
    const char      *name,
    struct hostent  *res_buf,
    char            *buf,
    size_t           buflen,
    struct hostent **result,
    int             *h_errno_p)
{
  if (result)
     *result = res_buf;
  if (h_errno_p)
     *h_errno_p = h_errno;
  return (-1);
}

int CIDNA_resolver::gethostbyaddr_r (
    const char      *name,
    struct hostent  *res_buf,
    char            *buf,
    size_t           buflen,
    struct hostent **result,
    int             *h_errno_p)
{
  if (result)
     *result = res_buf;
  if (h_errno_p)
     *h_errno_p = h_errno;
  return (-1);
}

int CIDNA_resolver::gethostbyaddr_r (
    const char      *addr,
    int              len,
    int              type,
    struct hostent  *res_buf,
    char            *buf,
    size_t           buflen,
    struct hostent **result,
    int             *h_errno_p)
{
  if (result)
     *result = res_buf;
  if (h_errno_p)
     *h_errno_p = h_errno;
  return (-1);
}
#endif
#endif

