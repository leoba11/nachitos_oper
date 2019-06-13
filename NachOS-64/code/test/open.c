#include "syscall.h"

int main(){
	char arreglo[64];
	char arreglo2[16];
	int id = Open("algo.txt");
	Read(arreglo, 64, id);
	Write(arreglo, 64, 1);
	Read(arreglo2, 64, 0);
	Create("PruebaWrite.txt");
	id = Open("PruebaWrite.txt");
	Write(arreglo2, 12, id);
	Close("algo.txt");
	Close("PruebaWrite.txt");
	Halt();
    return 0;
}