#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#define out "out"
#define in "in"
static int device_open(struct inode *,struct file *);
static ssize_t device_write(struct file *,const char *, size_t, loff_t *);
static ssize_t device_read(struct file *,char *, size_t, loff_t *);
static int device_release(struct inode *inode, struct file *file);
static int gpio_label_get(void);
static int gpio_initi(void);
dev_t dev = 0;
static struct class *dev_class;
static struct cdev etx_cdev;
static const char gpio_label_c[28][10] = {{"ID_SDA"},{"ID_SCL"},{"SDA1"},{"SCL1"},{"GPIO_GCLK"},{"GPIO5"},{"GPIO6"},
{"SPI_CE1_N"},{"SPI_CE0_N"},{"SPI_MISO"},{"SPI_SCLK"},{"GPIO12"},{"GPIO12"},{"TXD1"},{"RXD1"},
{"GPIO16"},{"GPIO17"},{"GPIO18"},{"GPIO19"},{"GPIO20"},{"GPIO21"},{"GPIO22"},{"GPIO23"},
{"GPIO24"},{"GPIO25"},{"GPIO26"},{"GPIO27"}};


static struct file_operations fops={
	.owner=THIS_MODULE,
	.write=device_write,
	.read=device_read,
	.open=device_open,
	.release=device_release,
};

static struct gpio_data{

	int gpio_pin;
	int gpio_val;
	char dir[4];
	char gpio_nm[10];
};

static struct gpio_data un1;

static int val_check(char* str){
	int val = (int)str[0];
	switch(val){
		case 48:
			return 0;
			break;
		case 49:
			return 1;
			break;
		default:
			return -1;
	}

}


static int kern_atoi(char* str){
    int i;
    int result;
    for(i =0;str[i] != '\0';i++){
        if(i == 0){
            result = (str[0]-0x30);
        }else{
            result =result*10;
            result = result + (str[i]-0x30);
        }
    }
    return result;

}

static int gpio_label_get(){
	int num = un1.gpio_pin;
	if(strncpy(un1.gpio_nm,gpio_label_c[num],10)!=un1.gpio_nm){
		printk(KERN_ALERT "gpio Label not set");
		return -1;
	}
	return 0;
}

static int gpio_initi(){
	if(gpio_is_valid(un1.gpio_pin)==0){
		printk(KERN_ALERT " GPIO pin not available\n");
		return -1;
	}
	if(gpio_request(un1.gpio_pin,un1.gpio_nm)<0){
		printk(KERN_ALERT "GPIO pin cannot be requested at the moment\n");
		gpio_free(un1.gpio_pin);
		return -2;
	}
	if(gpio_export(un1.gpio_pin,true)!=0){
		printk(KERN_ALERT "Export of gpio failed\n");
		return -3;
		gpio_free(un1.gpio_pin);
	}
	if(strncmp(un1.dir,"in",4)==0){
		if(gpio_direction_input(un1.gpio_pin)!=0){
			printk(KERN_ALERT "Direction set as input failed\n");
			gpio_free(un1.gpio_pin);
			return -1;}
	}
	if(strncmp(un1.dir,"out",4)==0){
		if(gpio_direction_output(un1.gpio_pin,un1.gpio_val)!=0){
			printk(KERN_ALERT "Direction set as output failed\n");
			gpio_free(un1.gpio_pin);
			return -1;}
	}
	printk(KERN_INFO "gpio pin %d set as %s with value as %d \n",un1.gpio_pin,un1.dir,un1.gpio_val);
	return 0;
}

static int device_open(struct inode *inode,struct file *file){
	printk(KERN_INFO "device opened");
	return 0;
}

static int device_release(struct inode *inode,struct file *file){
	printk(KERN_INFO "device closed");
	return 0;
}

static ssize_t device_read(struct file* filp,char __user *buffer, size_t len, loff_t * offset)
{
	if(un1.gpio_pin==NULL){
		printk(KERN_ALERT "no gpio selected,write to device first\n");
		return -1;
	}
	uint8_t gpio_state = 0;

	len = 1;
	if(copy_to_user(buffer,&gpio_state, len) > 0){
	printk(KERN_ALERT " Not all the bytes have been copied to user\n");
	return -1;
	}
	printk("read function : %s = %d \n",un1.gpio_nm,gpio_state);
	return 0;
}


static ssize_t device_write(struct file* filp,const char __user *buffer,
			size_t len,loff_t *off){

	char *p1,*p2,*p3,*p4;
	char rec_buf[10]; 
	if(copy_from_user(rec_buf,buffer,len)>0){
		printk(KERN_ALERT "Not all bytes have been copied from user\n");
		return -1;
	}
	int leng = sizeof(rec_buf)/sizeof(rec_buf[0]);
	for(int i=0;i<leng;i++){
		if(rec_buf[i]==' '){
			rec_buf[i]='\0';
			p1=rec_buf;
			p2=(rec_buf+i+1);
			break;
		}
	}

	for(int i=0;i<leng;i++){
		if(p2[i]==' '){
			p2[i]='\0';
			p3=p2;
			p4=(p2+i+1);
			break;
		}
	}
	printk(KERN_INFO"GPIO selected is %d the dir is %s and val is %d",kern_atoi(p1),p3,val_check(p4));
	if(kern_atoi(p1)>27){
		printk(KERN_ALERT "GPIO pin out of scope\n");
		return -2;
	}
	if((val_check(p4)==-1)||((strncmp(p3,out,4)!=0)&&(strncmp(p3,in,4)!=0))){
		printk(KERN_ALERT "Invalid command\n");
		return -3;
	}

	strncpy(un1.dir,p3,4);
	un1.gpio_pin=kern_atoi(p1);
	un1.gpio_val=val_check(p4);
	if(gpio_label_get() != 0){
		printk(KERN_ALERT "GPIO Label not obtained\n");
		return -3;
	}
	volatile int tempo = gpio_initi();
	if(tempo != 0){
		printk(KERN_ALERT "GPIO PIN not accessable\n");
		return -3;
	}
	
	return 0;

}


static int __init hello(void){
	if((alloc_chrdev_region(&dev,0,1,"etx_Dev"))<0){
	pr_err("cannot allocate major number\n");
	goto r_unreg;
	}
	pr_info("Major = %d minor = %d \n");
	
	cdev_init(&etx_cdev,&fops);

	if((cdev_add(&etx_cdev,dev,1)) < 0){
		pr_err("Cannot add the device to the system\n");
		goto r_del;
	}


	if(IS_ERR(dev_class = class_create(THIS_MODULE,"etx_class"))){
		pr_err("Cannot create the struct class\n");
		goto r_class;

	}


	if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"etx_device"))){
	pr_err("Cannot create the Device \n");
	goto r_device;
	}
	
	pr_info("Device Driver Insert...Done\n");
	return 0;
	
	r_device:
	 device_destroy(dev_class,dev);
	r_class:
	 class_destroy(dev_class);
	r_del:
	 cdev_del(&etx_cdev);
	r_unreg:
	 unregister_chrdev_region(dev,1);
	return -1;

}


static void __exit bye(void){
	gpio_unexport(un1.gpio_pin);
	gpio_free(un1.gpio_pin);
	device_destroy(dev_class,dev);
	class_destroy(dev_class);
	cdev_del(&etx_cdev);
	unregister_chrdev_region(dev,1);
	printk(KERN_INFO "Module has been deloaded sucessfully");
}

module_init(hello);
module_exit(bye);

MODULE_AUTHOR("Aniruddh");
MODULE_LICENSE("GPL");




