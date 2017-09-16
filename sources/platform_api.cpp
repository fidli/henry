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


struct PlatformState{
    GtkWindow * window;
    GtkWidget * drawArea;
    GdkPixbuf * pixbuf;
    GByteArray bytearray;
    int cameraHandle;
    int gamepadMotorHandle;
    int gamepadInputHandle;
    v4l2_capability cameraCapabilities;
    v4l2_format cameraFormat;
    v4l2_requestbuffers cameraIOmode;
    v4l2_buffer cameraBuffer;
    uint32 frame;
    uint32 lastFrameMark;
    float32 accumulator;
    ff_effect effect;
    float response;
    input_event play;
    bool validInit;
};


#include "domaincode.h"


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
    
    
    
    void * memoryStart = valloc(TEMP_MEM + PERSISTENT_MEM + STACK_MEM);
    if (memoryStart)
    {
        initMemory(memoryStart);
        
        platformState = (PlatformState *) mem.persistent;
        ASSERT(PERSISTENT_MEM >= sizeof(PlatformState) + sizeof(DomainInterface));
        platformState->validInit = false;
        
        bool hotload = hotloadDomain();
        if(hotload)
        {
            gtk_init(NULL, NULL);
            platformState->window = (GtkWindow *)gtk_window_new(GTK_WINDOW_TOPLEVEL);
            platformState->cameraHandle = open("/dev/video0", O_RDWR);
            platformState->gamepadMotorHandle = open("/dev/input/event2", O_RDWR);
            platformState->gamepadInputHandle = open("/dev/input/js0", O_RDONLY  | O_NONBLOCK);
            int index = 0;
            platformState->cameraFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            platformState->cameraIOmode.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            platformState->cameraIOmode.count = 1;
            platformState->cameraIOmode.memory =  V4L2_MEMORY_MMAP;
            platformState->cameraIOmode.reserved[0] = platformState->cameraIOmode.reserved[1] = 0;
            
            
            domainInterface->cameras[0].feed.info.interpretation = BitmapInterpretationType_YUY2;
            domainInterface->cameras[0].feed.info.origin = BitmapOriginType_TopLeft;
            domainInterface->cameras[0].feed.info.bitsPerSample = 8;
            domainInterface->cameras[0].feed.info.samplesPerPixel = 3;
            
            platformState->cameraBuffer = {};
            platformState->cameraBuffer.type = platformState->cameraIOmode.type;
            platformState->cameraBuffer.memory = V4L2_MEMORY_MMAP;
            platformState->cameraBuffer.index = 0;
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
            
            
            bool camera = platformState->cameraHandle != -1 && ioctl(platformState->cameraHandle, VIDIOC_QUERYCAP, &platformState->cameraCapabilities) != -1 && platformState->cameraCapabilities.device_caps & V4L2_CAP_VIDEO_CAPTURE && platformState->cameraCapabilities.device_caps & V4L2_CAP_STREAMING  && ioctl(platformState->cameraHandle, VIDIOC_G_FMT, &platformState->cameraFormat) != -1 && platformState->cameraFormat.fmt.pix.pixelformat & V4L2_PIX_FMT_YUYV && 
                ioctl(platformState->cameraHandle, VIDIOC_REQBUFS, &platformState->cameraIOmode) != -1 && platformState->cameraIOmode.count == 1 && ioctl(platformState->cameraHandle, VIDIOC_QUERYBUF, &platformState->cameraBuffer) != -1 &&
                ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settingsExposureManual) != -1 &&
                //ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settingsAutogain) != -1 &&
            ioctl(platformState->cameraHandle, VIDIOC_G_CTRL, &settingsExposure) != -1 &&
                ioctl(platformState->cameraHandle, VIDIOC_G_CTRL, &settingsBrightness) != -1 &&
                ioctl(platformState->cameraHandle, VIDIOC_G_CTRL, &settingsContrast) != -1 &&
                ioctl(platformState->cameraHandle, VIDIOC_G_CTRL, &settingsSharpness) != -1 &&
                ioctl(platformState->cameraHandle, VIDIOC_G_CTRL, &settingsGain) != -1 &&
                ioctl(platformState->cameraHandle, VIDIOC_G_CTRL, &settingsBacklight) != -1 &&
                ioctl(platformState->cameraHandle, VIDIOC_G_CTRL, &settingsWhiteBalance) != -1;
            //https://www.kernel.org/doc/Documentation/input/joystick-api.txt
            
            v4l2_queryctrl a;
            a.id = V4L2_CID_BASE;
            
            while(ioctl(platformState->cameraHandle, VIDIOC_QUERYCTRL, &a) != -1){
                
                printf("%s\n", a.name);
                a.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            }
            
            
            platformState->effect = {};
            platformState->effect.type = FF_RUMBLE;
            platformState->effect.id = -1;
            platformState->effect.u.rumble.strong_magnitude = 0;
            platformState->effect.u.rumble.weak_magnitude   = 0;
            platformState->effect.replay.length = 0;
            platformState->effect.replay.delay  = 0;
            
            
            
            
            
            bool gamepadMotor = platformState->gamepadMotorHandle != -1 && ioctl(platformState->gamepadMotorHandle, EVIOCSFF, &platformState->effect) != -1;
            
            platformState->play.type = EV_FF;
            platformState->play.code =  platformState->effect.id;
            platformState->play.value = 1;
            
            gamepadMotor = gamepadMotor && write(platformState->gamepadMotorHandle, (const void*) &platformState->play, sizeof(platformState->play)) != -1;
            bool gamepadInput =
                platformState->gamepadInputHandle != -1;
            
            bool window = platformState->window != NULL;
            
            
            
            if(window && camera && gamepadMotor && gamepadInput){
                domainInterface->cameras[0].feed.info.width = platformState->cameraFormat.fmt.pix.width;
                domainInterface->cameras[0].feed.info.height = platformState->cameraFormat.fmt.pix.height;
                domainInterface->cameras[0].feed.info.totalSize = domainInterface->cameras[0].feed.info.width * domainInterface->cameras[0].feed.info.height * 2;
                
                ASSERT(platformState->cameraBuffer.length == domainInterface->cameras[0].feed.info.totalSize);
                
                domainInterface->cameras[0].feed.data = (byte *) mmap(NULL, platformState->cameraBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, platformState->cameraHandle, platformState->cameraBuffer.m.offset);
                
                if(MAP_FAILED != (void *)domainInterface->cameras[0].feed.data){
                    
                    if(ioctl(platformState->cameraHandle, VIDIOC_QBUF, &platformState->cameraBuffer) != -1 &&
                       ioctl(platformState->cameraHandle, VIDIOC_STREAMON, &platformState->cameraIOmode.type) != -1){
                        
                        domainInterface->cameras[0].settings.contrast = settingsContrast.value;
                        domainInterface->cameras[0].settings.sharpness = settingsSharpness.value;
                        domainInterface->cameras[0].settings.brightness = settingsBrightness.value;
                        domainInterface->cameras[0].settings.exposure = settingsExposure.value;
                        
                        domainInterface->cameras[0].settings.gain = settingsGain.value;
                        domainInterface->cameras[0].settings.backlight = settingsBacklight.value;
                        
                        domainInterface->cameras[0].settings.whiteBalance = settingsWhiteBalance.value;
                        
                        
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
                    }
                }
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
            
            
            GdkEvent * event;
            
            while((event = gtk_get_current_event()) != NULL){
                
                
                gdk_event_free(event);
            }
            
            gtk_main_iteration_do(false);
            //https://www.linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec-single/v4l2.html#camera-controls
            
            
            
            
            
            
            pollfd p;
            p.fd = platformState->cameraHandle;
            p.events = POLLIN;
            //wait for camera frame
            poll(&p, 1, 0);
            //get camera frame
            while(ioctl(platformState->cameraHandle, VIDIOC_DQBUF, &platformState->cameraBuffer) == -1){
                printf("%u\n", errno);};
            
            
            //get gamepad input
            js_event jevent;
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
            
            //set settings
            v4l2_control settings;
            settings.id = V4L2_CID_EXPOSURE_ABSOLUTE;
            settings.value = domainInterface->cameras[0].settings.exposure;
            
            if(ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                printf("errno: %d\n", errno);
            }
            
            settings.id = V4L2_CID_BRIGHTNESS;
            settings.value = domainInterface->cameras[0].settings.brightness;
            
            if(ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                printf("errno: %d\n", errno);
            }
            
            settings.id = V4L2_CID_CONTRAST;
            settings.value = domainInterface->cameras[0].settings.contrast;
            
            if(ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                printf("errno: %d\n", errno);
            }
            
            settings.id = V4L2_CID_SHARPNESS;
            settings.value = domainInterface->cameras[0].settings.sharpness;
            
            if(ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                printf("errno: %d\n", errno);
            }
            
            settings.id = V4L2_CID_GAIN;
            settings.value = domainInterface->cameras[0].settings.gain;
            
            if(ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                printf("errno: %d\n", errno);
            }
            
            settings.id = V4L2_CID_BACKLIGHT_COMPENSATION;
            settings.value = domainInterface->cameras[0].settings.backlight;
            
            if(ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                printf("errno: %d\n", errno);
            }
            
            settings.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
            settings.value = domainInterface->cameras[0].settings.whiteBalance;
            
            if(ioctl(platformState->cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
                printf("errno: %d\n", errno);
            }
            
            
            //do the domain code iteration
            
            iterateDomain(keepRunning);
            
            //get data from camera next frame
            while(ioctl(platformState->cameraHandle, VIDIOC_QBUF, &platformState->cameraBuffer) == -1){
                printf("%u\n", errno);
            };
            
            
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
        munmap((void*)domainInterface->cameras[0].feed.data, domainInterface->cameras[0].feed.info.totalSize);
        ioctl(platformState->cameraHandle, VIDIOC_STREAMOFF, &platformState->cameraIOmode.type);
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
        close(platformState->cameraHandle);
        
        closeDll();
        free(mem.persistent);
        
    }
    
    
    
}




