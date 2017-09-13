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


#include <linux/videodev2.h>
#include "util_time.h"
#include "linux_time.cpp"


bool * gassert;
char * gassertMessage;

#define ASSERT(expression) if(!(expression)) { sprintf(gassertMessage, "ASSERT FAILED on line %d in file %s\n", __LINE__, __FILE__); *gassert = true; printf(gassertMessage);}


#include "common.h"
#include <linux/input.h>
#include "linux_joystick.h"


#define PERSISTENT_MEM 0
#define TEMP_MEM MEGABYTE(10)
#define STACK_MEM MEGABYTE(10)


#include "util_mem.h"
#include "util_filesystem.h"
#include "linux_filesystem.cpp"

#include "util_font.cpp"
#include "util_image.cpp"
#include "util_conv.cpp"

struct Context{
    bool validInit;
    Image cameraFeed;
};

Context context;

union GamepadInput{
    union {
        struct{
            struct {
                int16 x, y;
            } left;
            int16 zLeft;
            struct {
                int16 x, y;
            } right;
        };
        int16 values[5];
    } analog;
    struct {
        bool up;
        bool left;
        bool right;
        bool down;
    } digital;
};



struct State{
    BitmapFont font;
    GtkWindow * window;
    GtkWidget * drawArea;
    GdkPixbuf * pixbuf;
    GByteArray bytearray;
    Image renderTarget;
    int cameraHandle;
    int gamepadMotorHandle;
    int gamepadInputHandle;
    v4l2_capability cameraCapabilities;
    v4l2_format cameraFormat;
    v4l2_requestbuffers cameraIOmode;
    v4l2_buffer cameraBuffer;
    char status[256];
    uint32 frame;
    uint32 lastFps;
    uint32 lastFrameMark;
    float32 accumulator;
    ff_effect effect;
    float response;
    input_event play;
    GamepadInput gamepadInput;
    int32 exposure;
};

State state;

gboolean
draw(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    gdk_draw_pixbuf(widget->window, NULL, state.pixbuf, 0, 0, 0, 0, state.renderTarget.info.width, state.renderTarget.info.height, GDK_RGB_DITHER_NONE, 0, 0);
    
    
    return TRUE;
}

extern "C" void initPlatform(bool * assert, char * assertMessage){
    context.validInit = false;
    gassertMessage = assertMessage;
    gassert = assert;
    void * memoryStart = valloc(TEMP_MEM + PERSISTENT_MEM + STACK_MEM);
    if (memoryStart) {
        initMemory(memoryStart);
        FileContents fontContents;
        Image fontImage;
        bool font = readFile("data/font.bmp", &fontContents) && decodeBMP(&fontContents, &fontImage) && flipY(&fontImage) && initBitmapFont(&state.font, &fontImage, fontImage.info.width / 16);
        gtk_init(NULL, NULL);
        state.window = (GtkWindow *)gtk_window_new(GTK_WINDOW_TOPLEVEL);
        state.cameraHandle = open("/dev/video0", O_RDWR);
        state.gamepadMotorHandle = open("/dev/input/event4", O_RDWR);
        state.gamepadInputHandle = open("/dev/input/js0", O_RDONLY  | O_NONBLOCK);
        int index = 0;
        state.cameraFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        state.cameraIOmode.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        state.cameraIOmode.count = 1;
        state.cameraIOmode.memory =  V4L2_MEMORY_MMAP;
        state.cameraIOmode.reserved[0] = state.cameraIOmode.reserved[1] = 0;
        context.cameraFeed.info.interpretation = BitmapInterpretationType_YUY2;
        context.cameraFeed.info.origin = BitmapOriginType_TopLeft;
        context.cameraFeed.info.bitsPerSample = 8;
        context.cameraFeed.info.samplesPerPixel = 3;
        state.cameraBuffer = {};
        state.cameraBuffer.type = state.cameraIOmode.type;
        state.cameraBuffer.memory = V4L2_MEMORY_MMAP;
        state.cameraBuffer.index = 0;
        //https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/vidioc-querycap.html#vidioc-querycap
        //https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-yuyv.html Y is greyscale
        //https://www.linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec-single/v4l2.html#camera-controls
        v4l2_control settings;
        settings.id = V4L2_CID_EXPOSURE_AUTO;
        settings.value = V4L2_EXPOSURE_MANUAL;
        bool camera = state.cameraHandle != -1 && ioctl(state.cameraHandle, VIDIOC_QUERYCAP, &state.cameraCapabilities) != -1 && state.cameraCapabilities.device_caps & V4L2_CAP_VIDEO_CAPTURE && state.cameraCapabilities.device_caps & V4L2_CAP_STREAMING  && ioctl(state.cameraHandle, VIDIOC_G_FMT, &state.cameraFormat) != -1 && state.cameraFormat.fmt.pix.pixelformat & V4L2_PIX_FMT_YUYV && 
            ioctl(state.cameraHandle, VIDIOC_REQBUFS, &state.cameraIOmode) != -1 && state.cameraIOmode.count == 1 && ioctl(state.cameraHandle, VIDIOC_QUERYBUF, &state.cameraBuffer) != -1 &&
            ioctl(state.cameraHandle, VIDIOC_S_CTRL, &settings) != -1;
        //https://www.kernel.org/doc/Documentation/input/joystick-api.txt
        
        
        state.effect = {};
        state.effect.type = FF_RUMBLE;
        state.effect.id = -1;
        state.effect.u.rumble.strong_magnitude = 0;
        state.effect.u.rumble.weak_magnitude   = 0;
        state.effect.replay.length = 0;
        state.effect.replay.delay  = 0;
        
        
        
        
        
        bool gamepadMotor = state.gamepadMotorHandle != -1 && ioctl(state.gamepadMotorHandle, EVIOCSFF, &state.effect) != -1;
        
        state.play.type = EV_FF;
        state.play.code =  state.effect.id;
        state.play.value = 1;
        
        gamepadMotor = gamepadMotor && write(state.gamepadMotorHandle, (const void*) &state.play, sizeof(state.play)) != -1;
        bool gamepadInput =
            state.gamepadInputHandle != -1;
        
        bool window = state.window != NULL;
        
        
        
        if(window && font && camera && gamepadMotor && gamepadInput){
            context.cameraFeed.info.width = state.cameraFormat.fmt.pix.width;
            context.cameraFeed.info.height = state.cameraFormat.fmt.pix.height;
            context.cameraFeed.info.totalSize = context.cameraFeed.info.width * context.cameraFeed.info.height * 2;
            
            ASSERT(state.cameraBuffer.length == context.cameraFeed.info.totalSize);
            
            context.cameraFeed.data = (byte *) mmap(NULL, state.cameraBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, state.cameraHandle, state.cameraBuffer.m.offset);
            
            if(MAP_FAILED != (void *)context.cameraFeed.data){
                
                if(ioctl(state.cameraHandle, VIDIOC_QBUF, &state.cameraBuffer) != -1 &&
                   ioctl(state.cameraHandle, VIDIOC_STREAMON, &state.cameraIOmode.type) != -1){
                    
                    gtk_window_set_screen(state.window, gdk_screen_get_default());
                    gtk_window_maximize(state.window);
                    state.drawArea = gtk_drawing_area_new();
                    gtk_container_add((GtkContainer * )state.window, state.drawArea);
                    g_signal_connect (G_OBJECT (state.drawArea), "expose_event",
                                      G_CALLBACK (draw), NULL);
                    gtk_widget_show(state.drawArea);
                    gtk_window_set_decorated(state.window, false);
                    gtk_widget_show_now((GtkWidget *)state.window);
                    gint w;
                    gint h;
                    gtk_window_get_size(state.window, &w, &h);
                    state.renderTarget.info.width = w;
                    state.renderTarget.info.height = h;
                    state.renderTarget.info.origin = BitmapOriginType_TopLeft;
                    state.renderTarget.info.interpretation = BitmapInterpretationType_ABGR;
                    state.renderTarget.info.bitsPerSample = 8;
                    state.renderTarget.info.samplesPerPixel = 4;
                    
                    
                    state.pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, w, h);
                    state.renderTarget.data = gdk_pixbuf_get_pixels(state.pixbuf);
                    
                    state.accumulator = 0;
                    state.frame = 0;
                    state.lastFps = 0;
                    state.response = 0;
                    state.gamepadInput = {};
                    context.validInit = true;
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
            if(!font){
                printf("Font init err\n");
            }
            printf("errno: %d\n", errno);
        }
    }
    
}

extern "C" void iterate(bool * keepRunning){
    if(context.validInit){
        float32 startTime = getProcessCurrentTime();
        if(state.accumulator >= 1){
            uint32 seconds = (uint32)state.accumulator;
            state.accumulator = state.accumulator - seconds;
            state.lastFrameMark = state.frame;
        }else{
            state.lastFps = (uint32)((state.frame - state.lastFrameMark) / state.accumulator);
        }
        
        sprintf(state.status, "FPS:%u R:%.3f E:%d", state.lastFps, state.response, state.exposure);
        GdkEvent * event;
        
        while((event = gtk_get_current_event()) != NULL){
            
            
            gdk_event_free(event);
        }
        
        gtk_main_iteration_do(false);
        //https://www.linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec-single/v4l2.html#camera-controls
        
        
        
        
        
        
        pollfd p;
        p.fd = state.cameraHandle;
        p.events = POLLIN;
        poll(&p, 1, 3000);
        while(ioctl(state.cameraHandle, VIDIOC_DQBUF, &state.cameraBuffer) == -1){
            printf("%u\n", errno);};
        
        js_event jevent;
        int bytesRead = -1;
        while((bytesRead = read(state.gamepadInputHandle, &jevent, sizeof(jevent))) != -1){
            if(bytesRead == sizeof(jevent)){
                if(!(jevent.type & JS_EVENT_INIT)){
                    if(jevent.type & JS_EVENT_AXIS){
                        if(jevent.number < 6){
                            state.gamepadInput.analog.values[jevent.number] = jevent.value;
                        }else{
                            if(jevent.number == 6){
                                if(jevent.value < 0){
                                    state.gamepadInput.digital.left = true;
                                    state.gamepadInput.digital.right = false;
                                }else if(jevent.value > 0){
                                    state.gamepadInput.digital.right = true;
                                    state.gamepadInput.digital.left = false;
                                }else{
                                    state.gamepadInput.digital.left = state.gamepadInput.digital.right = false;
                                }
                            }else if(jevent.number == 7){
                                if(jevent.value < 0){
                                    state.gamepadInput.digital.up = true;
                                    state.gamepadInput.digital.down = false;
                                }else if(jevent.value > 0){
                                    state.gamepadInput.digital.down = true;
                                    state.gamepadInput.digital.up = false;
                                }else{
                                    state.gamepadInput.digital.down = state.gamepadInput.digital.up = false;
                                }
                            }
                        }
                    }else if(jevent.type & JS_EVENT_BUTTON){
                        printf("n: %hhu v: %hd\n", jevent.number, jevent.value);
                    }
                }
            }
        }
        
        if(state.gamepadInput.digital.up){
            state.exposure += 25;
        }else if(state.gamepadInput.digital.down){
            state.exposure -= 25;
        }
        if(state.exposure < 0){
            state.exposure = 0;
        }
        
        //printf("%hhd %hhd %hhd %hhd\n", state.gamepadInput.digital.up, state.gamepadInput.digital.right, state.gamepadInput.digital.down, state.gamepadInput.digital.left);
        
        state.response = state.gamepadInput.analog.right.y / 32767.0f;
        if(state.response != 0) state.response *= -1;
        if(state.response < 0) state.response = 0;
        if(state.response > 1) state.response = 1;
        
        state.effect.u.rumble.weak_magnitude  = (uint16) (state.response * ((uint16) -1));
        
        if (ioctl(state.gamepadMotorHandle, EVIOCSFF, &state.effect) == -1) { 
            strcat(state.status, "EU err");
            printf("%u\n", errno);
        }
        
        v4l2_control settings;
        settings.id = V4L2_CID_EXPOSURE_ABSOLUTE;
        settings.value = state.exposure;
        
        if(ioctl(state.cameraHandle, VIDIOC_S_CTRL, &settings) == -1){
            printf("errno: %d\n", errno);
            strcat(state.status, " ERR");
        }
        
        //render phase
        gint width;
        gint height;
        gtk_window_get_size(state.window, &width, &height);
        gtk_widget_set_size_request(state.drawArea, width, height);
        state.renderTarget.info.width = width;
        state.renderTarget.info.height = height;
        
        
        for(uint32 h = 0; h < height; h++){
            uint32 pitch = h * width;
            for(uint32 w = 0; w < width; w++){
                ((uint32*)state.renderTarget.data)[pitch + w] = 0xFF000000;
            }
        }
        
        //4b == 2 pixels, Y Cb Y Cr, Y are greyscale
        for(uint32 h = 0; h < height && h < context.cameraFeed.info.height; h++){
            uint32 pitch = h * width;
            uint32 spitch= h * context.cameraFeed.info.width;
            for(uint32 w = 0; w < width && w < context.cameraFeed.info.width; w++){
                uint32 pix = context.cameraFeed.data[(spitch + w) * 2];
                ((uint32*)state.renderTarget.data)[pitch + w] = 0xFF000000 | pix | (pix << 8) | (pix << 16);
            }
        }
        
        while(ioctl(state.cameraHandle, VIDIOC_QBUF, &state.cameraBuffer) == -1){
            printf("%u\n", errno);
        };
        
        
        
        
        
        
        
        for(uint32 h = height - 32; h < height; h++){
            uint32 pitch = h * width;
            for(uint32 w = 0; w < width; w++){
                ((uint32*)state.renderTarget.data)[pitch + w] = 0xFF000000;
            }
        }
        
        
        ASSERT(printToBitmap(&state.renderTarget, 0, height - 32, state.status, &state.font, 16));
        
        gtk_widget_queue_draw((GtkWidget *)state.window);
        gdk_window_process_all_updates();
        //sleep(1);
        //printf("VALID\n");
        state.frame++;
        state.accumulator += getProcessCurrentTime() - startTime;
    }else{
        sleep(1);
        
    }
}

extern "C" void closePlatform(){
    printf("calling close\n");
    if(mem.persistent != NULL){
        munmap((void*)context.cameraFeed.data, context.cameraFeed.info.totalSize);
        ioctl(state.cameraHandle, VIDIOC_STREAMOFF, &state.cameraIOmode.type);
        if(state.window != NULL){
            printf("destroying window\n");
            //gtk_widget_destroy((GtkWidget *)state.drawArea);
            gtk_widget_destroy((GtkWidget *)state.window);
        }
        
        input_event play;
        play.type = EV_FF;
        play.code =  state.effect.id;
        play.value = 0;
        write(state.gamepadMotorHandle, (const void*) &play, sizeof(play));
        close(state.gamepadInputHandle);
        close(state.gamepadMotorHandle);
        close(state.cameraHandle);
        free(mem.persistent);
    }
    
    
}




