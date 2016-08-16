################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../avunafcgi/avunafcgi.c \
../avunafcgi/fcgi.c \
../avunafcgi/util.c \
../avunafcgi/xstring.c 

OBJS += \
./avunafcgi/avunafcgi.o \
./avunafcgi/fcgi.o \
./avunafcgi/util.o \
./avunafcgi/xstring.o 

C_DEPS += \
./avunafcgi/avunafcgi.d \
./avunafcgi/fcgi.d \
./avunafcgi/util.d \
./avunafcgi/xstring.d 


# Each subdirectory must supply rules for building sources it contributes
avunafcgi/%.o: ../avunafcgi/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -std=gnu11 -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


