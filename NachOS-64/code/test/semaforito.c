int main(){
	int fd;
	int x = SemCreate(5);	
	Create("brbr.txt");
	fd = Open("brbr.txt");
	Write("wait", 4, fd);	
	SemWait(x);
	SemSignal(x);
	Write("signal", 6, fd);
	Close(fd);


	SemDestroy(x);
	Halt();
} 