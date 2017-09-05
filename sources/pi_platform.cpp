#include <sys/stat.h>
#include <time.h>
#include <dlfcn.h>
#include <unistd.h>

#include "stdio.h"

#undef ASSERT
#ifdef RELEASE
#define ASSERT 
#else
bool gassert = false;
char gabuffer[1024];
#define ASSERT(expression) if(!(expression)) { sprintf(assertMessage, "ASSERT FAILED on line %d in file %s\n", __LINE__, __FILE__); gassert = true; printf(assertMessage);}
#endif


int main(int argc, char ** argv) {
    
    char platformDll[] = "./platform.so";
    
    long lastChange = 0;
    
    bool run = true;
    
    void (*initPlatform)(bool*,char*) = NULL;
    void (*iterate)(bool*) = NULL;
    void (*closePlatform)(void) = NULL;
    
    void * platformHandle = NULL;
    while(run){
        struct stat attr;
        stat(platformDll, &attr);
        if(lastChange != (long)attr.st_mtime){
            if(platformHandle != NULL){
                closePlatform();
                dlclose(platformHandle);
            }
            gassert = false;
            platformHandle = dlopen(platformDll, RTLD_NOW | RTLD_GLOBAL);
            if(platformHandle){
                initPlatform = (void (*)(bool*,char*))dlsym(platformHandle, "initPlatform");
                iterate = (void (*)(bool*))dlsym(platformHandle, "iterate");
                closePlatform = (void (*)(void))dlsym(platformHandle, "closePlatform");
                if(closePlatform == NULL || iterate == NULL || closePlatform == NULL){
                    printf("Failed to find functions\n");
                    dlclose(platformHandle);
                    platformHandle = NULL;
                }else{
                    initPlatform(&gassert, gabuffer);
                }
                
            }else{
                printf("dlerr: %s\n", dlerror());
            }
            
            
            lastChange = (long)attr.st_mtime;
        }
        if(gassert){
            printf(gabuffer);
        }else{
            if(platformHandle != NULL){
                iterate(&run);
            }else{
                printf("Failed to load platform module. Sleeping for 1 second.\n");
                sleep(1);
            }
        }
    }
    if(platformHandle != NULL){
        closePlatform();
        dlclose(platformHandle);
    }
    
    return 0;
    
    
}