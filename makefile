###############################################################
# makefile for scd30. October 2018 / paulvha
#
# To create a build with only the SCD30 monitor type: 
#		make
#
# To create a build with BOTH the SCD30 and DYLOS monitor type:
# 		make BUILD=DYLOS
#
###############################################################
BUILD := scd30

# set the right flags and objects to include
ifeq ($(BUILD),scd30)
CXXFLAGS := -Wall -Werror -c
OBJ := scd30_lib.o scd30.o
fresh:
else
CXXFLAGS := -DDYLOS -Wall -Werror -c 
OBJ := scd30_lib.o scd30.o dylos.o
fresh:
endif

# set variables
CC := gcc
DEPS := SCD30.h bcm2835.h twowire.h
LIBS := -lm -ltwowire -lbcm2835 

# how to create .o from .c or .cpp files
.c.o: %c $(DEPS)
	$(CC) $(CXXFLAGS) -o $@ $<

.cpp.o: %c $(DEPS)
	$(CC) $(CXXFLAGS) -o $@ $<

.PHONY : clean scd30 fresh newscd
	
scd30 : $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

clean :
	rm scd30 $(OBJ)

# scd30.o is removed as this is only impacted by including
# Dylos monitor or not. 
newscd :
	rm -f scd30.o
	
# first execute newscd then build scd30
fresh : newscd scd30
