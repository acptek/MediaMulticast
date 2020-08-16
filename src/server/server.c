#include <stdio.h>
#include <stdlib.h>
#include <zconf.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "server_conf.h"
#include "../include/proto.h"
#include "medialib.h"
#include "thr_list.h"
#include "thr_channel.h"

struct server_conf_st server_conf = {
        .rcvport = DEFAULT_RCVPORT,
        .mgroup = DEFAULT_MGROUP,
        .media_dir = DEFAULT_MEDIADIR,
        .runmode = RUN_DAEMON,
        .ifname = DEFAULT_IF
};

extern struct server_conf_st server_conf;

static void printhelp(){
    printf("-M\tassign multicast group\n");
    printf("-P\tassign receive port\n");
    printf("-F\tassign run front\n");
    printf("-D\tassign the directory of media library\n");
    printf("-I\tassign network interface\n");
    printf("-H\tshow help\n");
}

static void daemon_exit(int s){

    closelog();

    exit(0);
}

static int daemonize(){
    pid_t pid = fork();
    if(pid < 0){
        // perror("fork()");
        syslog(LOG_ERR, "fork() : %s", strerror(errno));
        return -1;
    }
    if(pid > 0){
        exit(0);
    }

    int fd = open("/dev/null", O_RDWR);
    if(fd < 0){
        // perror("open()");
        syslog(LOG_WARNING, "open() : %s", strerror(errno));
        return -2;
    } else {

        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        if (fd > 2)
            close(fd);
    }
    setsid();

    chdir("/");
    umask(0);

    return 0;
}

static void socket_init(){
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sfd < 0){
        syslog(LOG_ERR, "socket() : %s", strerror(errno));
        exit(1);
    }

    struct ip_mreqn mreq;
    inet_pton(AF_INET, server_conf.mgroup, &mreq.imr_multiaddr);
    inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
    mreq.imr_ifindex = if_nametoindex(server_conf.ifname);

    if(setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0){
        syslog(LOG_ERR, "setsockopt() : %s", strerror(errno));
        exit(1);
    }

    // bind();
}

int main(int argc, char **argv)
{
    // 打开一个到系统日志的连接
    openlog("media", LOG_PID|LOG_PERROR, LOG_DAEMON); // 写消息时包含任务PID

    // 设置信号处理
    struct sigaction sa;
    sa.sa_handler = daemon_exit();
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGQUIT);
    sigaddset(&sa.sa_mask, SIGTERM);

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    /*
     * 命令行分析
     *
     * -M   指定多播组
     * -P   指定接收端口
     * -F   前台运行
     * -D   指定媒体库位置
     * -I   指定网络接口
     * -H   帮助
     * */
    while(1) {
        int c = getopt(argc, argv, "M:P:FD:I:H");
        if(c < 0){
            break;
        }
        switch (c) {
            case 'M':
                server_conf.mgroup = optarg;
                break;
            case 'P':
                server_conf.rcvport = optarg;
                break;
            case 'F':
                server_conf.runmode = RUN_FOREGROUND;
                break;
            case 'D':
                server_conf.media_dir = optarg;
                break;
            case 'I':
                server_conf.ifname = optarg;
                break;
            case 'H':
                printhelp();
                exit(0);
                break;
            default:
                abort();
                break;
        }

    }

    /*
     * 守护进程
     * */
    if(server_conf.runmode == RUN_DAEMON) {
        if(daemonize() != 0){
            exit(1);
        }
    } else if(server_conf.runmode == RUN_FOREGROUND){
        //
    } else {
        // fprintf(stderr, "EINVAL\n");
        syslog(LOG_ERR, "EINVAL server_conf.runmode"); // 往系统日志上报错
        exit(1);
    }


    /*
     * 初始化Socket
     * */
    socket_init();


    /*
     * 获取频道信息
     * (调用媒体库函数 medialib.h）
     * */
    struct mlib_listentry_st *list;
    int list_size;

    int err = mlib_getchnlist(&list, &list_size);
    if(err < 0){

    }

    /*
     * 创建节目单线程
     * (thr_list.h)
     * */
    err = thr_list_create(list, list_size);
    if(err < 0){

    }

    /*
     * 创建频道线程
     * (thr_channel.h)
     * */
    for(int i = 0; i < list_size; ++i){
        thr_channel_create(list+i);
        /*if error*/
    }
    syslog(LOG_DEBUG, "%d channel threads created .", i);

    while (1){
        pause();
    }

    exit(0);
}