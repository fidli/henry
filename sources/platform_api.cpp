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


bool * gassert;
char * gassertMessage;

#define ASSERT(expression) if(!(expression)) { sprintf(gassertMessage, "ASSERT FAILED on line %d in file %s\n", __LINE__, __FILE__); *gassert = true; printf(gassertMessage);}


#include "common.h"

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



struct State{
    BitmapFont font;
    GtkWindow * window;
    GtkWidget * drawArea;
    GdkPixbuf * pixbuf;
    GByteArray bytearray;
    Image renderTarget;
    int cameraHandle;
    v4l2_capability cameraCapabilities;
    v4l2_format cameraFormat;
    v4l2_requestbuffers cameraIOmode;
    v4l2_buffer cameraBuffer;
    char status[256];
    uint32 frame;
};

State state;

gboolean
draw(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    gdk_draw_pixbuf(widget->window, NULL, state.pixbuf, 0, 0, 0, 0, state.renderTarget.info.width, state.renderTarget.info.height, GDK_RGB_DITHER_NONE, 0, 0);
    
    printf("draw w: %u h: %u \n", state.renderTarget.info.width, state.renderTarget.info.height);
    
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
        bool result = readFile("data/font.bmp", &fontContents) && decodeBMP(&fontContents, &fontImage) && flipY(&fontImage) && initBitmapFont(&state.font, &fontImage, fontImage.info.width / 16);
        gtk_init(NULL, NULL);
        state.window = (GtkWindow *)gtk_window_new(GTK_WINDOW_TOPLEVEL);
        state.cameraHandle = open("/dev/video0", O_RDWR);
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
        if(state.window != NULL && result && state.cameraHandle != -1 && ioctl(state.cameraHandle, VIDIOC_QUERYCAP, &state.cameraCapabilities) != -1 && state.cameraCapabilities.device_caps & V4L2_CAP_VIDEO_CAPTURE && state.cameraCapabilities.device_caps & V4L2_CAP_STREAMING  && ioctl(state.cameraHandle, VIDIOC_G_FMT, &state.cameraFormat) != -1 && state.cameraFormat.fmt.pix.pixelformat & V4L2_PIX_FMT_YUYV && 
           ioctl(state.cameraHandle, VIDIOC_REQBUFS, &state.cameraIOmode) != -1 && state.cameraIOmode.count == 1 && ioctl (state.cameraHandle, VIDIOC_QUERYBUF, &state.cameraBuffer) != -1){
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
                    state.renderTarget.data = &PPUSHA(byte, w*h*4);
                    
                    state.bytearray.len = w*h*4;
                    state.bytearray.data = (guint8 *) state.renderTarget.data;
                    state.pixbuf = gdk_pixbuf_new_from_bytes((GBytes*) &state.bytearray ,GDK_COLORSPACE_RGB, true, 8, w, h, w*4);
                    
                    
                    
                    state.frame = 0;
                    context.validInit = true;
                }
            }
        }
    }
    
}

extern "C" void iterate(bool * keepRunning){
    if(context.validInit){
        
        printf("Frame: %d\n", state.frame);
        sprintf(state.status, "Frame: %d", state.frame);
        GdkEvent * event;
        
        while((event = gtk_get_current_event()) != NULL){
            
            
            gdk_event_free(event);
        }
        
        gtk_main_iteration_do(false);
        
        pollfd p;
        p.fd = state.cameraHandle;
        p.events = POLLIN;
        /*poll(&p, 1, 3000);
        while(ioctl(state.cameraHandle, VIDIOC_DQBUF, &state.cameraBuffer) == -1){
            printf("%u\n", errno);};
            */
        
        
        
        //render phase
        gint width;
        gint height;
        gtk_window_get_size(state.window, &width, &height);
        gtk_widget_set_size_request(state.drawArea, width, height);
        state.renderTarget.info.width = width;
        state.renderTarget.info.height = height;
        
        
        
        for(uint32 h = 0; h < height && h < context.cameraFeed.info.height; h++){
            uint32 pitch = h * width;
            for(uint32 w = 0; w < width && w < context.cameraFeed.info.width; w++){
                uint32 pix = context.cameraFeed.data[(pitch + w) * 2];
                ((uint32*)state.renderTarget.data)[pitch + w] = 0xFF000000 | pix | (pix << 8) | (pix << 16);
            }
        }
        
        /*while(ioctl(state.cameraHandle, VIDIOC_QBUF, &state.cameraBuffer) == -1){
            printf("%u\n", errno);
        };*/
        
        for(uint32 h = context.cameraFeed.info.height; h < height; h++){
            uint32 pitch = h * width;
            for(uint32 w = w < context.cameraFeed.info.width; w < width; w++){
                ((uint32*)state.renderTarget.data)[pitch + w] = 0xFF0000FF;
            }
        }
        
        //4b == 2 pixels, Y Cb Y Cr, Y are greyscale
        
        //state.cameraFormat.fmt.pix.sizeimage;
        
        //sprintf(state.status, "%u %u", state.cameraFormat.fmt.pix.width, state.cameraFormat.fmt.pix.height); 
        
        
        
        
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
        
    }else{
        sleep(1);
        printf("invalid init\n");
    }
}

extern "C" void closePlatform(){
    if(mem.persistent != NULL){
        munmap((void*)context.cameraFeed.data, context.cameraFeed.info.totalSize);
        if(context.validInit){
            if(state.window != NULL){
                
                gtk_widget_destroy((GtkWidget *)state.drawArea);
                gtk_widget_destroy((GtkWidget *)state.window);
            }
            
            
            
            ioctl(state.cameraHandle, VIDIOC_STREAMOFF, &state.cameraIOmode.type);
            free(mem.persistent);
            close(state.cameraHandle);
        }
    }
}




