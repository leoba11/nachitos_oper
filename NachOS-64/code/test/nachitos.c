#include "syscall.h"

main()
{
    OpenFileId f1;


    f1  = Open( "nachos.1" );
    Close( f1 );

    Exit( 0 );
}