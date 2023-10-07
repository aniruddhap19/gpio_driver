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
#define device_name "driver_test"

static int device_open(struct inode *,struct file *);
static ssize_t device_write(struct file *,const char *, size_t, loff_t *);
static ssize_t device_read(struct file *,char *, size_t, loff_t *);
static int device_release(struct inode *inode, struct file *file);
static int gpio_label_get(void);
static int gpio_initi(void);
static int major;
dev_t dev = 0;
static struct class *dev_class;
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

static int kern_atoi(char* arr){
	int index=0;
	int arr1[3];
	int sum=0;
	while(*(arr+index)!='\0'){
		index++;
	}
	for(int i=0;i<index;i++){
		arr1[i]=(int)arr[i]-48;
	}
	int c=(index-1);
	for(int i=0;i<index;i++){
		int temp=arr1[i];
		for(int j=c;j>0;j--){
			temp=temp*10;
		}
		sum=sum+temp;
		c--;
	}
	return sum;
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
		printk(KERN_ALERT " GPIO pin not available");
		return -1;
	}
	if(gpio_request(un1.gpio_pin,un1.gpio_nm)<0){
		printk(KERN_ALERT "GPIO pin cannot be requested at the moment");
		gpio_free(un1.gpio_pin);
		return -2;
	}
	if(gpio_export(un1.gpio_pin,true)!=0){
		printk(KERN_ALERT "Export of gpio failed");
		return -3;
		gpio_free(un1.gpio_pin);
	}
	if(strncmp(un1.dir,"in",4)==0){
		if(gpio_direction_input(un1.gpio_pin)!=0){
			printk(KERN_ALERT "Direction set as input failed");
			gpio_free(un1.gpio_pin);
			return -1;}
	}
	if(strncmp(un1.dir,"out",4)==0){
		if(gpio_direction_output(un1.gpio_pin,un1.gpio_val)!=0){
			printk(KERN_ALERT "Direction set as output failed");
			gpio_free(un1.gpio_pin);
			return -1;}
	}
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
	if(kern_atoi(p1)>27){
		printk(KERN_ALERT "GPIO pin out of scope\n");
		return -2;
	}
	if((kern_atoi(p4)>1)||(strncmp(p3,"out",4)!=0)||(strncmp(p3,"in",4)!=0)||(kern_atoi(p4)<0)){
		printk(KERN_ALERT "Invalid command\n");
		return -3;
	}

	if(strncpy(un1.dir,p3,4)!=p3){
		printk(KERN_ALERT "Failed to copy string");
		return -2;
	}
	un1.gpio_pin=kern_atoi(p1);
	un1.gpio_val=kern_atoi(p4);
	if(gpio_label_get != 0){
		printk(KERN_ALERT "GPIO Label not obtained\n");
		return -3;
	}
	if(gpio_initi() != 0){
		printk(KERN_ALERT "GPIO PIN not accessable\n");
		return -3;
	}
	return 0;

}


static int __init hello(void){
	major = register_chrdev(0,device_name,&fops);
	if(major<0){
		printk(KERN_ALERT "Device registration failed with error code %d\n",major);
		return major;
	}
	dev_class = class_create(THIS_MODULE,"etx_class");
	if(IS_ERR(dev_class)){
		printk(KERN_ALERT "Cannot create the struct class for device\n");
		class_destroy(dev_class);
		unregister_chrdev(major,device_name);
		goto err;
	}
	if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"etx_device"))){
		printk(KERN_ALERT "Cannot create the device\n");
		class_destroy(dev_class);
		unregister_chrdev(major,device_name);
		goto err;
	}
	printk(KERN_INFO "Module loaded sucessfully with major number %d\n",major);
	printk(KERN_INFO "With device name %s\n",device_name);
	return 0;
	err:
	return -1;
}


static void __exit bye(void){
	gpio_unexport(un1.gpio_pin);
	gpio_free(un1.gpio_pin);
	device_destroy(dev_class,dev);
	class_destroy(dev_class);
	unregister_chrdev(major,device_name);
	printk(KERN_INFO "Module has been deloaded sucessfully");
}

module_init(hello);
module_exit(bye);

MODULE_AUTHOR("Aniruddh");
MODULE_LICENSE("GPL");
