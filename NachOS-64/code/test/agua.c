//  agua.cc 
//	Test for user semaphore operations
//
//---------------------------------------------------------------------

int sH, sO;
int cH, cO;

void H() {

	if(cH > 0 && cO > 0) {
		cH--;
		cO--;
		Write("H haciendo agua!!\n", 17, 1);
		SemSignal( sH );
		SemSignal( sO );
	}
	else {
		cH++;
		Write("H esperando ...!!\n", 17, 1);
		SemWait( sH );
	}
}

void O() {

	if( cH > 1) {
		cH -= 2;
		Write("O haciendo agua!!\n", 17, 1);
		SemSignal( sH ); 
		SemSignal( sH ); 
	}
	else {
		cO++;
		Write("O esperando ...!!\n", 17, 1);
		SemWait( sO );
	}
}

int main() {

    int i;
    cH = cO = 0;
    sO = SemCreate( 0 );	// Creates the semaphores
    sH = SemCreate( 0 );

    for (i=0; i<30 ; i++) {
        if ( (i % 2) == 0 )
           Fork( H );
        else
           Fork( O );
        Yield();
    }

    SemDestroy( sO );
    SemDestroy( sH );

    return 0;
}