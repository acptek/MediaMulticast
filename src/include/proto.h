#ifndef PROTO_H__
#define PROTO_H__

#include "site_type.h"

// 约定当前多播组
#define DEFAULT_MGROUP      "224.2.2.2"
// 默认设置端口
#define DEFAULT_RCVPORT     "1989"

// 频道数
#define CHNNR               100
// 规定发送节目单的频道为0
#define LISTCHNID           0
// 最小频道号
#define MINCHNID            1
// 最大频道号
#define MAXCHNID            (MINCHNID+CHNNR-1)

// 包最大长度
#define MSG_CHANNEL_MAX     (65536-20-8)
// 包数据的最大长度
#define MAX_DATA            (MSG_CHANNEL_MAX-sizeof(chnid_t))

// 节目最大长度
#define MSG_LIST_MAX        (65536-20-8)
// 节目中数据最大长度
#define MAX_ENTRY           (MSG_LIST_MAX-sizeof(chnid_t))

// 频道数据
struct msg_channel_st{
    chnid_t chnid; // 频道号
    uint8_t data[1]; // 变长
}__attribute__((packed));


// 节目数据 // 表示节目单中的一条记录
struct msg_listentry_st{
    chnid_t chnid;
    uint8_t desc[1]; // 变长
}__attribute__((packed));


// 节目单数据
/*
 * 1 music: ...
 * 2 sport: ...
 * 3 adv:
 * 4
 * ...
 * */
struct msg_list_st{
    chnid_t chnid;  // LISTCHNID
    struct msg_listentry_st entry[1];

}__attribute__((packed));


#endif