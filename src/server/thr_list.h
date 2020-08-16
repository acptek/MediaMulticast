#ifndef THR_LIST_H__
#define THR_LIST_H__

#include "medialib.h"

/*
 * 创建节目单线程
 *
 */
int thr_list_create(struct mlib_listentry_st *, int );

int thr_list_destroy(void);

#endif