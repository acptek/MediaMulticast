#ifndef CLIENT_H__
#define CLIENT_H__

// 可以由用户指定的标识
struct client_conf_st{
    char *rcvport;
    char *mgroup;
    char *player_cmd; // 命令行
};

// 声明成extern，扩展全局变量的作用域
/*
 * 如果全局变量会在其他文件中使用到时
 * 需要做成全局变量，并且在.h中声明
 * 其余的文件在包含此.h文件后也能够使用该变量
 */
extern struct client_conf_st client_conf;

#endif