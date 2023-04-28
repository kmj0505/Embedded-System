#include <autoconf.h>
#include <stdio.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <stdlib.h>
#pragma GCC diagnostic ignored "-Wunused-result"
int main(int argc, char* argv[])
{
	long val;
	int i;
	if(argc < 2)
	{
		printf("Usage : %s led_val\n",argv[0]);
		return 1;
	}
	val = atoi(argv[1]);
  	val = syscall(__NR_mysyscall,val);
	if(val<0)
	{
		perror("syscall");
		return 2;
	}
	printf("mysyscall return value = %ld\n",val);
	puts("1:2:3:4");
	for(i=0;i<4;i++)
    {
        if(val & (0x1<<i))
            putchar('O');
        else
            putchar('X');
        if(i != 3)
            putchar(':');
        else
            putchar('\n');
    }
	return 0;
}
