#include "syscall.h"

int main(){
	int id1, id2;

	Write("Exec: Prueba de Exec y Join", 27, 1);
	id1 = Exec("n");
        
	Write("Exec: Fin de Prueba ...", 23, 1);
        Exit(0);

	return 0;
}