#include "stdio.h"
#include "linux_types.h"
#include <errno.h>
#include <stdlib.h>
#include <malloc.h>
#include "string.h"
#include <gtk/gtk.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <dlfcn.h>

#include <linux/videodev2.h>
#include "util_time.h"
#include "linux_time.cpp"


bool * gassert;
char * gassertMessage;

#define ASSERT(expression) if(!(expression)) { sprintf(gassertMessage, "ASSERT FAILED on line %d in file %s\n", __LINE__, __FILE__); *gassert = true; printf(gassertMessage);}


#include "common.h"
#include <linux/input.h>
#include "linux_joystick.h"


#define PERSISTENT_MEM MEGABYTE(10)
#define TEMP_MEM MEGABYTE(10)
#define STACK_MEM MEGABYTE(10)


#include "util_mem.h"
#include "util_filesystem.h"
#include "linux_filesystem.cpp"

#include "util_font.cpp"
#include "util_image.cpp"




#include "domaincode.h"

struct PlatformState{
    GtkWindow * window;
    GtkWidget * drawArea;
    GdkPixbuf * pixbuf;
    GByteArray bytearray;
    struct Camera{
        int cameraHandle;
        v4l2_capability cameraCapabilities;
        v4l2_format cameraFormat;
        v4l2_requestbuffers cameraIOmode;
        v4l2_buffer cameraBuffer;
    } cameras[CameraPositionCount];
    
    int gamepadMotorHandle;
    int gamepadInputHandle;
    uint32 frame;
    uint32 lastFrameMark;
    float32 accumulator;
    ff_effect effect;
    float response;
    input_event play;
    bool validInit;
    bool swap;
    bool wasSwap;
};


char domainDll[] = "./domain.so";

long lastChange = 0;
void * domainHandle = NULL;
void (*initDomain)(bool*,char*,void*) = NULL;
void (*iterateDomain)(bool*) = NULL;
void (*closeDomain)(void) = NULL;

PlatformState * platformState;



inline void closeDll(){
    if(domainHandle != NULL){
        if(closeDomain != NULL) closeDomain();
        dlclose(domainHandle);
        domainHandle = NULL;
        initDomain = NULL;
        closeDomain = NULL;
        iterateDomain = NULL;
    }
}

inline bool hotloadDomain(){
    struct stat attr;
    stat(domainDll, &attr);
    
    if(lastChange != (long)attr.st_mtime){
        printf("Domaincode changed\n");
        closeDll();
        *gassert = false;
        domainHandle = dlopen(domainDll, RTLD_NOW | RTLD_GLOBAL);
        if(domainHandle){
            initDomain = (void (*)(bool*,char*,void*))dlsym(domainHandle, "initDomain");
            iterateDomain = (void (*)(bool*))dlsym(domainHandle, "iterateDomain");
            closeDomain = (void (*)(void))dlsym(domainHandle, "closeDomain");
            if(closeDomain == NULL || iterateDomain == NULL || closeDomain == NULL){
                printf("Error: Failed to find functions\n");
                dlclose(domainHandle);
                domainHandle = NULL;
                return false;
            }else{
                initDomain(gassert, gassertMessage, (void*)(platformState+1));
            }
            
        }else{
            printf("dlerr: %s\n", dlerror());
            return false;
        }
        
        
        lastChange = (long)attr.st_mtime;
    }
    return true;
}

DomainInterface * domainInterface;


gboolean
draw(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    gdk_draw_pixbuf(widget->window, NULL, platformState->pixbuf, 0, 0, 0, 0, domainInterface->renderTarget.info.width, domainInterface->renderTarget.info.height, GDK_RGB_DITHER_NONE, 0, 0);
    
    
    return TRUE;
}



extern "C" void initPlatform(bool * assert, char * assertMessage)
{
    gassertMessage = assertMessage;
    gassert = assert;
    lastChange = 0;
    
    
    
    void * memoryStart = valloc(TEMP_MEM + PERSISTENT_MEM + STACK_MEM);
    if (memoryStart)
    {
        initMemory(memoryStart);
        
        platformState = (PlatformState *) mem.persistent;
        ASSERT(PERSISTENT_MEM >= sizeof(PlatformState) + sizeof(DomainInterface));
        platformState->validInit = false;
        platformState->swap = false;
        platformState->wasSwap = false;
        bool hotload = hotloadDomain();
        if(hotload)
        {
            gtk_init(NULL, NULL);
            
            bool camera = true;
            
            //init cameras
            char deviceName[64];
            for(uint8 ci = 0; ci < CameraPositionCount; ci++){
                
                sprintf(deviceName, "/dev/video%hhu", ci);
                printf("dn: %s\n", deviceName);
                
                platformState->cameras[ci].cameraHandle = open(deviceName, O_RDWR);
                platformState->cameras[ci].cameraFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                platformState->cameras[ci].cameraIOmode.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                platformState->cameras[ci].cameraIOmode.count = 1;
                platformState->cameras[ci].cameraIOmode.memory =  V4L2_MEMORY_MMAP;
                platformState->cameras[ci].cameraIOmode.reserved[0] = platformState->cameras[ci].cameraIOmode.reserved[1] = 0;
                
                
#if 0
                //cam features
                v4l2_queryctrl a;
                a.id = V4L2_CID_BASE;
                
                while(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_QUERYCTRL, &a) != -1){
                    
                    printf("ci: %u, %s\n", ci, a.name);
                    a.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
                }
#endif
                
                
                domainInterface->cameras[ci].feed.info.interpretation = BitmapInterpretationType_YUY2;
                domainInterface->cameras[ci].feed.info.origin = BitmapOriginType_TopLeft;
                domainInterface->cameras[ci].feed.info.bitsPerSample = 8;
                domainInterface->cameras[ci].feed.info.samplesPerPixel = 3;
                
                platformState->cameras[ci].cameraBuffer = {};
                platformState->cameras[ci].cameraBuffer.type = platformState->cameras[ci].cameraIOmode.type;
                platformState->cameras[ci].cameraBuffer.memory = V4L2_MEMORY_MMAP;
                platformState->cameras[ci].cameraBuffer.index = 0;
                
                
                //driver setings
                //https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/vidioc-querycap.html#vidioc-querycap
                //https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-yuyv.html Y is greyscale
                //https://www.linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec-single/v4l2.html#camera-controls
                v4l2_control settingsExposureManual;
                settingsExposureManual.id = V4L2_CID_EXPOSURE_AUTO;
                settingsExposureManual.value = V4L2_EXPOSURE_MANUAL;
                
                
                
                v4l2_control settingsExposure;
                settingsExposure.id = V4L2_CID_EXPOSURE_ABSOLUTE;
                
                v4l2_control settingsBrightness;
                settingsBrightness.id = V4L2_CID_BRIGHTNESS;
                
                v4l2_control settingsContrast;
                settingsContrast.id = V4L2_CID_CONTRAST;
                
                v4l2_control settingsSharpness;
                settingsSharpness.id = V4L2_CID_SHARPNESS;
                
                v4l2_control settingsGain;
                settingsGain.id = V4L2_CID_GAIN;
                
                v4l2_control settingsWhiteBalance;
                settingsWhiteBalance.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
                
                
                v4l2_control settingsBacklight;
                settingsBacklight.id = V4L2_CID_BACKLIGHT_COMPENSATION;
                
                camera &= 
                    platformState->cameras[ci].cameraHandle != -1 &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_QUERYCAP, &platformState->cameras[ci].cameraCapabilities) != -1 &&
                    platformState->cameras[ci].cameraCapabilities.device_caps & V4L2_CAP_VIDEO_CAPTURE &&
                    platformState->cameras[ci].cameraCapabilities.device_caps & V4L2_CAP_STREAMING  &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_G_FMT, &platformState->cameras[ci].cameraFormat) != -1 &&
                    platformState->cameras[ci].cameraFormat.fmt.pix.pixelformat & V4L2_PIX_FMT_YUYV && 
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_REQBUFS, &platformState->cameras[ci].cameraIOmode) != -1 &&
                    platformState->cameras[ci].cameraIOmode.count == 1  &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_QUERYBUF, &platformState->cameras[ci].cameraBuffer) != -1 &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_S_CTRL, &settingsExposureManual) != -1 &&
                    //ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settingsAutogain) != -1 &&
                ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_G_CTRL, &settingsExposure) != -1 &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_G_CTRL, &settingsBrightness) != -1 &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_G_CTRL, &settingsContrast) != -1 &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_G_CTRL, &settingsSharpness) != -1 &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_G_CTRL, &settingsGain) != -1 &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_G_CTRL, &settingsBacklight) != -1 &&
                    ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_G_CTRL, &settingsWhiteBalance) != -1;
                
                if(camera){
                    
                    domainInterface->cameras[ci].feed.info.width = platformState->cameras[ci].cameraFormat.fmt.pix.width;
                    domainInterface->cameras[ci].feed.info.height = platformState->cameras[ci].cameraFormat.fmt.pix.height;
                    domainInterface->cameras[ci].feed.info.totalSize = domainInterface->cameras[ci].feed.info.width * domainInterface->cameras[ci].feed.info.height * 2;
                    
                    ASSERT(platformState->cameras[ci].cameraBuffer.length == domainInterface->cameras[ci].feed.info.totalSize);
                    domainInterface->cameras[ci].feed.data = (byte *) mmap(NULL, platformState->cameras[ci].cameraBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, platformState->cameras[ci].cameraHandle, platformState->cameras[ci].cameraBuffer.m.offset);
                    
                    
                    camera &= 
                        MAP_FAILED != (void *)domainInterface->cameras[ci].feed.data &&
                        ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_QBUF, &platformState->cameras[ci].cameraBuffer) != -1 &&
                        ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_STREAMON, &platformState->cameras[ci].cameraIOmode.type) != -1;
                    
                    
                    
                    domainInterface->cameras[ci].settings.contrast = settingsContrast.value;
                    domainInterface->cameras[ci].settings.sharpness = settingsSharpness.value;
                    domainInterface->cameras[ci].settings.brightness = settingsBrightness.value;
                    domainInterface->cameras[ci].settings.exposure = settingsExposure.value;
                    
                    domainInterface->cameras[ci].settings.gain = settingsGain.value;
                    domainInterface->cameras[ci].settings.backlight = settingsBacklight.value;
                    
                    domainInterface->cameras[ci].settings.whiteBalance = settingsWhiteBalance.value;
                    
                    
                    
                    
                }
            }
            platformState->window = (GtkWindow *)gtk_window_new(GTK_WINDOW_TOPLEVEL);
            
            
            platformState->gamepadInputHandle = open("/dev/input/js0", O_RDONLY  | O_NONBLOCK);
            int index = 0;
            
            
            //https://www.kernel.org/doc/Documentation/input/joystick-api.txt
            
            bool gamepadMotor = false;
            for(uint8 i = 0; i < 10; i++){
                sprintf(deviceName, "/dev/input/event%hhu", i);
                platformState->gamepadMotorHandle = open(deviceName, O_RDWR);
                platformState->effect = {};
                platformState->effect.type = FF_RUMBLE;
                platformState->effect.id = -1;
                platformState->effect.u.rumble.strong_magnitude = 0;
                platformState->effect.u.rumble.weak_magnitude   = 0;
                platformState->effect.replay.length = 0;
                platformState->effect.replay.delay  = 0;
                
                gamepadMotor = platformState->gamepadMotorHandle != -1 && ioctl(platformState->gamepadMotorHandle, EVIOCSFF, &platformState->effect) != -1;
                
                platformState->play.type = EV_FF;
                platformState->play.code =  platformState->effect.id;
                platformState->play.value = 1;
                
                gamepadMotor &=  write(platformState->gamepadMotorHandle, (const void*) &platformState->play, sizeof(platformState->play)) != -1;
                if(gamepadMotor){
                    printf("motor dn: %s\n", deviceName);
                    break;
                }
            }
            
            
            bool gamepadInput =
                platformState->gamepadInputHandle != -1;
            
            bool window = platformState->window != NULL;
            
            
            
            if(window && camera && gamepadMotor && gamepadInput){
                
                gtk_window_set_screen(platformState->window, gdk_screen_get_default());
                gtk_window_maximize(platformState->window);
                platformState->drawArea = gtk_drawing_area_new();
                gtk_container_add((GtkContainer * )platformState->window, platformState->drawArea);
                g_signal_connect (G_OBJECT (platformState->drawArea), "expose_event",
                                  G_CALLBACK (draw), NULL);
                gtk_widget_show(platformState->drawArea);
                gtk_window_set_decorated(platformState->window, false);
                gtk_widget_show_now((GtkWidget *)platformState->window);
                gint w;
                gint h;
                gtk_window_get_size(platformState->window, &w, &h);
                domainInterface->renderTarget.info.width = w;
                domainInterface->renderTarget.info.height = h;
                domainInterface->renderTarget.info.origin = BitmapOriginType_TopLeft;
                domainInterface->renderTarget.info.interpretation = BitmapInterpretationType_ABGR;
                domainInterface->renderTarget.info.bitsPerSample = 8;
                domainInterface->renderTarget.info.samplesPerPixel = 4;
                
                
                platformState->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, w, h);
                domainInterface->renderTarget.data = gdk_pixbuf_get_pixels(platformState->pixbuf);
                
                platformState->accumulator = 0;
                platformState->frame = 0;
                domainInterface->lastFps = 0;
                platformState->response = 0;
                domainInterface->input = {};
                platformState->validInit = true;
                
                printf("Valid platform init\n");
                
            }else{
                printf("invalid init\n");
                if(!gamepadMotor){
                    printf("Gamepad motor init err\n");
                }
                if(!gamepadInput){
                    printf("Gamepad input init err\n");
                }
                if(!window){
                    printf("Window init err\n");
                }
                if(!camera){
                    printf("Camera init err\n");
                }
                printf("errno: %d\n", errno);
            }
        }
    }
    
}

extern "C" void iterate(bool * keepRunning){
    
    if(platformState->validInit)
    {
        
        hotloadDomain();
        //printf("platform iterate\n");
        if(domainHandle != NULL)
        {
            
            float32 startTime = getProcessCurrentTime();
            if(platformState->accumulator >= 1){
                uint32 seconds = (uint32)platformState->accumulator;
                platformState->accumulator = platformState->accumulator - seconds;
                platformState->lastFrameMark = platformState->frame;
            }else{
                domainInterface->lastFps = (uint32)((platformState->frame - platformState->lastFrameMark) / platformState->accumulator);
            }
            
#if 0
            
            GdkEvent * event;
            
            while((event = gtk_get_current_event()) != NULL){
                
                
                gdk_event_free(event);
            }
            
#endif
            gtk_main_iteration_do(false);
            //https://www.linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec-single/v4l2.html#camera-controls
            
            
            
            
            for(uint8 ci = 0; ci < CameraPositionCount; ci++){
                pollfd p;
                p.fd = platformState->cameras[ci].cameraHandle;
                p.events = POLLIN;
                //wait for camera frame
                poll(&p, 1, 0);
                //get camera frame
                while(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_DQBUF, &platformState->cameras[ci].cameraBuffer) == -1){
                    printf("%u\n", errno);};
                
            }
            
            
            
            //get gamepad input
            js_event jevent;
            platformState->swap = false;
            int bytesRead = -1;
            while((bytesRead = read(platformState->gamepadInputHandle, &jevent, sizeof(jevent))) != -1){
                if(bytesRead == sizeof(jevent)){
                    if(!(jevent.type & JS_EVENT_INIT)){
                        if(jevent.type & JS_EVENT_AXIS){
                            if(jevent.number < 6){
                                domainInterface->input.analog.values[jevent.number] = jevent.value;
                            }else{
                                if(jevent.number == 6){
                                    if(jevent.value < 0){
                                        domainInterface->input.digital.left = true;
                                        domainInterface->input.digital.right = false;
                                    }else if(jevent.value > 0){
                                        domainInterface->input.digital.right = true;
                                        domainInterface->input.digital.left = false;
                                    }else{
                                        domainInterface->input.digital.left = domainInterface->input.digital.right = false;
                                    }
                                }else if(jevent.number == 7){
                                    if(jevent.value < 0){
                                        domainInterface->input.digital.up = true;
                                        domainInterface->input.digital.down = false;
                                    }else if(jevent.value > 0){
                                        domainInterface->input.digital.down = true;
                                        domainInterface->input.digital.up = false;
                                    }else{
                                        domainInterface->input.digital.down = domainInterface->input.digital.up = false;
                                    }
                                }
                            }
                        }else if(jevent.type & JS_EVENT_BUTTON){
                            if(jevent.number == 6){
                                domainInterface->input.digital.menu = jevent.value == 1;
                            }else if(jevent.number == 0){
                                platformState->swap = true;
                                domainInterface->input.digital.x = jevent.value == 1;
                            }else{
                                printf("n: %hhu v: %hd\n", jevent.number, jevent.value);
                            }
                        }
                    }
                }
            }
            
            
            
            //set renderer dimensions
            gint width;
            gint height;
            gtk_window_get_size(platformState->window, &width, &height);
            gtk_widget_set_size_request(platformState->drawArea, width, height);
            domainInterface->renderTarget.info.width = width;
            domainInterface->renderTarget.info.height = height;
            
            
            //printf("%hhd %hhd %hhd %hhd\n", platformState->gamepadInput.digital.up, platformState->gamepadInput.digital.right, platformState->gamepadInput.digital.down, platformState->gamepadInput.digital.left);
            
            platformState->effect.u.rumble.weak_magnitude  = (uint16) (domainInterface->feedback * ((uint16) -1));
            
            //pudate effect
            if (ioctl(platformState->gamepadMotorHandle, EVIOCSFF, &platformState->effect) == -1) { 
                printf("%u\n", errno);
            }
            
            for(uint8 ci = 0; ci < CameraPositionCount; ci++){
                
                //set settings
                v4l2_control settings;
                settings.id = V4L2_CID_EXPOSURE_ABSOLUTE;
                settings.value = domainInterface->cameras[ci].settings.exposure;
                
                if(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                    printf("errno: %d\n", errno);
                }
                
                settings.id = V4L2_CID_BRIGHTNESS;
                settings.value = domainInterface->cameras[ci].settings.brightness;
                
                if(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                    printf("errno: %d\n", errno);
                }
                
                settings.id = V4L2_CID_CONTRAST;
                settings.value = domainInterface->cameras[ci].settings.contrast;
                
                if(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                    printf("errno: %d\n", errno);
                }
                
                settings.id = V4L2_CID_SHARPNESS;
                settings.value = domainInterface->cameras[ci].settings.sharpness;
                
                if(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                    printf("errno: %d\n", errno);
                }
                
                settings.id = V4L2_CID_GAIN;
                settings.value = domainInterface->cameras[ci].settings.gain;
                
                if(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                    printf("errno: %d\n", errno);
                }
                
                settings.id = V4L2_CID_BACKLIGHT_COMPENSATION;
                settings.value = domainInterface->cameras[ci].settings.backlight;
                
                if(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                    printf("errno: %d\n", errno);
                }
                
                settings.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
                settings.value = domainInterface->cameras[ci].settings.whiteBalance;
                
                if(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                    printf("errno: %d\n", errno);
                }
            }
            
            if(platformState->swap && !platformState->wasSwap){
                PlatformState::Camera tmp2 = platformState->cameras[0];
                platformState->cameras[0] = platformState->cameras[1];
                platformState->cameras[0] = tmp2;
            }
            
            platformState->wasSwap = platformState->swap;
            //do the domain code iteration
            //printf("iterating domain \n");
            iterateDomain(keepRunning);
            
            for(uint8 ci = 0; ci < CameraPositionCount; ci++){
                //get data from camera next frame
                while(ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_QBUF, &platformState->cameras[ci].cameraBuffer) == -1){
                    printf("%u\n", errno);
                };
            }
            
            
            //call draw
            
            gtk_widget_queue_draw((GtkWidget *)platformState->window);
            gdk_window_process_all_updates();
            platformState->frame++;
            platformState->accumulator += getProcessCurrentTime() - startTime;
        }
        else
        {
            sleep(1);
        }
    }else{
        sleep(1);
        
    }
}

extern "C" void closePlatform(){
    printf("calling close\n");
    if(mem.persistent != NULL){
        for(uint32 ci = 0; ci < CameraPositionCount; ci++){
            munmap((void*)domainInterface->cameras[ci].feed.data, domainInterface->cameras[ci].feed.info.totalSize);
            ioctl(platformState->cameras[ci].cameraHandle, VIDIOC_STREAMOFF, &platformState->cameras[ci].cameraIOmode.type);
            close(platformState->cameras[ci].cameraHandle);
        }
        
        if(platformState->window != NULL){
            printf("destroying window\n");
            //gtk_widget_destroy((GtkWidget *)platformState->drawArea);
            gtk_widget_destroy((GtkWidget *)platformState->window);
        }
        
        input_event play;
        play.type = EV_FF;
        play.code =  platformState->effect.id;
        play.value = 0;
        write(platformState->gamepadMotorHandle, (const void*) &play, sizeof(play));
        close(platformState->gamepadInputHandle);
        close(platformState->gamepadMotorHandle);
        
        
        closeDll();
        free(mem.persistent);
        
    }
    
    
    
}




