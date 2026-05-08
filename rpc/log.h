#ifndef _LOG_H_RPCGEN
#define _LOG_H_RPCGEN

#include <rpc/rpc.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif


struct log_args {
	char *username;
	char *op;
	char *filename;
};
typedef struct log_args log_args;

#define LOGGING 100522371
#define LOGVER 1

#if defined(__STDC__) || defined(__cplusplus)
#define log 1
extern  enum clnt_stat log_1(log_args , int *, CLIENT *);
extern  bool_t log_1_svc(log_args , int *, struct svc_req *);
extern int logging_1_freeresult (SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define log 1
extern  enum clnt_stat log_1();
extern  bool_t log_1_svc();
extern int logging_1_freeresult ();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_log_args (XDR *, log_args*);

#else /* K&R C */
extern bool_t xdr_log_args ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_LOG_H_RPCGEN */
