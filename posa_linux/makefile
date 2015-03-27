BINDIR=../bin
COM_H_DIR=../common
RM = rm -rf

LIBS=-lpthread -lrt

CPP_FILE += ./source/posa_linux.cpp \
            ./source/posaapios.cpp \
			./source/posabuffer.cpp \
			./source/posaproactor.cpp \
			./source/posatelnetserver.cpp

OBJS += ./source/posa_linux.o \
            ./source/posaapios.o \
			./source/posabuffer.o \
			./source/posaproactor.o \
			./source/posatelnetserver.o

CPP_DEPS += \
./source/posa_linux.d \
./source/posaapios.d \
./source/posabuffer.d \
./source/posaproactor.d \
./source/posatelnetserver.d 

source/%.o: ./source/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I../common -I./include -O0 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

all: libposa_linux.so

libposa_linux.so: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	g++ -shared -o "libposa_linux.so" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(OBJS) $(CPP_DEPS) libposa_linux.so
	-@echo ' '

post-build:
	-cp ./*.so  ../bin
	-@echo ' '
