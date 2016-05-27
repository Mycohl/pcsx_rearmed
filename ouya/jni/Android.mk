LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := pcsx-rearmed-ouya

TARGET_PLATFORM := android-16

LOCAL_ARM_MODE := arm

LOCAL_ARM_NEON := true

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include jni/src/include

LOCAL_CFLAGS += -DANDROID_ARM -DNEON_BUILD

LOCAL_SRC_FILES += ../../libpcsxcore/gte_arm.S \
                   ../../libpcsxcore/gte_neon.S 
                   

# dynarec
LOCAL_SRC_FILES += ../../libpcsxcore/new_dynarec/new_dynarec.c \
                   ../../libpcsxcore/new_dynarec/linkage_arm.S \
                   ../../libpcsxcore/new_dynarec/emu_if.c \
                   ../../libpcsxcore/new_dynarec/pcsxmem.c

# gpu
LOCAL_CFLAGS += -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
LOCAL_SRC_FILES += ../../plugins/gpu_neon/psx_gpu_if.c \
                   ../../plugins/gpu_neon/psx_gpu/psx_gpu_arm_neon.S 

# core
LOCAL_SRC_FILES += ../../libpcsxcore/cdriso.c \
                   ../../libpcsxcore/cdrom.c \
                   ../../libpcsxcore/cheat.c \
                   ../../libpcsxcore/debug.c \
                   ../../libpcsxcore/decode_xa.c \
                   ../../libpcsxcore/disr3000a.c \
                   ../../libpcsxcore/mdec.c \
                   ../../libpcsxcore/misc.c \
                   ../../libpcsxcore/plugins.c \
                   ../../libpcsxcore/ppf.c \
                   ../../libpcsxcore/psxbios.c \
                   ../../libpcsxcore/psxcommon.c \
                   ../../libpcsxcore/psxcounters.c \
                   ../../libpcsxcore/psxdma.c \
                   ../../libpcsxcore/psxhle.c \
                   ../../libpcsxcore/psxhw.c \
                   ../../libpcsxcore/psxinterpreter.c \
                   ../../libpcsxcore/psxmem.c \
                   ../../libpcsxcore/r3000a.c \
                   ../../libpcsxcore/sio.c \
                   ../../libpcsxcore/socket.c \
                   ../../libpcsxcore/spu.c \
                   ../../libpcsxcore/gte.c \
                   ../../libpcsxcore/gte_nf.c \
                   ../../libpcsxcore/gte_divider.c

# spu
LOCAL_SRC_FILES += ../../plugins/dfsound/dma.c \
                   ../../plugins/dfsound/freeze.c \
                   ../../plugins/dfsound/registers.c \
                   ../../plugins/dfsound/spu.c \
                   ../../plugins/dfsound/out.c \
                   ../../plugins/dfsound/nullsnd.c \
                   ../../plugins/dfsound/arm_utils.S

# builtin gpu
LOCAL_SRC_FILES += ../../plugins/gpulib/gpu.c \
                   ../../plugins/gpulib/vout_pl.c

# cdrcimg
LOCAL_SRC_FILES += ../../plugins/cdrcimg/cdrcimg.c

# dfinput
LOCAL_SRC_FILES += ../../plugins/dfinput/main.c \
                   ../../plugins/dfinput/pad.c \
                   ../../plugins/dfinput/guncon.c

# misc
LOCAL_CFLAGS += -DHAVE_GLES -DDO_BGR_TO_RGB
LOCAL_SRC_FILES += ../../frontend/ouya.c \
                   ../../frontend/main.c \
                   ../../frontend/plugin.c \
                   ../../frontend/cspace_neon.S \
                   ../../frontend/libpicofe/arm/neon_scale2x.S \
                   ../../frontend/libpicofe/arm/neon_eagle2x.S 
                   
# jni
LOCAL_SRC_FILES += src/jniapi.c \
                   src/renderer.c \
                   src/audio.c \
                   src/input.c \
                   src/settings.c 


LOCAL_CFLAGS += -O3 -ffast-math -funroll-loops -DNDEBUG -D_FILE_OFFSET_BITS=64 -DHAVE_OUYA -DNO_FRONTEND -DFRONTEND_SUPPORTS_RGB565

LOCAL_LDLIBS := -landroid -lEGL -lGLESv2 -lOpenSLES -lz -llog 

LOCAL_STATIC_LIBRARIES := 

include $(BUILD_SHARED_LIBRARY)

