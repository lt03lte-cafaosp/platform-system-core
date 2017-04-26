/******************************************************************************
 *
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <poll.h>
#include <grp.h>
#include <sys/un.h>
#include <cutils/sockets.h>
#include <fcntl.h>

#include "property_ops.h"

static int sock_fd = -1;
static pthread_mutex_t leprop_mutex = PTHREAD_MUTEX_INITIALIZER;

static int open_prop_socket()
{
    int fd, ret = 0;

    if (sock_fd < 0) {
        fd = TEMP_FAILURE_RETRY(socket(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
        if (fd < 0) {
            ret = -errno;
        } else if (TEMP_FAILURE_RETRY(fcntl(fd, F_SETFL, O_NONBLOCK)) < 0) {
            ret = -errno;
            close(fd);
        } else {
            struct sockaddr_un un;
            memset(&un, 0, sizeof(struct sockaddr_un));
            un.sun_family = AF_UNIX;
            snprintf(un.sun_path, sizeof(un.sun_path),
                        ANDROID_SOCKET_DIR"/%s", PROP_SERVICE_NAME);

            if (TEMP_FAILURE_RETRY(connect(fd, (struct sockaddr *)&un,
                                       sizeof(struct sockaddr_un))) < 0) {
                ALOGE("Failed to connect to %s socket (%s)", PROP_SERVICE_NAME,
                strerror(errno));
                close(fd);
                ret = -errno;
            } else {
                sock_fd = fd;
            }
        }
    }
    return ret;
}

static int send_prop_msg(const char *msg, char *resp)
{
    int ret = -1;

    if (sock_fd < 0) {
        ret = open_prop_socket();
        if (ret < 0) {
            ALOGE("Failed to open Socket");
            return ret; // pass error back to caller.
        }
    }

    if (sock_fd > 0) {
        int msg_len = strlen(msg);
        int status = -1;
        // ensure only one thread at a time
        // send message to property service
        pthread_mutex_lock(&leprop_mutex);
        do {
            status = send(sock_fd, msg, msg_len, 0);
            if ( status == -1 ) {
                if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                    LOG("Send error: %s", strerror(errno));
                    ret = -errno;
                    pthread_mutex_unlock(&leprop_mutex);
                    break;
                }
                continue;
            } else if (status == 0) {
                LOG("Sent zero size data: May be property service down");
                ret = -1;
                pthread_mutex_unlock(&leprop_mutex);
                break;
            }
            LOG( "Sent %d bytes on sock_fd: %d", status, sock_fd);
        } while (status <= 0);

        // Successfully wrote to the property service
        // now handle data sent from service
        char recv_buf[MAX_ALLOWED_LINE_LEN+1];
        memset(recv_buf, 0 , sizeof(recv_buf));

        do {
            status = recv(sock_fd, recv_buf, sizeof(recv_buf), 0);
            if (status == -1) {
                if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                    LOG("Receive error: %s", strerror(errno));
                    ret = -errno;
                    break;
                }
                continue;
            } else if (status == 0) {
                LOG("Received zero size data: May be property service down");
                ret = -1;
                break;
            } else {
                recv_buf[status] = '\0';
                LOG("Received %d bytes of data (%s) from Socket",status, recv_buf);
                int i = 1;
                while(i < strlen(recv_buf)){
                    resp[i-1] = recv_buf[i];
                    i++;
                }
                ret = 0;
                break;
            }
        } while (status <= 0);
        pthread_mutex_unlock(&leprop_mutex);
    }
    return ret;
}

bool set_property_value(const char* prop_name, unsigned char *prop_val)
{
    const char msg[MAX_ALLOWED_LINE_LEN+1]; // +1 for msg type.
    char resp[MAX_ALLOWED_LINE_LEN];
    memset(msg, 0 , sizeof(msg));
    memset(resp, 0 , sizeof(resp));

    LOG("Received request to setprop: %s", prop_name);
    snprintf(msg, MAX_ALLOWED_LINE_LEN+1, "%c%s=%s",
             PROP_MSG_SETPROP, prop_name, prop_val);

    const int err = send_prop_msg(&msg, &resp);
    if (err < 0) {
       LOG("Failed to send message to Set %s", prop_name);
       return false;
    }
    LOG("Completed request to setprop: %s", prop_name);
    return true;
}

bool get_property_value(const char* prop_name, unsigned char *prop_val)
{
    const char msg[MAX_ALLOWED_LINE_LEN+1]; // +1 for msg type.
    char resp[MAX_ALLOWED_LINE_LEN];
    memset(msg,  0 , sizeof(msg));
    memset(resp, 0 , sizeof(resp));

    LOG("Received request to getprop: %s", prop_name);
    snprintf(msg, sizeof(msg), "%c%s=", PROP_MSG_GETPROP, prop_name);

    const int err = send_prop_msg(&msg, &resp);
    if (err < 0) {
       LOG("Failed to send message to Get %s", prop_name);
       return false;
    }

    // Extract prop value from response.
    char *delimiter = strchr(resp, '=');
    const char *curr_line_ptr = delimiter+1; //+1 for delimiter
    if(strlen(curr_line_ptr) <= 0) {
        LOG("%s has invalid length", prop_name);
        return false;
    }

    strlcpy(prop_val, curr_line_ptr, PROP_VALUE_MAX);
    LOG("Completed request to getprop: %s", prop_name);
    return true;
}

void dump_persist(void)
{
    //TO BE IMPLEMENTED
}
