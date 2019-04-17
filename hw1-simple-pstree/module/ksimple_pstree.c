#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/list.h>
#define NETLINK_USER 31

struct sock *nl_sk = NULL;
void DFS(struct task_struct *task, char *msg, int tab)
{
    if(task == NULL) {
        printk(KERN_INFO "PID not found");
        msg = " \n";
        return;
    }
    int i;
    for (i = 0; i < tab; i++) {
        msg = strcat(msg, "    ");
    }
    struct task_struct *task_next;
    struct list_head *list = NULL;
    char str[40];
    sprintf(str, "%s(%d)\n", task->comm, task->pid);
    msg = strcat(msg, str);

    list_for_each(list, &task->children) {
        task_next = list_entry(list, struct task_struct, sibling);
        tab++;
        DFS(task_next, msg, tab);
        tab--;
    }
}
void find_sibling(struct task_struct *task, char *msg)
{
    if(task == NULL) {
        printk(KERN_INFO "PID not found");
        return;
    }
    struct task_struct *task_next;
    struct list_head *list = NULL;
    char str[40];
    //sprintf(str, "%s(%d)\n", task->comm, task->pid);
    //msg = strcat(msg, str);
    list_for_each(list, &task->parent->children) {
        task_next = list_entry(list, struct task_struct, sibling);

        sprintf(str, "%s(%d)\n", task_next->comm, task_next->pid);
        msg = strcat(msg, str);
        //}
    }
}

int DFS_parent(struct task_struct *task, char *msg, int tab)
{
    if (task == NULL) {
        printk(KERN_INFO "PID not found");
        return;
    }
    int i;
    struct task_struct *task_next;
    struct list_head *list = NULL;
    char str[40];
    //sprintf(str, "%s(%d)\n", task->comm, task->pid);
    //msg = strcat(msg, str);
    //printk(KERN_INFO "breakpoint 1");
    if(task->pid == 1) {
        sprintf(str, "%s(%d)\n", task->comm, task->pid);
        msg = strcat(msg, str);
        return tab + 1;
        // DFS_parent(task_next, msg);
    } else {
        task_next = task->parent;
        tab = DFS_parent(task_next, msg, tab);
        int i;
        for (i = 0; i < tab; i++) {
            strcat(msg, "    ");
        }
        sprintf(str, "%s(%d)\n", task->comm, task->pid);
        msg = strcat(msg, str);
        return tab + 1;
    }
}
static void nl_recv_msg(struct sk_buff *skb)
{
    printk(KERN_INFO "Entering: %s\n", __FUNCTION__);
    struct nlmsghdr *nlh;
    int pid;
    struct sk_buff *skb_out;
    int msg_size;
    char *msg = (char *)vmalloc(sizeof(char) * 10240);
    char *str;
    int res;
    char option;
    int query_pid;
    nlh = (struct nlmsghdr *)skb->data;
    pid = nlh->nlmsg_pid; /*pid of sending process */
    str = (char *)nlmsg_data(nlh);
    //if(str[0] == '\0')
    //    printk(KERN_INFO "0");
    printk(KERN_INFO "string: %s", str);

    if(str[0] == '-') {
        option = str[1];
        if(str[2] != '\0')
            kstrtoint(str+2, 10, &query_pid);
        else if (option == 'c')
            query_pid = 1;
        else if (option == 's')
            query_pid = current->pid;
        else if (option == 'p')
            query_pid = current->pid;
        else
            printk(KERN_ERR "ERROR option\n");
    } else {
        if(str[0] == '\0') {
            option = 'c';
            query_pid = 1;
        } else {
            option = 'c'; //default;
            kstrtoint(str, 10, &query_pid);
        }
    }

    struct pid *pid_struct = find_get_pid(query_pid);
    struct task_struct *task = pid_task(pid_struct, PIDTYPE_PID);
    printk(KERN_INFO "option: %c", option);
    printk(KERN_INFO "query_pid: %d", query_pid);

    if(option=='c') {
        DFS(task, msg, 0);
    } else if(option == 's') {
        if(query_pid == current->pid) {
            msg = "";
        } else find_sibling(task, msg);
    } else if (option == 'p') {
        DFS_parent(task, msg, 0);
    } else {
        msg = "";
    }

    msg_size = strlen(msg);
    printk(KERN_INFO "msg length: %d\n", msg_size);
    //printk(KERN_INFO "Netlink received msg payload:%s\n", (char *)nlmsg_data(nlh));

    skb_out = nlmsg_new(msg_size, 0);
    if (!skb_out) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }

    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
    strncpy(nlmsg_data(nlh), msg, msg_size);

    res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0)
        printk(KERN_INFO "Error while sending bak to user\n");
}

static int __init pstree_init(void)
{
    printk("Entering: %s\n", __FUNCTION__);

    struct netlink_kernel_cfg cfg = {
        .input = nl_recv_msg,
    };

    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (!nl_sk) {
        printk(KERN_ALERT "Error creating socket.\n");
        return -10;
    }

    return 0;
}

static void __exit pstree_exit(void)
{

    printk(KERN_INFO "exiting hello module\n");
    netlink_kernel_release(nl_sk);
}

module_init(pstree_init);
module_exit(pstree_exit);

MODULE_LICENSE("GPL");
