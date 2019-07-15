// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "synch.h"	//Para usar la clase Semaphore
#include "addrspace.h"


#include <fcntl.h>  
#include <unistd.h>	
#include <iostream>	//Para usar std::cin
#include <map>

#define FILE_SIZE 128

using namespace std;

struct execFiles
{
    long id;
    string name;
    Semaphore* sema;

    execFiles(){
        id = -1;
        name = "";
        sema = NULL;
    }
};

struct threadStruct
{
    int threadNum;
    Semaphore* structSem;
};

// Semaforos 
Semaphore* console = new Semaphore("console semaphore", 1);       //Se utiliza semáforo para Nachos_Write
Semaphore* semaphore_vec[FILE_SIZE];                      //Vector de semáforos

//Bitmaps
BitMap* sem_bitMap = new BitMap(FILE_SIZE);  //Bitmap de semáforos abiertos
BitMap process_bitMap(10);

execFiles** exFile = new execFiles*[128];
BitMap* execFilesMap = new BitMap(128);

map<long, threadStruct*> threadMap;



//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else youll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------

//Cambia los contadores
void returnFromSystemCall()
{
    int pc, npc;

    pc = machine->ReadRegister( PCReg );
    npc = machine->ReadRegister( NextPCReg );
    machine->WriteRegister( PrevPCReg, pc );        // PrevPC <- PC
    machine->WriteRegister( PCReg, npc );           // PC <- NextPC
    machine->WriteRegister( NextPCReg, npc + 4 );   // NextPC <- NextPC + 4
}       // returnFromSystemCall



// Metodo necesario para el exec de nachos, recibe como parametro lo que contiene el registro 4 que se ejecuta en Nachos_Exec
void Nachos_Exec_Thread(void * p)
{
    //Lee el nombre del archivo
	long memPos = (long)p;
	int* value = new int;
	char* filename = new char[40];
	
	int i = 0;
	machine->ReadMem(memPos++,1,value);
	
	while(*value != 0){
		filename[i] = (char)*value;
		++i;
		machine->ReadMem(memPos++,1,value);
	}
    
	printf("Nombre: %s\n",filename);
	OpenFile *executable = fileSystem->Open(filename);//Inicia el start process simulado para un proceso nuevo
    AddrSpace *space;

    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    
    space = new AddrSpace(executable);    
    currentThread->space = space;

    delete executable;			// close file

    space->InitRegisters();		// set the initial register values
    space->RestoreState();		// load page table register

    machine->Run();			// jump to the user progam
    ASSERT(false);
}


// Método necesario para el Fork() de NAchos.
void Nachos_Fork_Thread( void * p ){
  AddrSpace *space; //Se crea una nueva addrspace
  space = currentThread->space; //Se asigna al addrespace del hilo actual
  space->InitRegisters();             // inicializa los valores de los registros
  space->RestoreState();              // carga la tabla de páginas
  machine->WriteRegister( RetAddrReg, 4 );
  machine->WriteRegister( PCReg, (long) p );
  machine->WriteRegister( NextPCReg, (long) p + 4 );
  machine->Run(); //Se va al progama del usuario, nunca retorna
  ASSERT(false);
}


// System call 0
void Nachos_Halt()  
{
    DEBUG('a', "Shutdown, initiated by user program.\n");
    interrupt->Halt();
}       // Nachos_Halt


// System call # 1
void Nachos_Exit()
{
    int status = machine->ReadRegister(4);
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
    // Para pruebas
	// if(status == 0){
	// 	printf("Proceso Termino con %d \n", status);
	// }

	//Busca en el mapa conteniendo estructuras de semaforos y la cantidad de ellos si hay mas de un hilo o bien si este no es el ultimo
	if(threadMap.find((long)currentThread) != threadMap.end()){
		printf("Corriendo joins.\n");
    //recorre la estructura haciendole signal a todos los semaforos que se encuentren en wait
		threadStruct* d = threadMap[(long)currentThread];
		for(int i = 0; i < d->threadNum; ++i){
			d->structSem->V();
		}
    //Borra el hilo
		threadMap.erase((long)currentThread);
	}
	
	//Recorre los hilos corriendo el siguiente si hay o bien terminandolos si ya no hay mas
	Thread *nextThread;
	nextThread = scheduler->FindNextToRun();
	
	if(nextThread != NULL){
		scheduler->Run(nextThread);
	}else{
		currentThread->Finish();
	}
	interrupt->SetLevel(oldLevel);
	
	returnFromSystemCall();
}


// System call # 2
void Nachos_Exec()
{
   DEBUG('n', "Start Exec.\n");
    // Crea un hilo y una estructura para guardar en el mapa
	Thread *hilito = new Thread("new threadsito");
	threadStruct* data = new threadStruct;
	data->threadNum = 0;
	data->structSem = new Semaphore("Thread Semaphore", 0);	
	threadMap[(long)hilito] = data;

    //Ejecuta el open de la tabla de archivos basado en el id del hilo actual
	int spaceId = currentThread->open_files->Open((long)hilito);
	
	(spaceId == -1) ? printf("Fail.\n"): printf("Thread Save\n");	
	
	machine->WriteRegister( 2, spaceId );

	//Ejecuta el nuevo hilo manda como parametro la direccion del exec donde esta el nombre del archivo (parametro)
	hilito->Fork(Nachos_Exec_Thread, (void*)machine->ReadRegister(4));
	
	returnFromSystemCall();
	DEBUG('n', "End Exec.\n");
}



// System call # 3
void Nachos_Join()
{
    SpaceId id = machine->ReadRegister(4);  //Se lee el id del hilo

    // Limpiamos el bitmap de archivos ejecutables y hacemos wait, luego escribimos 0 en registro 2, sino -1
    if(execFilesMap->Test(id))
    {
        Semaphore* semaforo = new Semaphore("Join_sem", 0);
        exFile[id]->sema = semaforo;
        semaforo->P();                                      //wait
        execFilesMap->Clear(id);
        machine->WriteRegister(2,0);                        
        delete exFile[id];                                  // borro el id del vec de punteros
    }else
    {
	    machine->WriteRegister(2,-1);
    }
    returnFromSystemCall();
}



// System call # 4
void Nachos_Create()
{
    //Lee el nombre del archivo y lo crea con el creat de unix usando este nombre
    int register4 = machine->ReadRegister(4);
    char name[128] = "";

    for (int i = 0, c = 1; c != 0; i++)
    {
        machine->ReadMem(register4, 1, &c);
        name[i] = c;
        register4++;
    }
    
	int id = creat((const char*)name, O_CREAT | S_IRWXU);
    printf("Creado correctamente archivo %d....\n", id);
    close(id);
	machine->WriteRegister(2, 0);
    
    returnFromSystemCall(); 
}



// System call # 5
void Nachos_Open() 
{                    
    int register4 = machine->ReadRegister(4);
    int lect = 0;
    int index = 0;

    char name[FILE_SIZE] = {0};
    machine->ReadMem(register4, 1, &lect); // lee el registro 4 y lo guarda en lect. (el 1 significa un byte)
    
    // mientras lect no esté vacío, se coloca su coontenido en el vector name.
    while (lect != 0)
    {
        name[index] = lect;
        register4++;
        index++;
        machine->ReadMem(register4, 1, &lect);
    }
    
    /*------------------------------------------------------------------------------------------------------*/

    int unix_file = open(name, O_RDWR);     // abrir archivo de unix | O_RDWR= read & write

    /*
        si se abre correctamente, se busca espacio en tabla y se escribe en el reg 2 la pocisión,
        si no se imprime un error y el nombre de archivo
    */
    if (unix_file != -1)                   
    {
        int nachos_file = currentThread->open_files->Open(unix_file);
        if (nachos_file != -1)
        {
            machine->WriteRegister(2, nachos_file);
            printf("Se abrió correctamente %s...\n", name);
        }
        
    }else
        {
            printf("ERROR.... %s no se pudo abrir ", name);
        }
    
    
    returnFromSystemCall();

}       // Nachos_Open


// System call # 6
void Nachos_Read()
{	
    // Se leen los reg 4,5,6 para ver direccion, tamano y id del archivo
    int register4 = machine->ReadRegister(4);			
    int size = machine->ReadRegister(5);		    
    OpenFileId id = machine->ReadRegister(6);	

    int totalChar = 0;							    //Guarda cantidad de chars leidos
	char buffer[size + 1] = {0};					// buffer que guarda lo que lee
	int last_i = 0;

    switch(id)
    {
    	case ConsoleInput: // Este es para el caso de la lectura por medio del teclado

            printf("Estamos en ConsoleInput, tiene que terminar con (.): ");
        	for(int i = 0; i < size; i++, last_i = i)
        	{
				cin >> buffer[i];	//Se guarda en buffer[i] lo escrito por el usuario
				// printf("buffer[%d] = %d\n", i, buffer[i]);
				if(buffer[i] == '.')
					i = size;
			}

			buffer[last_i + 1] = '\0';
			totalChar = strlen(buffer);	//strlen: cantidad de chars ingresados por el usuario
			
			for(int index = 0; index < totalChar; index++)
			{
				machine->WriteMem(register4,1,buffer[index]);	//Almacenamiento en r4 lo escrito por el usuario
				register4 += 1;
			}
			machine->WriteRegister(2, totalChar); //Almacenando en el registro 2 el total_caracteres
        break;
		
		case ConsoleOutput:
			printf("ERROR salida estandar!!!!\n");
		break;
		
		case ConsoleError:
			printf("ERROR del error estandar!!!!\n");
		break;
		
		default:
            /*
                Si el archivo está abierto en la TAA, trata de abrir el archivo, lo escribe en registro 4 
                y en el 2 el numero de chars que se leyeron. De lo contrario escribe en registro 2; -1 nada más.
            */
			if(currentThread->open_files->isOpened(id)) 	
			{
				int open_unix_file_id = currentThread->open_files->getUnixHandle(id);	
				totalChar = read(open_unix_file_id, buffer, size);				 
				for(int index = 0; index < totalChar; index++)
				{
					machine->WriteMem(register4, 1, buffer[index]);	
					register4 += 1;
				}
				machine->WriteRegister(2, totalChar);
                printf("Archivo ID %d leído...\n", id);
			}
			else
			{
				machine->WriteRegister(2, -1);	//Se escribe -1 en el registro 2
                printf("ERROR leyendo archivo ID %d", id);
			}
		break;
    }
    returnFromSystemCall();
}


// System call # 7
void Nachos_Write() 
{    
    // char * buffer = NULL;
    int register4 = machine->ReadRegister(4);
    int size = machine->ReadRegister(5);	// Read size to write
    OpenFileId id = machine->ReadRegister( 6 );	// Read file descriptor
    char buffer[size+1] = {NULL};

    int index;
    int charsDone;
    int letra;

    machine->ReadMem(register4, 1, &letra);

    // mientras no se sobrepase el tamaño, se va guardando en el buffer 
    while (index != size)
    {
        buffer[index] = letra;
        register4++;
        index++;
        machine->ReadMem(register4, 1, &letra);
    }
    
	// Need a semaphore to synchronize access to console
	console->P();
	switch (id) {
		case  ConsoleInput:	// User could not write to standard input
			machine->WriteRegister( 2, -1 );
			break;
		case  ConsoleOutput:
			buffer[ size ] = 0;
			printf( "%s\n", buffer );
		break;
		case ConsoleError:	// This trick permits to write integers to console
			printf( "ERROR: %d\n", machine->ReadRegister( 4 ) );
			break;
		default:	// All other opened files
			// Verify if the file is opened, if not return -1 in r2
            if (!currentThread->open_files->isOpened(id))
            {
                machine->WriteRegister(2, -1);
            }else
            {
                // Get the unix handle from our table for open files
                int unixId = currentThread->open_files->getUnixHandle(id); 
                // Do the write to the already opened Unix file
                charsDone = write(unixId, buffer, size);
                // Return the number of chars written to user, via r2
                machine->WriteRegister(2, charsDone);
            }
			break;
	}
	// Update simulation stats, see details in Statistics class in machine/stats.cc
    stats->numConsoleCharsWritten += size;
	console->V();

    returnFromSystemCall();		// Update the PC registers

}       // Nachos_Write


// System call # 8
void Nachos_Close(){
    //Cierra el archivo basado en su id que esta guardado en la tabla de archivos abiertos TAA

    // printf("estamos aqui");
    OpenFileId id = machine->ReadRegister(4);                           // leer el reg 4
    int unix_fileId = currentThread->open_files->getUnixHandle(id);     // identicador de la openFilesTable
    int nachos_close = currentThread->open_files->Close(id);
    int unix_close = close(unix_fileId);
    
    //Si no cierra, imprimir ERROR
    if ((nachos_close == -1 || unix_close == -1))
    {
        printf("ERROR cerrando archivo!!!!\n");
    } else
    {
        printf("Se cerro correctamente....\n");
    }
    
    returnFromSystemCall();
}


// System call # 9
void Nachos_Fork()
{
    DEBUG( 'u', "Entering Fork System call\n" );
  // We need to create a new kernel thread to execute the user thread
  Thread * threadsito = new Thread( "execute Fork" );

  // We need to share the Open File Table structure with this new child

  threadsito->open_files = currentThread->open_files;               //Se asigna la tabla de archivos abiertos al nuevo hijo
  threadsito->open_files->addThread();                              //Se añade el nuevo hilo

  threadsito->space = new AddrSpace( currentThread->space );

  threadsito->Fork( Nachos_Fork_Thread, (void*)(long)machine->ReadRegister( 4 ) );
  currentThread->Yield();
  returnFromSystemCall();	// This adjust the PrevPC, PC, and NextPC registers

  DEBUG( 'u', "Exiting Fork System call\n" );
}


// System call # 10
void Nachos_Yield()
{
    // llamar al Yield de nachos, con current thread
    currentThread->Yield();
    returnFromSystemCall();
}


// // System call # 11
void Nachos_SemCreate()
{
    int initVal = machine->ReadRegister(4);
    Semaphore* sema = new Semaphore("semaforito", initVal);

    /*
        si se crea el semaforo correctamente, se busca un espacio en bitmap y se agrega el semaforo a la tabla
        sino escribe -1 en el registro 2
    */ 
    if (sema != NULL)
    {
        int semId = sem_bitMap->Find();
        semaphore_vec[semId] = sema;
        currentThread->open_semaphores->Open(semId);
        currentThread->open_semaphores->addThread();
        machine->WriteRegister(2, semId);
    }else
    {
        machine->WriteRegister(2, -1);
    }
    
    returnFromSystemCall();
}

// System call # 12
void Nachos_SemDestroy()
{
    int semaphore_id = machine->ReadRegister(4);

    // si existe en tabla 
    if (sem_bitMap->Test(semaphore_id) == true)
    {
        currentThread->open_semaphores->Close(semaphore_id);
        currentThread->open_semaphores->delThread();
        sem_bitMap->Clear(semaphore_id);
        machine->WriteRegister(2, 0);
        delete semaphore_vec[semaphore_id];
    }else
    {
        machine->WriteRegister(2, -1);
    }
    
    returnFromSystemCall();
}


// System call # 13
void Nachos_SemSignal()
{
    int semaphore_id = machine->ReadRegister(4);    //Se lee el id del semáforo del registro 4

    if(sem_bitMap->Test(semaphore_id) == true)      //Si el semáforo existe en la tabla
    { 
	    semaphore_vec[semaphore_id]->V();           //Se hace signal
	    machine->WriteRegister( 2, 0 );             //Se devuelve 0 en el registro 2
    }else
    {
	    machine->WriteRegister( 2, -1 );            //Se devuelve -1
    }
    returnFromSystemCall();
}


// System call # 14
void Nachos_SemWait()
{
    int semaphore_id = machine->ReadRegister(4);

    //si existe el semaforo en la tabla, se hace wait() y se escribe 0 en reg 2, si no se escribe -1
    if (sem_bitMap->Test(semaphore_id))
    {
        semaphore_vec[semaphore_id]->P();   // wait()
        machine->WriteRegister(2, 0);
    }else
    {
        machine->WriteRegister(2, -1);
    }
    returnFromSystemCall();
}




//---------------------       PageFaultException        --------------------------




// Calcula el valor del page fault de acuerdo a la dirección lógica.
unsigned int calc_valor_pag()
{
    //Se calcula el número de página faltante dividiendo la dirección lógica entre el tamaño de página
    unsigned int paginaFaltante = (machine->ReadRegister(39)) / PageSize;
    printf("Falta página( %d ) ---- Dirección( %d )\n", paginaFaltante, machine->ReadRegister(39));
    return paginaFaltante;
}


// Metodo que actualiza la TLB con entrada en el pageTable de la pag que falta, si es -1 no se puede usar la tlb(invalida)
void actualizarTLB(int entradaTLB, unsigned int paginaFaltante)
{
    //Se obtiene el pageTable del hilo actual
    TranslationEntry* pageTable = currentThread->space->getPageTable();

    //Si la entrada TLB es nula, se asignan mismos datos del pageTable al TLB(con posición actual) 
    //Sino se asignan los mismos datos del pageTable al TLB pero con la entrada de la RLB
    if ( entradaTLB == -1 )
    {
        
        machine->tlb[posTLB].physicalPage = pageTable[paginaFaltante].physicalPage;
        machine->tlb[posTLB].virtualPage =  pageTable[paginaFaltante].virtualPage;
        machine->tlb[posTLB].dirty = pageTable[paginaFaltante].dirty;
        machine->tlb[posTLB].valid = pageTable[paginaFaltante].valid;
        machine->tlb[posTLB].use = pageTable[paginaFaltante].use;
        machine->tlb[posTLB].readOnly = pageTable[paginaFaltante].readOnly;
        
        posTLB = (posTLB + 1) % TLBSize;                        //Se aumenta en 1 la posición del TLB
    }
    else
    {
        machine->tlb[entradaTLB].physicalPage = pageTable[paginaFaltante].physicalPage;
        machine->tlb[entradaTLB].virtualPage =  pageTable[paginaFaltante].virtualPage;
        machine->tlb[entradaTLB].dirty = pageTable[paginaFaltante].dirty;
        machine->tlb[entradaTLB].valid = pageTable[paginaFaltante].valid;
        machine->tlb[entradaTLB].use = pageTable[paginaFaltante].use;
        machine->tlb[entradaTLB].readOnly = pageTable[paginaFaltante].readOnly;
    }
}

// Método que actualiza pagina fisica, bit de validez, invertida y el TLB
void actualiza(int libre, unsigned int pagina, TranslationEntry* pageTable, OpenFile* exe, NoffHeader noffH, int p)
{
    pageTable[pagina].physicalPage = libre;         //Se actualiza la página física con la nueva virtual
    //Copiar en la memoria principal lo que hay en el archivo donde falta la página
    exe->ReadAt(&(machine->mainMemory[(libre * PageSize)]),PageSize, noffH.code.inFileAddr + PageSize * pagina );
    pageTable[pagina].valid = true;
    invertida[libre] = &(pageTable[pagina]);
    actualizarTLB(p, pagina);  
}

// captura la excepcion de pageFault, si no está en la tlb
void Nachos_PageFaultException()
{
    int libre;
    unsigned int pagina = calc_valor_pag();                //Para saber cual hay que "arreglar"

    TranslationEntry* pageTable = currentThread->space->getPageTable();

    //Página no es valida ni esta sucia.
    if (!pageTable[pagina].valid && !pageTable[pagina].dirty)
    {
        printf("La pagina es invalida y limpia\n");
        // printf("Nombre del archivo: %s\n", currentThread->space->getFileName().c_str());

        stats->numPageFaults++;                                        //Se actualizan las estadisticas para llevar la cuenta de los pagefaults
        OpenFile* exe = fileSystem->Open(currentThread->space->getFileName().c_str());      //Se abre el archivo del cual hace falta la página

        //Si el archivo no se puede abrir se cae el programa
        if (exe == NULL)
            ASSERT(false);

        NoffHeader noffH;
        //Se guarda en el archivo noffh lo que está en el archivo donde está la página faltante
        exe->ReadAt((char *)&noffH, sizeof(noffH), 0);
        
        unsigned int x = divRoundUp(noffH.code.size, PageSize);                     //cantidad de páginas de código del archivo    
        unsigned int z = x + divRoundUp(noffH.initData.size, PageSize);             //cantidad de páginas de código inicializados del archivo

        //Si la página faltante es mayor a 0 y ademas es menor a la cantidad de páginas de código entonces es una página de código
        if(pagina >= 0 && pagina < x)
        {
            //Se obtiene una posición libre en BitMap
            libre = MiMapa->Find();

            //Si no hay espacio en el bitmap se hace FIFO
            if(libre == -1)
            {
                //Se guarda en la posición del swap y se aumenta la posicion del swap
                int y = posSWAP;
                posSWAP = (posSWAP+1)%NumPhysPages;
                int p = -1;

                for (int i = 0; i < TLBSize; ++i)
                {
                    //Posición del tlb válida y además la página física es igual a la página física de la posición del swap
                    if (machine->tlb[i].valid && machine->tlb[i].physicalPage == invertida[y]->physicalPage)
                    {

                        machine->tlb[i].valid = false;
                        invertida[y]->use = machine->tlb[i].use;
                        invertida[y]->dirty = machine->tlb[i].dirty;
                        p = i;
                        break;
                    }
                }

                if (invertida[y]->dirty)
                {  
                    currentThread->space->saveToSwap(invertida[y]->physicalPage);   //Se guarda al Swap la página física  
                    libre = MiMapa->Find();                                         //Se busca un espacion en el bitmap
 
                    if (libre == -1)
                    {
                        printf("FRAME INVALIDO: %d\n", libre);
                        ASSERT( false );
                    }else
                    {
                        actualiza(libre, pagina, pageTable, exe, noffH, p);
                    }    
                }else
                {
                    //Se guarda la página física
                    int paginaVieja = invertida[y]->physicalPage;
                    invertida[y]->valid = false;                  
                    invertida[y]->physicalPage = -1;        //Se invalida
                    MiMapa->Clear( paginaVieja ); 
                    libre = MiMapa->Find();
                    
                    if (libre == -1)
                    {
                        printf("FRAME INVALIDO: %d\n", libre);
                        ASSERT(false);
                    }else
                    {
                        actualiza(libre, pagina, pageTable, exe, noffH, p);
                    }    
                }
            }else        //Si está libre
            {  
                DEBUG('v',"Frame libre es: %d\n", libre);
                int p = -1;
                actualiza(libre, pagina, pageTable, exe, noffH, p);
            }
        }else if(pagina >= x && pagina < z)                 // Sino es una página de datos
        {
            //Si la página faltante es mayor a la cantidad de páginas de código y es menor a la cantidad de datos Inicializados entonces es una página de datos inicializados
            libre = MiMapa->Find();
            int p = -1;
            //Si hay espacio
            if (libre != -1 )
            {
                DEBUG('a',"Frame libre es: %d\n",libre);
                actualiza(libre, pagina, pageTable, exe, noffH, p);
            }else
            {
                //Si no hay espacio se hace lo mismo que cuando no habia espacio y era una página de código
                int y = posSWAP;
                posSWAP = (posSWAP+1)%NumPhysPages;
                int p = -1;
                for (int i = 0; i < TLBSize; ++i)
                {
                    if (machine->tlb[i].valid && machine->tlb[i].physicalPage == invertida[y]->physicalPage )
                    {
                        machine->tlb[i].valid = false;
                        invertida[y]->use = machine->tlb[i].use;
                        invertida[y]->dirty = machine->tlb[i].dirty;
                        p = i;
                        break;
                    }
                }
                //Si se ha modificado
                if (invertida[y]->dirty)
                {
                    //Se envía al swap
                    currentThread->space->saveToSwap(invertida[y]->physicalPage);
                    libre = MiMapa->Find();
                    //Si no hay campo en el bitmap el programa se cae
                    if (libre == -1)
                    {
                        printf("FRAME INVALIDO: %d\n", libre);
                        ASSERT( false );
                    }
                    actualiza(libre, pagina, pageTable, exe, noffH, p);
                }else
                {   
                    int paginaVieja = invertida[y]->physicalPage;   //Si no se ha modificado, se guarda la página física 
                    MiMapa->Clear( paginaVieja );                   //Se limpia la página vieja del Bitmap     
                    invertida[y]->valid = false;                    //Se invalida la página en la TLB      
                    invertida[y]->physicalPage = -1;                //Se pone la página en invalida)                   
                    libre = MiMapa->Find();                         //Se busca una página libre en el bitmap                   

                    //Si no hay espacio en el bitmap se cae el programa
                    if (libre == -1)
                    {
                        printf("FRAME INVALIDO: %d\n", libre);
                        ASSERT(false);
                    }
                    actualiza(libre, pagina, pageTable, exe, noffH, p);
                }
            }
        }
        //Página de datos no inicializados o pila
        else if(pagina >= z && pagina < currentThread->space->numPages)
        {
            DEBUG('v',"\tDatos no Inicializados o Pila\n");
            //Se busca un espacio en el bitmap
            libre = MiMapa->Find();
            int p = -1;
            DEBUG('v',"\t\t\tBuscar una página nueva\n");
            //Si hay espacio
            if (libre != -1)
            {
                actualiza(libre, pagina, pageTable, exe, noffH, p);
            }
            else
            {
                //Se realiza lo mismo que cuando no habia espacio y era una página de código
                int y = posSWAP;
                posSWAP = (posSWAP+1)%NumPhysPages;

                int p = -1;
                for (int i = 0; i < TLBSize; ++i)
                {
                    if (machine->tlb[ i ].valid && machine->tlb[ i ].physicalPage == invertida[y]->physicalPage )
                    {
                        machine->tlb[ i ].valid = false;
                        invertida[y]->use = machine->tlb[ i ].use;
                        invertida[y]->dirty = machine->tlb[ i ].dirty;
                        p = i;
                        break;
                    }
                }
                if ( invertida[y]->dirty )
                {
                    currentThread->space->saveToSwap(invertida[y]->physicalPage);
                    libre = MiMapa->Find();

                    if ( -1 == libre)
                    {
                        printf("FRAME INVALIDO: %d\n", libre);
                        ASSERT( false );
                    }
                    actualiza(libre, pagina, pageTable, exe, noffH, p);
                }
                else
                {   
                    int paginaVieja = invertida[y]->physicalPage;       //Si no se ha modificado se guarda la página física
                    invertida[y]->valid = false;                        //Se invalida la página en la TLB
                    invertida[y]->physicalPage = -1;                    //Se pone la página invalida
                    MiMapa->Clear(paginaVieja);                         //Se limpia la página vieja del Bitmap
                    libre = MiMapa->Find();

                    if (libre == -1 )
                    {
                        ASSERT( false );
                    }
                    actualiza(libre, pagina, pageTable, exe, noffH, p);
                }
            }
        }
        else
        {
            printf("%s %d\n", "Numero de página inválido", pagina);
            ASSERT(false);
        }  
        delete exe;                                                     //Eliminar instancia del archivo
    }else if(!pageTable[pagina].valid && pageTable[pagina].dirty)       //Si la pagina no es valida y esta sucia
    {
        //Debo traer la pagina del area de SWAP
        DEBUG('v', "\tPagina invalida y sucia\n");
        DEBUG('v', "\t\tPagina física: %d, pagina virtual= %d\n", pageTable[pagina].physicalPage, pageTable[pagina].virtualPage );
        libre = MiMapa->Find();
        
        //Si hay campo en el bitmap
        if(libre != -1)
        { 
            int pagVieja = pageTable [pagina].physicalPage;         //Se guarda la página física           
            pageTable [pagina].physicalPage = libre;                //Se pone la página físcia en libre
            currentThread->space->getFromSwap(libre, pagVieja);     //Se carga de swap
            pageTable [pagina].valid = true;
            invertida[libre] = &(pageTable[pagina]);
            actualizarTLB(posTLB, pagina);
        }
        else
        {
            //swap
            int y = posSWAP;
            posSWAP = (posSWAP+1) % NumPhysPages;

            int p = -1;
            for (int i = 0; i < TLBSize; ++i)
            {
                if (machine->tlb[i].valid && machine->tlb[i].physicalPage == invertida[y]->physicalPage )
                {
                    machine->tlb[i].valid = false;
                    invertida[y]->use = machine->tlb[i].use;
                    invertida[y]->dirty = machine->tlb[i].dirty;
                    p = i;
                    break;
                }
            }
            if (invertida[y]->dirty)
            {
                //Se envía al swap
                currentThread->space->saveToSwap(invertida[y]->physicalPage);
                libre = MiMapa->Find();                //Se busca un frame libre

                if ( libre == -1 )
                {
                    printf("FRAME INVALIDO %d\n", libre);
                    ASSERT(false);
                }

                int pagVieja = pageTable [pagina].physicalPage;
                pageTable[pagina].physicalPage = libre;  
                currentThread->space->getFromSwap(libre, pagVieja);     //Se carga la pagina que DESDE el SWAP
                pageTable[pagina].valid = true;
                invertida[libre] = &(pageTable[pagina]);
                actualizarTLB(p, pagina);
            }
            else
            {
                //Se guarda la página física
                int paginaVieja = invertida[y]->physicalPage;
                invertida[y]->valid = false;
                invertida[y]->physicalPage = -1;
                MiMapa->Clear( paginaVieja );
                libre = MiMapa->Find();

                if ( libre == -1 )
                {
                    printf("No hay frames libres %d\n", libre );
                    ASSERT( false );
                }
                int pagVieja = pageTable[pagina].physicalPage;
                pageTable[pagina].physicalPage = libre;
                currentThread->space->getFromSwap(libre, pagVieja);     //Se carga DE swap
                pageTable[pagina].valid = true;
                invertida[libre] = &(pageTable[pagina]);
                actualizarTLB(p, pagina);
            }
        }
    }
    // La pagina es valida y no está sucia
    else if(pageTable[pagina].valid && !pageTable[pagina].dirty)
    {
        //Hay que actualizar el TLB, página por ser válida ya está en memoria
        machine->tlb[posTLB].virtualPage =  pageTable[pagina].virtualPage;
        machine->tlb[posTLB].physicalPage = pageTable[pagina].physicalPage;
        machine->tlb[posTLB].valid = pageTable[pagina].valid;
        machine->tlb[posTLB].use = pageTable[pagina].use;
        machine->tlb[posTLB].dirty = pageTable[pagina].dirty;
        machine->tlb[posTLB].readOnly = pageTable[pagina].readOnly;
        posTLB = (posTLB + 1) % TLBSize;
    }else            //La pagina es valida y esta sucia
    {
        machine->tlb[posTLB].virtualPage =  pageTable[pagina].virtualPage;
        machine->tlb[posTLB].physicalPage = pageTable[pagina].physicalPage;
        machine->tlb[posTLB].valid = pageTable[pagina].valid;
        machine->tlb[posTLB].use = pageTable[pagina].use;
        machine->tlb[posTLB].dirty = pageTable[pagina].dirty;
        machine->tlb[posTLB].readOnly = pageTable[pagina].readOnly;
        posTLB = (posTLB + 1) % TLBSize;
    }
}



/*----------------------------------TERMINAN SYSCALLS, EMPIEZA SWITCH-----------------------------------------*/


void ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    switch ( which )
    {

     case PageFaultException:
        printf("----------- PageFaultException -----------\n");
        Nachos_PageFaultException();                // Metodo de excepcion de page fault
        break;

    case SyscallException:
        switch ( type )
        {
        case SC_Halt:
            Nachos_Halt();	            // System call # 0
            break;
        case SC_Exit:                   // System call # 1
            Nachos_Exit();
            break;
        case SC_Exec:                   // System call # 2
            Nachos_Exec();
            break;
        case SC_Join:                   //System call # 3
            Nachos_Join();
            break;
        case SC_Create:
            Nachos_Create();            // System call # 4
            break;
        case SC_Open:
            Nachos_Open();	            // System call # 5
            break;
        case SC_Read:
            Nachos_Read();	            // System call # 6
            break;
        case SC_Write:
            Nachos_Write();	            // System call # 7
            break;
        case SC_Close:
            Nachos_Write();             // System call # 8
            break;
        case SC_Fork:
            Nachos_Fork();	            // System call # 9
            break;
        case SC_Yield:
            Nachos_Yield();             //System call # 10
            break;
        case SC_SemCreate:
            Nachos_SemCreate();         //System call # 11
            break;
        case SC_SemDestroy:
            Nachos_SemDestroy();        //System call # 12
            break;
        case SC_SemSignal:
            Nachos_SemSignal();         //System call # 13
            break;
        case SC_SemWait:
            Nachos_SemWait();           //System call # 14
            break;
        default:
            printf("Unexpected syscall exception %d\n", type );
            ASSERT(false);
            break;
        }
        break;

   
    default:
        printf( "Unexpected exception %d\n", which );
        ASSERT(false);
        break;
    }
}
