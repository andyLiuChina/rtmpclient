RM := rm -rf

LIBS := -lcrypto -lposa_linux

CPP_SRCS += \
./source/posartmppacket.cpp \
./source/posartmpprotocol.cpp \
./source/posartmpclient.cpp

OBJS += \
./source/posartmppacket.o \
./source/posartmpprotocol.o \
./source/posartmpclient.o

CPP_DEPS += \
./source/posartmppacket.d \
./source/posartmpprotocol.d \
./source/posartmpclient.d

source/%.o: ./source/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I../common -I./include -O0 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

all: librtmpclient.so

librtmpclient.so: $(OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	g++ -L../bin -shared -o "librtmpclient.so" $(OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(OBJS) $(CPP_DEPS) librtmpclient.so
	-@echo ' '

post-build:
	-cp ./*.so  ../bin
	-@echo ' '
