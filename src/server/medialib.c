#include <stdio.h>
#include <stdlib.h>
#include <glob.h>

#include "medialib.h"

struct channel_context_st{
    chnid_t chnid;
    char *desc;
    glob_t mp3glob;
    int pos;
    int fd;
    off_t offset; //数据分段发送出去
    mytbf_t *tbf; // 做流量控制
};

int mlib_getchnlist(struct mlib_listentry_st ** mdia, int * size){

}
