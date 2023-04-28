#include <autoconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/unistd.h>
#define LEFT 0
#define RIGHT 1
void print_led(unsigned long led);
int main(int argc,char *argv[])
{
	int i;
	unsigned long led;
	unsigned long led_shift;
  	if(argc > 2)
	{
		printf("Usage : %s [led_data(0x0~0xf)]\n",argv[0]);
		return 1;
	}
	puts("1:2:3:4");
	if(argc == 1)
	{
		int j=0;
		int dir=0;
		led_shift=0x01;
		for(i=0;i<100;i++)
		{
			for(j=0;j<5;j++)
			{
				print_led(led_shift);
				if(dir == LEFT)
					led_shift <<=  1;   //led_shift = led_shift << 1;
				else
					led_shift >>=  1;
//				sleep(1);
  				usleep(100000); //0.1Sec
			}					
			if(dir == LEFT)
			{
				dir = RIGHT;
				led_shift=0x08;
			}
			else
			{
				dir = LEFT;
				led_shift=0x01;
			}
		}
	}
	else //if(argc == 2)
	{
		led = strtoul(argv[1],NULL,16);
  		if(led < 0 || 15 < led)
		{
			printf("Usage : %s [led_data(0x0~0xf)]\n",argv[0]);
			return 2;
		}
		print_led(led);
	}
	return 0;
}
void print_led(unsigned long led)
{
	int i;
  	unsigned long ret = (unsigned long)syscall(__NR_mysyscall,led);
	if(ret < 0)
    {
    	perror("syscall");
    	exit(3);
    }
	for(i=0;i<=3;i++)
	{
		if(ret & (0x01 << i))
			putchar('O');
		else
			putchar('X');
		if(i < 3 )
			putchar(':');
		else
			putchar('\n');
	}
	return;
}
