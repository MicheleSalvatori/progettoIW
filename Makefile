CC = gcc
LIB?= ./lib/
# FILEC = $(LIB)receiver.c client.c -lm -lpthread -lrt
# FILES = $(LIB)receiver.c server.c -lm -lpthread -lrt
FILEC = $(LIB)receiver.c $(LIB)sender.c client.c -lm -lpthread -lrt
FILES = $(LIB)receiver.c $(LIB)sender.c server.c -lm -lpthread -lrt

do:
	$(CC) $(FILEC) -o client
	$(CC) $(FILES) -o server

	@echo " "
	@echo "Compilato"
clean:
	rm client
	rm server

	@echo " "
	@echo "File eliminati!"
