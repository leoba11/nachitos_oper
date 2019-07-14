// addrspace.h
//	Data structures to keep track of executing user programs
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include <string>
#include "noff.h"

#define UserStackSize		1024 	// increase this as necessary!

class AddrSpace {
  public:
    AddrSpace(AddrSpace* x);
    AddrSpace(OpenFile *executable);
	
    ~AddrSpace();			// De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers, before jumping to user code

    void SaveState();	
    void RestoreState();

    // Métodos para pageFaultException
    TranslationEntry* getPageTable(); //Devuelve la pageTable del hilo
    void getFromSwap(int vpn, int s); // Obtiene pag desde el el swap
    void saveToSwap(int vpn); // Guarda pag en el swap
    void setFileName(std::string name); // Setea el nombre del archivo
    std::string getFileName();  // Obtiene el nombre del archivo
    NoffHeader NoffH;
    unsigned int numPages;		// Numero de pags en addresspace virtual


  private:
    TranslationEntry *pageTable;
    // Atributos para pageFaultException
    OpenFile *swap;
    char *swapname;
    std::string fileName;
};

#endif // ADDRSPACE_H
