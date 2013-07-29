#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include "font.h"

// prototypes
void st7565_init_lcd(void);
void lcd_ascii5x7_string(unsigned int xPos, unsigned int yPos, unsigned char * str);
void lcd_ascii5x7(unsigned int xPos, unsigned int yPos, unsigned char c);
void lcd_clear(void);
void lcd_set_page(unsigned char page, unsigned int column);
void lcd_transfer_data(unsigned char value, unsigned int A0);
void lcd_byte(unsigned char bits);

// macro to convert bank and gpio into pin number
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

// Data and control lines
#define ST7565_CS	GPIO_TO_PIN(1, 13)
#define ST7565_RST	GPIO_TO_PIN(1, 12)
#define ST7565_A0	GPIO_TO_PIN(0, 26)
#define ST7565_CLK	GPIO_TO_PIN(1, 15)
#define ST7565_SI	GPIO_TO_PIN(1, 14)

// used for buffer
char * rx_buffer;
int BUFFER_SIZE = 64;

// Struct for each GPIO pin
struct gpio_pin {
	const char * name;
	unsigned gpio;
};

// Struct to point to all GPIO pins
struct gpio_platform_data {
	struct gpio_pin * pins;
	int num_pins;
};

// Struct for interface definition
static struct gpio_pin st7565_gpio_pins[] = {
	{
		.name                   = "st7565::cs",
		.gpio                   = ST7565_CS,
	},
	{
		.name                   = "st7565::rst",
		.gpio                   = ST7565_RST,
	},
	{
		.name                   = "st7565::a0",
		.gpio                   = ST7565_A0,
	},
	{
		.name                   = "st7565::clk",
		.gpio                   = ST7565_CLK,
	},
	{
		.name                   = "st7565::si",
		.gpio                   = ST7565_SI,
	},
};

static struct gpio_platform_data st7565_gpio_pin_info = {
	.pins           = st7565_gpio_pins,
	.num_pins       = ARRAY_SIZE(st7565_gpio_pins),
};

static dev_t first; // Global variable for the first device number 
static struct cdev c_dev; // Global variable for the character device structure
static struct class *cl; // Global variable for the device class
//static unsigned char lcd_x, lcd_y;

static int st7565_open(struct inode *i, struct file *f)
{
	// nothing to do here
	return 0;
}

static int st7565_close(struct inode *i, struct file *f)
{
	// nothing to do here
	return 0;
}

static ssize_t st7565_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	// nothing to do here
	return 0;
}

static ssize_t st7565_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	//zero the input buffer
    memset(rx_buffer,0,BUFFER_SIZE);

    // copy the incoming data from userspace to a buffer in kernel space
    int retval = copy_from_user(rx_buffer,buf,len);
    
    // display the data on the LCD
    lcd_ascii5x7_string(0,1,rx_buffer);

    // return the number of characters written
    return len;
}

static struct file_operations pugs_fops =
{
	.owner = THIS_MODULE,
	.open = st7565_open,
	.release = st7565_close,
	.read = st7565_read,
	.write = st7565_write
};

static int __init st7565_init(void)
{
	// allocate a buffer and zero it out
	rx_buffer = kmalloc(BUFFER_SIZE,  GFP_KERNEL);
	memset(rx_buffer,0, BUFFER_SIZE);

	// register a character device
	if (alloc_chrdev_region(&first, 0, 1, "st7565") < 0)
	{
		return -1;
	}
	if ((cl = class_create(THIS_MODULE, "chardrv")) == NULL)
	{
		unregister_chrdev_region(first, 1);
		return -1;
	}
	if (device_create(cl, NULL, first, NULL, "st7565") == NULL)
	{
		class_destroy(cl);
		unregister_chrdev_region(first, 1);
		return -1;
	}
	cdev_init(&c_dev, &pugs_fops);
	if (cdev_add(&c_dev, first, 1) == -1)
	{
		device_destroy(cl, first);
		class_destroy(cl);
		unregister_chrdev_region(first, 1);
		return -1;
	}

	// request access to GPIO, set them all as outputs (initially low)
	int err, i = 0;
	for(i = 0; i < st7565_gpio_pin_info.num_pins; i++) {
		err = gpio_request(st7565_gpio_pins[i].gpio, st7565_gpio_pins[i].name);
		if(err) {
			printk("Could not get access to GPIO %i, error code: %i\n", st7565_gpio_pins[i].gpio, err);
		}
		err = gpio_direction_output(st7565_gpio_pins[i].gpio, 0);
		if(err) {
			printk("Could not set value of GPIO %i, error code: %i\n", st7565_gpio_pins[i].gpio, err);
		}
	}

	// initialize display
	st7565_init_lcd();

	// ready to go!
	printk("st7565 registered\n");

	return 0;
}

static void __exit st7565_exit(void)
{
	// release buffer
	if (rx_buffer) {
        kfree(rx_buffer);
    }

	// release GPIO
	int i = 0;
	for(i = 0; i < st7565_gpio_pin_info.num_pins; i++) {
		gpio_free(st7565_gpio_pins[i].gpio);
	}

	// unregister character device
	cdev_del(&c_dev);
	device_destroy(cl, first);
	class_destroy(cl);
	unregister_chrdev_region(first, 1);
	printk("st7565 unregistered\n");
}

void st7565_init_lcd(void)
{
	gpio_set_value(ST7565_CS, 1);
	gpio_set_value(ST7565_RST, 0);
	gpio_set_value(ST7565_RST, 1);
	lcd_transfer_data(0xe2,0);    //Internal reset

	lcd_transfer_data(0xa2,0);    //Sets the LCD drive voltage bias ratio
	           //A2: 1/9 bias
	           //A3: 1/7 bias (ST7565V)
	           
	lcd_transfer_data(0xa0,0);   //Sets the display RAM address SEG output correspondence
	           //A0: normal
	           //A1: reverse
	           
	lcd_transfer_data(0xc8,0);    //#Select COM output scan direction
	           //C0~C7: normal direction
	           //C8~CF: reverse direction
	           
	lcd_transfer_data(0xa4,0);    //#Display all points ON/OFF
	           //A4: normal display
	           //A5: all points ON
	           
	lcd_transfer_data(0xa7,0);   //#Sets the LCD display normal/reverse
	           //A6: normal
	           //A7: reverse
	           
	lcd_transfer_data(0x2F,0);   //#select internal power supply operating mode
	           //28~2F: Operating mode
	           
	lcd_transfer_data(0x60,0);   //#Display start line set
	           //40~7F Display start address
	           
	lcd_transfer_data(0x27,0);    //#V5 voltage regulator internal resistor ratio set(contrast)
	           //20~27 small~large
	           
	lcd_transfer_data(0x81,0);    //#Electronic volume mode set
	           //81: Set the V5 output voltage
	          
	lcd_transfer_data(0x30,0);    //#Electronic volume register set
	           //00~3F: electronic volume register 
	           
	lcd_transfer_data(0xaf,0);      //#Display ON/OFF
	           //AF: ON
	           //AE: OFF
	lcd_clear();
}

void lcd_ascii5x7_string(unsigned int xPos, unsigned int yPos, unsigned char * str)
{
	if(str)
	{
		int i = 0;
		for(i = 0; i < strlen(str) - 1; i++)
		{
			lcd_ascii5x7(xPos, yPos + (i * 6), str[i]);
		}
	}
}

void lcd_ascii5x7(unsigned int xPos, unsigned int yPos, unsigned char c)
{
	c -= 32;	// adjust for the fact that our table of characters is not really ASCII
	lcd_set_page(xPos, yPos);
	unsigned int i = 0;
	for(i = 0; i < 5; i++)
	{
		lcd_transfer_data(font5x7[i + (c * 5)], 1);
	}
}

void lcd_clear(void)
{
	gpio_set_value(ST7565_CS, 0);
	unsigned int i = 0;
	for(i = 0; i < 8; i++)
	{
		lcd_set_page(i, 0);
		unsigned int j = 0;
		for(j = 0; j < 129; j++)
		{
			lcd_transfer_data(0x0, 1);
		}
	}
	gpio_set_value(ST7565_CS, 1);
}

void lcd_set_page(unsigned char page, unsigned int column)
{
	unsigned int lsb = column & 0x0f;
	unsigned int msb = column & 0xf0;
	msb = msb >> 4;
	msb = msb | 0x10;
	page = page | 0xb0;
	lcd_transfer_data(page,0);
	lcd_transfer_data(msb,0);
	lcd_transfer_data(lsb,0);
}

void lcd_transfer_data(unsigned char value, unsigned int A0)
{
	gpio_set_value(ST7565_CS, 0);
	gpio_set_value(ST7565_CLK, 1);
	if(A0)
	{
		gpio_set_value(ST7565_A0, 1);
	}
	else
	{
		gpio_set_value(ST7565_A0, 0);
	}
	lcd_byte(value);
	gpio_set_value(ST7565_CS, 1);
}

void lcd_byte(unsigned char bits)
{
	unsigned char tmp = bits;
	int i = 0;
	for(i = 0; i < 8; i++)
	{
		gpio_set_value(ST7565_CLK, 0);
		if(tmp & 0x80)
		{
			gpio_set_value(ST7565_SI, 1);
		}
		else
		{
			gpio_set_value(ST7565_SI, 0);
		}
		tmp = (tmp << 1);
		gpio_set_value(ST7565_CLK, 1);
	} 
}


module_init(st7565_init);
module_exit(st7565_exit);
MODULE_LICENSE("GPL");