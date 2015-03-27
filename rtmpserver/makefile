RM := rm -rf

LIBS := -lposa_linux -lcrypto

CPP_SRCS += \
./source/posartmpserver.cpp 

OBJS += \
./source/posartmpserver.o 

CPP_DEPS += \
./source/posartmpserver.d 


# Each subdirectory must supply rules for building sources it contributes
source/%.o: ./source/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I../common -I./include -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

all: librtmpserver.so

# Tool invocations
librtmpserver.so: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	g++ -L../bin -shared -o "librtmpserver.so" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(OBJS) $(CPP_DEPS) librtmpserver.so
	-@echo ' '

post-build:
	-cp ./*.so   ../bin
	-@echo ' '
