/****************************************************************************
 * apps/nshlib/nsh_telnetlogin.c
 *
 *   Copyright (C) 2007-2013, 2016 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name Gregory Nutt nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "fsutils/passwd.h"

#include "nsh.h"
#include "nsh_console.h"

#ifdef CONFIG_NSH_TELNET_LOGIN

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TELNET_IAC              255
#define TELNET_WILL             251
#define TELNET_WONT             252
#define TELNET_DO               253
#define TELNET_DONT             254
#define TELNET_USE_ECHO         1
#define TELNET_NOTUSE_ECHO      0

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nsh_telnetecho
 ****************************************************************************/

static void nsh_telnetecho(FAR struct console_stdio_s *pstate,
                           uint8_t is_use)
{
  uint8_t optbuf[4];
  optbuf[0] = TELNET_IAC;
  optbuf[1] = (is_use == TELNET_USE_ECHO) ? TELNET_WILL : TELNET_DO;
  optbuf[2] = 1;
  optbuf[3] = 0;
  fputs((char *)optbuf, pstate->cn_outstream);
  fflush(pstate->cn_outstream);
}

/****************************************************************************
 * Name: nsh_telnettoken
 ****************************************************************************/

static void nsh_telnettoken(FAR struct console_stdio_s *pstate,
                            FAR char *buffer, size_t buflen)
{
  FAR char *start;
  FAR char *endp1;
  bool quoted = false;

  /* Find the start of token.  Either (1) the first non-white space
   * character on the command line or (2) the character immediately after
   * a quotation mark.
   */

  for (start = pstate->cn_line; *start; start++)
    {
      /* Does the token open with a quotation mark */

      if (*start == '"')
        {
          /* Yes.. break out with start set to the character after the
           * quotation mark.
           */

          quoted = true;
          start++;
          break;
        }

      /* No, then any non-whitespace is the first character of the token */

      else if (!isspace(*start))
        {
          /* Break out with start set to the first character of the token */

          break;
        }
    }

  /* Find the terminating character after the token on the command line.  The
   * terminating character is either (1) the matching quotation mark, or (2)
   * any whitespace.
   */

  for (endp1 = start; *endp1; endp1++)
    {
      /* Did the token begin with a quotation mark? */

      if (quoted)
        {
          /* Yes.. then only the matching quotation mark (or end of string)
           * terminates
           */

          if (*endp1 == '"')
            {
              /* Break out... endp1 points to closing quotation mark */

              break;
            }
        }

      /* No.. any whitespace (or end of string) terminates */

      else if (isspace(*endp1))
        {
          /* Break out... endp1 points to first while space encountered */

           break;
        }
    }

  /* Replace terminating character with a NUL terminator */

  *endp1 = '\0';

  /* Copied the token into the buffer */

  strncpy(buffer, start, buflen);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nsh_telnetlogin
 ****************************************************************************/

int nsh_telnetlogin(FAR struct console_stdio_s *pstate)
{
  char username[16];
  char password[16];
  int i;

  /* Present the NSH Telnet greeting */

  fputs(g_telnetgreeting, pstate->cn_outstream);
  fflush(pstate->cn_outstream);

  /* Loop for the configured number of retries */

  for (i = 0; i < CONFIG_NSH_LOGIN_FAILCOUNT; i++)
    {
      /* Ask for the login username */

      fputs(g_userprompt, pstate->cn_outstream);
      fflush(pstate->cn_outstream);

      username[0] = '\0';
      if (fgets(pstate->cn_line, CONFIG_NSH_LINELEN,
                INSTREAM(pstate)) != NULL)
        {
          /* Parse out the username */

          nsh_telnettoken(pstate, username, sizeof(username));
        }

      /* Ask for the login password */

      fputs(g_passwordprompt, pstate->cn_outstream);
      fflush(pstate->cn_outstream);
      nsh_telnetecho(pstate, TELNET_NOTUSE_ECHO);

      password[0] = '\0';
      if (fgets(pstate->cn_line, CONFIG_NSH_LINELEN,
                INSTREAM(pstate)) != NULL)
        {
          /* Parse out the password */

          nsh_telnettoken(pstate, password, sizeof(password));

          /* Verify the username and password */

#if defined(CONFIG_NSH_LOGIN_PASSWD)
          if (PASSWORD_VERIFY_MATCH(passwd_verify(username, password)))
#elif defined(CONFIG_NSH_LOGIN_PLATFORM)
          if (PASSWORD_VERIFY_MATCH(platform_user_verify(username,
                                                         password)))
#elif defined(CONFIG_NSH_LOGIN_FIXED)
          if (strcmp(password, CONFIG_NSH_LOGIN_PASSWORD) == 0 &&
              strcmp(username, CONFIG_NSH_LOGIN_USERNAME) == 0)
#else
#  error No user verification method selected
#endif
            {
              fputs(g_loginsuccess, pstate->cn_outstream);
              fflush(pstate->cn_outstream);
              nsh_telnetecho(pstate, TELNET_USE_ECHO);
              return OK;
            }
          else
            {
              fputs(g_badcredentials, pstate->cn_outstream);
              fflush(pstate->cn_outstream);
#if CONFIG_NSH_LOGIN_FAILDELAY > 0
              usleep(CONFIG_NSH_LOGIN_FAILDELAY * 1000L);
#endif
            }
        }

      nsh_telnetecho(pstate, TELNET_USE_ECHO);
    }

  /* Too many failed login attempts */

  fputs(g_loginfailure, pstate->cn_outstream);
  fflush(pstate->cn_outstream);
  return -1;
}

#endif /* CONFIG_NSH_TELNET_LOGIN */
