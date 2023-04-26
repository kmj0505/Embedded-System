#include <common.h>
#include <command.h>
#include <asm/io.h>

#define IOMUXC_SW_MUX_CTL_PAD_SD1_DATA0 0X020E0340 //GPIO1_IO16
#define IOMUXC_SW_MUX_CTL_PAD_SD1_DATA1 0X020E033C //GPIO1_IO17
#define IOMUXC_SW_MUX_CTL_PAD_SD1_CMD   0X020E0348 //GPIO1_IO18
#define IOMUXC_SW_MUX_CTL_PAD_SD1_DATA2 0X020E034C //GPIO1_IO19
#define GPIO1_DR	0x0209C000
#define GPIO1_GDIR	0x0209C004
#define GPIO1_PSR	0x0209C008

void led_init(void)
{
	unsigned long temp;
	temp = 0x05;		//Select signal GPIO
	writel(temp, IOMUXC_SW_MUX_CTL_PAD_SD1_DATA0);	//GPIO1_IO16
	writel(temp, IOMUXC_SW_MUX_CTL_PAD_SD1_DATA1);	//GPIO1_IO17
	writel(temp, IOMUXC_SW_MUX_CTL_PAD_SD1_CMD);	//GPIO1_IO18
	writel(temp, IOMUXC_SW_MUX_CTL_PAD_SD1_DATA2);	//GPIO1_IO19

	temp = 0x0f << 16;		//GPIO1_IO16 ~ IO19 Direction : Output

	temp = (readl(GPIO1_GDIR) & ~temp) | temp;   	//temp = readl(GPIO1_GDIR) | temp;
	writel(temp, GPIO1_GDIR);

	temp = ~(0x0f << 16);	//LED1~4 : Off
	temp = readl(GPIO1_DR) & temp;
	writel(temp, GPIO1_DR);
}

void led_write(unsigned long led_data)
{
	led_data = led_data << 16;
	writel(led_data, GPIO1_DR);
}

void led_read(unsigned long * led_data)
{

	*led_data = (readl(GPIO1_DR) >> 16) & 0x0f;
}

void led_status(unsigned long led_data)
{
	int i;
	puts("0:1:2:3\n");
	for(i = 0; i<4; i++)
	{
		if(led_data & (0x01 << i))
			putc('O');
		else
			putc('X');
		if(i != 3)
			putc(':');
		else 
			putc('\n');
	}
}
static int do_KCCI_LED(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned long led_data;
	if(argc != 2)
	{
		cmd_usage(cmdtp);
		return 1;
	}
	printf("*LED TEST START(작성자:KMJ)\n");
	led_init();
	led_data = simple_strtoul(argv[1], NULL, 16);
	led_write(led_data);
	led_read(&led_data);
	led_status(led_data);
	printf("*LED TEST END(%s : %#04x)\n\n ", argv[0],(unsigned int)led_data);

	return 0;
}
U_BOOT_CMD(
	led,2,0,do_KCCI_LED,
	"led - kcci LED TEST.",
	"number - Input argument is only one.(led [0x00~0x0f])\n");
