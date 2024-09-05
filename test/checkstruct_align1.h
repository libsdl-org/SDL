#pragma pack(push,1)

typedef struct SDL_CommonEvent_pack1 {
    Uint32 type;        
    Uint32 reserved;
    Uint64 timestamp;   
} SDL_CommonEvent_pack1;

typedef struct SDL_DisplayEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_DisplayID displayID;
    Sint32 data1;       
    Sint32 data2;       
} SDL_DisplayEvent_pack1;

typedef struct SDL_WindowEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID; 
    Sint32 data1;       
    Sint32 data2;       
} SDL_WindowEvent_pack1;

typedef struct SDL_KeyboardDeviceEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_KeyboardID which;   
} SDL_KeyboardDeviceEvent_pack1;

typedef struct SDL_KeyboardEvent_pack1 {
    SDL_EventType type;     
    Uint32 reserved;
    Uint64 timestamp;       
    SDL_WindowID windowID;  
    SDL_KeyboardID which;   
    SDL_Scancode scancode;  
    SDL_Keycode key;        
    SDL_Keymod mod;         
    Uint16 raw;             
    Uint8 state;            
    Uint8 repeat;           
} SDL_KeyboardEvent_pack1;

typedef struct SDL_TextEditingEvent_pack1 {
    SDL_EventType type;         
    Uint32 reserved;
    Uint64 timestamp;           
    SDL_WindowID windowID;      
    const char *text;           
    Sint32 start;               
    Sint32 length;              
} SDL_TextEditingEvent_pack1;

typedef struct SDL_TextEditingCandidatesEvent_pack1 {
    SDL_EventType type;         
    Uint32 reserved;
    Uint64 timestamp;           
    SDL_WindowID windowID;      
    const char * const *candidates;    
    Sint32 num_candidates;      
    Sint32 selected_candidate;  
    SDL_bool horizontal;          
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_TextEditingCandidatesEvent_pack1;

typedef struct SDL_TextInputEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID; 
    const char *text;   
} SDL_TextInputEvent_pack1;

typedef struct SDL_MouseDeviceEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_MouseID which;  
} SDL_MouseDeviceEvent_pack1;

typedef struct SDL_MouseMotionEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID; 
    SDL_MouseID which;  
    SDL_MouseButtonFlags state;       
    float x;            
    float y;            
    float xrel;         
    float yrel;         
} SDL_MouseMotionEvent_pack1;

typedef struct SDL_MouseButtonEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID; 
    SDL_MouseID which;  
    Uint8 button;       
    Uint8 state;        
    Uint8 clicks;       
    Uint8 padding;
    float x;            
    float y;            
} SDL_MouseButtonEvent_pack1;

typedef struct SDL_MouseWheelEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID; 
    SDL_MouseID which;  
    float x;            
    float y;            
    SDL_MouseWheelDirection direction; 
    float mouse_x;      
    float mouse_y;      
} SDL_MouseWheelEvent_pack1;

typedef struct SDL_JoyAxisEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which; 
    Uint8 axis;         
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;       
    Uint16 padding4;
} SDL_JoyAxisEvent_pack1;

typedef struct SDL_JoyBallEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which; 
    Uint8 ball;         
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 xrel;        
    Sint16 yrel;        
} SDL_JoyBallEvent_pack1;

typedef struct SDL_JoyHatEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which; 
    Uint8 hat;          
    Uint8 value;        /**< The hat position value.
                         *   \sa SDL_HAT_LEFTUP SDL_HAT_UP SDL_HAT_RIGHTUP
                         *   \sa SDL_HAT_LEFT SDL_HAT_CENTERED SDL_HAT_RIGHT
                         *   \sa SDL_HAT_LEFTDOWN SDL_HAT_DOWN SDL_HAT_RIGHTDOWN
                         *
                         *   Note that zero means the POV is centered.
                         */
    Uint8 padding1;
    Uint8 padding2;
} SDL_JoyHatEvent_pack1;

typedef struct SDL_JoyButtonEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which; 
    Uint8 button;       
    Uint8 state;        
    Uint8 padding1;
    Uint8 padding2;
} SDL_JoyButtonEvent_pack1;

typedef struct SDL_JoyDeviceEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which;       
} SDL_JoyDeviceEvent_pack1;

typedef struct SDL_JoyBatteryEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which; 
    SDL_PowerState state; 
    int percent;          
} SDL_JoyBatteryEvent_pack1;

typedef struct SDL_GamepadAxisEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which; 
    Uint8 axis;         
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;       
    Uint16 padding4;
} SDL_GamepadAxisEvent_pack1;

typedef struct SDL_GamepadButtonEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which; 
    Uint8 button;       
    Uint8 state;        
    Uint8 padding1;
    Uint8 padding2;
} SDL_GamepadButtonEvent_pack1;

typedef struct SDL_GamepadDeviceEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which;       
} SDL_GamepadDeviceEvent_pack1;

typedef struct SDL_GamepadTouchpadEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which; 
    Sint32 touchpad;    
    Sint32 finger;      
    float x;            
    float y;            
    float pressure;     
} SDL_GamepadTouchpadEvent_pack1;

typedef struct SDL_GamepadSensorEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_JoystickID which; 
    Sint32 sensor;      
    float data[3];      
    Uint64 sensor_timestamp; 
} SDL_GamepadSensorEvent_pack1;

typedef struct SDL_AudioDeviceEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_AudioDeviceID which;       
    Uint8 recording;    
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_AudioDeviceEvent_pack1;

typedef struct SDL_CameraDeviceEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_CameraID which;       
} SDL_CameraDeviceEvent_pack1;

typedef struct SDL_TouchFingerEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_TouchID touchID; 
    SDL_FingerID fingerID;
    float x;            
    float y;            
    float dx;           
    float dy;           
    float pressure;     
    SDL_WindowID windowID; 
} SDL_TouchFingerEvent_pack1;

typedef struct SDL_PenProximityEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID; 
    SDL_PenID which;        
} SDL_PenProximityEvent_pack1;

typedef struct SDL_PenMotionEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID; 
    SDL_PenID which;        
    SDL_PenInputFlags pen_state;   
    float x;                
    float y;                
} SDL_PenMotionEvent_pack1;

typedef struct SDL_PenTouchEvent_pack1 {
    SDL_EventType type;     
    Uint32 reserved;
    Uint64 timestamp;       
    SDL_WindowID windowID;  
    SDL_PenID which;        
    SDL_PenInputFlags pen_state;   
    float x;                
    float y;                
    Uint8 eraser;           
    Uint8 state;            
} SDL_PenTouchEvent_pack1;

typedef struct SDL_PenButtonEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID; 
    SDL_PenID which;        
    SDL_PenInputFlags pen_state;   
    float x;                
    float y;                
    Uint8 button;       
    Uint8 state;        
} SDL_PenButtonEvent_pack1;

typedef struct SDL_PenAxisEvent_pack1 {
    SDL_EventType type;     
    Uint32 reserved;
    Uint64 timestamp;       
    SDL_WindowID windowID;  
    SDL_PenID which;        
    SDL_PenInputFlags pen_state;   
    float x;                
    float y;                
    SDL_PenAxis axis;       
    float value;            
} SDL_PenAxisEvent_pack1;

typedef struct SDL_DropEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID;    
    float x;            
    float y;            
    const char *source; 
    const char *data;   
} SDL_DropEvent_pack1;

typedef struct SDL_ClipboardEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
} SDL_ClipboardEvent_pack1;

typedef struct SDL_SensorEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_SensorID which; 
    float data[6];      
    Uint64 sensor_timestamp; 
} SDL_SensorEvent_pack1;

typedef struct SDL_QuitEvent_pack1 {
    SDL_EventType type; 
    Uint32 reserved;
    Uint64 timestamp;   
} SDL_QuitEvent_pack1;

typedef struct SDL_UserEvent_pack1 {
    Uint32 type;        
    Uint32 reserved;
    Uint64 timestamp;   
    SDL_WindowID windowID; 
    Sint32 code;        
    void *data1;        
    void *data2;        
} SDL_UserEvent_pack1;

typedef union SDL_Event_pack1 {
    Uint32 type;                            
    SDL_CommonEvent common;                 
    SDL_DisplayEvent display;               
    SDL_WindowEvent window;                 
    SDL_KeyboardDeviceEvent kdevice;        
    SDL_KeyboardEvent key;                  
    SDL_TextEditingEvent edit;              
    SDL_TextEditingCandidatesEvent edit_candidates; 
    SDL_TextInputEvent text;                
    SDL_MouseDeviceEvent mdevice;           
    SDL_MouseMotionEvent motion;            
    SDL_MouseButtonEvent button;            
    SDL_MouseWheelEvent wheel;              
    SDL_JoyDeviceEvent jdevice;             
    SDL_JoyAxisEvent jaxis;                 
    SDL_JoyBallEvent jball;                 
    SDL_JoyHatEvent jhat;                   
    SDL_JoyButtonEvent jbutton;             
    SDL_JoyBatteryEvent jbattery;           
    SDL_GamepadDeviceEvent gdevice;         
    SDL_GamepadAxisEvent gaxis;             
    SDL_GamepadButtonEvent gbutton;         
    SDL_GamepadTouchpadEvent gtouchpad;     
    SDL_GamepadSensorEvent gsensor;         
    SDL_AudioDeviceEvent adevice;           
    SDL_CameraDeviceEvent cdevice;          
    SDL_SensorEvent sensor;                 
    SDL_QuitEvent quit;                     
    SDL_UserEvent user;                     
    SDL_TouchFingerEvent tfinger;           
    SDL_PenProximityEvent pproximity;       
    SDL_PenTouchEvent ptouch;               
    SDL_PenMotionEvent pmotion;             
    SDL_PenButtonEvent pbutton;             
    SDL_PenAxisEvent paxis;                 
    SDL_DropEvent drop;                     
    SDL_ClipboardEvent clipboard;           

    /* This is necessary for ABI compatibility between Visual C++ and GCC.
       Visual C++ will respect the push pack pragma and use 52 bytes (size of
       SDL_TextEditingEvent, the largest structure for 32-bit and 64-bit
       architectures) for this union, and GCC will use the alignment of the
       largest datatype within the union, which is 8 bytes on 64-bit
       architectures.

       So... we'll add padding to force the size to be the same for both.

       On architectures where pointers are 16 bytes, this needs rounding up to
       the next multiple of 16, 64, and on architectures where pointers are
       even larger the size of SDL_UserEvent will dominate as being 3 pointers.
    */
    Uint8 padding[128];
} SDL_Event_pack1;

typedef struct SDL_CameraSpec_pack1 {
    SDL_PixelFormat format;     
    SDL_Colorspace colorspace;  
    int width;                  
    int height;                 
    int framerate_numerator;     
    int framerate_denominator;   
} SDL_CameraSpec_pack1;

typedef struct SDL_HapticDirection_pack1 {
    Uint8 type;         
    Sint32 dir[3];      
} SDL_HapticDirection_pack1;

typedef struct SDL_HapticConstant_pack1 {
    
    Uint16 type;            
    SDL_HapticDirection direction;  

    
    Uint32 length;          
    Uint16 delay;           

    
    Uint16 button;          
    Uint16 interval;        

    
    Sint16 level;           

    
    Uint16 attack_length;   
    Uint16 attack_level;    
    Uint16 fade_length;     
    Uint16 fade_level;      
} SDL_HapticConstant_pack1;

typedef struct SDL_HapticPeriodic_pack1 {
    
    Uint16 type;        /**< SDL_HAPTIC_SINE, SDL_HAPTIC_SQUARE
                             SDL_HAPTIC_TRIANGLE, SDL_HAPTIC_SAWTOOTHUP or
                             SDL_HAPTIC_SAWTOOTHDOWN */
    SDL_HapticDirection direction;  

    
    Uint32 length;      
    Uint16 delay;       

    
    Uint16 button;      
    Uint16 interval;    

    
    Uint16 period;      
    Sint16 magnitude;   
    Sint16 offset;      
    Uint16 phase;       

    
    Uint16 attack_length;   
    Uint16 attack_level;    
    Uint16 fade_length; 
    Uint16 fade_level;  
} SDL_HapticPeriodic_pack1;

typedef struct SDL_HapticCondition_pack1 {
    
    Uint16 type;            /**< SDL_HAPTIC_SPRING, SDL_HAPTIC_DAMPER,
                                 SDL_HAPTIC_INERTIA or SDL_HAPTIC_FRICTION */
    SDL_HapticDirection direction;  

    
    Uint32 length;          
    Uint16 delay;           

    
    Uint16 button;          
    Uint16 interval;        

    
    Uint16 right_sat[3];    
    Uint16 left_sat[3];     
    Sint16 right_coeff[3];  
    Sint16 left_coeff[3];   
    Uint16 deadband[3];     
    Sint16 center[3];       
} SDL_HapticCondition_pack1;

typedef struct SDL_HapticRamp_pack1 {
    
    Uint16 type;            
    SDL_HapticDirection direction;  

    
    Uint32 length;          
    Uint16 delay;           

    
    Uint16 button;          
    Uint16 interval;        

    
    Sint16 start;           
    Sint16 end;             

    
    Uint16 attack_length;   
    Uint16 attack_level;    
    Uint16 fade_length;     
    Uint16 fade_level;      
} SDL_HapticRamp_pack1;

typedef struct SDL_HapticLeftRight_pack1 {
    
    Uint16 type;            

    
    Uint32 length;          

    
    Uint16 large_magnitude; 
    Uint16 small_magnitude; 
} SDL_HapticLeftRight_pack1;

typedef struct SDL_HapticCustom_pack1 {
    
    Uint16 type;            
    SDL_HapticDirection direction;  

    
    Uint32 length;          
    Uint16 delay;           

    
    Uint16 button;          
    Uint16 interval;        

    
    Uint8 channels;         
    Uint16 period;          
    Uint16 samples;         
    Uint16 *data;           

    
    Uint16 attack_length;   
    Uint16 attack_level;    
    Uint16 fade_length;     
    Uint16 fade_level;      
} SDL_HapticCustom_pack1;

typedef union SDL_HapticEffect_pack1 {
    
    Uint16 type;                    
    SDL_HapticConstant constant;    
    SDL_HapticPeriodic periodic;    
    SDL_HapticCondition condition;  
    SDL_HapticRamp ramp;            
    SDL_HapticLeftRight leftright;  
    SDL_HapticCustom custom;        
} SDL_HapticEffect_pack1;

typedef struct SDL_StorageInterface_pack1 {
    
    SDL_bool (SDLCALL *close)(void *userdata);

    
    SDL_bool (SDLCALL *ready)(void *userdata);

    
    SDL_bool (SDLCALL *enumerate)(void *userdata, const char *path, SDL_EnumerateDirectoryCallback callback, void *callback_userdata);

    
    SDL_bool (SDLCALL *info)(void *userdata, const char *path, SDL_PathInfo *info);

    
    SDL_bool (SDLCALL *read_file)(void *userdata, const char *path, void *destination, Uint64 length);

    
    SDL_bool (SDLCALL *write_file)(void *userdata, const char *path, const void *source, Uint64 length);

    
    SDL_bool (SDLCALL *mkdir)(void *userdata, const char *path);

    
    SDL_bool (SDLCALL *remove)(void *userdata, const char *path);

    
    SDL_bool (SDLCALL *rename)(void *userdata, const char *oldpath, const char *newpath);

    
    SDL_bool (SDLCALL *copy)(void *userdata, const char *oldpath, const char *newpath);

    
    Uint64 (SDLCALL *space_remaining)(void *userdata);
} SDL_StorageInterface_pack1;

typedef struct SDL_DateTime_pack1 {
    int year;                  
    int month;                 
    int day;                   
    int hour;                  
    int minute;                
    int second;                
    int nanosecond;            
    int day_of_week;           
    int utc_offset;            
} SDL_DateTime_pack1;

typedef struct SDL_Finger_pack1 {
    SDL_FingerID id;  
    float x;  
    float y;  
    float pressure; 
} SDL_Finger_pack1;

typedef struct SDL_GamepadBinding_pack1 {
    SDL_GamepadBindingType input_type;
    union
    {
        int button;

        struct
        {
            int axis;
            int axis_min;
            int axis_max;
        } axis;

        struct
        {
            int hat;
            int hat_mask;
        } hat;

    } input;

    SDL_GamepadBindingType output_type;
    union
    {
        SDL_GamepadButton button;

        struct
        {
            SDL_GamepadAxis axis;
            int axis_min;
            int axis_max;
        } axis;

    } output;
} SDL_GamepadBinding_pack1;

typedef struct SDL_Locale_pack1 {
    const char *language;  
    const char *country;  
} SDL_Locale_pack1;

typedef struct SDL_AudioSpec_pack1 {
    SDL_AudioFormat format;     
    int channels;               
    int freq;                   
} SDL_AudioSpec_pack1;

typedef struct SDL_DialogFileFilter_pack1 {
    const char *name;
    const char *pattern;
} SDL_DialogFileFilter_pack1;

typedef struct SDL_IOStreamInterface_pack1 {
    /**
     *  Return the number of bytes in this SDL_IOStream
     *
     *  \return the total size of the data stream, or -1 on error.
     */
    Sint64 (SDLCALL *size)(void *userdata);

    /**
     *  Seek to `offset` relative to `whence`, one of stdio's whence values:
     *  SDL_IO_SEEK_SET, SDL_IO_SEEK_CUR, SDL_IO_SEEK_END
     *
     *  \return the final offset in the data stream, or -1 on error.
     */
    Sint64 (SDLCALL *seek)(void *userdata, Sint64 offset, SDL_IOWhence whence);

    /**
     *  Read up to `size` bytes from the data stream to the area pointed
     *  at by `ptr`.
     *
     *  On an incomplete read, you should set `*status` to a value from the
     *  SDL_IOStatus enum. You do not have to explicitly set this on
     *  a complete, successful read.
     *
     *  \return the number of bytes read
     */
    size_t (SDLCALL *read)(void *userdata, void *ptr, size_t size, SDL_IOStatus *status);

    /**
     *  Write exactly `size` bytes from the area pointed at by `ptr`
     *  to data stream.
     *
     *  On an incomplete write, you should set `*status` to a value from the
     *  SDL_IOStatus enum. You do not have to explicitly set this on
     *  a complete, successful write.
     *
     *  \return the number of bytes written
     */
    size_t (SDLCALL *write)(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status);

    /**
     *  Close and free any allocated resources.
     *
     *  The SDL_IOStream is still destroyed even if this fails, so clean up anything
     *  even if flushing to disk returns an error.
     *
     *  \return SDL_TRUE if successful or SDL_FALSE on write error when flushing data.
     */
    SDL_bool (SDLCALL *close)(void *userdata);

} SDL_IOStreamInterface_pack1;

typedef struct SDL_GPUDepthStencilValue_pack1 {
    float depth;
    Uint8 stencil;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUDepthStencilValue_pack1;

typedef struct SDL_GPUViewport_pack1 {
    float x;
    float y;
    float w;
    float h;
    float minDepth;
    float maxDepth;
} SDL_GPUViewport_pack1;

typedef struct SDL_GPUTextureTransferInfo_pack1 {
    SDL_GPUTransferBuffer *transferBuffer;
    Uint32 offset;      
    Uint32 imagePitch;  
    Uint32 imageHeight; 
} SDL_GPUTextureTransferInfo_pack1;

typedef struct SDL_GPUTransferBufferLocation_pack1 {
    SDL_GPUTransferBuffer *transferBuffer;
    Uint32 offset;
} SDL_GPUTransferBufferLocation_pack1;

typedef struct SDL_GPUTextureLocation_pack1 {
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layer;
    Uint32 x;
    Uint32 y;
    Uint32 z;
} SDL_GPUTextureLocation_pack1;

typedef struct SDL_GPUTextureRegion_pack1 {
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layer;
    Uint32 x;
    Uint32 y;
    Uint32 z;
    Uint32 w;
    Uint32 h;
    Uint32 d;
} SDL_GPUTextureRegion_pack1;

typedef struct SDL_GPUBlitRegion_pack1 {
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layerOrDepthPlane;
    Uint32 x;
    Uint32 y;
    Uint32 w;
    Uint32 h;
} SDL_GPUBlitRegion_pack1;

typedef struct SDL_GPUBufferLocation_pack1 {
    SDL_GPUBuffer *buffer;
    Uint32 offset;
} SDL_GPUBufferLocation_pack1;

typedef struct SDL_GPUBufferRegion_pack1 {
    SDL_GPUBuffer *buffer;
    Uint32 offset;
    Uint32 size;
} SDL_GPUBufferRegion_pack1;

typedef struct SDL_GPUIndirectDrawCommand_pack1 {
    Uint32 vertexCount;   
    Uint32 instanceCount; 
    Uint32 firstVertex;   
    Uint32 firstInstance; 
} SDL_GPUIndirectDrawCommand_pack1;

typedef struct SDL_GPUIndexedIndirectDrawCommand_pack1 {
    Uint32 indexCount;    
    Uint32 instanceCount; 
    Uint32 firstIndex;    
    Sint32 vertexOffset;  
    Uint32 firstInstance; 
} SDL_GPUIndexedIndirectDrawCommand_pack1;

typedef struct SDL_GPUIndirectDispatchCommand_pack1 {
    Uint32 groupCountX;
    Uint32 groupCountY;
    Uint32 groupCountZ;
} SDL_GPUIndirectDispatchCommand_pack1;

typedef struct SDL_GPUSamplerCreateInfo_pack1 {
    SDL_GPUFilter minFilter;
    SDL_GPUFilter magFilter;
    SDL_GPUSamplerMipmapMode mipmapMode;
    SDL_GPUSamplerAddressMode addressModeU;
    SDL_GPUSamplerAddressMode addressModeV;
    SDL_GPUSamplerAddressMode addressModeW;
    float mipLodBias;
    float maxAnisotropy;
    SDL_bool anisotropyEnable;
    SDL_bool compareEnable;
    Uint8 padding1;
    Uint8 padding2;
    SDL_GPUCompareOp compareOp;
    float minLod;
    float maxLod;

    SDL_PropertiesID props;
} SDL_GPUSamplerCreateInfo_pack1;

typedef struct SDL_GPUVertexBinding_pack1 {
    Uint32 binding;
    Uint32 stride;
    SDL_GPUVertexInputRate inputRate;
    Uint32 instanceStepRate; 
} SDL_GPUVertexBinding_pack1;

typedef struct SDL_GPUVertexAttribute_pack1 {
    Uint32 location;
    Uint32 binding;
    SDL_GPUVertexElementFormat format;
    Uint32 offset;
} SDL_GPUVertexAttribute_pack1;

typedef struct SDL_GPUVertexInputState_pack1 {
    const SDL_GPUVertexBinding *vertexBindings;
    Uint32 vertexBindingCount;
    const SDL_GPUVertexAttribute *vertexAttributes;
    Uint32 vertexAttributeCount;
} SDL_GPUVertexInputState_pack1;

typedef struct SDL_GPUStencilOpState_pack1 {
    SDL_GPUStencilOp failOp;
    SDL_GPUStencilOp passOp;
    SDL_GPUStencilOp depthFailOp;
    SDL_GPUCompareOp compareOp;
} SDL_GPUStencilOpState_pack1;

typedef struct SDL_GPUColorAttachmentBlendState_pack1 {
    SDL_bool blendEnable;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    SDL_GPUBlendFactor srcColorBlendFactor;
    SDL_GPUBlendFactor dstColorBlendFactor;
    SDL_GPUBlendOp colorBlendOp;
    SDL_GPUBlendFactor srcAlphaBlendFactor;
    SDL_GPUBlendFactor dstAlphaBlendFactor;
    SDL_GPUBlendOp alphaBlendOp;
    SDL_GPUColorComponentFlags colorWriteMask;
} SDL_GPUColorAttachmentBlendState_pack1;

typedef struct SDL_GPUShaderCreateInfo_pack1 {
    size_t codeSize;
    const Uint8 *code;
    const char *entryPointName;
    SDL_GPUShaderFormat format;
    SDL_GPUShaderStage stage;
    Uint32 samplerCount;
    Uint32 storageTextureCount;
    Uint32 storageBufferCount;
    Uint32 uniformBufferCount;

    SDL_PropertiesID props;
} SDL_GPUShaderCreateInfo_pack1;

typedef struct SDL_GPUTextureCreateInfo_pack1 {
    SDL_GPUTextureType type;
    SDL_GPUTextureFormat format;
    SDL_GPUTextureUsageFlags usageFlags;
    Uint32 width;
    Uint32 height;
    Uint32 layerCountOrDepth;
    Uint32 levelCount;
    SDL_GPUSampleCount sampleCount;

    SDL_PropertiesID props;
} SDL_GPUTextureCreateInfo_pack1;

typedef struct SDL_GPUBufferCreateInfo_pack1 {
    SDL_GPUBufferUsageFlags usageFlags;
    Uint32 sizeInBytes;

    SDL_PropertiesID props;
} SDL_GPUBufferCreateInfo_pack1;

typedef struct SDL_GPUTransferBufferCreateInfo_pack1 {
    SDL_GPUTransferBufferUsage usage;
    Uint32 sizeInBytes;

    SDL_PropertiesID props;
} SDL_GPUTransferBufferCreateInfo_pack1;

typedef struct SDL_GPURasterizerState_pack1 {
    SDL_GPUFillMode fillMode;
    SDL_GPUCullMode cullMode;
    SDL_GPUFrontFace frontFace;
    SDL_bool depthBiasEnable;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
} SDL_GPURasterizerState_pack1;

typedef struct SDL_GPUMultisampleState_pack1 {
    SDL_GPUSampleCount sampleCount;
    Uint32 sampleMask;
} SDL_GPUMultisampleState_pack1;

typedef struct SDL_GPUDepthStencilState_pack1 {
    SDL_bool depthTestEnable;
    SDL_bool depthWriteEnable;
    SDL_bool stencilTestEnable;
    Uint8 padding1;
    SDL_GPUCompareOp compareOp;
    SDL_GPUStencilOpState backStencilState;
    SDL_GPUStencilOpState frontStencilState;
    Uint8 compareMask;
    Uint8 writeMask;
    Uint8 reference;
    Uint8 padding2;
} SDL_GPUDepthStencilState_pack1;

typedef struct SDL_GPUColorAttachmentDescription_pack1 {
    SDL_GPUTextureFormat format;
    SDL_GPUColorAttachmentBlendState blendState;
} SDL_GPUColorAttachmentDescription_pack1;

typedef struct SDL_GPUGraphicsPipelineAttachmentInfo_pack1 {
    const SDL_GPUColorAttachmentDescription *colorAttachmentDescriptions;
    Uint32 colorAttachmentCount;
    SDL_bool hasDepthStencilAttachment;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    SDL_GPUTextureFormat depthStencilFormat;
} SDL_GPUGraphicsPipelineAttachmentInfo_pack1;

typedef struct SDL_GPUGraphicsPipelineCreateInfo_pack1 {
    SDL_GPUShader *vertexShader;
    SDL_GPUShader *fragmentShader;
    SDL_GPUVertexInputState vertexInputState;
    SDL_GPUPrimitiveType primitiveType;
    SDL_GPURasterizerState rasterizerState;
    SDL_GPUMultisampleState multisampleState;
    SDL_GPUDepthStencilState depthStencilState;
    SDL_GPUGraphicsPipelineAttachmentInfo attachmentInfo;
    float blendConstants[4];

    SDL_PropertiesID props;
} SDL_GPUGraphicsPipelineCreateInfo_pack1;

typedef struct SDL_GPUComputePipelineCreateInfo_pack1 {
    size_t codeSize;
    const Uint8 *code;
    const char *entryPointName;
    SDL_GPUShaderFormat format;
    Uint32 readOnlyStorageTextureCount;
    Uint32 readOnlyStorageBufferCount;
    Uint32 writeOnlyStorageTextureCount;
    Uint32 writeOnlyStorageBufferCount;
    Uint32 uniformBufferCount;
    Uint32 threadCountX;
    Uint32 threadCountY;
    Uint32 threadCountZ;

    SDL_PropertiesID props;
} SDL_GPUComputePipelineCreateInfo_pack1;

typedef struct SDL_GPUColorAttachmentInfo_pack1 {
    
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layerOrDepthPlane; 

    
    SDL_FColor clearColor;

    /* Determines what is done with the texture at the beginning of the render pass.
     *
     *   LOAD:
     *     Loads the data currently in the texture.
     *
     *   CLEAR:
     *     Clears the texture to a single color.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture memory.
     *     This is a good option if you know that every single pixel will be touched in the render pass.
     */
    SDL_GPULoadOp loadOp;

    /* Determines what is done with the texture at the end of the render pass.
     *
     *   STORE:
     *     Stores the results of the render pass in the texture.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture memory.
     *     This is often a good option for depth/stencil textures.
     */
    SDL_GPUStoreOp storeOp;

    
    SDL_bool cycle;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUColorAttachmentInfo_pack1;

typedef struct SDL_GPUDepthStencilAttachmentInfo_pack1 {
    
    SDL_GPUTexture *texture;

    
    SDL_GPUDepthStencilValue depthStencilClearValue;

    /* Determines what is done with the depth values at the beginning of the render pass.
     *
     *   LOAD:
     *     Loads the depth values currently in the texture.
     *
     *   CLEAR:
     *     Clears the texture to a single depth.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the memory.
     *     This is a good option if you know that every single pixel will be touched in the render pass.
     */
    SDL_GPULoadOp loadOp;

    /* Determines what is done with the depth values at the end of the render pass.
     *
     *   STORE:
     *     Stores the depth results in the texture.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture memory.
     *     This is often a good option for depth/stencil textures.
     */
    SDL_GPUStoreOp storeOp;

    /* Determines what is done with the stencil values at the beginning of the render pass.
     *
     *   LOAD:
     *     Loads the stencil values currently in the texture.
     *
     *   CLEAR:
     *     Clears the texture to a single stencil value.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the memory.
     *     This is a good option if you know that every single pixel will be touched in the render pass.
     */
    SDL_GPULoadOp stencilLoadOp;

    /* Determines what is done with the stencil values at the end of the render pass.
     *
     *   STORE:
     *     Stores the stencil results in the texture.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture memory.
     *     This is often a good option for depth/stencil textures.
     */
    SDL_GPUStoreOp stencilStoreOp;

    
    SDL_bool cycle;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUDepthStencilAttachmentInfo_pack1;

typedef struct SDL_GPUBufferBinding_pack1 {
    SDL_GPUBuffer *buffer;
    Uint32 offset;
} SDL_GPUBufferBinding_pack1;

typedef struct SDL_GPUTextureSamplerBinding_pack1 {
    SDL_GPUTexture *texture;
    SDL_GPUSampler *sampler;
} SDL_GPUTextureSamplerBinding_pack1;

typedef struct SDL_GPUStorageBufferWriteOnlyBinding_pack1 {
    SDL_GPUBuffer *buffer;

    
    SDL_bool cycle;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUStorageBufferWriteOnlyBinding_pack1;

typedef struct SDL_GPUStorageTextureWriteOnlyBinding_pack1 {
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layer;

    
    SDL_bool cycle;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUStorageTextureWriteOnlyBinding_pack1;

typedef struct SDL_Surface_pack1 {
    SDL_SurfaceFlags flags;     
    SDL_PixelFormat format;     
    int w, h;                   
    int pitch;                  
    void *pixels;               

    int refcount;               

    SDL_SurfaceData *internal;  

} SDL_Surface_pack1;

typedef struct SDL_Vertex_pack1 {
    SDL_FPoint position;        
    SDL_FColor color;           
    SDL_FPoint tex_coord;       
} SDL_Vertex_pack1;

typedef struct SDL_DisplayMode_pack1 {
    SDL_DisplayID displayID;        
    SDL_PixelFormat format;         
    int w;                          
    int h;                          
    float pixel_density;            
    float refresh_rate;             
    int refresh_rate_numerator;     
    int refresh_rate_denominator;   

    SDL_DisplayModeData *internal;  

} SDL_DisplayMode_pack1;

typedef struct SDL_AssertData_pack1 {
    SDL_bool always_ignore;  
    unsigned int trigger_count; 
    const char *condition;  
    const char *filename;  
    int linenum;  
    const char *function;  
    const struct SDL_AssertData *next;  
} SDL_AssertData_pack1;

typedef struct SDL_hid_device_info_pack1 {
    
    char *path;
    
    unsigned short vendor_id;
    
    unsigned short product_id;
    
    wchar_t *serial_number;
    /** Device Release Number in binary-coded decimal,
        also known as Device Version Number */
    unsigned short release_number;
    
    wchar_t *manufacturer_string;
    
    wchar_t *product_string;
    /** Usage Page for this Device/Interface
        (Windows/Mac/hidraw only) */
    unsigned short usage_page;
    /** Usage for this Device/Interface
        (Windows/Mac/hidraw only) */
    unsigned short usage;
    /** The USB interface which this logical device
        represents.

        Valid only if the device is a USB HID device.
        Set to -1 in all other cases.
    */
    int interface_number;

    /** Additional information about the USB interface.
        Valid on libusb and Android implementations. */
    int interface_class;
    int interface_subclass;
    int interface_protocol;

    
    SDL_hid_bus_type bus_type;

    
    struct SDL_hid_device_info *next;

} SDL_hid_device_info_pack1;

typedef struct SDL_Point_pack1 {
    int x;
    int y;
} SDL_Point_pack1;

typedef struct SDL_FPoint_pack1 {
    float x;
    float y;
} SDL_FPoint_pack1;

typedef struct SDL_Rect_pack1 {
    int x, y;
    int w, h;
} SDL_Rect_pack1;

typedef struct SDL_FRect_pack1 {
    float x;
    float y;
    float w;
    float h;
} SDL_FRect_pack1;

typedef struct SDL_VirtualJoystickTouchpadDesc_pack1 {
    Uint16 nfingers;    
    Uint16 padding[3];
} SDL_VirtualJoystickTouchpadDesc_pack1;

typedef struct SDL_VirtualJoystickSensorDesc_pack1 {
    SDL_SensorType type;    
    float rate;             
} SDL_VirtualJoystickSensorDesc_pack1;

typedef struct SDL_VirtualJoystickDesc_pack1 {
    Uint16 type;        
    Uint16 padding;     
    Uint16 vendor_id;   
    Uint16 product_id;  
    Uint16 naxes;       
    Uint16 nbuttons;    
    Uint16 nballs;      
    Uint16 nhats;       
    Uint16 ntouchpads;  
    Uint16 nsensors;    
    Uint16 padding2[2]; 
    Uint32 button_mask; /**< A mask of which buttons are valid for this controller
                             e.g. (1 << SDL_GAMEPAD_BUTTON_SOUTH) */
    Uint32 axis_mask;   /**< A mask of which axes are valid for this controller
                             e.g. (1 << SDL_GAMEPAD_AXIS_LEFTX) */
    const char *name;   
    const SDL_VirtualJoystickTouchpadDesc *touchpads;   
    const SDL_VirtualJoystickSensorDesc *sensors;       

    void *userdata;     
    void (SDLCALL *Update)(void *userdata); 
    void (SDLCALL *SetPlayerIndex)(void *userdata, int player_index); 
    SDL_bool (SDLCALL *Rumble)(void *userdata, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble); 
    SDL_bool (SDLCALL *RumbleTriggers)(void *userdata, Uint16 left_rumble, Uint16 right_rumble); 
    SDL_bool (SDLCALL *SetLED)(void *userdata, Uint8 red, Uint8 green, Uint8 blue); 
    SDL_bool (SDLCALL *SendEffect)(void *userdata, const void *data, int size); 
    SDL_bool (SDLCALL *SetSensorsEnabled)(void *userdata, SDL_bool enabled); 
    void (SDLCALL *Cleanup)(void *userdata); 
} SDL_VirtualJoystickDesc_pack1;

typedef struct SDL_PathInfo_pack1 {
    SDL_PathType type;          
    Uint64 size;                
    SDL_Time create_time;   
    SDL_Time modify_time;   
    SDL_Time access_time;   
} SDL_PathInfo_pack1;

typedef struct SDL_MessageBoxButtonData_pack1 {
    SDL_MessageBoxButtonFlags flags;
    int buttonID;       
    const char *text;   
} SDL_MessageBoxButtonData_pack1;

typedef struct SDL_MessageBoxColor_pack1 {
    Uint8 r, g, b;
} SDL_MessageBoxColor_pack1;

typedef struct SDL_MessageBoxColorScheme_pack1 {
    SDL_MessageBoxColor colors[SDL_MESSAGEBOX_COLOR_MAX];
} SDL_MessageBoxColorScheme_pack1;

typedef struct SDL_MessageBoxData_pack1 {
    SDL_MessageBoxFlags flags;
    SDL_Window *window;                 
    const char *title;                  
    const char *message;                

    int numbuttons;
    const SDL_MessageBoxButtonData *buttons;

    const SDL_MessageBoxColorScheme *colorScheme;   
} SDL_MessageBoxData_pack1;

typedef struct SDL_Color_pack1 {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
} SDL_Color_pack1;

typedef struct SDL_FColor_pack1 {
    float r;
    float g;
    float b;
    float a;
} SDL_FColor_pack1;

typedef struct SDL_Palette_pack1 {
    int ncolors;        
    SDL_Color *colors;  
    Uint32 version;     
    int refcount;       
} SDL_Palette_pack1;

typedef struct SDL_PixelFormatDetails_pack1 {
    SDL_PixelFormat format;
    Uint8 bits_per_pixel;
    Uint8 bytes_per_pixel;
    Uint8 padding[2];
    Uint32 Rmask;
    Uint32 Gmask;
    Uint32 Bmask;
    Uint32 Amask;
    Uint8 Rbits;
    Uint8 Gbits;
    Uint8 Bbits;
    Uint8 Abits;
    Uint8 Rshift;
    Uint8 Gshift;
    Uint8 Bshift;
    Uint8 Ashift;
} SDL_PixelFormatDetails_pack1;

#pragma pack(pop)

