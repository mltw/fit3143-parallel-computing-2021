ALL: Main Base_station Node

Main:	main.c
	mpicc -Wall main.c base_station.c node.c -o mainOut -lm 

Base_station: base_station.c

Node:	node.c

run:
	mpirun --oversubscribe -np 5 mainOut 2 2

clean :
	/bin/rm -f mainOut *.o *.txt

