#include <stdio.h>
#include <stdlib.h>
#include <zconf.h>
#include <sys/socket.h>
#include <getopt.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <asm/errno.h>
#include <errno.h>
#include "client.h"
#include "../include/proto.h"
// #include <proto.h>

/**
 * 命令行格式
 * -M --mgroup : 指定多播组
 * -P --port   : 指定几首端口
 * -p --player : 指定播放器
 * -H --help   : 显示帮助
 * @param argc
 * @param argv
 * @return
 */

// 默认值
struct client_conf_st client_conf = {
        .rcvport = DEFAULT_RCVPORT,
        .mgroup = DEFAULT_MGROUP,
        .player_cmd = DEFAULT_PLAYERCMD
};

static void printhelp(){
    printf("-P --port  \tassign receive port\n");
    printf("-M --mgroup\tassign multicast group\n");
    printf("-p --player\tassign player command\n");
    printf("-H --help  \tshow help\n");
}

static ssize_t writen(int fd, const char *buf, size_t len){
    int pos = 0;
    while(len > 0) {
        ssize_t ret = write(fd, buf+pos, len);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            perror("write()");
            return -1;
        }
        len -= ret;
        pos += ret;
    }
    return pos;
}

int main(int argc, char **argv)
{
    /*
     * 命令行分析
     * 初始化
     * 级别：默认值，配制文件，环境变量，命令行参数
     */
    int c;
    int index = 0;
    struct option argarr[] = {
            {"port", 1, NULL, 'P'},
            {"mgroup", 1, NULL, 'M'},
            {"player", 1, NULL, 'p'},
            {"help", 0, NULL, 'H'},
            {NULL, 0, NULL, 0} // 最后一个是空值
    };
    while(1){
        c = getopt_long(argc, argv, "P:M:p:H", argarr, &index);
        if(c < 0) break;
        switch (c) {
            case 'P':
                client_conf.rcvport = optarg;
                break;
            case 'M':
                client_conf.mgroup = optarg;
                break;
            case 'p':
                client_conf.player_cmd = optarg;
                break;
            case 'H':
                printhelp();
                exit(0);
            default:
                abort();
                break;
        }
    }

    /*
     * 创建socket，添加socket选项
     */
    int sfd;
    struct ip_mreqn mreq;
    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sfd < 0){
        perror("socket()");
        exit(1);
    }
    // 设置多播选项
    inet_pton(AF_INET, client_conf.mgroup, &mreq.imr_multiaddr); // 多播地址
    inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address); // 本机地址
    mreq.imr_ifindex = (int)if_nametoindex("eth0");// 当前所用到网络设备的索引号
    if(setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0){
        perror("setsockopt()");
        exit(1);
    }
    // 设置 是否发送多播包应该回环到本地socket上 // 提高效率
    int val = 1;
    if(setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val)) < 0){
        perror("setsockopt()");
        exit(1);
    }

    /*
     * 绑定socket
     */
    struct sockaddr_in laddr;
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(strtol(client_conf.rcvport, NULL, 10));
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr);
    if(bind(sfd, (void *)&laddr, sizeof(laddr)) < 0){
        perror("bind()");
        exit(1);
    }

    /*
     * 创建管道
     * 创建子进程
     * 子进程从管道中读内容进行播放
     */
    int pd[2];
    if(pipe(pd) < 0){
        perror("pipe()");
        exit(1);
    }

    pid_t  pid;
    pid = fork();
    if(pid < 0){
        perror("fork()");
        exit(1);
    }

    if(pid == 0){
        // 子进程从管道读数据
        close(sfd);
        close(pd[1]);

        dup2(pd[0], 0);
        if(pd[0] > 0)
            close(pd[0]);

        // 调用解码器
        execl("/bin/sh", "sh", "-c", client_conf.player_cmd, NULL); // 直接借助shell来解析命令行
        perror("execl()");
        exit(1);
    }

    // 父进程从网络接收数据发送给子进程
    // 接收节目单
    struct msg_list_st *msg_list;
    struct sockaddr_in raddr;
    socklen_t raddr_len;

    msg_list = malloc(MSG_LIST_MAX);
    if(msg_list == NULL){
        perror("malloc()");
        exit(1);
    }

    ssize_t len;
    while (1) {
        len = recvfrom(sfd, msg_list, MSG_LIST_MAX, 0, (void *) &raddr, &raddr_len);
        if (len < sizeof(struct msg_list_st)) {
            fprintf(stderr, "message is too small. \n");
            continue;
        }
        if(msg_list->chnid != LISTCHNID){
            fprintf(stderr, "chnid is not match. \n");
            continue;
        }
        break;
    }


    // 打印节目单，选择频道
    int chooseid;
    struct msg_listentry_st *pos;
    for(pos = msg_list->entry; (char *)pos < ((char *)msg_list + len); pos = (void*)(((char *) pos) + ntohs(pos->len))){
        printf("channel %d : %s\n", pos->chnid, pos->desc);
    }
    free(msg_list);

    if(scanf("%d", &chooseid) != 1){
        exit(1);
    }


    // 接收频道，发送给子进程
    struct sockaddr_in raddr_chn;
    socklen_t raddr_chn_len;
    ssize_t chn_len;
    struct msg_channel_st *msg_channel;
    msg_channel = malloc(MSG_CHANNEL_MAX);
    if(msg_channel == NULL){
        perror("malloc()");
        exit(1);
    }

    while (1) {
        chn_len = recvfrom(sfd, msg_channel, MSG_CHANNEL_MAX, 0, (void *)&raddr_chn, &raddr_chn_len);
        if(raddr_chn.sin_addr.s_addr != raddr.sin_addr.s_addr
        || raddr_chn.sin_port != raddr.sin_port){
            fprintf(stderr, "Ignore: address mot match. \n");
            continue;
        }

        // 接收到报文的最小长度
        if(len < sizeof(struct msg_channel_st)){
            fprintf(stderr, "Ignore: message too small. \n");
            continue;
        }

        // 从接收到的数据中选择需要的频道数据写入管道
        if(msg_channel->chnid == chooseid){
            fprintf(stdout, "Accepted msg: %d received. \n", msg_channel->chnid);
            if(writen(pd[1], msg_channel->data, chn_len-sizeof(chnid_t)) < 0){
                exit(1);
            }
        }
    }

    // 放到信号处理函数
    free(msg_channel);
    close(sfd);

    exit(0);
}