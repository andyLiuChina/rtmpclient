RM:= rm -rf
LIBS := -lposa_linux -lrtmpclient -lrtmpserver

CPP_SRCS += \
../demo.cpp

OBJS += \
./demo.o

CPP_DEPS += \
./demo.d

%.o: ./%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I../common -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

all: demo

demo: $(OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	g++ -L../bin -o "demo" ./demo.o $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(OBJS) $(CPP_DEPS) demo
	-@echo ' '

post-build:
	-cp ./demo ../bin 
	-@echo ' '
