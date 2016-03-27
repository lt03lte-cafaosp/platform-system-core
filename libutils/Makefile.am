lib_LIBRARIES := libutils.a

libutils_a_CPPFLAGS := -I../include -I../libcutils
libutils_a_CPPFLAGS += -I../../../frameworks/native/include
libutils_a_CPPFLAGS += -I../../../external/safe-iop/include
libutils_a_CPPFLAGS += -I${WORKSPACE}/external/safe-iop/include

libutils_a_CPPFLAGS += -DHAVE_PTHREADS
libutils_a_CPPFLAGS += -DLINUX_ENABLED -fPIC
libutils_a_CPPFLAGS += -DANDROID_SMP=0 -DHAVE_OFF64_T
libutils_a_CPPFLAGS += -include ../include/arch/linux-arm/OEConfig.h

common_sources :=
# NOTE: Only supports builds of shared/static libraries for the target
#common_sources := BasicHashtable.cpp
#common_sources += BlobCache.cpp
#common_sources += CallStack.cpp
#common_sources += FileMap.cpp
#common_sources += JenkinsHash.cpp
#common_sources += Log.cpp
#common_sources += LinearTransform.cpp
#common_sources += Looper.cpp
#common_sources += misc.cpp
#common_sources += Printer.cpp
#common_sources += ProcessCallStack.cpp
common_sources += RefBase.cpp
common_sources += SharedBuffer.cpp
common_sources += Static.cpp
#common_sources += StopWatch.cpp
common_sources += String16.cpp
common_sources += String8.cpp
#common_sources += SystemClock.cpp
common_sources += Threads.cpp
#common_sources += Timers.cpp
#common_sources += Tokenizer.cpp
#common_sources += Trace.cpp
common_sources += Unicode.cpp
common_sources += VectorImpl.cpp

requiredlibs = -lpthread

libutils_a_SOURCES := $(common_sources)

# Build a DSO too
lib_LTLIBRARIES := libutils.la
libutils_la_SOURCES := $(libutils_a_SOURCES)
libutils_la_CPPFLAGS := $(libutils_a_CPPFLAGS)
