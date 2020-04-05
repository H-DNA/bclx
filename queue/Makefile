#The main file name of your program
OUT = test

#The path of input files
DIR_IN = ./ben

#The path of output files
DIR_OUT = ./out

#The path of BCL Core
DIR_LIB = ../bcl_ext

#The number of units
NUM_UNITS = 4

#The performance flags for the compiler
FLAGS = -std=gnu++17 -O3

.PHONY : all run clean

#Compile your program
all : $(DIR_OUT)/$(OUT)

$(DIR_OUT)/$(OUT) : $(DIR_IN)/$(OUT).cpp
	mpic++ $(DIR_IN)/$(OUT).cpp -o $(DIR_OUT)/$(OUT) $(FLAGS) -I$(DIR_LIB)

#Run your program
run : $(DIR_OUT)/$(OUT)
	mpirun -np $(NUM_UNITS) $(DIR_OUT)/$(OUT)

#Remove your executable
clean :
	rm -f $(DIR_OUT)/*
