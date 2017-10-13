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
                    uint8 currentCamera;
                    bool wasMenu;
                    bool wasSwap;
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
                    domainState->wasMenu = false;
                    
                    if(!font){
                        printf("Font init err\n");
                    }
                    if(domainState->valid){
                        printf("Domain init all good\n");
                    }else{
                        printf("Domain init BAD");
                    }
                }
                
                
                extern "C" void iterateDomain(bool * keepRunning){
                    if(domainState->valid){
                        
                        
                        if(domainState->input.digital.menu && !domainState->wasMenu){
                            domainState->setting = !domainState->setting;
                        }
                        
                        if(domainState->input.digital.x && !domainState->wasSwap){
                            printf("swap \n");
                            Camera tmp = domainState->cameras[0];
                            domainState->cameras[0] = domainState->cameras[1];
                            domainState->cameras[1] = tmp;
                            
                        }
                        
                        domainState->wasSwap = domainState->input.digital.x;
                        
                        
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
                                    domainState->cameras[domainState->currentCamera].settings.exposure += 25;
                                }else if(domainState->currentSetting == 1){
                                    domainState->cameras[domainState->currentCamera].settings.contrast++;
                                }else if(domainState->currentSetting == 2){
                                    domainState->cameras[domainState->currentCamera].settings.brightness++;
                                }else if(domainState->currentSetting == 3){
                                    domainState->cameras[domainState->currentCamera].settings.sharpness++;
                                }else if(domainState->currentSetting == 4){
                                    domainState->cameras[domainState->currentCamera].settings.gain += 5;
                                }else if(domainState->currentSetting == 5){
                                    domainState->cameras[domainState->currentCamera].settings.backlight++;
                                }else if(domainState->currentSetting == 6){
                                    domainState->cameras[domainState->currentCamera].settings.whiteBalance+=100;
                                }
                            }else if(domainState->input.digital.down){
                                if(domainState->currentSetting == 0){
                                    domainState->cameras[domainState->currentCamera].settings.exposure -= 25;
                                }else if(domainState->currentSetting == 1){
                                    domainState->cameras[domainState->currentCamera].settings.contrast--;
                                }else if(domainState->currentSetting == 2){
                                    domainState->cameras[domainState->currentCamera].settings.brightness--;
                                }else if(domainState->currentSetting == 3){
                                    domainState->cameras[domainState->currentCamera].settings.sharpness--;
                                }else if(domainState->currentSetting == 4){
                                    domainState->cameras[domainState->currentCamera].settings.gain -= 5;
                                }else if(domainState->currentSetting == 5){
                                    domainState->cameras[domainState->currentCamera].settings.backlight--;
                                }else if(domainState->currentSetting == 6){
                                    domainState->cameras[domainState->currentCamera].settings.whiteBalance-=100;
                                }
                            }
                            
                            
                            
                            
                            
                            
                        }else{
                            if(domainState->input.digital.up){
                                if(domainState->currentCamera != 0){
                                    domainState->currentCamera--;
                                }
                            }else if(domainState->input.digital.down){
                                if(domainState->currentCamera != CameraPositionCount-1){
                                    domainState->currentCamera++;
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
                        
                        printToBitmap(&domainState->renderTarget, 0, 0, "LEFT EYE", &domainState->font, 16);
                        printToBitmap(&domainState->renderTarget, 321, 0, "RIGHT EYE", &domainState->font, 16);
                        
                        for(uint8 ci = 0; ci < CameraPositionCount; ci++){
                            //4b == 2 pixels, Y Cb Y Cr, Y are greyscale
                            for(uint32 h = 0; h < height && h*2 < domainState->cameras[ci].feed.info.height; h++){
                                uint32 pitch = (h + 16) * width;
                                uint32 spitch= h*2 * domainState->cameras[ci].feed.info.width;
                                for(uint32 w = 0; w < width && w*2 < domainState->cameras[ci].feed.info.width; w++){
                                    uint32 pix = domainState->cameras[ci].feed.data[(spitch + w*2) * 2];
                                    ((uint32*)domainState->renderTarget.data)[pitch + w + 321*ci] = 0xFF000000 | pix | (pix << 8) | (pix << 16);
                                }
                            }
                            
                        }
                        
                        
                        uint32 offset = 0;
                        char buf[50];
                        //fps
                        sprintf(buf, "FPS:%u", domainState->lastFps);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 48, buf, &domainState->font, 16));
                        offset += strlen(buf);
                        
                        //response
                        sprintf(buf, "|R:%.2f", domainState->feedback);
                        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 48, buf, &domainState->font, 16));
                        offset += strlen(buf);
                        
                        
                        Color w;
                        w.full = 0xFFFFFFFF;
                        Color r;
                        r.full = 0xFF0000FF;
                        
                        for(uint8 ci = 0; ci < CameraPositionCount; ci++){
                            offset = 0;
                            
                            if(ci == CameraPosition_Left){
                                ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32 + ci*16, "LEFT", &domainState->font, 16, w));
                                offset += strlen("LEFT: ");
                                
                            }else if(ci == CameraPosition_Right){
                                ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32 + ci*16, "RIGHT", &domainState->font, 16, w));
                                offset += strlen("RIGHT:");
                            }
                            
                            //exposure
                            sprintf(buf, "E:%d", domainState->cameras[ci].settings.exposure);
                            ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32 + ci*16, buf,&domainState->font, 16, (domainState->setting && domainState->currentSetting == 0 && domainState->currentCamera == ci) ? r : w));
                            offset += strlen(buf);
                            
                            //contrast
                            sprintf(buf, "|C:%d", domainState->cameras[ci].settings.contrast);
                            ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32 + ci*16, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 1 && domainState->currentCamera == ci) ? r : w));
                            offset += strlen(buf);
                            
                            //brightness
                            sprintf(buf, "|B:%d", domainState->cameras[ci].settings.brightness);
                            ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32 + ci*16, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 2 && domainState->currentCamera == ci) ? r : w));
                            offset += strlen(buf);
                            
                            //sharpness
                            sprintf(buf, "|S:%d", domainState->cameras[ci].settings.sharpness);
                            ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32 + ci*16, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 3 && domainState->currentCamera == ci) ? r : w));
                            offset += strlen(buf);
                            
                            //gain
                            sprintf(buf, "|G+:%d", domainState->cameras[ci].settings.gain);
                            ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32 + ci*16, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 4 && domainState->currentCamera == ci) ? r : w));
                            offset += strlen(buf);
                            //backlight
                            sprintf(buf, "|BL:%d", domainState->cameras[ci].settings.backlight);
                            ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32 + ci*16, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 5 && domainState->currentCamera == ci) ? r : w));
                            offset += strlen(buf);
                            
                            //whiteBalance
                            sprintf(buf, "|WB:%d", domainState->cameras[ci].settings.whiteBalance);
                            ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 32 + ci*16, buf, &domainState->font, 16, (domainState->setting && domainState->currentSetting == 6 && domainState->currentCamera == ci) ? r : w));
                            offset += strlen(buf);
                            
                            
                        }
                    }
                }
                
                extern "C" void closeDomain(){
                    
                }
                
                
