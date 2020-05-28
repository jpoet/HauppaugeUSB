
## Override path set by Hauppauge's make files, to use our 'wrappers'

override TOP := ./Hauppauge

override VPATH = ${TOP} $(TOP)/Common/Rx $(TOP)/Common/Rx/ADV7842	\
$(TOP)/Common/Rx/ADV7842/RX/LIB $(TOP)/Common/Rx/ADV7842/RX/HAL	\
$(TOP)/Common/Rx/ADV7842/RX/HAL/4G				\
$(TOP)/Common/Rx/ADV7842/RX/HAL/4G/ADV7842/HAL $(TOP)/Common	\
./Wrappers/$(OS) $(TOP)/Common/FX2API $(TOP)/Common/EncoderDev	\
$(TOP)/Common/EncoderDev/HAPIHost				\
$(TOP)/Common/EncoderDev/HAPIHost/MChip

override INC = $(OS_INC) -I.. -I$(TOP)/Common -I./Wrappers/$(OS)	\
-I$(TOP)/Common/FX2API -I$(TOP)/Common/Rx/ADV7842			\
-I$(TOP)/Common/Rx/ADV7842/RX -I$(TOP)/Common/Rx/ADV7842/RX/LIB		\
-I$(TOP)/Common/Rx/ADV7842/RX/HAL					\
-I$(TOP)/Common/Rx/ADV7842/RX/HAL/4G					\
-I$(TOP)/Common/Rx/ADV7842/RX/HAL/4G/ADV7842/HAL			\
-I$(TOP)/Common/Rx/ADV7842/RX/HAL/4G/ADV7842/MACROS			\
-I$(TOP)/Common/Rx -I$(TOP)/Common/EncoderDev				\
-I$(TOP)/Common/EncoderDev/HAPIHost					\
-I$(TOP)/Common/EncoderDev/HAPIHost/MChip                               \
`pkg-config --cflags libusb-1.0` \
`pkg-config --cflags libavformat` 


override OBJS_WRAPPERS = log.o baseif.o registryif.o USBif.o I2Cif.o
override OS_INC := `pkg-config --cflags libusb-1.0`

# override CXXFLAGS := -g -c -Wall -std=c++11 ${CFLAGS}

include ./Hauppauge/TestApp/build-ADV7842/Makefile

REC_CXX = g++
REC_CXXFLAGS := -g -c -Wall -std=c++11 -fdiagnostics-color -DBOOST_LOG_DYN_LINK ${CFLAGS} -ffast-math
REC_LDFLAGS = -lswscale -lavdevice -lavformat -lavcodec -lavutil

#	        `pkg-config --libs libsystemd` \

REC_LDFLAGS  += `pkg-config --libs libusb-1.0` \
		`pkg-config --libs libavformat` \
	        -lpthread

REC_SOURCES = Logger.cpp Common.cpp MythTV.cpp FlipInterlacedFields.cpp HauppaugeDev.cpp hauppauge2.cpp Transcoder.cpp StreamBuffer.cpp AudioDecoder.cpp StreamWriter.cpp AudioBuffer.cpp AudioEncoder.cpp
REC_HEADERS = Logger.h Common.h MythTV.h FlipInterlacedFields.h HauppaugeDev.h Transcoder.h StreamBuffer.h AudioDecoder.h StreamWriter.h AudioBuffer.h AudioEncoder.h PhaseShifter.h AllpassFilter.h LowpassFilter.h AudioDelay.h
REC_OBJECTS = $(REC_SOURCES:.cpp=.o)

CONF = etc/sample.conf
FIRMWARE = Hauppauge/Common/EncoderDev/HAPIHost/bin/llama_usb_vx_host_slave_t22_24.bin Hauppauge/Common/EncoderDev/HAPIHost/bin/mips_vx_host_slave.bin
TRANSIENT = FX2Firmware.cpp mchip_binary.cpp
REC_EXE  = hauppauge2
REC_LIBS = libADV7842.a
REC_LIBS += -lboost_program_options -lboost_log -lboost_log_setup -lboost_system -lboost_thread -lboost_filesystem

all: ${REC_EXE}

${REC_EXE}: ${REC_OBJECTS} ${REC_LIBS}
	${REC_CXX} ${REC_OBJECTS} -o $@ ${REC_LIBS} ${REC_LDFLAGS} 

${REC_OBJECTS}: ${REC_SOURCES}

.cpp.o:
	${REC_CXX} ${REC_CXXFLAGS} $< -o $@

#.c.o:
#	${REC_CXX} ${REC_CXXFLAGS} $< -o $@

install: ${REC_EXE}
	ln -snf ${TOP}/Common/*.cfg .
	ln -snf $(TOP)/Common/EncoderDev/HAPIHost/bin/*.bin .

clean:
	$(RM) *.o *.a ${REC_EXE} ${TRANSIENT}

install:
	install -D --target-directory /opt/Hauppauge/bin ${REC_EXE}
	install -D --target-directory /opt/Hauppauge/firmware ${FIRMWARE}
	install -D --target-directory /opt/Hauppauge/etc ${CONF}
