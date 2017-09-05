#include "stdio.h"
#include "linux_types.h"
#include <stdlib.h>
#include <malloc.h>
#include "string.h"
#include <gtk/gtk.h>


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
};

Context context;

struct State{
    BitmapFont font;
    GtkWindow * window;
    GtkWidget * drawArea;
    GdkPixbuf * pixbuf;
    GByteArray bytearray;
    Image renderTarget;
    
};

State state;

gboolean
draw(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    gdk_draw_pixbuf(widget->window, NULL, state.pixbuf, 0, 0, 0, 0, widget->allocation.width, widget->allocation.height, GDK_RGB_DITHER_NONE, 0, 0);
    
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
        if(state.window != NULL && result){
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
            state.renderTarget.info.samplesPerPixel = 3;
            state.renderTarget.data = &PPUSHA(byte, w*h*4);
            
            state.bytearray.len = w*h*4;
            state.bytearray.data = (guint8 *) state.renderTarget.data;
            state.pixbuf = gdk_pixbuf_new_from_bytes((GBytes*) &state.bytearray ,GDK_COLORSPACE_RGB, true, 8, w, h, w*4);
            
            
            context.validInit = true;
        }
    }
    
}

extern "C" void iterate(bool * keepRunning){
    if(context.validInit){
        GdkEvent * event;
        
        while((event = gtk_get_current_event()) != NULL){
            
            printf("event\n");
            gdk_event_free(event);
        }
        
        gtk_main_iteration_do(false);
        gint width;
        gint height;
        gtk_window_get_size(state.window, &width, &height);
        gtk_widget_set_size_request(state.drawArea, width, height);
        
        uint8 i = 0;
        for(uint32 h = 0; h < height; h++){
            uint32 pitch = h * width;
            for(uint32 w = 0; w < width; w++){
                ((uint32*)state.renderTarget.data)[pitch + w] = 0xFF0000FF;
            }
        }
        
        gtk_widget_queue_draw((GtkWidget *)state.window);
        //sleep(1);
        //printf("VALID\n");
    }else{
        //printf("invalid init\n");
    }
}

extern "C" void closePlatform(){
    if(mem.persistent != NULL){
        if(state.window != NULL){
            
            gtk_widget_destroy((GtkWidget *)state.drawArea);
            gtk_widget_destroy((GtkWidget *)state.window);
        }
        free(mem.persistent);
    }
}




