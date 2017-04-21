/******************************************************************************
 *
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <grp.h>
#include <fcntl.h>
#include <sys/un.h>
#include <cutils/sockets.h>
#include "property_service.h"

static int persist_storage_ready = 0;

int main(int argc, char* argv[]) {

    int listen_fd = 0;
    int conn_sock[MAX_CONN];
    memset(conn_sock, 0, sizeof(conn_sock));

    if(false == load_default_properties()) {
        LOG("Failed to load default properties");
    }

    listen_fd = create_socket(PROP_SERVICE_NAME,
                              SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                              0666, 0, 0);
    if (listen_fd < 0) {
        LOG("property_service socket creation failed (%s)", strerror(errno));
        exit(1);
    }

    if (listen(listen_fd, 10) < 0) {
        LOG("property_service error on listen (%s)", strerror(errno));
        exit (1);
    }

    LOG("properties service is ready!!!");

    while (1) {
        fd_set read_fds;
        int fdmax = 0;
        int conn_fd = 0;
        int i = 0;
        int recv_size = 0;
        char recv_buf[MAX_ALLOWED_LINE_LEN+1];
        memset(recv_buf, 0, sizeof(recv_buf));

        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds); // add listen fd to read list
        fdmax = listen_fd;

        //add existing connection sockets to set
        for (i = 0; i < MAX_CONN; i++) {
            conn_fd = conn_sock[i];
            if(conn_fd > 0) FD_SET(conn_fd , &read_fds);
            if(conn_fd > fdmax) fdmax = conn_fd;
        }

        LOG("Waiting for select!!!");
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            LOG("select failed (%s) fdmax=%d", strerror(errno), fdmax);
            continue;
        }

        // new connection
        if(FD_ISSET(listen_fd, &read_fds)) {
            struct sockaddr* addrp;
            int alen;
            do {
                alen = sizeof(addrp);
                conn_fd = accept(listen_fd, (struct sockaddr *)&addrp, &alen);
                LOG("Got conn_fd=%d from accept", conn_fd);
            } while (conn_fd < 0 && errno == EINTR);

            if (conn_fd < 0) {
                LOG("accept failed (%s)", strerror(errno));
                continue;
            }
            fcntl(conn_fd, F_SETFD, FD_CLOEXEC | SOCK_NONBLOCK);

            //add new socket to read list
            for (i = 0; i < MAX_CONN; i++)
            {
               if( conn_sock[i] == 0 ) {
                conn_sock[i] = conn_fd;
                LOG("Adding to read list at %d" , i);
                break;
               }
            }

            //handle data
            while (1) {
                int fd = 0;
                recv_size = TEMP_FAILURE_RETRY(recv(conn_fd, recv_buf, sizeof(recv_buf), 0));
                if( recv_size == -1) {
                    if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                        LOG("Receive error (%s) on new conn: %d", strerror(errno), conn_fd);
                        break;
                    }
                    continue;
                } else if (recv_size == 0) {
                    LOG("Received zero size data closing new conn:%d", conn_fd);
                    break;
                }
                LOG("Received %d bytes (%s) on new conn:%d", recv_size, recv_buf, conn_fd);
                break;
            }
        } else {
            //data on existing connection
            for (i = 0; i < MAX_CONN; i++)
            {
                int fd = conn_sock[i];
                if (FD_ISSET(fd , &read_fds)) {
                    //handle data
                    while (1) {
                        recv_size = TEMP_FAILURE_RETRY(recv(fd, recv_buf, sizeof(recv_buf), 0));
                        if (recv_size == -1) {
                            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                                LOG("Receive error (%s) on existing conn: %d", strerror(errno), fd);
                                break;
                            }
                            continue;
                        } else if (recv_size == 0) {
                            LOG("Received zero size data closing conn:%d", fd);
                            close(fd); //Close the socket and remove from read list
                            conn_sock[i] = 0;
                            break;
                        }
                        LOG("Received %d bytes (%s) on existing conn:%d" ,recv_size, recv_buf, fd);
                        conn_fd = fd;
                        break;
                    }
                    if (conn_fd == fd)
                        break; // got a valid connection skip remaining.
                }
            }
        }

        // process data
        if (recv_size > 0) {
            LOG("Successfully received %d bytes (%s)", recv_size, recv_buf);
            recv_buf[recv_size] = '\0';
            property_db *node = NULL;
            const char msg[MAX_ALLOWED_LINE_LEN+1]; // +1 for cmd.
            memset(msg, 0 , sizeof(msg));

            if (recv_buf[0] == PROP_MSG_SETPROP) {
                node = process_setprop_msg(recv_buf);

                snprintf(msg, MAX_ALLOWED_LINE_LEN+1, "%c%s=%s",
                        PROP_MSG_SETPROP, node->unit.property_name,
                        node->unit.property_value);
            } else if(recv_buf[0] == PROP_MSG_GETPROP) {
                // read data from ds and pass to client
                node = process_getprop_msg(recv_buf);

                snprintf(msg, MAX_ALLOWED_LINE_LEN+1, "%c%s=%s",
                        PROP_MSG_GETPROP, node->unit.property_name,
                        node->unit.property_value);
            } else {
                LOG("Invalid msg received");
            }

            int msg_len = strlen(msg);
            while (1) {
                int status = TEMP_FAILURE_RETRY(send(conn_fd, msg, msg_len, 0));
                if ( status == -1 ) {
                    if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                        LOG("Send error on conn_fd:%d (%s)", conn_fd, strerror(errno));
                        break;
                    }
                    continue;
                } else if (status == 0) {
                    LOG("Sent zero size data may be client is down");
                    break;
                }
                LOG("Sent %d bytes (%s) to client", status, msg);
                break;
            }
            if (node) free(node);
        }
    }
    close(listen_fd);
    exit(0);
}

bool valid_trigger(const char* name)
{
    FILE *fp = fopen(PROP_TRIGGER_CONF, "r");

    if (NULL != fp)
    {
        int line_num = 0;
        char line[MAX_ALLOWED_LINE_LEN];
        memset(line, 0, sizeof(line));

        while(fgets(line, sizeof(line), fp))
        {
            // fgets adds '\n'. Replace it with '\0'
            char *p;
            if ((p=strchr(line, '\n')) != NULL)  *p = '\0';

            LOG("%s, line: %s num: %d ", __func__, line, line_num);
            if (strcmp(name, line) == 0) {
               LOG("%s, Found trigger: %s", __func__, line);
               fclose(fp);
               return true;
            }
            line_num++;
        }
        LOG("%s, reached EOF", __func__);
        fclose(fp);
    }
    return false;
}

void prop_trigger(char* name, char* val)
{
    char *argv[] = { NULL, NULL, NULL, NULL };

    argv[0] = PROP_TRIGGER;
    argv[1] = name;
    argv[2] = val;
    argv[3] = NULL;

    switch (fork()) {
        case 0:
            execv(argv[0], argv);
            ALOGE("execv(\"%s\") failed: %s", argv[0], strerror(errno));
            break;
        case -1:
            ALOGE("Cannot fork: %s", strerror(errno));
            return -1;
            break;
        default:
            break;
   }
   return 0;
}

property_db* process_setprop_msg(char* buff)
{
    property_db *node = NULL;
    char line[MAX_ALLOWED_LINE_LEN];
    memset(line, 0, sizeof(line));

    int i = 1;
    while(i < strlen(buff)){
        line[i-1] = buff[i];
        i++;
    }
    LOG("Setprop cmd received");

    node = (property_db *)pull_one_line_data(line);
    if (NULL != node) {
        __update_prop_value(node->unit.property_name, node->unit.property_value);

        if ( strcmp("le.persistprop.enable", node->unit.property_name) == 0 &&
             strcmp("true", node->unit.property_value) == 0 ) {

            //first load the persist properties into ds.
            load_persist_properties();
            //save ds to reflect new properites added before file is available.
            save_persist_ds_to_file();
        }
        if ( persist_storage_ready && strncmp("persist.",
                node->unit.property_name, strlen("persist.")) == 0 ) {
            save_persist_ds_to_file();
            LOG("Completed storing data to persist file");
        }

        //start property trigger
        if (true == valid_trigger(node->unit.property_name))
            prop_trigger(node->unit.property_name, node->unit.property_value);

    }
    return node;
}

property_db* process_getprop_msg(char* buff)
{
    property_db *node = NULL;
    char line[MAX_ALLOWED_LINE_LEN];
    memset(line, 0, sizeof(line));

    int i = 1;
    while(i < strlen(buff)){
        line[i-1] = buff[i];
    i++;
    }
    LOG("Getprop cmd received");

    node = (property_db *)pull_one_line_data(line);
    if (NULL != node) {
        __retrive_prop_value(node->unit.property_name,
                             node->unit.property_value);
        LOG("Found prop:%s with val:%s",
               node->unit.property_name, node->unit.property_value);
    }
    return node;
}

int create_socket(const char *name, int type, mode_t perm, uid_t uid, gid_t gid)
{
    struct sockaddr_un addr;
    int fd, ret;

    fd = socket(PF_UNIX, type, 0);
    if (fd < 0) {
        ALOGE("Failed to open socket '%s': %s\n", name, strerror(errno));
        return -1;
    }

    memset(&addr, 0 , sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), ANDROID_SOCKET_DIR"/%s",
             name);

    ret = unlink(addr.sun_path);
    if (ret != 0 && errno != ENOENT) {
        LOG("Failed to unlink old socket '%s': %s\n", name, strerror(errno));
        goto out_close;
    }

    ret = bind(fd, (struct sockaddr *) &addr, sizeof (addr));
    if (ret) {
        LOG("Failed to bind socket '%s': %s\n", name, strerror(errno));
        goto out_unlink;
    }

    chown(addr.sun_path, uid, gid);
    chmod(addr.sun_path, perm);

    return fd;

out_unlink:
    unlink(addr.sun_path);
out_close:
    close(fd);
    return -1;
}

bool save_persist_ds_to_file(void)
{
    property_db *ln = (property_db*)__get_list_head();
    bool retval = false;
    int filewrite_status = -1;

    FILE *fp = fopen(PROP_FILE_PERSIST_PATH, "w");
    LOG("Save DS to persist file %s\n",PROP_FILE_PERSIST_PATH);

    if (NULL != fp)
    {
        LOG("File truncate mode\n");
        for (; ln != NULL; ln = ln->next)
        {
            if ( strncmp("persist.", ln->unit.property_name,
                 strlen("persist.")) == 0 ) {

                unsigned char stringtowrite[MAX_ALLOWED_LINE_LEN];
                memset(stringtowrite, 0 , sizeof(stringtowrite));

                snprintf(stringtowrite, MAX_ALLOWED_LINE_LEN, "%s=%s",
                     ln->unit.property_name,ln->unit.property_value );

                LOG("Writing to persist %s \n", stringtowrite);

                filewrite_status = fputs(stringtowrite, fp);
                if (filewrite_status < 0 || filewrite_status == EOF)
                {
                    retval = false;
                    break;
                } else {
                    retval = true;
                }
            }
        }
        fclose(fp);
    } else {
        ALOGE("Persist File doesnt exist");
        retval = false;
    }
    return retval;
}

int remove_ds_node(const char* prop_name)
{
    return __remove_node_from_list(prop_name);
}

bool add_ds_node(property_db *node)
{
    LOG("[%s] => Adding Node to DS %x\n", __func__, node);
    return __list_add(node);
}

bool clear_current_ds(void)
{
    LOG("[%s] => Free all nodes in DS\n", __func__);
    return __free_list();
}

void dump_current_ds(void)
{
    __dump_nodes();
}

void dump_persist(void)
{
    //TO BE IMPLEMENTED
}

property_db* pull_one_line_data(const char* line)
{
    int curr_length = 0;
    const char *curr_line_ptr = line;
    char *delimiter = NULL;
    int iterator = 0;

    //TODO : We can do this using strtok - will be short code
    property_db *extracted_val = (property_db*)calloc(1, sizeof(property_db)
            *sizeof(unsigned char));
    if (extracted_val == NULL) {
        LOG("No Memory");
        return extracted_val;
    }
    extracted_val->next = NULL; //null added here no need to add in ll.c
    delimiter = strchr(curr_line_ptr, '=');

    for(iterator=0 ; iterator< MAX_PROPERTY_ITER; iterator++)
    {
        LOG("[%s] => line pulled: %s", __func__,curr_line_ptr);

        if(extracted_val != NULL)
        {
            switch (iterator)
            {
                case EXT_NAME:
                    curr_length = delimiter - curr_line_ptr;
                    if (curr_length > PROP_NAME_MAX || curr_length < 0)
                    {
                       curr_length = PROP_NAME_MAX;
                    }
                    strncpy(extracted_val->unit.property_name,
                            curr_line_ptr, curr_length);
                    LOG("[%s] => Extracted Name: %s\n", __func__,
                            extracted_val->unit.property_name);
                    break;

                case EXT_VAL:
                    curr_line_ptr = delimiter+1; //+1 for the delimiter itself
                    curr_length = strlen(curr_line_ptr);
                    if (curr_length > PROP_VALUE_MAX || curr_length < 0)
                    {
                       curr_length = PROP_VALUE_MAX;
                    }
                    strncpy(extracted_val->unit.property_value,
                            curr_line_ptr, curr_length);
                    LOG("[%s] => Extracted Value: %s\n", __func__
                            ,extracted_val->unit.property_value);
                    break;

                default:
                    break;
            }
        }

        LOG("[%s] => iterator: %d, curr_line_ptr: %s, delimiter: %s\n", __func__,
                iterator,curr_line_ptr, delimiter);
    }
    return extracted_val;
}

bool search_and_add_property_val(const char* fpath)
{
    FILE *fp = fopen(fpath, "r");
    int line_num =0;
    int retval = -1;
    bool list_add_status = false;
    property_db *extracted_node = NULL;

    if (NULL != fp)
    {
        char line[MAX_ALLOWED_LINE_LEN];
        memset(line, 0, sizeof(line));

        while(fgets(line, sizeof(line), fp))
        {
            line_num++;
            LOG("%s, lineread = %s line_num=%d ", __func__,
                            line, line_num);
            extracted_node = (property_db *)pull_one_line_data(line);
            if (NULL != extracted_node)
            {
                LOG("Node Extracted, adding to list %x\n",
                    extracted_node);
                list_add_status = add_ds_node(extracted_node);
                LOG("%s, extracted_node=%0x added status=%d",
                        __func__, extracted_node,list_add_status);
            }
            memset(line, 0, sizeof(line));
            continue;
        }
        LOG("%s, reached EOF", __func__);
        fclose(fp);
    } else {
        LOG("%s, no %s", __func__, line);
        list_add_status = false;
    }
    return list_add_status;
}

bool load_properties_from_file(const char *filename)
{
    LOG("Loading properties from %s\n", filename);
    return search_and_add_property_val(filename);
}

bool load_default_properties() {
    return load_properties_from_file(PROP_FILE_DEFAULT_PATH);
}

bool load_persist_properties() {
    persist_storage_ready = 1;
    return load_properties_from_file(PROP_FILE_PERSIST_PATH);
}
