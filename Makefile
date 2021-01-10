SRC=shell.c vec.c parser.c cd.c

CC=gcc
FLAGS=-O3 -Wall -Wextra
INCLUDES=$(wildcard include/*.h)
OBJ=$(SRC:.c=.o)
TARGET=shell
INCLUDE=-Iinclude
LIB=-lreadline

$(TARGET): $(OBJ)
	$(CC) $(FLAGS) $(INCLUDE) -o $(TARGET) $(OBJ) $(LIB)

%.o : src/%.c $(INCLUDES)
	$(CC) $(FLAGS) $(INCLUDE) -c $< -o $@ $(LIB)

all: $(TARGET)

clean:
	$(RM) *.o *~ $(TARGET)
