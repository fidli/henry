                #include <stdlib.h>
#include "string.h"
#include "stdio.h"
                
                bool * gassert;
                char * gassertMessage;
                
#define ASSERT(expression) if(!(expression)) { sprintf(gassertMessage, "ASSERT FAILED on line %d in file %s\n", __LINE__, __FILE__); *gassert = true; printf(gassertMessage);}
                
                
#define PERSISTENT_MEM 0
#define TEMP_MEM 0
#define STACK_MEM 0
                
#include "common.h"
#include "util_mem.h"
#include "util_filesystem.h"
#include "util_font.cpp"
#include "util_image.cpp"
#include "util_conv.cpp"
                
#include "domaincode.h"
                
                struct DomainState : DomainInterface{
                    BitmapFont font;
                    bool valid;
                    bool setting;
                    uint8 currentSetting;
                    bool wasMenu;
                };
                
                
                DomainState * domainState;
                extern DomainInterface * domainInterface;
                
                
                
                extern "C" void initDomain(bool * assert, char * assertMessage, void * memstart){
                    gassertMessage = assertMessage;
                    gassert = assert;
                    
                    domainState = (DomainState *) memstart;
                    domainInterface = (DomainInterface *) memstart;
                    
                    ASSERT((byte*)memstart + sizeof(DomainState) < (byte*)mem.stack);
                    
                    FileContents fontContents;
                    Image fontImage;
                    bool font = readFile("data/font.bmp", &fontContents) && decodeBMP(&fontContents, &fontImage) && flipY(&fontImage) && initBitmapFont(&domainState->font, &fontImage, fontImage.info.width / 16);
                    domainState->valid = font;
                    domainState->setting = false;
                    domainState->currentSetting = 0;
                    if(!font){
                        printf("Font init err\n");
                    }
                }
                
                
                extern "C" void iterateDomain(bool * keepRunning){
                    if(domainState->valid){
                        
                        
                        if(domainState->input.digital.menu && !domainState->wasMenu){
                            domainState->setting = !domainState->setting;
                        }
                        
                        
                        
                        //input
                        if(domainState->setting){
                            
                            if(domainState->input.digital.left){
                                if(domainState->currentSetting != 0){
                                    domainState->currentSetting--;
                                }
                            }else if(domainState->input.digital.right){
                                if(domainState->currentSetting != 6){
                                    domainState->currentSetting++;
                                }
                            }
                            
                            
                            if(domainState->input.digital.up){
                                if(domainState->currentSetting == 0){
                                    domainState->cameras[0].settings.exposure += 25;
                                }else if(domainState->currentSetting == 1){
                                    domainState->cameras[0].settings.contrast++;
                                }else if(domainState->currentSetting == 2){
                                    domainState->cameras[0].settings.brightness++;
                                }else if(domainState->currentSetting == 3){
                                    domainState->cameras[0].settings.sharpness++;
                                }else if(domainState->currentSetting == 4){
                                    domainState->cameras[0].settings.gain += 5;
                                }else if(domainState->currentSetting == 5){
                                    domainState->cameras[0].settings.backlight++;
                                }else if(domainState->currentSetting == 6){
                                    domainState->cameras[0].settings.whiteBalance+=100;
                                }
                            }else if(domainState->input.digital.down){
                                if(domainState->currentSetting == 0){
                                    domainState->cameras[0].settings.exposure -= 25;
                                }else if(domainState->currentSetting == 1){
                                    domainState->cameras[0].settings.contrast--;
                                }else if(domainState->currentSetting == 2){
                                    domainState->cameras[0].settings.brightness--;
                                }else if(domainState->currentSetting == 3){
                                    domainState->cameras[0].settings.sharpness--;
                                }else if(domainState->currentSetting == 4){
                                    domainState->cameras[0].settings.gain -= 5;
                                }else if(domainState->currentSetting == 5){
                                    domainState->cameras[0].settings.backlight--;
                                }else if(domainState->currentSetting == 6){
                                    domainState->cameras[0].settings.whiteBalance-=100;
                                }
                            }
                            
                            
                            
                            
                            
                            
                        }
                        
                        domainState->feedback = domainState->input.analog.right.y / 32767.0f;
                        if(domainState->feedback != 0) domainState->feedback *= -1;
                        if(domainState->feedback < 0) domainState->feedback = 0;
                        if(domainState->feedback > 1) domainState->feedback = 1;
                        
                        domainState->wasMenu = domainState->input.digital.menu;
                        
                        //render
                        
                        uint32 height = domainState->renderTarget.info.height;
                        uint32 width = domainState->renderTarget.info.width;
                        
                        for(uint32 h = 0; h < height; h++){
                            uint32 pitch = h * width;
                            for(uint32 w = 0; w < width; w++){
                                ((uint32*)domainState->renderTarget.data)[pitch + w] = 0xFF000000;
                            }
                        }
                        
                        //4b == 2 pixels, Y Cb Y Cr, Y are greyscale
                        for(uint32 h = 0; h < height && h < domainState->cameras[0].feed.info.height; h++){
                            uint32 pitch = h * width;
                            uint32 spitch= h * domainState->cameras[0].feed.info.width;
                            for(uint32 w = 0; w < width && w < domainState->cameras[0].feed.info.width; w++){
                                uint32 pix = domainState->cameras[0].feed.data[(spitch + w) * 2];
                                ((uint32*)domainState->renderTarget.data)[pitch + w] = 0xFF000000 | pix | (pix << 8) | (pix << 16);
                            }
                        }
                        
                        for(uint32 h = height - 32; h < height; h++){
                            uint32 pitch = h * width;
                            for(uint32 w = 0; w < width; w++){
                                ((uint32*)domainState->renderTarget.data)[pitch + w] = 0xFF000000;
                            }
                        }
                        
                        uint32 offset = 0;
                        char buf[50];
                        //fps
                        sprintf(buf, "FPS:%u", domainState->lastFps);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32, buf, &domainState->font, 16));
                        offset += strlen(buf);
                        
                        //response
                        sprintf(buf, "|R:%.2f", domainState->feedback);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32, buf, &domainState->font, 16));
                        offset += strlen(buf);
                        
                        
                        Color w;
                        w.full = 0xFFFFFFFF;
                        Color r;
                        r.full = 0xFF0000FF;
                        
                        //exposure
                        sprintf(buf, "|E:%d", domainState->cameras[0].settings.exposure);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32, buf,&domainState->font, 16, (domainState->setting && domainState->currentSetting == 0) ? r : w));
                        offset += strlen(buf);
                        
                        //contrast
                        sprintf(buf, "|C:%d", domainState->cameras[0].settings.contrast);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 1) ? r : w));
                        offset += strlen(buf);
                        
                        //brightness
                        sprintf(buf, "|B:%d", domainState->cameras[0].settings.brightness);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 2) ? r : w));
                        offset += strlen(buf);
                        
                        //sharpness
                        sprintf(buf, "|S:%d", domainState->cameras[0].settings.sharpness);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 3) ? r : w));
                        offset += strlen(buf);
                        
                        //gain
                        sprintf(buf, "|G+:%d", domainState->cameras[0].settings.gain);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 4) ? r : w));
                        offset += strlen(buf);
                        //backlight
                        sprintf(buf, "|BL:%d", domainState->cameras[0].settings.backlight);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 5) ? r : w));
                        offset += strlen(buf);
                        
                        offset = 0;
                        //whiteBalance
                        sprintf(buf, "|WB:%d", domainState->cameras[0].settings.whiteBalance);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 16, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 6) ? r : w));
                        offset += strlen(buf);
                        
                        
                    }
                }
                
                extern "C" void closeDomain(){
                    
                }
                
                
