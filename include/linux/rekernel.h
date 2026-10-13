#ifndef _LINUX_REKERNEL_H
#define _LINUX_REKERNEL_H

#include <linux/cred.h>
#include <linux/string.h>

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/freezer.h>

#define MIN_USERAPP_UID		(10000)
#define MAX_SYSTEM_UID		(2000)
#define PACKET_SIZE		128
#define RESERVE_ORDER		17
#define WARN_AHEAD_SPACE	(1 << RESERVE_ORDER)

/* Implemented in kernel/rekernel.c */
bool rekernel_active(void);
int rekernel_send_message(char *msg, uint16_t len);

static inline bool line_is_frozen(struct task_struct *task)
{
	return frozen(task->group_leader) || freezing(task->group_leader);
}

#endif /* _LINUX_REKERNEL_H */
