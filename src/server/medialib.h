#ifndef MEDIALIB_H__
#define MEDIALIB_H__

#include <stdio.h>
#include "../include/site_type.h"

struct mlib_listentry_st {
    chnid_t chnid; // 频道号
    char *desc; // 频道描述
};

int mlib_getchnlist(struct mlib_listentry_st **, int *);

int mlib_freechnlist(struct  mlib_listentry_st *);

/*
 * 获取频道信息
 * 读取频道chnid_t中size_t字节数据到void*位置，返回读到的字节数
 */
ssize_t mlib_readchn(chnid_t, void *, size_t);

#endif