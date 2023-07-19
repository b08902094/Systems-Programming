all: battle.c player.c
	gcc battle.c -o battle
	gcc player.c -o player
clean:
	find . ! -name player.c ! -name battle.c ! -name README.md ! -name Makefile ! -name status.h ! -name player_status.txt ! -name sample -maxdepth 1 -type f -delete
	rm *.fifo
