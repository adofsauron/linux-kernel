/*
 *  drivers/s390/char/sclp.c
 *     core function to access sclp interface
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Peschke <mpeschke@de.ibm.com>
 *		 Martin Schwidefsky <schwidefsky@de.ibm.com>
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/reboot.h>
#include <linux/jiffies.h>
#include <asm/types.h>
#include <asm/s390_ext.h>

#include "sclp.h"

#define SCLP_HEADER		"sclp: "

/* Structure for register_early_external_interrupt. */
static ext_int_info_t ext_int_info_hwc;

/* Lock to protect internal data consistency. */
static DEFINE_SPINLOCK(sclp_lock);

/* Mask of events that we can receive from the sclp interface. */
static sccb_mask_t sclp_receive_mask;

/* Mask of events that we can send to the sclp interface. */
static sccb_mask_t sclp_send_mask;

/* List of registered event listeners and senders. */
static struct list_head sclp_reg_list;

/* List of queued requests. */
static struct list_head sclp_req_queue;

/* Data for read and and init requests. */
static struct sclp_req sclp_read_req;
static struct sclp_req sclp_init_req;
static char sclp_read_sccb[PAGE_SIZE] __attribute__((__aligned__(PAGE_SIZE)));
static char sclp_init_sccb[PAGE_SIZE] __attribute__((__aligned__(PAGE_SIZE)));

/* Timer for request retries. */
static struct timer_list sclp_request_timer;

/* Internal state: is the driver initialized? */
static volatile enum sclp_init_state_t {
	sclp_init_state_uninitialized,
	sclp_init_state_initializing,
	sclp_init_state_initialized
} sclp_init_state = sclp_init_state_uninitialized;

/* Internal state: is a request active at the sclp? */
static volatile enum sclp_running_state_t {
	sclp_running_state_idle,
	sclp_running_state_running
} sclp_running_state = sclp_running_state_idle;

/* Internal state: is a read request pending? */
static volatile enum sclp_reading_state_t {
	sclp_reading_state_idle,
	sclp_reading_state_reading
} sclp_reading_state = sclp_reading_state_idle;

/* Internal state: is the driver currently serving requests? */
static volatile enum sclp_activation_state_t {
	sclp_activation_state_active,
	sclp_activation_state_deactivating,
	sclp_activation_state_inactive,
	sclp_activation_state_activating
} sclp_activation_state = sclp_activation_state_active;

/* Internal state: is an init mask request pending? */
static volatile enum sclp_mask_state_t {
	sclp_mask_state_idle,
	sclp_mask_state_initializing
} sclp_mask_state = sclp_mask_state_idle;

/* Maximum retry counts */
#define SCLP_INIT_RETRY		3
#define SCLP_MASK_RETRY		3
#define SCLP_REQUEST_RETRY	3

/* Timeout intervals in seconds.*/
#define SCLP_BUSY_INTERVAL	2
#define SCLP_RETRY_INTERVAL	5

static void sclp_process_queue(void);
static int sclp_init_mask(int calculate);
static int sclp_init(void);

/* Perform service call. Return 0 on success, non-zero otherwise. */
static int
service_call(sclp_cmdw_t command, void *sccb)
{
	int cc;

	__asm__ __volatile__(
		"   .insn rre,0xb2200000,%1,%2\n"  /* servc %1,%2 */
		"   ipm	  %0\n"
		"   srl	  %0,28"
		: "=&d" (cc)
		: "d" (command), "a" (__pa(sccb))
		: "cc", "memory" );
	if (cc == 3)
		return -EIO;
	if (cc == 2)
		return -EBUSY;
	return 0;
}

/* Request timeout handler. Restart the request queue. If DATA is non-zero,
 * force restart of running request. */
static void
sclp_request_timeout(unsigned long data)
{
	unsigned long flags;

	if (data) {
		spin_lock_irqsave(&sclp_lock, flags);
		sclp_running_state = sclp_running_state_idle;
		spin_unlock_irqrestore(&sclp_lock, flags);
	}
	sclp_process_queue();
}

/* Set up request retry timer. Called while sclp_lock is locked. */
static inline void
__sclp_set_request_timer(unsigned long time, void (*function)(unsigned long),
			 unsigned long data)
{
	del_timer(&sclp_request_timer);
	sclp_request_timer.function = function;
	sclp_request_timer.data = data;
	sclp_request_timer.expires = jiffies + time;
	add_timer(&sclp_request_timer);
}

/* Try to start a request. Return zero if the request was successfully
 * started or if it will be started at a later time. Return non-zero otherwise.
 * Called while sclp_lock is locked. */
static int
__sclp_start_request(struct sclp_req *req)
{
	int rc;

	if (sclp_running_state != sclp_running_state_idle)
		return 0;
	del_timer(&sclp_request_timer);
	if (req->start_count <= SCLP_REQUEST_RETRY) {
		rc = service_call(req->command, req->sccb);
		req->start_count++;
	} else
		rc = -EIO;
	if (rc == 0) {
		/* Sucessfully started request */
		req->status = SCLP_REQ_RUNNING;
		sclp_running_state = sclp_running_state_running;
		__sclp_set_request_timer(SCLP_RETRY_INTERVAL * HZ,
					 sclp_request_timeout, 1);
		return 0;
	} else if (rc == -EBUSY) {
		/* Try again later */
		__sclp_set_request_timer(SCLP_BUSY_INTERVAL * HZ,
					 sclp_request_timeout, 0);
		return 0;
	}
	/* Request failed */
	req->status = SCLP_REQ_FAILED;
	return rc;
}

/* Try to start queued requests. */
static void
sclp_process_queue(void)
{
	struct sclp_req *req;
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&sclp_lock, flags);
	if (sclp_running_state != sclp_running_state_idle) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return;
	}
	del_timer(&sclp_request_timer);
	while (!list_empty(&sclp_req_queue)) {
		req = list_entry(sclp_req_queue.next, struct sclp_req, list);
		rc = __sclp_start_request(req);
		if (rc == 0)
			break;
		/* Request failed. */
		list_del(&req->list);
		if (req->callback) {
			spin_unlock_irqrestore(&sclp_lock, flags);
			req->callback(req, req->callback_data);
			spin_lock_irqsave(&sclp_lock, flags);
		}
	}
	spin_unlock_irqrestore(&sclp_lock, flags);
}

/* Queue a new request. Return zero on success, non-zero otherwise. */
int
sclp_add_request(struct sclp_req *req)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	if ((sclp_init_state != sclp_init_state_initialized ||
	     sclp_activation_state != sclp_activation_state_active) &&
	    req != &sclp_init_req) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EIO;
	}
	req->status = SCLP_REQ_QUEUED;
	req->start_count = 0;
	list_add_tail(&req->list, &sclp_req_queue);
	rc = 0;
	/* Start if request is first in list */
	if (req->list.prev == &sclp_req_queue) {
		rc = __sclp_start_request(req);
		if (rc)
			list_del(&req->list);
	}
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

EXPORT_SYMBOL(sclp_add_request);

/* Dispatch events found in request buffer to registered listeners. Return 0
 * if all events were dispatched, non-zero otherwise. */
static int
sclp_dispatch_evbufs(struct sccb_header *sccb)
{
	unsigned long flags;
	struct evbuf_header *evbuf;
	struct list_head *l;
	struct sclp_register *reg;
	int offset;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	rc = 0;
	for (offset = sizeof(struct sccb_header); offset < sccb->length;
	     offset += evbuf->length) {
		/* Search for event handler */
		evbuf = (struct evbuf_header *) ((addr_t) sccb + offset);
		reg = NULL;
		list_for_each(l, &sclp_reg_list) {
			reg = list_entry(l, struct sclp_register, list);
			if (reg->receive_mask & (1 << (32 - evbuf->type)))
				break;
			else
				reg = NULL;
		}
		if (reg && reg->receiver_fn) {
			spin_unlock_irqrestore(&sclp_lock, flags);
			reg->receiver_fn(evbuf);
			spin_lock_irqsave(&sclp_lock, flags);
		} else if (reg == NULL)
			rc = -ENOSYS;
	}
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

/* Read event data request callback. */
static void
sclp_read_cb(struct sclp_req *req, void *data)
{
	unsigned long flags;
	struct sccb_header *sccb;

	sccb = (struct sccb_header *) req->sccb;
	if (req->status == SCLP_REQ_DONE && (sccb->response_code == 0x20 ||
	    sccb->response_code == 0x220))
		sclp_dispatch_evbufs(sccb);
	spin_lock_irqsave(&sclp_lock, flags);
	sclp_reading_state = sclp_reading_state_idle;
	spin_unlock_irqrestore(&sclp_lock, flags);
}

/* Prepare read event data request. Called while sclp_lock is locked. */
static inline void
__sclp_make_read_req(void)
{
	struct sccb_header *sccb;

	sccb = (struct sccb_header *) sclp_read_sccb;
	clear_page(sccb);
	memset(&sclp_read_req, 0, sizeof(struct sclp_req));
	sclp_read_req.command = SCLP_CMDW_READDATA;
	sclp_read_req.status = SCLP_REQ_QUEUED;
	sclp_read_req.start_count = 0;
	sclp_read_req.callback = sclp_read_cb;
	sclp_read_req.sccb = sccb;
	sccb->length = PAGE_SIZE;
	sccb->function_code = 0;
	sccb->control_mask[2] = 0x80;
}

/* Search request list for request with matching sccb. Return request if found,
 * NULL otherwise. Called while sclp_lock is locked. */
static inline struct sclp_req *
__sclp_find_req(u32 sccb)
{
	struct list_head *l;
	struct sclp_req *req;

	list_for_each(l, &sclp_req_queue) {
		req = list_entry(l, struct sclp_req, list);
		if (sccb == (u32) (addr_t) req->sccb)
				return req;
	}
	return NULL;
}

/* Handler for external interruption. Perform request post-processing.
 * Prepare read event data request if necessary. Start processing of next
 * request on queue. */
static void
sclp_interrupt_handler(struct pt_regs *regs, __u16 code)
{
	struct sclp_req *req;
	u32 finished_sccb;
	u32 evbuf_pending;

	spin_lock(&sclp_lock);
	finished_sccb = S390_lowcore.ext_params & 0xfffffff8;
	evbuf_pending = S390_lowcore.ext_params & 0x3;
	if (finished_sccb) {
		req = __sclp_find_req(finished_sccb);
		if (req) {
			/* Request post-processing */
			list_del(&req->list);
			req->status = SCLP_REQ_DONE;
			if (req->callback) {
				spin_unlock(&sclp_lock);
				req->callback(req, req->callback_data);
				spin_lock(&sclp_lock);
			}
		}
		sclp_running_state = sclp_running_state_idle;
	}
	if (evbuf_pending && sclp_receive_mask != 0 &&
	    sclp_reading_state == sclp_reading_state_idle &&
	    sclp_activation_state == sclp_activation_state_active ) {
		sclp_reading_state = sclp_reading_state_reading;
		__sclp_make_read_req();
		/* Add request to head of queue */
		list_add(&sclp_read_req.list, &sclp_req_queue);
	}
	spin_unlock(&sclp_lock);
	sclp_process_queue();
}

/* Return current Time-Of-Day clock. */
static inline u64
sclp_get_clock(void)
{
	u64 result;

	asm volatile ("STCK 0(%1)" : "=m" (result) : "a" (&(result)) : "cc");
	return result;
}

/* Convert interval in jiffies to TOD ticks. */
static inline u64
sclp_tod_from_jiffies(unsigned long jiffies)
{
	return (u64) (jiffies / HZ) << 32;
}

/* Wait until a currently running request finished. Note: while this function
 * is running, no timers are served on the calling CPU. */
void
sclp_sync_wait(void)
{
	unsigned long psw_mask;
	unsigned long cr0, cr0_sync;
	u64 timeout;

	/* We'll be disabling timer interrupts, so we need a custom timeout
	 * mechanism */
	timeout = 0;
	if (timer_pending(&sclp_request_timer)) {
		/* Get timeout TOD value */
		timeout = sclp_get_clock() +
			  sclp_tod_from_jiffies(sclp_request_timer.expires -
						jiffies);
	}
	/* Prevent bottom half from executing once we force interrupts open */
	local_bh_disable();
	/* Enable service-signal interruption, disable timer interrupts */
	__ctl_store(cr0, 0, 0);
	cr0_sync = cr0;
	cr0_sync |= 0x00000200;
	cr0_sync &= 0xFFFFF3AC;
	__ctl_load(cr0_sync, 0, 0);
	asm volatile ("STOSM 0(%1),0x01"
		      : "=m" (psw_mask) : "a" (&psw_mask) : "memory");
	/* Loop until driver state indicates finished request */
	while (sclp_running_state != sclp_running_state_idle) {
		/* Check for expired request timer */
		if (timer_pending(&sclp_request_timer) &&
		    sclp_get_clock() > timeout &&
		    del_timer(&sclp_request_timer))
			sclp_request_timer.function(sclp_request_timer.data);
		barrier();
		cpu_relax();
	}
	/* Restore interrupt settings */
	asm volatile ("SSM 0(%0)"
		      : : "a" (&psw_mask) : "memory");
	__ctl_load(cr0, 0, 0);
	__local_bh_enable();
}

EXPORT_SYMBOL(sclp_sync_wait);

/* Dispatch changes in send and receive mask to registered listeners. */
static inline void
sclp_dispatch_state_change(void)
{
	struct list_head *l;
	struct sclp_register *reg;
	unsigned long flags;
	sccb_mask_t receive_mask;
	sccb_mask_t send_mask;

	do {
		spin_lock_irqsave(&sclp_lock, flags);
		reg = NULL;
		list_for_each(l, &sclp_reg_list) {
			reg = list_entry(l, struct sclp_register, list);
			receive_mask = reg->receive_mask & sclp_receive_mask;
			send_mask = reg->send_mask & sclp_send_mask;
			if (reg->sclp_receive_mask != receive_mask ||
			    reg->sclp_send_mask != send_mask) {
				reg->sclp_receive_mask = receive_mask;
				reg->sclp_send_mask = send_mask;
				break;
			} else
				reg = NULL;
		}
		spin_unlock_irqrestore(&sclp_lock, flags);
		if (reg && reg->state_change_fn)
			reg->state_change_fn(reg);
	} while (reg);
}

struct sclp_statechangebuf {
	struct evbuf_header	header;
	u8		validity_sclp_active_facility_mask : 1;
	u8		validity_sclp_receive_mask : 1;
	u8		validity_sclp_send_mask : 1;
	u8		validity_read_data_function_mask : 1;
	u16		_zeros : 12;
	u16		mask_length;
	u64		sclp_active_facility_mask;
	sccb_mask_t	sclp_receive_mask;
	sccb_mask_t	sclp_send_mask;
	u32		read_data_function_mask;
} __attribute__((packed));


/* State change event callback. Inform listeners of changes. */
static void
sclp_state_change_cb(struct evbuf_header *evbuf)
{
	unsigned long flags;
	struct sclp_statechangebuf *scbuf;

	scbuf = (struct sclp_statechangebuf *) evbuf;
	if (scbuf->mask_length != sizeof(sccb_mask_t))
		return;
	spin_lock_irqsave(&sclp_lock, flags);
	if (scbuf->validity_sclp_receive_mask)
		sclp_receive_mask = scbuf->sclp_receive_mask;
	if (scbuf->validity_sclp_send_mask)
		sclp_send_mask = scbuf->sclp_send_mask;
	spin_unlock_irqrestore(&sclp_lock, flags);
	sclp_dispatch_state_change();
}

static struct sclp_register sclp_state_change_event = {
	.receive_mask = EvTyp_StateChange_Mask,
	.receiver_fn = sclp_state_change_cb
};

/* Calculate receive and send mask of currently registered listeners.
 * Called while sclp_lock is locked. */
static inline void
__sclp_get_mask(sccb_mask_t *receive_mask, sccb_mask_t *send_mask)
{
	struct list_head *l;
	struct sclp_register *t;

	*receive_mask = 0;
	*send_mask = 0;
	list_for_each(l, &sclp_reg_list) {
		t = list_entry(l, struct sclp_register, list);
		*receive_mask |= t->receive_mask;
		*send_mask |= t->send_mask;
	}
}

/* Register event listener. Return 0 on success, non-zero otherwise. */
int
sclp_register(struct sclp_register *reg)
{
	unsigned long flags;
	sccb_mask_t receive_mask;
	sccb_mask_t send_mask;
	int rc;

	rc = sclp_init();
	if (rc)
		return rc;
	spin_lock_irqsave(&sclp_lock, flags);
	/* Check event mask for collisions */
	__sclp_get_mask(&receive_mask, &send_mask);
	if (reg->receive_mask & receive_mask || reg->send_mask & send_mask) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EBUSY;
	}
	/* Trigger initial state change callback */
	reg->sclp_receive_mask = 0;
	reg->sclp_send_mask = 0;
	list_add(&reg->list, &sclp_reg_list);
	spin_unlock_irqrestore(&sclp_lock, flags);
	rc = sclp_init_mask(1);
	if (rc) {
		spin_lock_irqsave(&sclp_lock, flags);
		list_del(&reg->list);
		spin_unlock_irqrestore(&sclp_lock, flags);
	}
	return rc;
}

EXPORT_SYMBOL(sclp_register);

/* Unregister event listener. */
void
sclp_unregister(struct sclp_register *reg)
{
	unsigned long flags;

	spin_lock_irqsave(&sclp_lock, flags);
	list_del(&reg->list);
	spin_unlock_irqrestore(&sclp_lock, flags);
	sclp_init_mask(1);
}

EXPORT_SYMBOL(sclp_unregister);

/* Remove event buffers which are marked processed. Return the number of
 * remaining event buffers. */
int
sclp_remove_processed(struct sccb_header *sccb)
{
	struct evbuf_header *evbuf;
	int unprocessed;
	u16 remaining;

	evbuf = (struct evbuf_header *) (sccb + 1);
	unprocessed = 0;
	remaining = sccb->length - sizeof(struct sccb_header);
	while (remaining > 0) {
		remaining -= evbuf->length;
		if (evbuf->flags & 0x80) {
			sccb->length -= evbuf->length;
			memcpy(evbuf, (void *) ((addr_t) evbuf + evbuf->length),
			       remaining);
		} else {
			unprocessed++;
			evbuf = (struct evbuf_header *)
					((addr_t) evbuf + evbuf->length);
		}
	}
	return unprocessed;
}

EXPORT_SYMBOL(sclp_remove_processed);

struct init_sccb {
	struct sccb_header header;
	u16 _reserved;
	u16 mask_length;
	sccb_mask_t receive_mask;
	sccb_mask_t send_mask;
	sccb_mask_t sclp_send_mask;
	sccb_mask_t sclp_receive_mask;
} __attribute__((packed));

/* Prepare init mask request. Called while sclp_lock is locked. */
static inline void
__sclp_make_init_req(u32 receive_mask, u32 send_mask)
{
	struct init_sccb *sccb;

	sccb = (struct init_sccb *) sclp_init_sccb;
	clear_page(sccb);
	memset(&sclp_init_req, 0, sizeof(struct sclp_req));
	sclp_init_req.command = SCLP_CMDW_WRITEMASK;
	sclp_init_req.status = SCLP_REQ_FILLED;
	sclp_init_req.start_count = 0;
	sclp_init_req.callback = NULL;
	sclp_init_req.callback_data = NULL;
	sclp_init_req.sccb = sccb;
	sccb->header.length = sizeof(struct init_sccb);
	sccb->mask_length = sizeof(sccb_mask_t);
	sccb->receive_mask = receive_mask;
	sccb->send_mask = send_mask;
	sccb->sclp_receive_mask = 0;
	sccb->sclp_send_mask = 0;
}

/* Start init mask request. If calculate is non-zero, calculate the mask as
 * requested by registered listeners. Use zero mask otherwise. Return 0 on
 * success, non-zero otherwise. */
static int
sclp_init_mask(int calculate)
{
	unsigned long flags;
	struct init_sccb *sccb = (struct init_sccb *) sclp_init_sccb;
	sccb_mask_t receive_mask;
	sccb_mask_t send_mask;
	int retry;
	int rc;
	unsigned long wait;

	spin_lock_irqsave(&sclp_lock, flags);
	/* Check if interface is in appropriate state */
	if (sclp_mask_state != sclp_mask_state_idle) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EBUSY;
	}
	if (sclp_activation_state == sclp_activation_state_inactive) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EINVAL;
	}
	sclp_mask_state = sclp_mask_state_initializing;
	/* Determine mask */
	if (calculate)
		__sclp_get_mask(&receive_mask, &send_mask);
	else {
		receive_mask = 0;
		send_mask = 0;
	}
	rc = -EIO;
	for (retry = 0; retry <= SCLP_MASK_RETRY; retry++) {
		/* Prepare request */
		__sclp_make_init_req(receive_mask, send_mask);
		spin_unlock_irqrestore(&sclp_lock, flags);
		if (sclp_add_request(&sclp_init_req)) {
			/* Try again later */
			wait = jiffies + SCLP_BUSY_INTERVAL * HZ;
			while (time_before(jiffies, wait))
				sclp_sync_wait();
			spin_lock_irqsave(&sclp_lock, flags);
			continue;
		}
		while (sclp_init_req.status != SCLP_REQ_DONE &&
		       sclp_init_req.status != SCLP_REQ_FAILED)
			sclp_sync_wait();
		spin_lock_irqsave(&sclp_lock, flags);
		if (sclp_init_req.status == SCLP_REQ_DONE &&
		    sccb->header.response_code == 0x20) {
			/* Successful request */
			if (calculate) {
				sclp_receive_mask = sccb->sclp_receive_mask;
				sclp_send_mask = sccb->sclp_send_mask;
			} else {
				sclp_receive_mask = 0;
				sclp_send_mask = 0;
			}
			spin_unlock_irqrestore(&sclp_lock, flags);
			sclp_dispatch_state_change();
			spin_lock_irqsave(&sclp_lock, flags);
			rc = 0;
			break;
		}
	}
	sclp_mask_state = sclp_mask_state_idle;
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

/* Deactivate SCLP interface. On success, new requests will be rejected,
 * events will no longer be dispatched. Return 0 on success, non-zero
 * otherwise. */
int
sclp_deactivate(void)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	/* Deactivate can only be called when active */
	if (sclp_activation_state != sclp_activation_state_active) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EINVAL;
	}
	sclp_activation_state = sclp_activation_state_deactivating;
	spin_unlock_irqrestore(&sclp_lock, flags);
	rc = sclp_init_mask(0);
	spin_lock_irqsave(&sclp_lock, flags);
	if (rc == 0)
		sclp_activation_state = sclp_activation_state_inactive;
	else
		sclp_activation_state = sclp_activation_state_active;
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

EXPORT_SYMBOL(sclp_deactivate);

/* Reactivate SCLP interface after sclp_deactivate. On success, new
 * requests will be accepted, events will be dispatched again. Return 0 on
 * success, non-zero otherwise. */
int
sclp_reactivate(void)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	/* Reactivate can only be called when inactive */
	if (sclp_activation_state != sclp_activation_state_inactive) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EINVAL;
	}
	sclp_activation_state = sclp_activation_state_activating;
	spin_unlock_irqrestore(&sclp_lock, flags);
	rc = sclp_init_mask(1);
	spin_lock_irqsave(&sclp_lock, flags);
	if (rc == 0)
		sclp_activation_state = sclp_activation_state_active;
	else
		sclp_activation_state = sclp_activation_state_inactive;
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

EXPORT_SYMBOL(sclp_reactivate);

/* Handler for external interruption used during initialization. Modify
 * request state to done. */
static void
sclp_check_handler(struct pt_regs *regs, __u16 code)
{
	u32 finished_sccb;

	finished_sccb = S390_lowcore.ext_params & 0xfffffff8;
	/* Is this the interrupt we are waiting for? */
	if (finished_sccb == 0)
		return;
	if (finished_sccb != (u32) (addr_t) sclp_init_sccb) {
		printk(KERN_WARNING SCLP_HEADER "unsolicited interrupt "
		       "for buffer at 0x%x\n", finished_sccb);
		return;
	}
	spin_lock(&sclp_lock);
	if (sclp_running_state == sclp_running_state_running) {
		sclp_init_req.status = SCLP_REQ_DONE;
		sclp_running_state = sclp_running_state_idle;
	}
	spin_unlock(&sclp_lock);
}

/* Initial init mask request timed out. Modify request state to failed. */
static void
sclp_check_timeout(unsigned long data)
{
	unsigned long flags;

	spin_lock_irqsave(&sclp_lock, flags);
	if (sclp_running_state == sclp_running_state_running) {
		sclp_init_req.status = SCLP_REQ_FAILED;
		sclp_running_state = sclp_running_state_idle;
	}
	spin_unlock_irqrestore(&sclp_lock, flags);
}

/* Perform a check of the SCLP interface. Return zero if the interface is
 * available and there are no pending requests from a previous instance.
 * Return non-zero otherwise. */
static int
sclp_check_interface(void)
{
	struct init_sccb *sccb;
	unsigned long flags;
	int retry;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	/* Prepare init mask command */
	rc = register_early_external_interrupt(0x2401, sclp_check_handler,
					       &ext_int_info_hwc);
	if (rc) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return rc;
	}
	for (retry = 0; retry <= SCLP_INIT_RETRY; retry++) {
		__sclp_make_init_req(0, 0);
		sccb = (struct init_sccb *) sclp_init_req.sccb;
		rc = service_call(sclp_init_req.command, sccb);
		if (rc == -EIO)
			break;
		sclp_init_req.status = SCLP_REQ_RUNNING;
		sclp_running_state = sclp_running_state_running;
		__sclp_set_request_timer(SCLP_RETRY_INTERVAL * HZ,
					 sclp_check_timeout, 0);
		spin_unlock_irqrestore(&sclp_lock, flags);
		/* Enable service-signal interruption - needs to happen
		 * with IRQs enabled. */
		ctl_set_bit(0, 9);
		/* Wait for signal from interrupt or timeout */
		sclp_sync_wait();
		/* Disable service-signal interruption - needs to happen
		 * with IRQs enabled. */
		ctl_clear_bit(0,9);
		spin_lock_irqsave(&sclp_lock, flags);
		del_timer(&sclp_request_timer);
		if (sclp_init_req.status == SCLP_REQ_DONE &&
		    sccb->header.response_code == 0x20) {
			rc = 0;
			break;
		} else
			rc = -EBUSY;
	}
	unregister_early_external_interrupt(0x2401, sclp_check_handler,
					    &ext_int_info_hwc);
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

/* Reboot event handler. Reset send and receive mask to prevent pending SCLP
 * events from interfering with rebooted system. */
static int
sclp_reboot_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	sclp_deactivate();
	return NOTIFY_DONE;
}

static struct notifier_block sclp_reboot_notifier = {
	.notifier_call = sclp_reboot_event
};

/* Initialize SCLP driver. Return zero if driver is operational, non-zero
 * otherwise. */
static int
sclp_init(void)
{
	unsigned long flags;
	int rc;

	if (!MACHINE_HAS_SCLP)
		return -ENODEV;
	spin_lock_irqsave(&sclp_lock, flags);
	/* Check for previous or running initialization */
	if (sclp_init_state != sclp_init_state_uninitialized) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return 0;
	}
	sclp_init_state = sclp_init_state_initializing;
	/* Set up variables */
	INIT_LIST_HEAD(&sclp_req_queue);
	INIT_LIST_HEAD(&sclp_reg_list);
	list_add(&sclp_state_change_event.list, &sclp_reg_list);
	init_timer(&sclp_request_timer);
	/* Check interface */
	spin_unlock_irqrestore(&sclp_lock, flags);
	rc = sclp_check_interface();
	spin_lock_irqsave(&sclp_lock, flags);
	if (rc) {
		sclp_init_state = sclp_init_state_uninitialized;
		spin_unlock_irqrestore(&sclp_lock, flags);
		return rc;
	}
	/* Register reboot handler */
	rc = register_reboot_notifier(&sclp_reboot_notifier);
	if (rc) {
		sclp_init_state = sclp_init_state_uninitialized;
		spin_unlock_irqrestore(&sclp_lock, flags);
		return rc;
	}
	/* Register interrupt handler */
	rc = register_early_external_interrupt(0x2401, sclp_interrupt_handler,
					       &ext_int_info_hwc);
	if (rc) {
		unregister_reboot_notifier(&sclp_reboot_notifier);
		sclp_init_state = sclp_init_state_uninitialized;
		spin_unlock_irqrestore(&sclp_lock, flags);
		return rc;
	}
	sclp_init_state = sclp_init_state_initialized;
	spin_unlock_irqrestore(&sclp_lock, flags);
	/* Enable service-signal external interruption - needs to happen with
	 * IRQs enabled. */
	ctl_set_bit(0, 9);
	sclp_init_mask(1);
	return 0;
}
