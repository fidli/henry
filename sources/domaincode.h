                                                                                                    struct Camera{
                                                                                                    Image feed;
                                                                                                    struct {
                                                                                                        int32 contrast;
                                                                                                        int32 brightness;
                                                                                                        int32 exposure;
                                                                                                        int32 sharpness;
                                                                                                        int32 gain;
                                                                                                        int32 backlight;
                                                                                                        int32 whiteBalance;
                                                                                                    } settings;
                                                                                                };
                                                                                                
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
                                                                                                        bool menu;
                                                                                                        bool x;
                                                                                                    } digital;
                                                                                                };
                                                                                                
                                                                                                
                                                                                                enum CameraPosition{
                                                                                                    CameraPosition_Left,
                                                                                                    CameraPosition_Right,
                                                                                                    CameraPositionCount
                                                                                                };
                                                                                                
                                                                                                struct DomainInterface{
                                                                                                    Camera cameras[CameraPositionCount];
                                                                                                    GamepadInput input;
                                                                                                    float32 feedback;
                                                                                                    uint32 lastFps;
                                                                                                    Image renderTarget;
                                                                                                };
                