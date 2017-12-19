all: prog2_arq.out prog2_gbn.out

prog2_arq.out: prog2.c prog2_arq.c
	gcc -I. prog2.c prog2_arq.c -o prog2_arq.out

prog2_gbn.out: prog2.c prog2_gbn.c
	gcc -I. prog2.c prog2_gbn.c -o prog2_gbn.out

Then run the .out files to get the desired results
./prog2_arq.out
./prog2_gbn.out
