#ifndef _IP_CONNTRACK_SCTP_H
#define _IP_CONNTRACK_SCTP_H
/* SCTP tracking. */

enum sctp_conntrack {
	SCTP_CONNTRACK_NONE,
	SCTP_CONNTRACK_CLOSED,
	SCTP_CONNTRACK_COOKIE_WAIT,
	SCTP_CONNTRACK_COOKIE_ECHOED,
	SCTP_CONNTRACK_ESTABLISHED,
	SCTP_CONNTRACK_SHUTDOWN_SENT,
	SCTP_CONNTRACK_SHUTDOWN_RECD,
	SCTP_CONNTRACK_SHUTDOWN_ACK_SENT,
	SCTP_CONNTRACK_MAX
};

struct ip_ct_sctp
{
	enum sctp_conntrack state;

	u_int32_t vtag[IP_CT_DIR_MAX];
	u_int32_t ttag[IP_CT_DIR_MAX];
};

#endif /* _IP_CONNTRACK_SCTP_H */
