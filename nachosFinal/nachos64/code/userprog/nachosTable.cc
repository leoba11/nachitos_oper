#include "nachosTable.h"
#include <iostream>

using namespace std;

// se crea vector de openFile, el bitMap se inicializa
NachosOpenFilesTable::NachosOpenFilesTable()
{
    openFiles = new int[PAGES];  
    openFilesMap = new BitMap(PAGES);

    for (int i = 0; i < 3; i++)  // marcar stdin, stdout y stderror    
    {
        openFiles[i] = i;
        openFilesMap->Mark(i);
    }
    
    usage = 0;
}
    
NachosOpenFilesTable::~NachosOpenFilesTable()
{
    delete openFiles;
    delete openFilesMap;
} 

int NachosOpenFilesTable::Open( int UnixHandle )
{
    int free = openFilesMap->Find();    // Devuelve el primer bit libre, y lo marca como en uso (busca y lo aloca)
    if (free != -1)
    {
        openFiles[free] = UnixHandle;   //asigna el unixHandle al archivo libre
    }
    return free;
}
    
// si se cierra devuelve 0 sino -1
int NachosOpenFilesTable::Close( int NachosHandle )
{
    int close = -1;
    if (isOpened(NachosHandle))
    {
        openFilesMap->Clear(NachosHandle);  // Limpia el NachosHandle'esimo numero
        openFiles[NachosHandle] = 0;
        close = 0;
    }
    return close;
}    
    
bool NachosOpenFilesTable::isOpened( int NachosHandle )
{
    return openFilesMap->Test(NachosHandle);    // retorna true si NachosHandle esta seteado
}

// devuelve el identificador si esta abierto(si lo esta usando), sino devuelve -1   
int NachosOpenFilesTable::getUnixHandle( int NachosHandle )
{
    // printf("NachosHandle ---->  %d", NachosHandle);
    int identifier = -1;
    if (isOpened(NachosHandle))
    {
        identifier = openFiles[NachosHandle];
    }
    return identifier;
}
    
// aumenta en uno la cantidad de threads 
void NachosOpenFilesTable::addThread()
{
    usage++;
}

// decrementa en uno la cantidad de threads  
void NachosOpenFilesTable::delThread()
{
    usage--;
}		

// Imprime ambos handle de cada openFile.
void NachosOpenFilesTable::Print()
{
    for (int i = 0; i < PAGES; i++)
    {
        cout << "Nachos: " << i << "\n" << "Unix: " << openFiles[i] << endl;
    }   
}