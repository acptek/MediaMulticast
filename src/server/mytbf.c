#include "mytbf.h"
#include <stdio.h>
#include <stdlib.h>

mytbf_t *mytbf_init(int cps, int brust);

int mytbf_fetchtoken(mytbf_t *, int);

int mytbf_returntoken(mytbf_t *, int);

int mytbf_destory(mytbf_t *);