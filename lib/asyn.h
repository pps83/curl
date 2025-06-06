#ifndef HEADER_CURL_ASYN_H
#define HEADER_CURL_ASYN_H
/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/

#include "curl_setup.h"
#include "curl_addrinfo.h"
#include "httpsrr.h"

struct addrinfo;
struct hostent;
struct Curl_easy;
struct connectdata;
struct Curl_dns_entry;

#ifdef CURLRES_THREADED
#include "curl_threads.h"

#ifdef _WIN32
#include "system_win32.h"
/* Thread sync data used by GetAddrInfoExW for win8+ */
struct thread_sync_data_w8
{
  OVERLAPPED overlapped;
  ADDRINFOEXW_ *res;
  HANDLE cancel_ev;
  ADDRINFOEXW_ hints;
};
#endif

/* Data for synchronization between resolver thread and its parent */
struct thread_sync_data {
#ifdef _WIN32
  struct thread_sync_data_w8 w8;
#endif
  curl_mutex_t *mtx;
  char *hostname;        /* hostname to resolve, Curl_async.hostname
                            duplicate */
#ifndef CURL_DISABLE_SOCKETPAIR
  curl_socket_t sock_pair[2]; /* eventfd/pipes/socket pair */
#endif
  struct Curl_addrinfo *res;
#ifdef HAVE_GETADDRINFO
  struct addrinfo hints;
#endif
  struct thread_data *td; /* for thread-self cleanup */
  int port;
  int sock_error;
  bool done;
};

struct thread_data {
#ifdef _WIN32
  HANDLE complete_ev;
#endif
  curl_thread_t thread_hnd;
  unsigned int poll_interval;
  timediff_t interval_end;
  struct thread_sync_data tsd;
#if defined(USE_HTTPSRR) && defined(USE_ARES)
  struct Curl_https_rrinfo hinfo;
  ares_channel channel;
#endif
};

#elif defined(CURLRES_ARES) /* CURLRES_THREADED */

struct thread_data {
  int num_pending; /* number of outstanding c-ares requests */
  struct Curl_addrinfo *temp_ai; /* intermediary result while fetching c-ares
                                    parts */
  int last_status;
#ifndef HAVE_CARES_GETADDRINFO
  struct curltime happy_eyeballs_dns_time; /* when this timer started, or 0 */
#endif
#ifdef USE_HTTPSRR
  struct Curl_https_rrinfo hinfo;
#endif
  char hostname[1];
};

#endif /* CURLRES_ARES */

#ifdef USE_ARES
#include <ares.h>

/* for HTTPS RR purposes as well */
int Curl_ares_getsock(struct Curl_easy *data,
                      ares_channel channel,
                      curl_socket_t *socks);
int Curl_ares_perform(ares_channel channel,
                      timediff_t timeout_ms);
#endif


/*
 * This header defines all functions in the internal asynch resolver interface.
 * All asynch resolvers need to provide these functions.
 * asyn-ares.c and asyn-thread.c are the current implementations of asynch
 * resolver backends.
 */

/*
 * Curl_resolver_global_init()
 *
 * Called from curl_global_init() to initialize global resolver environment.
 * Returning anything else than CURLE_OK fails curl_global_init().
 */
int Curl_resolver_global_init(void);

/*
 * Curl_resolver_global_cleanup()
 * Called from curl_global_cleanup() to destroy global resolver environment.
 */
void Curl_resolver_global_cleanup(void);

/*
 * Curl_resolver_init()
 * Called from curl_easy_init() -> Curl_open() to initialize resolver
 * URL-state specific environment ('resolver' member of the UrlState
 * structure). Should fill the passed pointer by the initialized handler.
 * Returning anything else than CURLE_OK fails curl_easy_init() with the
 * correspondent code.
 */
CURLcode Curl_resolver_init(struct Curl_easy *easy, void **resolver);

/*
 * Curl_resolver_cleanup()
 * Called from curl_easy_cleanup() -> Curl_close() to cleanup resolver
 * URL-state specific environment ('resolver' member of the UrlState
 * structure). Should destroy the handler and free all resources connected to
 * it.
 */
void Curl_resolver_cleanup(void *resolver);

/*
 * Curl_resolver_duphandle()
 * Called from curl_easy_duphandle() to duplicate resolver URL-state specific
 * environment ('resolver' member of the UrlState structure). Should
 * duplicate the 'from' handle and pass the resulting handle to the 'to'
 * pointer. Returning anything else than CURLE_OK causes failed
 * curl_easy_duphandle() call.
 */
CURLcode Curl_resolver_duphandle(struct Curl_easy *easy, void **to,
                                 void *from);

/*
 * Curl_resolver_cancel().
 *
 * It is called from inside other functions to cancel currently performing
 * resolver request. Should also free any temporary resources allocated to
 * perform a request. This never waits for resolver threads to complete.
 *
 * It is safe to call this when conn is in any state.
 */
void Curl_resolver_cancel(struct Curl_easy *data);

/*
 * Curl_resolver_kill().
 *
 * This acts like Curl_resolver_cancel() except it will block until any threads
 * associated with the resolver are complete. This never blocks for resolvers
 * that do not use threads. This is intended to be the "last chance" function
 * that cleans up an in-progress resolver completely (before its owner is about
 * to die).
 *
 * It is safe to call this when conn is in any state.
 */
void Curl_resolver_kill(struct Curl_easy *data);

/* Curl_resolver_getsock()
 *
 * This function is called from the multi_getsock() function.  'sock' is a
 * pointer to an array to hold the file descriptors, with 'numsock' being the
 * size of that array (in number of entries). This function is supposed to
 * return bitmask indicating what file descriptors (referring to array indexes
 * in the 'sock' array) to wait for, read/write.
 */
int Curl_resolver_getsock(struct Curl_easy *data, curl_socket_t *sock);

/*
 * Curl_resolver_is_resolved()
 *
 * Called repeatedly to check if a previous name resolve request has
 * completed. It should also make sure to time-out if the operation seems to
 * take too long.
 *
 * Returns normal CURLcode errors.
 */
CURLcode Curl_resolver_is_resolved(struct Curl_easy *data,
                                   struct Curl_dns_entry **dns);

/*
 * Curl_resolver_wait_resolv()
 *
 * Waits for a resolve to finish. This function should be avoided since using
 * this risk getting the multi interface to "hang".
 *
 * If 'entry' is non-NULL, make it point to the resolved dns entry
 *
 * Returns CURLE_COULDNT_RESOLVE_HOST if the host was not resolved,
 * CURLE_OPERATION_TIMEDOUT if a time-out occurred, or other errors.
 */
CURLcode Curl_resolver_wait_resolv(struct Curl_easy *data,
                                   struct Curl_dns_entry **dnsentry);

/*
 * Curl_resolver_getaddrinfo() - when using this resolver
 *
 * Returns name information about the given hostname and port number. If
 * successful, the 'hostent' is returned and the fourth argument will point to
 * memory we need to free after use. That memory *MUST* be freed with
 * Curl_freeaddrinfo(), nothing else.
 *
 * Each resolver backend must of course make sure to return data in the
 * correct format to comply with this.
 */
struct Curl_addrinfo *Curl_resolver_getaddrinfo(struct Curl_easy *data,
                                                const char *hostname,
                                                int port,
                                                int *waitp);

#ifndef CURLRES_ASYNCH
/* convert these functions if an asynch resolver is not used */
#define Curl_resolver_cancel(x) Curl_nop_stmt
#define Curl_resolver_kill(x) Curl_nop_stmt
#define Curl_resolver_is_resolved(x,y) CURLE_COULDNT_RESOLVE_HOST
#define Curl_resolver_wait_resolv(x,y) CURLE_COULDNT_RESOLVE_HOST
#define Curl_resolver_duphandle(x,y,z) CURLE_OK
#define Curl_resolver_init(x,y) CURLE_OK
#define Curl_resolver_global_init() CURLE_OK
#define Curl_resolver_global_cleanup() Curl_nop_stmt
#define Curl_resolver_cleanup(x) Curl_nop_stmt
#endif

#ifdef CURLRES_ASYNCH
#define Curl_resolver_asynch() 1
#else
#define Curl_resolver_asynch() 0
#endif


/********** end of generic resolver interface functions *****************/
#endif /* HEADER_CURL_ASYN_H */
