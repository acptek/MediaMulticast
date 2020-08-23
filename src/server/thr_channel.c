#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>

#include "thr_list.h"
#include "../include/proto.h"
#include "server_conf.h"

struct thr_channel_st {
    chnid_t chnid;
    pthread_t tid;
};

struct thr_channel_st thr_channel[CHNNR];
static int tid_nextpos = 0;

// 组织数据发送出去
static void *thr_channel_snder(void *ptr){
    struct msg_channel_st *sbufp;

    sbufp = malloc(MSG_CHANNEL_MAX);
    if(sbufp == NULL){
        syslog(LOG_ERR, "malloc(): %s\n", strerror(errno));
        exit(1);
    }

    struct mlib_listentry_st *ent = ptr;
    sbufp->chnid = ent->chnid;

    while (1) {
//        int len = mlib_readchn(ent->chnid, sbufp->data, MAX_DATA);
        int len = mlib_readchn(ent->chnid, sbufp->data, 128*1024/8);
        syslog(LOG_DEBUG, "mlib_readchn() len: %d", len);

        if (sendto(serversd, sbufp, len + sizeof(chnid_t), 0, (void *) &sndaddr, sizeof(sndaddr)) < 0) {
            syslog(LOG_ERR, "thr_channel(%d): sendto(): %s", ent->chnid, strerror(errno));
            break;
        } else {
            syslog(LOG_DEBUG, "thr_channel(%d): sendto() successed .", ent->chnid);
        }
        sched_yield();
    }
    pthread_exit(NULL);
}

int thr_channel_create(struct mlib_listentry_st *ptr){
    int err;
    err = pthread_create(&thr_channel[tid_nextpos].tid, NULL, thr_channel_snder, ptr);
    if(err) {
        syslog(LOG_WARNING, "pthread_create(): %s", strerror(err));
        return -err;
    }
    thr_channel[tid_nextpos].chnid = ptr->chnid;
    tid_nextpos++;

    return 0;
}

int thr_channel_destroy(struct mlib_listentry_st *ptr){
    for(int i = 0; i < CHNNR; ++i){
        if(ptr->chnid == thr_channel[i].chnid) {
            if(pthread_cancel(thr_channel[i].tid)){
                syslog(LOG_ERR, "pthread_cancel(): the thread of channel %d", ptr->chnid);
                return -ESRCH;
            }
            pthread_join(thr_channel[i].tid, NULL);
            thr_channel[i].chnid = -1;
            return 0;
        }
    }
    syslog(LOG_ERR, "channel %d doesnt exist", ptr->chnid);
    return -ESRCH;
}

int thr_channel_destroyall(void ){
    for(int i = 0; i < CHNNR; ++i){
        if(thr_channel[i].chnid > 0){
            if(pthread_cancel(thr_channel[i].chnid)){
                syslog(LOG_ERR, "the thread of channel %d", thr_channel[i].chnid);
                return -ESRCH;
            }
            pthread_join(thr_channel[i].tid, NULL);
            thr_channel[i].chnid = -1;
        }
    }
    return 0;
}