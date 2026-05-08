#include "log.h"

void logging_1(char *host, char *username, char *op, char *filename) {
	CLIENT *clnt;
	enum clnt_stat retval_1;
	int result_1;
	log_args log_1_arg1;

#ifndef	DEBUG
	clnt = clnt_create (host, LOGGING, LOGVER, "tcp");
	if (clnt == NULL) {
		clnt_pcreateerror (host);
		exit (1);
	}
#endif	/* DEBUG */

	strcpy(log_1_arg1.username, username);
	strcpy(log_1_arg1.op, op);
	strcpy(log_1_arg1.filename, filename);

	retval_1 = log_1(log_1_arg1, &result_1, clnt);
	if (retval_1 != RPC_SUCCESS) {
		clnt_perror (clnt, "call failed");
	}
#ifndef	DEBUG
	clnt_destroy (clnt);
#endif	 /* DEBUG */
	return result_1;
}


int
main (int argc, char *argv[])
{
	char *host;
	int ret;

	host = getenv("LOG_RPC_IP");
	if (host == NULL) {
		printf ("usage: export LOG_RPC_IP=localhost %s\n");
		exit (1);
	}
	
	ret = log_1(host, argv[1], argv[2], argv[3]);
	exit (0);
}
