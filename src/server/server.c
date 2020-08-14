#include <stdio.h>
#include <stdlib.h>
#include <zconf.h>
#include "server_conf.h"
#include "../include/proto.h"

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

int main(int argc, char **argv)
{

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

    /*
     * 初始化Socket
     * */

    /*
     * 获取频道信息
     * (调用媒体库函数 medialib.h）
     * */

    /*
     * 创建节目单线程
     * (thr_list.h)
     * */

    /*
     * 创建频道线程
     * (thr_channel.h)
     * */

    while (1){
        pause();
    }

    exit(0);
}