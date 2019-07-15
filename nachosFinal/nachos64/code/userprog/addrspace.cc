// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void SwapHeader (NoffHeader *noffH){
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------
AddrSpace::AddrSpace(AddrSpace* x){
    numPages = x->numPages;
    pageTable = new TranslationEntry[numPages];

    int paginas = numPages - 8;
    for(int i = 0; i < paginas; i++){	//Se asignan los mismos datos del parámetro en el pageTable para las páginas de código y datos
        pageTable[i].virtualPage = x->pageTable[i].virtualPage;
        pageTable[i].physicalPage = x->pageTable[i].physicalPage;
        pageTable[i].valid = x->pageTable[i].valid;
        pageTable[i].use = x->pageTable[i].use;
        pageTable[i].dirty = x->pageTable[i].dirty;
        pageTable[i].readOnly = x->pageTable[i].readOnly;
    }

    for (int i = paginas; i < numPages; i++){  	//Se asignan las páginas de pila
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = MiMapa->Find();	//Busca un nueva página física
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
    }
}

AddrSpace::AddrSpace(OpenFile *executable){
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);

    if ((noffH.noffMagic != NOFFMAGIC) && (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);
    //Se define el tamaño del addres space
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;	// Necesitamos incrementar el tamaño
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;
    DEBUG('a', "Initializing address space, num pages %d, size %d\n", numPages, size);
    pageTable = new TranslationEntry[numPages];//hacemos traducción
    for (i = 0; i < numPages; i++){
        pageTable[i].virtualPage = i;	// fvirtual pag = pag fisica
        #ifdef VM //Si esta definida virtual memory
            pageTable[i].valid = false; //Se invalidan las paginas logicas
            pageTable[i].physicalPage = -1; //Se invalidan las paginas fisicas
        #else
            pageTable[i].physicalPage = MiMapa->Find();	//Busca una página física del bitmap
            pageTable[i].valid = true;
        #endif
            pageTable[i].use = false;
            pageTable[i].dirty = false;
            pageTable[i].readOnly = false;  //segmento de otra pag seteado a read-only
    }

    //copiamos codigo y datos en memoria
    #ifndef VM //en caso de que no exista virtual memory se cargan las paginas de codigo y de datos
        int codigo = noffH.code.inFileAddr;
        int datos = noffH.initData.inFileAddr;
        int paginasCodigo = divRoundUp(noffH.code.size, numPages);
        int paginasDatos = divRoundUp(noffH.initData.size, numPages);

        if (noffH.code.size > 0){
            DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", noffH.code.virtualAddr, noffH.code.size);
            for(int j = 0; j < paginasCodigo; j++){
                executable->ReadAt(&(machine->mainMemory[pageTable[j].physicalPage * 128]), PageSize, codigo);	//Se lee del segmento de código 1 página
                codigo += PageSize;	//Se aumenta de posición la página que leyó
            }
        }

        if (noffH.initData.size > 0){
            DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", noffH.initData.virtualAddr, noffH.initData.size);
            for(int j = paginasCodigo; j < paginasDatos; j++){
                executable->ReadAt(&(machine->mainMemory[pageTable[j].physicalPage * 128]), PageSize, datos);	//Se lee del segmento de datos inicializados 1 página
                datos += PageSize;	//Se aumenta de posición los datos(la página que leyó)
            }
        }
    #endif

    //Creamos el archivo de Swap y además lo abrimos
    #ifdef USE_TLB
        swapname = new char[8];
        DEBUG('B', "Nombre del currentThread: %s\n",currentThread->getName());
        sprintf(swapname,"%d.swap",currentThread->getName());
        DEBUG('C', "Despues del swapname\n");
        fileSystem->Create(swapname, numPages*PageSize);
        DEBUG('D',"Después de fileSystem->create(%s)\n", swapname);
        swap = fileSystem->Open(swapname);
    #endif

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace(){
    for(int i = 0; i< numPages; i++){
        MiMapa->Clear(pageTable[i].physicalPage);  //Se limpian todas las paginas del bitmap
    }
    delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters(){
    int i;
    for (i = 0; i < NumTotalRegs; i++)
        machine->WriteRegister(i, 0);
    machine->WriteRegister(PCReg, 0);
    machine->WriteRegister(NextPCReg, 4);
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState(){
    #ifdef VM //Si la memoria virtual esta definida
        printf("Guardando el estado del hilo: %s\n", currentThread->getName());
        //Se recorre el pageTable para poner el uso y el dirty de las paginas igual a como este en el TLB
        for(int i = 0; i < TLBSize; ++i){
            pageTable[machine->tlb[i].virtualPage].use = machine->tlb[i].use; //pageTable[x].use=TLB[x].use
            pageTable[machine->tlb[i].virtualPage].dirty = machine->tlb[i].dirty; //pageTable[x].dirty=TLB[x].dirty
        }
        machine->tlb = new TranslationEntry[TLBSize]; //crea una nueva TLB
        for (int i = 0; i < TLBSize; ++i) {
            machine->tlb[i].valid = false; //pone las paginas en falso para poder hacer pageFaultException
        }
    #endif
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState()
{
    #ifndef VM 
        machine->pageTable = pageTable;
        machine->pageTableSize = numPages;
    #else	//Se restaura el estado
        machine->tlb = new TranslationEntry[TLBSize];
        for (int i = 0; i < TLBSize; ++i){
            machine->tlb[i].valid = false;
        }
    #endif
}

//------------------------------ Métodos para la implementación de pageFaultException ----------------------------------

// Método que devuelve el pageTable de un hilo
TranslationEntry* AddrSpace::getPageTable(){
    return pageTable;
}

// Método para cargar una página al swap, recibe el virtual page number
void AddrSpace::saveToSwap(int vpn){
    int espacio = SwapMap->Find();  //Se busca un espacio en el bitmap del swap
    ASSERT( vpn >= 0 );             //Debe ser mayor o igual a 0
    if (espacio == -1 ){            //Si no hay campo en el swap
        ASSERT( false ); 
    }
    invertida[vpn]->valid = false; //Se pone el bit de validez en falso
    invertida[vpn]->physicalPage = espacio; //Se asigna el espacio encontrado a la página física
    swap->WriteAt((&machine->mainMemory[vpn*PageSize]),PageSize, espacio*PageSize); //Se escribe en el swap
    MiMapa->Clear(vpn);  //Se quita del bitmap la página que se envió
    stats->numDiskWrites++;   //Se actualiza
    printf("--> Cargar página al swap \n");
}

// Método para sacar una página del swap
void AddrSpace::getFromSwap(int vpn, int espacio){
    SwapMap->Clear(espacio);   //Se quita del bitmap el espacio que se envió
    ASSERT( espacio >=0 && espacio < SWAPSize );  //El espacio tiene que ser mayor a 0 y menor al tamaño del swap
    swap->ReadAt((&machine->mainMemory[vpn*PageSize]), PageSize, espacio * PageSize); //Se guarda en la memoria pricipal lo que hay en el swap
    stats->numPageFaults++; //Se actualizan estadisticas de pageFaults
    stats->numDiskReads++; //Se actualiza
    printf("--> Sacar página al swap\n");
}

// Se le asigna el nombre del archivo
void AddrSpace::setFileName(std::string name){
    fileName = name;
}

// Obtiene el nombre del archivo
std::string AddrSpace::getFileName(){
    return fileName;
}
