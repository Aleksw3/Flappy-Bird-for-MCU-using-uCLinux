#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/err.h> //includes IS_ERR() macro
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <asm/signal.h>
#include <asm/siginfo.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>

#include "efm32gg.h"

static dev_t dev_num; //Will contain the device number of the driver
static struct cdev c_dev; //represents the char device within the kernel
static struct class *cl;
static struct device *dev_ret;
static struct fasync_struct* async_queue;
static uint32_t gamepad_status = 0;

/* struct cdev {  // This is defined in one of the libraries. This is just for understanding and documentation
    struct kobject kobj; 
    struct module *owner; 
    const struct file_operations *ops; 
    struct list_head list; 
    dev_t dev; 
    unsigned int count; 
};
*/


static int __init gamepad_init(void);
static void __exit gamepad_cleanup(void);
// Open, release, and read are the minimum to open the driver
static int gamepad_open(struct inode*, struct file*);
static int gamepad_release(struct inode*, struct file*);
static ssize_t gamepad_read(struct file*, char __user*, size_t, loff_t*);
// static ssize_t gamepad_write(struct file, const char __user, size_t, loff_t);
static irqreturn_t gamepad_interrupt_handler(int, void*, struct pt_regs*);
static int gamepad_fasync(int, struct file*, int);

struct file_operations gamepad_fops = { //implements functionality of the driver
	.owner   = THIS_MODULE,	
	.open    = gamepad_open,
	.release = gamepad_release, 
	.read    = gamepad_read,
	.fasync  = gamepad_fasync,
};

static int __init gamepad_init(void){
	int ret = alloc_chrdev_region(&dev_num,0, 1, "gamepad"); //create driver file
	if(ret < 0){ //ret is further used to check return code
		printk(KERN_ALERT "Failed to allocate device\n"); 
		return ret;
	}

	//request memory locations used for gpio bank PC
	request_mem_region((resource_size_t)GPIO_PC_BASE,(resource_size_t)0x20,"GPIO_PC_REGISTERS");

	//request memory locations for interrupt registers
	request_mem_region((resource_size_t)GPIO_PA_BASE+0x100,(resource_size_t)0x1C,"GPIO_PC_REGISTERS");

	//Request interrupt channel so others cannot use it. 
	//Should release when done. Similar concept as the reqeust_mem_region
	ret = request_irq((resource_size_t)GPIO_IRQ_ODD, (irq_handler_t)gamepad_interrupt_handler, IRQF_SHARED, "gamepad", &c_dev);
	if(ret<0){
		unregister_chrdev_region(dev_num, 1);
	}
	ret = request_irq((resource_size_t)GPIO_IRQ_EVEN, (irq_handler_t)gamepad_interrupt_handler, IRQF_SHARED, "gamepad", &c_dev);
	if(ret<0){
		unregister_chrdev_region(dev_num, 1);
	}

	iowrite32(0x33333333, GPIO_PC_MODEL); // Set drive-mode of GPIO pins to high
	iowrite32(0xFF      , GPIO_PC_DOUT); //Enable pull-up

	iowrite32(0x22222222, GPIO_EXTIPSELL); //Connect interrupts to pins on GPIO bank PC
	iowrite32(0xFF      , GPIO_EXTIFALL); //Trigger on falling edge, pins are active low
	iowrite32(0xFF      , GPIO_IEN); //Enable interrupts on GPIO



	cdev_init(&c_dev,&gamepad_fops);
	ret = cdev_add(&c_dev, dev_num, 1); // Adds the device to the system. The system can now call the drives functions
	if(ret<0){ //Returns negative error code on error
		free_irq (GPIO_IRQ_EVEN, &c_dev);
		free_irq (GPIO_IRQ_ODD,  &c_dev);
		unregister_chrdev_region(dev_num, 1);
		return ret;
	}

	cl = class_create(THIS_MODULE, "gamepad"); //creates struct class pointer for device_create()
	if(IS_ERR(cl)){ //Checks if there is an error in the creation of the struct class
		cdev_del(&c_dev);
		free_irq (GPIO_IRQ_EVEN, &c_dev);
		free_irq (GPIO_IRQ_ODD, &c_dev);
		unregister_chrdev_region(dev_num,1);
		return PTR_ERR(cl);
	}
	dev_ret = device_create(cl, NULL, dev_num, NULL, "gamepad"); //Creates file /dev/gamepad for the driver/exposes it to the userspace
	if(IS_ERR(dev_ret)){ //Checks if the file creation was successfull
		class_destroy(cl);
		cdev_del(&c_dev);
		free_irq (GPIO_IRQ_EVEN, &c_dev);
		free_irq (GPIO_IRQ_ODD, &c_dev);
		unregister_chrdev_region(dev_num,1);
		return PTR_ERR(dev_ret);
	}
	//see comments at declaration for definition of cdev struct

	return 0;
}

static void __exit gamepad_cleanup(void){
	//Undo everything we did in init in reverse order
	device_destroy(cl, dev_num);
	class_destroy(cl);
	cdev_del(&c_dev);

	free_irq (GPIO_IRQ_EVEN, &c_dev);
	free_irq (GPIO_IRQ_ODD, &c_dev);
	

	release_mem_region((resource_size_t)GPIO_PA_BASE+0x100,(resource_size_t)0x1C);
	release_mem_region((resource_size_t)GPIO_PC_BASE,(resource_size_t)0x20);

	unregister_chrdev_region(dev_num, 1);
}


static int gamepad_open(struct inode *inode, struct file *filp){
	return 0;
}
static int gamepad_release(struct inode *inode, struct file *filp){
	return 0;
}

static ssize_t gamepad_read(struct file *filp, char __user *buffer, size_t len, loff_t *offp){
	copy_to_user(buffer,&gamepad_status,1);
	return 0;
}


/*
Two functions for the interrupt: the handler and fasync
the handler is called when an interrupt is triggered and signals
the process if new data is available. 
Fasync is called to add or remove entries from a queue. Saying that an
event has occurred(?)
*/
static irqreturn_t gamepad_interrupt_handler(int irq_no, void *dev_id, struct pt_regs* regs){
	gamepad_status=~(ioread32(GPIO_PC_DIN)&0xFF);//read DIN
	iowrite32(ioread32(GPIO_IF),GPIO_IFC);
	if(async_queue)
	{
		kill_fasync(&async_queue, SIGIO,POLL_IN); //Signals the process that new data has arrived
	}
	return IRQ_HANDLED;
}

static int gamepad_fasync(int fd, struct file* filp, int mode){
	return fasync_helper(fd,filp,mode,&async_queue); // Adds or removes entries from the queue	
}





module_init(gamepad_init);
module_exit(gamepad_cleanup);

MODULE_DESCRIPTION("Gamepad Driver\n");
MODULE_LICENSE("GPL");
