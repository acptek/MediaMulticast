#include <stdio.h>
#include <stdlib.h>
#include <glob.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <zconf.h>

#include "medialib.h"
#include "mytbf.h"
#include "../include/proto.h"
#include "server_conf.h"

#define PATHSIZE 1024
#define LINEBUFSIZE 1024
#define MP3_BITRATE 64*1024

struct channel_context_st{
    chnid_t chnid;
    char *desc;
    glob_t mp3glob;
    int pos; // 当前播放到的文件的位置
    int fd;
    off_t offset; //数据分段发送出去
    mytbf_t *tbf; // 做流量控制
};

struct channel_context_st channel[MAXCHNID+1];

// 路径转换成列表对象
static struct channel_context_st *pathtoentry(const char *path){
    syslog(LOG_INFO, "current path: %s\n", path);
    char pathstr[PATHSIZE] = {'\0'};
    char linebuf[LINEBUFSIZE];


    /* 分析当前路径文件 */
    strcat(pathstr, path), strcat(pathstr, "/desc.text");
    FILE *fp = fopen(pathstr, "r");
    syslog(LOG_INFO, "channel dir: %s\n", pathstr);
    // 当前目录没有 desc.text
    if(fp == NULL){
        syslog(LOG_INFO, "%s is not a channel dir(cannot find desc.text)", path);
        fclose(fp);
        return NULL;
    }
    // 当前desc.text没有内容
    if(fgets(linebuf, LINEBUFSIZE, fp) == NULL){
        syslog(LOG_INFO, "%s is not a channel dir(cannot get desc.text)", path);
        fclose(fp);
        return NULL;
    }
    // 读取完 pathstr = desc.text 文件后关闭fp
    fclose(fp);

    /* 构造当前频道的详细内容 */
    struct channel_context_st *me;
    me = malloc(sizeof(*me));
    if(me == NULL){
        syslog(LOG_ERR, "malloc(): %s", strerror(errno));
        return NULL;
    }
    // 初始化流控
    me->tbf = mytbf_init(MP3_BITRATE/8*4, MP3_BITRATE/8*10);
    if(me->tbf == NULL){
        syslog(LOG_ERR, "mytbf_init(): %s", strerror(errno));
        free(me);
        return NULL;
    }
    // me->desc = strdup(linebuf); // 填入当前频道的描述
    me->desc = malloc(strlen(linebuf)+1); // if err
    strcpy(me->desc, linebuf);

    // 接下分析音乐路径
    strncpy(pathstr, path, PATHSIZE); // 最多复制PATHSIZE个字符到pathstr中
    strncat(pathstr, "/*.mp3", PATHSIZE-1); // 然后后面加上 /*.mp3

    static chnid_t curid = MINCHNID; // 静态全局变量
    if(glob(pathstr, 0, NULL, &me->mp3glob) != 0){
        curid++;
        syslog(LOG_ERR, "%s is not a channel dir(cannot find mp3 files)", path);
        free(me);
        return NULL;
    }
    me->pos = 0; // 当前所在媒体的位置
    me->offset = 0;
    me->fd = open(me->mp3glob.gl_pathv[me->pos], O_RDONLY);
    if(me->fd < 0){
        syslog(LOG_WARNING, "%s open failed", me->mp3glob.gl_pathv[me->pos]);
        free(me);
        return NULL;
    }
    me->chnid = curid++;
    return me;
}

// 可以读文件，也可以读数据库
// 进行回填
int mlib_getchnlist(struct mlib_listentry_st **result, int *resnum){
    char path[PATHSIZE];

    // 初始化channel数组
    for(int i = 0; i < MAXCHNID+1; ++i){
        channel[i].chnid = -1; // 当前channel未启用
    }

    // 组织路径 pattern
    snprintf(path, PATHSIZE, "%s/*", server_conf.media_dir);

    // 解析目录
    glob_t globres;
    if(glob(path, 0, NULL, &globres) != 0){
        return -1;
    }

    struct mlib_listentry_st *ptr;
    ptr = malloc(sizeof(struct mlib_listentry_st) * globres.gl_pathc);
    if(ptr == NULL){
        syslog(LOG_ERR, "malloc() error");
        exit(1);
    }


    int num = 0;
    struct channel_context_st *res;
    // 对每一个路径传入构造一个 channel_context_st
    for(int i = 0; i < globres.gl_pathc; ++i){
        res = pathtoentry(globres.gl_pathv[i]);
        if(res != NULL){
            syslog(LOG_INFO, "pathtoentry() return: CHN-%d %s", res->chnid, res->desc);
            memcpy(channel+res->chnid, res, sizeof(*res));
            ptr[num].chnid = res->chnid;
            //ptr[num].desc = malloc(sizeof(res->desc)+1);
            //strcpy(ptr->desc, res->desc);
            ptr[num].desc = strdup(res->desc);
            num++;
        }
    }
    // 重新分配内存，如果有多余则删去
    *result = realloc(ptr, sizeof(struct mlib_listentry_st) * num);
    if(*result == NULL){
        syslog(LOG_ERR, "realloc() failed");
    }

    *resnum = num;
    return 0;
}

int mlib_freechnlist(struct  mlib_listentry_st *ptr){
    free(ptr);
    return 0;
}

// 打开下一个文件
static int open_next(chnid_t chnid) {
    for(int i = 0; i < channel[chnid].mp3glob.gl_pathc; ++i) {
        channel[chnid].pos++;
        if(channel[chnid].pos == channel[chnid].mp3glob.gl_pathc) {
            // return -1; // 所有文件都打开失败
            break;
        }
        close(channel[chnid].fd);
        channel[chnid].fd = open(channel[chnid].mp3glob.gl_pathv[channel[chnid].pos], O_RDONLY);
        if(channel[chnid].fd < 0) {
            syslog(LOG_WARNING, "open(%s):%s", channel[chnid].mp3glob.gl_pathv[chnid], strerror(errno));
        } else {
            channel[chnid].offset = 0;
            return 0;
        }
    }
    syslog(LOG_ERR, "None of mp3s in channel %d is available", chnid);
    // exit(1);
}

// 由 thr_channel 模块调用
// 从chnid频道读取 大小为size的数据 到buf中
ssize_t mlib_readchn(chnid_t chnid, void *buf, size_t size){
    int tbfsize = mytbf_fetchtoken(channel[chnid].tbf, size);
    int len;
    while (1) {
        len = pread(channel[chnid].fd, buf, tbfsize, channel[chnid].offset);
        if(len < 0) {
            syslog(LOG_WARNING, "media file %s pread():%s", channel[chnid].mp3glob.gl_pathv[channel[chnid].pos], strerror(errno));
            open_next(chnid);
        } else if (len == 0) {
            syslog(LOG_WARNING, "media file %s is over", channel[chnid].mp3glob.gl_pathv[channel[chnid].pos]);
            open_next(chnid);
        } else {
            channel[chnid].offset += len;
            syslog(LOG_DEBUG, "epoch : %f", (channel[chnid].offset) / (16*1000*1.024));
            break;
        }
    }
    if(tbfsize - len > 0)
        mytbf_returntoken(channel[chnid].tbf, tbfsize-len);
    return len;
}