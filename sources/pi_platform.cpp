#include <sys/stat.h>
#include <time.h>
#include <dlfcn.h>
#include <unistd.h>

#include "stdio.h"



int main(int argc, char ** argv) {
    
    char platformDll[] = "./platform.so";
    
    long lastChange = 0;
    
    bool run = true;
    
    void (*init)(void) = NULL;
    void (*iterate)(bool*) = NULL;
    void (*close)(void) = NULL;
    
    void * platformHandle = NULL;
    while(run){
        struct stat attr;
        stat(platformDll, &attr);
        if(lastChange != (long)attr.st_mtime){
            if(platformHandle != NULL){
                close();
                dlclose(platformHandle);
            }
            platformHandle = dlopen(platformDll, RTLD_NOW | RTLD_GLOBAL);
            if(platformHandle){
                init = (void (*)(void))dlsym(platformHandle, "init");
                iterate = (void (*)(bool*))dlsym(platformHandle, "iterate");
                close = (void (*)(void))dlsym(platformHandle, "close");
                if(close == NULL || iterate == NULL || close == NULL){
                    printf("Failed to find functions\n");
                    dlclose(platformHandle);
                    platformHandle = NULL;
                }else{
                    init();
                }
                
            }else{
                printf("dlerr: %s\n", dlerror());
            }
            
            
            lastChange = (long)attr.st_mtime;
        }
        if(platformHandle != NULL){
            iterate(&run);
        }else{
            printf("Failed to load platform module. Sleeping for 1 second.\n");
            sleep(1);
        }
    }
    if(platformHandle != NULL){
        close();
        dlclose(platformHandle);
    }
    
    return 0;
    
    
}