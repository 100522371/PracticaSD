#include "log.h"

bool_t log_1_svc(log_args arg1, int *result,  struct svc_req *rqstp) {
	if (strcmp(arg1.op, "SENDATTACH")==0){
		printf("LOG: %s %s %s", arg1.username, arg1.op, arg1.filename);
	}
	else{
		printf("LOG: %s %s", arg1.username, arg1.op);
	}
	*result = 1;
	return TRUE;
}

int logging_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result) {
	xdr_free (xdr_result, result);
	return 1;
}
