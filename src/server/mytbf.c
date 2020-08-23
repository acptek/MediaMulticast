#include "mytbf.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <zconf.h>

struct mytbf_st{
    int cps;
    int brust;
    int token;
    int pos; // 当前结构体数组下标
    pthread_mutex_t mut;
    pthread_cond_t cond;
};

struct mytbf_st *job[MYTBF_MAX];
pthread_mutex_t mut_job = PTHREAD_MUTEX_INITIALIZER; // 限制数组大小
pthread_once_t  init_once = PTHREAD_ONCE_INIT;
static pthread_t tid; // 计时器线程

void *thr_alrm(){
    while (1){
        pthread_mutex_lock(&mut_job);
        for(int i = 0; i < MYTBF_MAX; ++i){
            if(job[i] != NULL){
                pthread_mutex_lock(&job[i]->mut);
                job[i]->token += job[i]->cps;
                if(job[i]->token > job[i]->brust)
                    job[i]->token = job[i]->brust;
                pthread_cond_broadcast(&job[i]->cond);
                pthread_mutex_unlock(&job[i]->mut);
            }
        }
        pthread_mutex_unlock(&mut_job);
        sleep(1);
    }
}

static void module_unload(){
    pthread_cancel(tid);
    pthread_join(tid, NULL);

    for(int i = 0; i < MYTBF_MAX; ++i){
        free(job[i]);
    }
}

static void module_load(){
    int err;

    err = pthread_create(&tid, NULL, thr_alrm, NULL);
    if(err){
        fprintf(stderr, "pthread(): %s\n", strerror(errno));
        exit(1);
    }

    atexit(module_unload);
}

static int get_free_pos_unlock(){
    int i;
    for(i = 0; i < MYTBF_MAX; ++i) {
        if (job[i] == NULL) {
            return i;
        }
    }
    return -1;
}

mytbf_t *mytbf_init(int cps, int brust){
    struct mytbf_st *me;

    // 每秒增加token的线程
    pthread_once(&init_once, module_load);

    me = malloc(sizeof(*me));
    if(me == NULL)
        return NULL;
    me->cps = cps;
    me->brust = brust;
    me->token = 0;
    pthread_mutex_init(&me->mut, NULL);
    pthread_cond_init(&me->cond, NULL);

    pthread_mutex_lock(&mut_job);
    int pos = get_free_pos_unlock();
    if(pos < 0){
        pthread_mutex_unlock(&mut_job);
        free(me);
        return NULL;
    }
    me->pos = pos;
    job[me->pos] = me;
    pthread_mutex_unlock(&mut_job);

    return me;
}

int mytbf_fetchtoken(mytbf_t *ptr, int size){
    struct mytbf_st *me = ptr;
    pthread_mutex_lock(&me->mut);
    while(me->token <= 0){
        pthread_cond_wait(&me->cond, &me->mut);
    }
    int n = me->token > size ? me->token : size;
    me->token -= n;

    pthread_mutex_unlock(&me->mut);
    return n;
}

int mytbf_returntoken(mytbf_t *ptr, int size){
    struct mytbf_st *me = ptr;

    pthread_mutex_lock(&me->mut);
    me->token += size;
    if(me->token > me->brust)
        me->token = me->brust;
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->mut);
    return 0;
}

int mytbf_destory(mytbf_t *ptr){
    struct mytbf_st *me = ptr;
    pthread_mutex_lock(&mut_job);
    job[me->pos] = NULL;
    pthread_mutex_unlock(&mut_job);
    pthread_mutex_destroy(&me->mut);
    pthread_cond_destroy(&me->cond);
    free(ptr);
    return 0;
}