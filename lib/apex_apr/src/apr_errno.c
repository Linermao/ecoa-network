#include <apr_errno.h>

#ifdef USE_APEX_API
#undef apr_strerror

int apex_apr_status_is_success(apr_status_t status)
{
	return status == APR_SUCCESS;
}

const char *apex_apr_status_to_string(apr_status_t status)
{
	switch (status) {
	case APR_SUCCESS:
		return "Success";
	case APR_EGENERAL:
		return "General failure";
	case APR_TIMEUP:
		return "Timed out";
	case APR_EBUSY:
		return "Resource busy";
	case APR_EAGAIN:
		return "Operation would block";
	case APR_ENOMEM:
		return "Out of memory";
	case APR_EINVAL:
		return "Invalid argument";
	case APR_EEXIST:
		return "Resource already exists";
	case APR_ENOTIMPL:
		return "Not implemented";
	case APR_EOF:
		return "End of file";
	case APR_CHILD_DONE:
		return "Child completed";
	case APR_CHILD_NOTDONE:
		return "Child not completed";
	default:
		return "Unknown APEX/APR status";
	}
}

static char *apex_apr_copy_status_string(char *buf, apr_size_t bufsize,
						  const char *message)
{
	apr_size_t i;

	if (buf == NULL || bufsize == 0) {
		return buf;
	}

	for (i = 0; i + 1 < bufsize && message[i] != '\0'; ++i) {
		buf[i] = message[i];
	}
	buf[i] = '\0';
	return buf;
}

char *apex_apr_strerror(apr_status_t statcode, char *buf, apr_size_t bufsize)
{
	return apex_apr_copy_status_string(buf, bufsize,
						   apex_apr_status_to_string(statcode));
}

#endif /* USE_APEX_API */
