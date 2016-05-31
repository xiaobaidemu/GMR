###########################################
#Makefile for simple programs
###########################################
INC= 
LIB= -lstdc++ 
CC= mpic++
CC_FLAG=-Wall -std=c++11

OBJ=
PRG=igmr

$(PRG): *.cpp *.h
		$(CC) $(CC_FLAG) $(INC) $(LIB) -o $@ $@.cpp
			
.SUFFIXES: .c .o .cpp
	.cpp.o:
		$(CC) $(CC_FLAG) $(INC) -c $*.cpp -o $*.o

.PHONY : clean
clean:
	@echo "Removing linked and compiled files......"
		rm -f $(OBJ) $(PRG) 
