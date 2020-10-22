/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* *INDENT-OFF* */

#ifndef SDL_KMSDRM_MODULE
#define SDL_KMSDRM_MODULE(modname)
#endif

#ifndef SDL_KMSDRM_SYM
#define SDL_KMSDRM_SYM(rc,fn,params)
#endif

#ifndef SDL_KMSDRM_SYM_CONST
#define SDL_KMSDRM_SYM_CONST(type, name)
#endif


SDL_KMSDRM_MODULE(LIBDRM)
SDL_KMSDRM_SYM(void,drmModeFreeResources,(drmModeResPtr ptr))
SDL_KMSDRM_SYM(void,drmModeFreeFB,(drmModeFBPtr ptr))
SDL_KMSDRM_SYM(void,drmModeFreeCrtc,(drmModeCrtcPtr ptr))
SDL_KMSDRM_SYM(void,drmModeFreeConnector,(drmModeConnectorPtr ptr))
SDL_KMSDRM_SYM(void,drmModeFreeEncoder,(drmModeEncoderPtr ptr))
SDL_KMSDRM_SYM(int,drmGetCap,(int fd, uint64_t capability, uint64_t *value))
SDL_KMSDRM_SYM(int,drmIoctl,(int fd, unsigned long request, void *arg))
SDL_KMSDRM_SYM(drmModeResPtr,drmModeGetResources,(int fd))

SDL_KMSDRM_SYM(int,drmModeAddFB,(int fd, uint32_t width, uint32_t height, uint8_t depth,
                                 uint8_t bpp, uint32_t pitch, uint32_t bo_handle,
                                 uint32_t *buf_id))

SDL_KMSDRM_SYM(int,drmModeAddFB2,(int fd, uint32_t width, uint32_t height,
                                  uint32_t pixel_format, const uint32_t bo_handles[4],
                                  const uint32_t pitches[4], const uint32_t offsets[4],
                                  uint32_t *buf_id, uint32_t flags))

SDL_KMSDRM_SYM(int,drmModeAddFB2WithModifiers,(int fd, uint32_t width, uint32_t height,
                                               uint32_t pixel_format, const uint32_t bo_handles[4],
                                               const uint32_t pitches[4], const uint32_t offsets[4],
                                               const uint64_t modifier[4], uint32_t *buf_id,
                                               uint32_t flags))

SDL_KMSDRM_SYM(int,drmModeRmFB,(int fd, uint32_t bufferId))
SDL_KMSDRM_SYM(drmModeFBPtr,drmModeGetFB,(int fd, uint32_t buf))
SDL_KMSDRM_SYM(drmModeCrtcPtr,drmModeGetCrtc,(int fd, uint32_t crtcId))
SDL_KMSDRM_SYM(drmModeEncoderPtr,drmModeGetEncoder,(int fd, uint32_t encoder_id))
SDL_KMSDRM_SYM(drmModeConnectorPtr,drmModeGetConnector,(int fd, uint32_t connector_id))

/* Atomic functions */

SDL_KMSDRM_SYM(int,drmSetClientCap,(int fd, uint64_t capability, uint64_t value))
SDL_KMSDRM_SYM(drmModePlaneResPtr,drmModeGetPlaneResources,(int fd))
SDL_KMSDRM_SYM(drmModePlanePtr,drmModeGetPlane,(int fd, uint32_t plane_id))
SDL_KMSDRM_SYM(drmModeObjectPropertiesPtr,drmModeObjectGetProperties,(int fd,uint32_t object_id,uint32_t object_type))
SDL_KMSDRM_SYM(drmModePropertyPtr,drmModeGetProperty,(int fd, uint32_t propertyId))

SDL_KMSDRM_SYM(void,drmModeFreeProperty,(drmModePropertyPtr ptr))
SDL_KMSDRM_SYM(void,drmModeFreeObjectProperties,(drmModeObjectPropertiesPtr ptr))
SDL_KMSDRM_SYM(void,drmModeFreePlane,(drmModePlanePtr ptr))
SDL_KMSDRM_SYM(void,drmModeFreePlaneResources,(drmModePlaneResPtr ptr))

SDL_KMSDRM_SYM(drmModeAtomicReqPtr,drmModeAtomicAlloc,(void))
SDL_KMSDRM_SYM(void,drmModeAtomicFree,(drmModeAtomicReqPtr req))
SDL_KMSDRM_SYM(int,drmModeAtomicCommit,(int fd,drmModeAtomicReqPtr req,uint32_t flags,void *user_data))
SDL_KMSDRM_SYM(int,drmModeAtomicAddProperty,(drmModeAtomicReqPtr req,uint32_t object_id,uint32_t property_id,uint64_t value))
SDL_KMSDRM_SYM(int,drmModeCreatePropertyBlob,(int fd,const void *data,size_t size,uint32_t *id))

/* End of atomic fns */

SDL_KMSDRM_MODULE(GBM)
SDL_KMSDRM_SYM(int,gbm_device_get_fd,(struct gbm_device *gbm))
SDL_KMSDRM_SYM(int,gbm_device_is_format_supported,(struct gbm_device *gbm,
                                                   uint32_t format, uint32_t usage))
SDL_KMSDRM_SYM(void,gbm_device_destroy,(struct gbm_device *gbm))
SDL_KMSDRM_SYM(struct gbm_device *,gbm_create_device,(int fd))
SDL_KMSDRM_SYM(unsigned int,gbm_bo_get_width,(struct gbm_bo *bo))
SDL_KMSDRM_SYM(unsigned int,gbm_bo_get_height,(struct gbm_bo *bo))
SDL_KMSDRM_SYM(uint32_t,gbm_bo_get_stride,(struct gbm_bo *bo))
SDL_KMSDRM_SYM(uint32_t,gbm_bo_get_stride_for_plane,(struct gbm_bo *bo,int plane))
SDL_KMSDRM_SYM(uint32_t,gbm_bo_get_format,(struct gbm_bo *bo))
SDL_KMSDRM_SYM(uint32_t,gbm_bo_get_offset,(struct gbm_bo *bo, int plane))
SDL_KMSDRM_SYM(int,gbm_bo_get_plane_count,(struct gbm_bo *bo))

SDL_KMSDRM_SYM(union gbm_bo_handle,gbm_bo_get_handle,(struct gbm_bo *bo))
SDL_KMSDRM_SYM(int,gbm_bo_write,(struct gbm_bo *bo, const void *buf, size_t count))
SDL_KMSDRM_SYM(struct gbm_device *,gbm_bo_get_device,(struct gbm_bo *bo))
SDL_KMSDRM_SYM(void,gbm_bo_set_user_data,(struct gbm_bo *bo, void *data,
                                          void (*destroy_user_data)(struct gbm_bo *, void *)))
SDL_KMSDRM_SYM(void *,gbm_bo_get_user_data,(struct gbm_bo *bo))
SDL_KMSDRM_SYM(void,gbm_bo_destroy,(struct gbm_bo *bo))
SDL_KMSDRM_SYM(struct gbm_bo *,gbm_bo_create,(struct gbm_device *gbm,
                                              uint32_t width, uint32_t height,
                                              uint32_t format, uint32_t usage))
SDL_KMSDRM_SYM(struct gbm_surface *,gbm_surface_create,(struct gbm_device *gbm,
                                                        uint32_t width, uint32_t height,
                                                        uint32_t format, uint32_t flags))
SDL_KMSDRM_SYM(void,gbm_surface_destroy,(struct gbm_surface *surf))
SDL_KMSDRM_SYM(struct gbm_bo *,gbm_surface_lock_front_buffer,(struct gbm_surface *surf))
SDL_KMSDRM_SYM(void,gbm_surface_release_buffer,(struct gbm_surface *surf, struct gbm_bo *bo))


#undef SDL_KMSDRM_MODULE
#undef SDL_KMSDRM_SYM
#undef SDL_KMSDRM_SYM_CONST

/* *INDENT-ON* */

/* vi: set ts=4 sw=4 expandtab: */
