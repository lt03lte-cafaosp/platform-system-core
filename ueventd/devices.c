/*
 * Copyright (c) 2009, 2016 The Linux Foundation. All rights reserved.
 * Not a contribution

 * ncr
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <linux/netlink.h>
#include <private/android_filesystem_config.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "devices.h"
#include "util.h"
#include "log.h"


#define SYSFS_PREFIX    "/sys"
#define FIRMWARE_DIR1   "/etc/firmware"
#define FIRMWARE_DIR2   "/vendor/firmware"

static int device_fd = -1;
static int u_disk_state = 0;
static pid_t mtp_pid = 0;

struct uevent {
    const char *action;
    const char *path;
    const char *subsystem;
    const char *usb_state;
    const char *firmware;
    const char *partition_name;
    int partition_num;
    int major;
    int minor;
};

static int open_uevent_socket(void)
{
    struct sockaddr_nl addr;
    int sz = 64*1024; // XXX larger? udev uses 16MB!
    int on = 1;
    int s;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if(s < 0)
        return -1;

    setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));
    setsockopt(s, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

    if(bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }

    return s;
}

#if LOG_UEVENTS

static inline suseconds_t get_usecs(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * (suseconds_t) 1000000 + tv.tv_usec;
}

#define log_event_print(x...) INFO(x)

#else

#define log_event_print(fmt, args...)   do { } while (0)
#define get_usecs()                     0

#endif

static void parse_event(const char *msg, struct uevent *uevent)
{
    uevent->action = "";
    uevent->path = "";
    uevent->subsystem = "";
    uevent->usb_state = "";
    uevent->firmware = "";
    uevent->major = -1;
    uevent->minor = -1;
    uevent->partition_name = NULL;
    uevent->partition_num = -1;

        /* currently ignoring SEQNUM */
    while(*msg) {
        if(!strncmp(msg, "ACTION=", 7)) {
            msg += 7;
            uevent->action = msg;
        } else if(!strncmp(msg, "DEVPATH=", 8)) {
            msg += 8;
            uevent->path = msg;
        } else if(!strncmp(msg, "SUBSYSTEM=", 10)) {
            msg += 10;
            uevent->subsystem = msg;
        } else if(!strncmp(msg, "USB_STATE=", 10)) {
            msg += 10;
            uevent->usb_state = msg;
        } else if(!strncmp(msg, "FIRMWARE=", 9)) {
            msg += 9;
            uevent->firmware = msg;
        } else if(!strncmp(msg, "MAJOR=", 6)) {
            msg += 6;
            uevent->major = atoi(msg);
        } else if(!strncmp(msg, "MINOR=", 6)) {
            msg += 6;
            uevent->minor = atoi(msg);
        } else if(!strncmp(msg, "PARTN=", 6)) {
            msg += 6;
            uevent->partition_num = atoi(msg);
        } else if(!strncmp(msg, "PARTNAME=", 9)) {
            msg += 9;
            uevent->partition_name = msg;
        }

            /* advance to after the next \0 */
        while(*msg++)
            ;
    }

    log_event_print("event { '%s', '%s', '%s', '%s', %d, %d }\n",
                    uevent->action, uevent->path, uevent->subsystem,
                    uevent->firmware, uevent->major, uevent->minor);
}

static char **parse_platform_block_device(struct uevent *uevent)
{
    const char *driver;
    const char *path;
    char *slash;
    int width;
    char buf[256];
    char link_path[256];
    int fd;
    int link_num = 0;
    int ret;
    char *p;
    unsigned int size;
    struct stat info;

    char **links = malloc(sizeof(char *) * 4);
    if (!links)
        return NULL;
    memset(links, 0, sizeof(char *) * 4);

    /* Drop "/devices/platform/" */
    path = uevent->path;
    driver = path + 18;
    slash = strchr(driver, '/');
    if (!slash)
        goto err;
    width = slash - driver;
    if (width <= 0)
        goto err;

    snprintf(link_path, sizeof(link_path), "/dev/block/platform/%.*s",
             width, driver);

    if (uevent->partition_name) {
        p = strdup(uevent->partition_name);
        sanitize(p);
        if (asprintf(&links[link_num], "%s/by-name/%s", link_path, p) > 0)
            link_num++;
        else
            links[link_num] = NULL;
        free(p);
    }

    if (uevent->partition_num >= 0) {
        if (asprintf(&links[link_num], "%s/by-num/p%d", link_path, uevent->partition_num) > 0)
            link_num++;
        else
            links[link_num] = NULL;
    }

    slash = strrchr(path, '/');
    if (asprintf(&links[link_num], "%s/%s", link_path, slash + 1) > 0)
        link_num++;
    else
        links[link_num] = NULL;

    return links;

err:
    free(links);
    return NULL;
}

static int write_to_file(const char *path, const char *value)
{
    int fd, ret, len;

    fd = open(path, O_WRONLY);

    if (fd < 0)
        return -errno;

    len = strlen(value);

    do {
        ret = write(fd, value, len);
    } while (ret < 0 && errno == EINTR);

    close(fd);
    if (ret < 0)  
    return -errno;
    else 
        return 0;
}

static int read_from_file(const char *path, char *buf, ssize_t count)
{
    int fd, ret;
    ssize_t pos = 0;
    ssize_t rv = 0;

    fd = open(path, O_RDONLY);

    if (fd < 0)
        return -errno;

    do {
    pos += rv;
    rv = read(fd, buf + pos, count - pos);
    } while (rv > 0);

    close(fd);
    return (rv < 0) ? -errno : rv;
}

void check_init_state(void)
{
    char data[2];
    struct stat info;

    if (check_mtp_mode() == 0) {
        if (access(block_path, W_OK) < 0) {
            printf("block path not writable\n");
            return;
        }

        /* sd plug-in? */
        if (stat(sd_card, &info) < 0)
            return;

        /* sd card has been mounted?  */
        if(u_disk_state)
            return;

        if (access(usb_power_path, R_OK) < 0) {
            printf("power supply path not readable\n");
            return;
        }

        /* use usb power state to judge if usb is present */
        read_from_file(usb_power_path, data, 1);
        data[1] = '\0';
        if(!strncmp(data, "1", 1))
            handle_sd_plug_in_out(1);
        else if(!strncmp(data, "0", 1))
            handle_sd_plug_in_out(0);
        else
            return;
    } else {
        /* use usb composite dirver state to judge if mtp driver is present */
        if(check_usb_status() == 1)
            handle_mtp_plug_in_out(1);
        else
            handle_mtp_plug_in_out(0);
        return;
    }
}

/* 1: in,  0: out */
void handle_sd_plug_in_out(int in_out)
{
    int i;
    struct stat info;
    char emp_str[2] = " ";

    if (access(block_path, W_OK) < 0) {
        printf("block path not writable\n");
        return;
    }

    if(in_out) {
        if (stat(sd_card, &info) < 0)
            return;

        /* sd card has been mounted?  */
        if(u_disk_state)
            return;

        i = write_to_file(block_path, sd_card);
        if (i < 0 ) {
            printf("add sd failed, ret = %d\n", i);
        } else {
            u_disk_state = 1;
        }
    } else {
        //TODO, there is an I/O error here, we can use this error to write empty string
        i = write_to_file(block_path, emp_str);
        u_disk_state = 0;
    }
}

/* 1: in,  0: out */
static void handle_mtp_plug_in_out(int in_out)
{
    if (in_out) {
        printf("run mtpserver\n");
        mtp_pid = fork();
        if(!mtp_pid) {
            execl("/usr/bin/mtpserver", "mtpserver", NULL);
            exit(-1);
        } else if (mtp_pid < 0) {
            printf("could not create child process to execute mtp server module\n");
        }
    } else if (in_out == 0 && mtp_pid > 0) {
        kill(mtp_pid, SIGTERM);
        waitpid(mtp_pid, NULL, 0);
        mtp_pid = 0;
    }
    return;
}

static int check_mtp_mode(void)
{
    FILE *fd;
    char buffer[128];
    int mtp_flag = 0;

    fd = fopen("/sys/class/android_usb/android0/functions", "r");
    while (fgets(buffer, 128, fd)) {
        if (strstr(buffer, "mtp") != NULL) {
            mtp_flag = 1;
            break;
        }
    }
    fclose(fd);
    return mtp_flag;
}

static int check_usb_status(void)
{
    FILE *fp;
    char buffer[16];
    int usb_status = 0;

    fp = fopen("/sys/class/android_usb/android0/state", "r");
    while(fgets(buffer, 16, fp)) {
        if (strstr(buffer, "CONFIGURED") != NULL) {
            usb_status = 1;
            break;
        }
    }
    fclose(fp);
    return usb_status;
}

static void handle_device_event(struct uevent *uevent)
{
    char devpath[96];
    int devpath_ready = 0;
    char *base, *name;
    char **links = NULL;

    /* do we have a name? */
    name = strrchr(uevent->path, '/');
    if(!name)
        return;
    name++;

    if (!strcmp(uevent->action,"add")){
        /* add block */
        if(!strncmp(uevent->subsystem, "block", 5) && (check_mtp_mode() == 0)) {
            /* SD card plug in */
            if(!strncmp(name, "mmcblk1p1", 9)) {
                handle_sd_plug_in_out(1);
                return;
            }
        }
    }
    if(!strcmp(uevent->action, "change")) {
        if(check_mtp_mode() != 0) { // mtp mode
	    if (!strncmp(uevent->usb_state, "CONFIGURED", 10)) {//USB has been configured
                /* USB plug in */
                handle_mtp_plug_in_out(1);
            } else if (!strncmp(uevent->usb_state, "DISCONNECTED", 12)) {//USB has been disconnected
                /* USB plug out */
                handle_mtp_plug_in_out(0);
            }
        }
    }

    if(!strcmp(uevent->action, "remove")) {
        /* add block */
        if(!strncmp(uevent->subsystem, "block", 5)) {
            /* SD card plug out*/
            if(!strncmp(name, "mmcblk1p1", 9) && (check_mtp_mode() == 0)) {
                handle_sd_plug_in_out(0);
                return;
            }
        }
    }
}

static int load_firmware(int fw_fd, int loading_fd, int data_fd)
{
    return 0;
}

static void process_firmware_event(struct uevent *uevent)
{
    char *root, *loading, *data, *file1 = NULL, *file2 = NULL;
    int l, loading_fd, data_fd, fw_fd;

    log_event_print("firmware event { '%s', '%s' }\n",
                    uevent->path, uevent->firmware);

    l = asprintf(&root, SYSFS_PREFIX"%s/", uevent->path);
    if (l == -1)
        return;

    l = asprintf(&loading, "%sloading", root);
    if (l == -1)
        goto root_free_out;

    l = asprintf(&data, "%sdata", root);
    if (l == -1)
        goto loading_free_out;

    l = asprintf(&file1, FIRMWARE_DIR1"/%s", uevent->firmware);
    if (l == -1)
        goto data_free_out;

    l = asprintf(&file2, FIRMWARE_DIR2"/%s", uevent->firmware);
    if (l == -1)
        goto data_free_out;

    loading_fd = open(loading, O_WRONLY);
    if(loading_fd < 0)
        goto file_free_out;

    write(loading_fd, "1", 1);  /* start transfer */

    data_fd = open(data, O_WRONLY);
    if(data_fd < 0) {
        write(loading_fd, "-1", 2); /* abort transfer */
        goto loading_close_out;
    }

    fw_fd = open(file1, O_RDONLY);
    if(fw_fd < 0) {
        fw_fd = open(file2, O_RDONLY);
        if(fw_fd < 0) {
            write(loading_fd, "-1", 2); /* abort transfer */
            goto data_close_out;
        }
    }

    if(!load_firmware(fw_fd, loading_fd, data_fd)) {
        write(loading_fd, "0", 1);  /* successful end of transfer */
        log_event_print("firmware copy success { '%s', '%s' }\n", root, uevent->firmware);
    } else {
        write(loading_fd, "-1", 2); /* abort transfer */
        log_event_print("firmware copy failure { '%s', '%s' }\n", root, uevent->firmware);
    }

    close(fw_fd);
data_close_out:
    close(data_fd);
loading_close_out:
    close(loading_fd);
file_free_out:
    free(file1);
    free(file2);
data_free_out:
    free(data);
loading_free_out:
    free(loading);
root_free_out:
    free(root);
}

static void handle_firmware_event(struct uevent *uevent)
{
    pid_t pid;
    int status;
    int ret;

    if(strcmp(uevent->subsystem, "firmware"))
        return;

    if(strcmp(uevent->action, "add"))
        return;

    /* we fork, to avoid making large memory allocations in init proper */
    pid = fork();
    if (!pid) {
        process_firmware_event(uevent);
        exit(EXIT_SUCCESS);
    } else {
        do {
            ret = waitpid(pid, &status, 0);
        } while (ret == -1 && errno == EINTR);
    }
}

#define UEVENT_MSG_LEN  1024
void handle_device_fd()
{
    for(;;) {
        char msg[UEVENT_MSG_LEN+2];
        char cred_msg[CMSG_SPACE(sizeof(struct ucred))];
        struct iovec iov = {msg, sizeof(msg)};
        struct sockaddr_nl snl;
        struct msghdr hdr = {&snl, sizeof(snl), &iov, 1, cred_msg, sizeof(cred_msg), 0};

        ssize_t n = recvmsg(device_fd, &hdr, 0);
        if (n <= 0) {
            break;
        }

        if ((snl.nl_groups != 1) || (snl.nl_pid != 0)) {
            /* ignoring non-kernel netlink multicast message */
            continue;
        }

        struct cmsghdr * cmsg = CMSG_FIRSTHDR(&hdr);
        if (cmsg == NULL || cmsg->cmsg_type != SCM_CREDENTIALS) {
            /* no sender credentials received, ignore message */
            continue;
        }

        struct ucred * cred = (struct ucred *)CMSG_DATA(cmsg);
        if (cred->uid != 0) {
            /* message from non-root user, ignore */
            continue;
        }

        if(n >= UEVENT_MSG_LEN)   /* overflow -- discard */
            continue;

        msg[n] = '\0';
        msg[n+1] = '\0';

        struct uevent uevent;
        parse_event(msg, &uevent);

        handle_device_event(&uevent);
        //handle_firmware_event(&uevent);
    }
}

/* Coldboot walks parts of the /sys tree and pokes the uevent files
** to cause the kernel to regenerate device add events that happened
** before init's device manager was started
**
** We drain any pending events from the netlink socket every time
** we poke another uevent file to make sure we don't overrun the
** socket's buffer.
*/

static void do_coldboot(DIR *d)
{
    struct dirent *de;
    int dfd, fd;

    dfd = dirfd(d);

    fd = openat(dfd, "uevent", O_WRONLY);
    if(fd >= 0) {
        write(fd, "add\n", 4);
        close(fd);
        handle_device_fd();
    }

    while((de = readdir(d))) {
        DIR *d2;

        if(de->d_type != DT_DIR || de->d_name[0] == '.')
            continue;

        fd = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY);
        if(fd < 0)
            continue;

        d2 = fdopendir(fd);
        if(d2 == 0)
            close(fd);
        else {
            do_coldboot(d2);
            closedir(d2);
        }
    }
}

static void coldboot(const char *path)
{
    DIR *d = opendir(path);
    if(d) {
        do_coldboot(d);
        closedir(d);
    }
}

void device_init(void)
{
    suseconds_t t0, t1;
    struct stat info;
    int fd;

    u_disk_state = 0; /* init sd as not mounted */
    device_fd = open_uevent_socket();
    if(device_fd < 0)
        return;

    fcntl(device_fd, F_SETFD, FD_CLOEXEC);
    fcntl(device_fd, F_SETFL, O_NONBLOCK);
#if 0
    if (stat(coldboot_done, &info) < 0) {
        t0 = get_usecs();
        coldboot("/sys/class");
        coldboot("/sys/block");
        coldboot("/sys/devices");
        t1 = get_usecs();
        fd = open(coldboot_done, O_WRONLY|O_CREAT, 0000);
        close(fd);
        log_event_print("coldboot %ld uS\n", ((long) (t1 - t0)));
    } else {
        log_event_print("skipping coldboot, already done\n");
    }
#endif
}

int get_device_fd()
{
    return device_fd;
}

void exit_handler(int num) {
    int ret, status;
    int pid = waitpid(-1, &status, WNOHANG);

    // Currently only handle mtpserver exit.
    if(mtp_pid != 0 && pid == mtp_pid && WIFEXITED(status)) {
        syslog(LOG_INFO, "ueventd: The child %d exit with code %d\n", pid, WEXITSTATUS(status));
        if (WEXITSTATUS(status) != 0) {
            syslog(LOG_INFO,"ueventd: mtp_pid exit unexpected, switch to mass_storage\n");

            pid = fork();
            if(!pid) {
                execl("/bin/sh", "sh", "/usr/bin/setusbcomposition", "mass_storage", NULL);
                exit(-1);
            } else if (pid < 0) {
                syslog(LOG_INFO, "ueventd: could not create child process to execute mtp server module\n");
            } else {
                do {
                    ret = waitpid(pid, NULL, 0);
                } while (ret == -1 && errno == EINTR);
            }
        }
    }
}
