#ifndef APEX_APR_APR_NETWORK_IO_H
#define APEX_APR_APR_NETWORK_IO_H

#include_next <apr_network_io.h>

#ifdef USE_APEX_API

/*
 * Network handle types stay aliased to APR for now so the existing socket
 * transport keeps compiling. APEX-specific port/channel objects can replace
 * these aliases later behind the same public names.
 */
typedef apr_socket_t apex_apr_socket_t;
typedef apr_sockaddr_t apex_apr_sockaddr_t;

#define apr_socket_t apex_apr_socket_t
#define apr_sockaddr_t apex_apr_sockaddr_t

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_NETWORK_IO_H */
