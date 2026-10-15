# choose your compiler
#CC=gcc
CC=gcc -Wall

SRCFLDR=./src
OBJFLDR=./obj
DESTFLDR=./dist

$(DESTFLDR)/mysh: $(OBJFLDR)/sh.o $(OBJFLDR)/get_path.o $(SRCFLDR)/main.c
	mkdir -p $(DESTFLDR)
	$(CC) -g $(SRCFLDR)/main.c $(OBJFLDR)/sh.o $(OBJFLDR)/get_path.o -o $(DESTFLDR)/mysh

$(OBJFLDR)/sh.o: $(SRCFLDR)/sh.c $(SRCFLDR)/sh.h
	mkdir -p $(OBJFLDR)
	$(CC) -g -c $(SRCFLDR)/sh.c -o $(OBJFLDR)/sh.o

$(OBJFLDR)/get_path.o: $(SRCFLDR)/get_path.c $(SRCFLDR)/get_path.h
	mkdir -p $(OBJFLDR)
	$(CC) -g -c $(SRCFLDR)/get_path.c -o $(OBJFLDR)/get_path.o

clean:
	rm -rf $(OBJFLDR)/*.o $(DESTFLDR)/mysh
