main: main.c print.o
	gcc -o main main.c print.o
print.o: print.asm
	nasm -f elf64 -o print.o print.asm
