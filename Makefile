SOURCE = tinyauto.c timer.c
BIN_NAME = tinyauto
CC = gcc
CFLAGS = -Wall -Wextra -lpaho-mqtt3c -lbsd

$(BIN_NAME) : $(SOURCE)
	$(CC) -o $(BIN_NAME) $(SOURCE) $(CFLAGS)

.PHONE : deploy
deploy : $(SOURCE) $(BIN_NAME)
	rsync Makefile $(SOURCE) auto:~/tinyauto/
	ssh auto 'cd tinyauto && make && sudo systemctl restart tinyauto'

.PHONY : clean
clean :
	rm *.o $(BIN_NAME)
.PHONY : intclean
intclean :
	rm *.o
