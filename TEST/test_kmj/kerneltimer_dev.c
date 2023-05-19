#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>

#include <linux/fs.h>          
#include <linux/errno.h>       
#include <linux/types.h>       
#include <linux/fcntl.h>       
#include <linux/gpio.h>
#include "ioctl_test.h"
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/slab.h>

// Using timer
#include <linux/timer.h>
#include <linux/time.h>

#define   KERNELTIMER_DEV_NAME            "kerneltimerdev"
#define   KERNELTIMER_DEV_MAJOR            240      
#define DEBUG 1
#define IMX_GPIO_NR(bank, nr)       (((bank) - 1) * 32 + (nr))
void kerneltimer_timeover(unsigned long arg);

DECLARE_WAIT_QUEUE_HEAD(WaitQueue_Read);
int irq_init(void);
int sw_irq[8];
char sw_no;
typedef struct
{
	struct timer_list timer;
	unsigned long led;
	int time_val;
} __attribute__ ((packed)) KERNEL_TIMER_MANAGER;

static int led[] = {
	IMX_GPIO_NR(1, 16),   //16
	IMX_GPIO_NR(1, 17),	  //17
	IMX_GPIO_NR(1, 18),   //18
	IMX_GPIO_NR(1, 19),   //19
};

static int key[] = {
	IMX_GPIO_NR(1, 20),
	IMX_GPIO_NR(1, 21),
	IMX_GPIO_NR(4, 8),
	IMX_GPIO_NR(4, 9),
	IMX_GPIO_NR(4, 5),
	IMX_GPIO_NR(7, 13),
	IMX_GPIO_NR(1, 7),
	IMX_GPIO_NR(1, 8),
};

static int kerneltimerdev_request(void)
{
	int ret = 0;
	int i;
	for (i = 0; i < ARRAY_SIZE(led); i++) {
		ret = gpio_request(led[i], "gpio led");
		if(ret<0){
			printk("#### FAILED Request gpio %d. error : %d \n", led[i], ret);
			break;
		}
		gpio_direction_output(led[i], 0);
	}
	for (i = 0; i < ARRAY_SIZE(key); i++) {
		sw_irq[i] = gpio_to_irq(key[i]);
		if(ret<0){
			printk("#### FAILED Request gpio %d. error : %d \n", key[i], ret);
			break;
		}
	}
	return ret;
}

static void kerneltimerdev_free(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(led); i++){
		gpio_free(led[i]);
	}
	for (i = 0; i < ARRAY_SIZE(key); i++){
		free_irq(sw_irq[i],NULL);
	}
}

static void led_write(unsigned char data)
{
	int i;
	for(i = 0; i < ARRAY_SIZE(led); i++){
		gpio_set_value(led[i], (data >> i ) & 0x01);
	}
#if DEBUG
	printk("#### %s, data = %d\n", __FUNCTION__, data);
#endif
}
void kerneltimer_registertimer(KERNEL_TIMER_MANAGER *pdata, unsigned long timeover)
{
	init_timer( &(pdata->timer) );
	pdata->timer.expires = get_jiffies_64() + timeover;  //10ms *100 = 1sec
	pdata->timer.data    = (unsigned long)pdata ;
	pdata->timer.function = kerneltimer_timeover;
	add_timer( &(pdata->timer) );
}
void kerneltimer_timeover(unsigned long arg)
{
	KERNEL_TIMER_MANAGER* pdata = NULL;
	if( arg )
	{
		pdata = ( KERNEL_TIMER_MANAGER *)arg;
		led_write(pdata->led & 0x0f);
#if DEBUG
		printk("led : %#04x\n",(unsigned int)(pdata->led & 0xff));
#endif
		pdata->led = ~(pdata->led);
		kerneltimer_registertimer( pdata, (unsigned long)(pdata->time_val));
	}
}
void kerneltimer_start(struct file *filp)
{
	KERNEL_TIMER_MANAGER *ptrmng =( KERNEL_TIMER_MANAGER*) filp->private_data;
	kerneltimer_registertimer( ptrmng, (unsigned long)(ptrmng->time_val));
	//	kerneltimer_registertimer( ptrmng);
	return;
}

void kerneltimer_stop(struct file *filp)
{
	KERNEL_TIMER_MANAGER *ptrmng = (KERNEL_TIMER_MANAGER* )filp->private_data;
	if(timer_pending(&(ptrmng->timer)))
		del_timer(&(ptrmng->timer));
}

static int kerneltimerdev_open (struct inode *inode, struct file *filp)
{
	KERNEL_TIMER_MANAGER * pKernelTimer;
	int num0 = MAJOR(inode->i_rdev); 
	int num1 = MINOR(inode->i_rdev); 

	pKernelTimer = (KERNEL_TIMER_MANAGER*)kmalloc(sizeof(KERNEL_TIMER_MANAGER), GFP_KERNEL);
	printk( "call open -> major : %d\n", num0 );
	printk( "call open -> minor : %d\n", num1 );

	memset(pKernelTimer, 0x00, sizeof(KERNEL_TIMER_MANAGER));
	filp->private_data = (void *)pKernelTimer;
	irq_init();

	return 0;
}

static ssize_t kerneltimerdev_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	int ret;

	if(!(filp->f_flags & O_NONBLOCK))
	{
		if(sw_no == 0)
			interruptible_sleep_on(&WaitQueue_Read);
	}

	ret=copy_to_user(buf, &sw_no, count);
	sw_no = 0;
	if(ret < 0)
		return -ENOMEM;
	return count;
}

static ssize_t kerneltimerdev_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	KERNEL_TIMER_MANAGER * pdata =(KERNEL_TIMER_MANAGER*)filp->private_data;
	int ret;
	char kbuf;
	ret = copy_from_user(&kbuf,buf,count);
	if(ret < 0)
		return -ENOMEM;
	else
		pdata->led =kbuf;
	return count;
}

static long kerneltimerdev_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	keyled_data ctrl_info= {0};
	int err,size;
	KERNEL_TIMER_MANAGER * ptrmng =(KERNEL_TIMER_MANAGER*)filp->private_data;

	if( _IOC_TYPE( cmd ) != IOCTLTEST_MAGIC ) return -EINVAL;
	if( _IOC_NR( cmd ) >= IOCTLTEST_MAXNR ) return -EINVAL;

	size = _IOC_SIZE( cmd );
	if( size ) //size = 4
	{
		err = 0;
		if( _IOC_DIR( cmd ) & _IOC_READ ) //메모리를 사용가능한지 확인
			err = access_ok( VERIFY_WRITE, (void *) arg, size);
		if( _IOC_DIR( cmd ) & _IOC_WRITE )
			err = access_ok( VERIFY_READ , (void *) arg, size);
		if( !err ) return err;
	}
	switch( cmd )
	{
		case TIMER_START :
			if(!timer_pending(&(ptrmng->timer))) //타이머가 중지 중인지 확인
				kerneltimer_start(filp); //중지 중이면 start실행
			break;
		case TIMER_STOP :
			kerneltimer_stop(filp);
			break;
		case TIMER_VALUE :
			err=copy_from_user((void*)&ctrl_info, (void *)arg, sizeof(keyled_data)); //sizeof(ctrl_info)
			ptrmng->time_val = ctrl_info.timer_val;
			break;

		default:
			err =-E2BIG;
			break;
	}	
	return err;
}
static unsigned int kerneltimerdev_poll (struct file *filp, struct poll_table_struct * wait)
{
	unsigned int mask = 0;
	printk("_key : %ld \n", (wait->_key & POLLIN));
	if(wait->_key & POLLIN)
		poll_wait(filp, &WaitQueue_Read, wait);
	if(sw_no > 0)
		mask = POLLIN;
	return mask;
}

static int kerneltimerdev_release (struct inode *inode, struct file *filp)
{
	printk( "call release \n" );
	kerneltimerdev_free();
	if(filp->private_data)
		kfree(filp->private_data);

	return 0;
}

struct file_operations kerneltimerdev_fops =
{
	.owner    = THIS_MODULE,
	.open     = kerneltimerdev_open,     
	.read     = kerneltimerdev_read,     
	.write    = kerneltimerdev_write,    
	.unlocked_ioctl = kerneltimerdev_ioctl,
	.poll = kerneltimerdev_poll,
	.release  = kerneltimerdev_release,  
};

irqreturn_t sw_isr(int irq, void *unuse)
{
	int i;
	for(i=0;i<ARRAY_SIZE(key);i++)
	{
		if(irq == sw_irq[i])
		{
			sw_no = i+1;
			break;
		}
	}
	printk("IRQ : %d, sw_no : %d\n",irq, sw_no);
	wake_up_interruptible(&WaitQueue_Read);
	return IRQ_HANDLED;
}


int irq_init(void)
{
	int i;
	char * sw_name[8] = {"key1","key2","key3","key4","key5","key6","key7","key8"};
	int result = kerneltimerdev_request();
	if(result < 0)
	{
		return result;     /* Device or resource busy */
	}
	for(i=0;i<ARRAY_SIZE(key);i++)
	{
		result = request_irq(sw_irq[i],sw_isr,IRQF_TRIGGER_RISING,sw_name[i],NULL);
		if(result)
		{
			printk("#### FAILED Request irq %d. error : %d \n", sw_irq[i], result);
			break;
		}
	}
	return result;
}

static int kerneltimerdev_init(void)
{
	int result = 0;

	printk( "call kerneltimerdev_init \n" );    

	result = register_chrdev( KERNELTIMER_DEV_MAJOR, KERNELTIMER_DEV_NAME, &kerneltimerdev_fops);
	if (result < 0) return result;

	return result;
}

static void kerneltimerdev_exit(void)
{
	printk( "call kerneltimerdev_exit \n" );    
	unregister_chrdev( KERNELTIMER_DEV_MAJOR, KERNELTIMER_DEV_NAME );
}

module_init(kerneltimerdev_init);
module_exit(kerneltimerdev_exit);

MODULE_AUTHOR("KMJ");
MODULE_DESCRIPTION("test module");
MODULE_LICENSE("Dual BSD/GPL");
