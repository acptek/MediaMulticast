#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/syslog.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <zconf.h>
#include "../include/proto.h"
#include "medialib.h"
#include "server_conf.h"
#include "thr_list.h"

static pthread_t tid_list;
// 节目单中节目的数量
static int list_num;
// 节目单列表
static struct mlib_listentry_st *list;

static void *thr_list(void *p) {
    struct msg_list_st *entlistp; // 节目单包

    int totalsize = sizeof(chnid_t); // 节目单包的总计大小
    for(int i = 0; i < list_num; ++i) {
        totalsize += sizeof(struct msg_listentry_st) + strlen(list[i].desc);
    }

    entlistp = malloc(totalsize);
    if(entlistp == NULL) {
        syslog(LOG_ERR, "malloc(): %s", strerror(errno));
        exit(1);
    }
    entlistp->chnid = LISTCHNID;

    struct msg_listentry_st *entryp = entlistp->entry; // 节目单单个数据指针
    for(int i = 0; i < list_num; ++i){
        int size = sizeof(struct msg_listentry_st) + strlen(list[i].desc);
        entryp->chnid = list[i].chnid;
        entryp->len = htons(size);
        strcpy(entryp->desc, list[i].desc);
        entryp = (void*)(((char *)entryp) + size); //扩展size个字节空间
    }

    // 每隔1s发送一次节目单
    while (1) {
        int ret = sendto(serversd, entlistp, totalsize, 0, (void*)&sndaddr, sizeof(sndaddr));
        if(ret < 0){
            syslog(LOG_WARNING, "sendto(serversd, enlistp...:%s", strerror(errno));
        } else {
            syslog(LOG_DEBUG, "sendto(serversd, enlistp....):success");
        }
        sleep(1);
    }

}

int thr_list_create(struct mlib_listentry_st *listp, int num) {
    int err;

    list = listp;
    list_num = num;
    err = pthread_create(&tid_list, NULL, thr_list, NULL);
    if(err) {
        syslog(LOG_ERR, "pthread_create(): %s", strerror(errno));
        return -1;
    }
    return 0;
}

int thr_list_destroy(void) {
    pthread_cancel(tid_list);
    pthread_join(tid_list, NULL);
    return 0;
}