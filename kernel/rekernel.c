/*
 * Re:Kernel - kernel side notifier for userspace app freezers.
 *
 * Single compilation unit: all state lives here. Binder/signal hooks call
 * rekernel_active() and rekernel_send_message() (see <linux/rekernel.h>).
 *
 * The netlink server is created once at boot (late_initcall) instead of
 * lazily from the hooks, because do_send_sig_info() may run in atomic
 * context where netlink_kernel_create()/proc_mkdir() must not be called.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/netlink.h>
#include <linux/rekernel.h>
#include <net/net_namespace.h>
#include <net/netlink.h>
#include <net/sock.h>
#include <linux/module.h>

#define NETLINK_REKERNEL_MAX	26
#define NETLINK_REKERNEL_MIN	22
#define USER_PORT		100

static struct sock *rekernel_netlink;
static int netlink_unit = NETLINK_REKERNEL_MIN;
static struct proc_dir_entry *rekernel_dir;

bool rekernel_active(void)
{
	return READ_ONCE(rekernel_netlink) != NULL;
}

/* Non-blocking and GFP_ATOMIC: safe to call from the hooks, even atomic ones. */
int rekernel_send_message(char *msg, uint16_t len)
{
	struct sock *sk = READ_ONCE(rekernel_netlink);
	struct sk_buff *skbuffer;
	struct nlmsghdr *nlhdr;

	if (!sk)
		return -1;

	skbuffer = nlmsg_new(len, GFP_ATOMIC);
	if (!skbuffer) {
		printk("netlink alloc failure.\n");
		return -1;
	}

	nlhdr = nlmsg_put(skbuffer, 0, 0, netlink_unit, len, 0);
	if (!nlhdr) {
		printk("nlmsg_put failaure.\n");
		nlmsg_free(skbuffer);
		return -1;
	}

	memcpy(nlmsg_data(nlhdr), msg, len);
	return netlink_unicast(sk, skbuffer, USER_PORT, MSG_DONTWAIT);
}

static void netlink_rcv_msg(struct sk_buff *skbuffer)
{
	/* Ignore messages from userspace. */
}

static struct netlink_kernel_cfg rekernel_cfg = {
	.input = netlink_rcv_msg,
};

static int rekernel_unit_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", netlink_unit);
	return 0;
}

static int rekernel_unit_open(struct inode *inode, struct file *file)
{
	return single_open(file, rekernel_unit_show, NULL);
}

static const struct file_operations rekernel_unit_fops = {
	.open		= rekernel_unit_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.owner		= THIS_MODULE,
};

static int __init rekernel_init(void)
{
	struct sock *sk = NULL;

	for (netlink_unit = NETLINK_REKERNEL_MIN;
	     netlink_unit < NETLINK_REKERNEL_MAX; netlink_unit++) {
		sk = netlink_kernel_create(&init_net, netlink_unit, &rekernel_cfg);
		if (sk)
			break;
	}
	if (!sk) {
		printk("Failed to create Re:Kernel server!\n");
		return -ENOMEM;
	}

	rekernel_dir = proc_mkdir("rekernel", NULL);
	if (!rekernel_dir) {
		printk("create /proc/rekernel failed!\n");
	} else {
		char buff[8];

		snprintf(buff, sizeof(buff), "%d", netlink_unit);
		if (!proc_create(buff, 0444, rekernel_dir, &rekernel_unit_fops))
			printk("create rekernel unit failed!\n");
	}

	/* Publish last: hooks only act once everything above is ready. */
	WRITE_ONCE(rekernel_netlink, sk);
	printk("Created Re:Kernel server! NETLINK UNIT: %d\n", netlink_unit);
	return 0;
}
late_initcall(rekernel_init);
