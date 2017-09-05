#include "stdio.h"
#include "linux_types.h"
#include <stdlib.h>
#include <malloc.h>

#define PERSISTENT_MEM 0
#define TEMP_MEM MEGABYTE(10)
#define STACK_MEM MEGABYTE(10)

#include "common.h"
#include "util_mem.h"

struct Context{
    bool validInit;
};

Context context;

extern "C" void init(){
    context.validInit = false;
    void * memoryStart = valloc(TEMP_MEM + PERSISTENT_MEM + STACK_MEM);
    if (memoryStart) {
        initMemory(memoryStart);
        context.validInit = true;
    }
}

extern "C" void iterate(bool * keepRunning){
    if(context.validInit){
        printf("VALID\n");
    }else{
        printf("invalid init\n");
    }
}

extern "C" void close(){
    if(mem.persistent != NULL){
        free(mem.persistent);
    }
}




