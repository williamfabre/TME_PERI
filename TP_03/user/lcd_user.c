/*******************************************************************************
 * lcdr_user.c - Controleur pour LCd HD44780 ( 20x4 )
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
/* #include <asm/delay.h>*/
/* #include <asm/delay.h>*/

/*******************************************************************************
 * GPIO Pins
 ******************************************************************************/
#define RS 7
#define E  27
#define D4 22
#define D5 23
#define D6 24
#define D7 25

#define GPIO_INPUT  0
#define GPIO_OUTPUT 1

#define RPI_BLOCK_SIZE  0xB4
#define RPI_GPIO_BASE   0x20200000

struct gpio_s
{
	uint32_t gpfsel[7];
	uint32_t gpset[3];
	uint32_t gpclr[3];
	uint32_t gplev[3];
	uint32_t gpeds[3];
	uint32_t gpren[3];
	uint32_t gpfen[3];
	uint32_t gphen[3];
	uint32_t gplen[3];
	uint32_t gparen[3];
	uint32_t gpafen[3];
	uint32_t gppud[1];
	uint32_t gppudclk[3];
	uint32_t test[1];
};
volatile struct gpio_s* gpio_regs;

/*******************************************************************************
 * GPIO Operations
 ******************************************************************************/

/** GPIO **/

int gpio_setup ( void ){

	int mmap_fd = open ("/dev/mem", O_RDWR | O_SYNC );
	if ( mmap_fd < 0 ) {
		return -1;
	}

	gpio_regs = mmap ( NULL, RPI_BLOCK_SIZE,
			PROT_READ | PROT_WRITE,
			MAP_SHARED, mmap_fd, RPI_GPIO_BASE );

	if ( gpio_regs == MAP_FAILED ) {
		close ( mmap_fd );
		return -1;
	}

	return 0;
}

void  gpio_teardown ( void ){
	munmap((void*)gpio_regs, RPI_BLOCK_SIZE);
}

void gpio_config(int gpio, int value)
{
	gpio_regs->gpfsel[gpio/10] = ( gpio_regs->gpfsel[gpio/10]
			& ~(0x7 << ((gpio % 10) * 3)) )
		| (value << ((gpio % 10) * 3));
}

void gpio_update(int gpio, int value)
{
	if( value )
		gpio_regs->gpset[gpio/32] = (0x1 << (gpio % 32) );
	else
		gpio_regs->gpclr[gpio/32] = (0x1 << (gpio % 32) );
}

/*******************************************************************************
 * LCD's Instructions ( source = doc )
 ******************************************************************************/

#define B_0000  0
#define B_0001  1
#define B_0010  2
#define B_0011  3
#define B_0100  4
#define B_0101  5
#define B_0110  6
#define B_0111  7
#define B_1000  8
#define B_1001  9
#define B_1010  a
#define B_1011  b
#define B_1100  c
#define B_1101  d
#define B_1110  e
#define B_1111  f

/* On veut definir une macro HEX()
 * qui prend en argument deux nombres de 4 bits exprimes en binaire 
 * representant resp. les 4 bits de poids fort et les 4 bits de poids faible d'un entier sur 8bits  
 * qui rend l'entier.
 * p. ex: HEX(0000,0010) rend 2
 * p. ex: HEX(0001,0000) rend 16
 *
 * QUESTION: expliquez comment ca marche ....
 */
#define __HEX(h,l) 0x##h##l
#define _HEX(h,l)  __HEX(h,l)
#define HEX(h,l)   _HEX(B_##h,B_##l)

/* QUESTION: que fait cette macro ?
*/
#define BIT(b,n)   (((n)>>(b))&1)

/* commands */
#define LCD_CLEARDISPLAY        HEX(0000,0001)
#define LCD_RETURNHOME          HEX(0000,0010)
#define LCD_ENTRYMODESET        HEX(0000,0100)
#define LCD_DISPLAYCONTROL      HEX(0000,1000)
#define LCD_CURSORSHIFT         HEX(0001,0000)
#define LCD_FUNCTIONSET         HEX(0010,0000)
#define LCD_SETCGRAMADDR        HEX(0100,0000)
#define LCD_SETDDRAMADDR        HEX(1000,0000)

/* flags for display entry mode : combine with LCD_ENTRYMODESET */
#define LCD_EM_RIGHT            HEX(0000,0000)
#define LCD_EM_LEFT             HEX(0000,0010)
#define LCD_EM_DISPLAYSHIFT     HEX(0000,0001)
#define LCD_EM_DISPLAYNOSHIFT   HEX(0000,0000)

/* flags for display on/off control : combine with LCD_DISPLAYCONTROL */
#define LCD_DC_DISPLAYON        HEX(0000,0100)
#define LCD_DC_DISPLAYOFF       HEX(0000,0000)
#define LCD_DC_CURSORON         HEX(0000,0010)
#define LCD_DC_CURSOROFF        HEX(0000,0000)
#define LCD_DC_BLINKON          HEX(0000,0001)
#define LCD_DC_BLINKOFF         HEX(0000,0000)

/* flags for display/cursor shift : combine with LCD_CURSORSHIFT */
#define LCD_CS_DISPLAYMOVE      HEX(0000,1000)
#define LCD_CS_CURSORMOVE       HEX(0000,0000)
#define LCD_CS_MOVERIGHT        HEX(0000,0100)
#define LCD_CS_MOVELEFT         HEX(0000,0000)

/* flags for function set : combine with LCD_FUNCTIONSET */
#define LCD_FS_8BITMODE         HEX(0001,0000)
#define LCD_FS_4BITMODE         HEX(0000,0000)
#define LCD_FS_2LINE            HEX(0000,1000)
#define LCD_FS_1LINE            HEX(0000,0000)
#define LCD_FS_5x10DOTS         HEX(0000,0100)
#define LCD_FS_5x8DOTS          HEX(0000,0000)

/*******************************************************************************
 * LCD's Operations
 ******************************************************************************/

/* generate E signal */
void lcd_strobe(void)
{
	/* gpio_config(E, 1);*/
	gpio_update(E, 1);
	usleep(1);
	gpio_update(E, 0);
	usleep(1);
}

/* send 4bits to LCD */
void lcd_write4bits(int data)
{
	/* first 4 bits */
	gpio_update(D7, BIT(7,data));
	gpio_update(D6, BIT(6,data));
	gpio_update(D5, BIT(5,data));
	gpio_update(D4, BIT(4,data));

	lcd_strobe();

	/* second 4 bits */
	gpio_update(D7, BIT(3,data));
	gpio_update(D6, BIT(2,data));
	gpio_update(D5, BIT(1,data));
	gpio_update(D4, BIT(0,data));  

	lcd_strobe();

	usleep(50); /* le délai minimum est de 37us */
}

void lcd_command(int cmd)
{
	gpio_update(RS, 0);
	lcd_write4bits(cmd);
	usleep(2000); /* delai nécessaire pour certaines commandes */
}

void lcd_data(int character)
{
	gpio_update(RS, 1);
	lcd_write4bits(character);
}

/* initialization 
 * Question : commenter ces initialisations:
 *
 * LCD_FUNCTIONSET: Set des fct suivantes
 * LCD_FS_4BITMODE: mode lecture 4bits
 * LCD_FS_2LINE: mode 2 lignes
 * LCD_FS_5x8DOTS: taille des char, 5L, 8H
 *
 * LCD_DISPLAYCONTROL: controle fonction suivante
 * LCD_DC_DISPLAYON: affichage autorise
 * LCD_DC_CURSOROFF: desactive le curseur
 *
 * LCD_ENTRYMODESET: mode set fonction suivante
 * LCD_EM_RIGHT: deplace le curseur < ou >
 * LCD_EM_DISPLAYNOSHIFT: pas de changement d'affichage
 */
void lcd_init(void)
{ 	 
	gpio_update(E, 0); // E = 0
	lcd_command(HEX(0011,0011)); /* initialization */ 
	lcd_command(HEX(0011,0010)); /* initialization */
	lcd_command(LCD_FUNCTIONSET | LCD_FS_4BITMODE
			| LCD_FS_2LINE | LCD_FS_5x8DOTS);
	lcd_command(LCD_DISPLAYCONTROL
			| LCD_DC_DISPLAYON | LCD_DC_CURSOROFF );
	lcd_command(LCD_ENTRYMODESET
			| LCD_EM_RIGHT | LCD_EM_DISPLAYNOSHIFT);
}

void lcd_clear(void)
{
	lcd_command(LCD_CLEARDISPLAY);
	lcd_command(LCD_RETURNHOME);
}

void lcd_set_cursor(int x, int y)
{	
	int i;
	if (y == 0) y = 0x00;
	else if (y == 1) y = 0x40;
	else if (y == 2) y = 0x14;
	else  y = 0x54;

	lcd_command(LCD_SETDDRAMADDR |  y);

	for(i = 0; i < x; i++)
		lcd_command(LCD_CURSORSHIFT |  LCD_CS_MOVERIGHT);
}


/* void lcd_set_cursor(int c, int l)*/
/* {	*/
/*         int DDRAM[4]={0x00,0x40,0x14,0x54};*/
/*         lcd_command(LCD_SETDDRAMADDR+DDRAM[l%4]+c%0x14);*/
/* }*/

void lcd_message(char* txt)
{
	int i;

	for(i=0; i<strlen(txt); i++){

		lcd_data(txt[i]);
		if (i == 0x13) {
			lcd_set_cursor(0, 1);
		}
		if (i == 0x53) {
			lcd_set_cursor(0, 3);
		}
		if (i == 0x27) {
			lcd_set_cursor(0, 2);
		}
		if (i == 0x67) {
			lcd_set_cursor(0, 0);
		}
	}
	lcd_set_cursor(0, 0);
}


void lcd_message2(char* txt)
{
	int i, j;
	for (i=0; i<20 && i<strlen(txt); i++){
		lcd_data(txt[i]);
	}
}

/*******************************************************************************
 * Finally, the main function
 ******************************************************************************/

int main(int argc, char** argv)
{
	/* arg */
	if( argc < 2 ){
		fprintf(stderr, "ERROR: must take a string as argument\n");
		exit(1);
	}

	/* Retreive the mapped GPIO memory */
	if( gpio_setup() == -1 ){
		perror("ERROR: gpio_setup\n");
		exit(1);
	}

	/* Setting up GPIOs to output */
	gpio_config(RS, GPIO_OUTPUT);
	gpio_config(E , GPIO_OUTPUT);
	gpio_config(D4, GPIO_OUTPUT);
	gpio_config(D5, GPIO_OUTPUT);
	gpio_config(D6, GPIO_OUTPUT);
	gpio_config(D7, GPIO_OUTPUT);

	/* initialization */
	lcd_init();
	lcd_clear();

	int i;
	int j = 0;

	while(1){
		FILE *fd = fopen("/proc/loadavg", "r");

		if (fd == NULL) {
			return -1;
		}

		i = 0;

		char buf[25];
		char res[256];

		fscanf(fd, "%[^\n]s", buf);
		printf("%s\n", buf);

		char *p = strtok (buf, " ");
		char *array[5];

		while (p != NULL)
		{
			array[i++] = p;
			p = strtok (NULL, " ");
		}

		for (i = 0; i < 5; ++i)
			printf("%s\n", array[i]);

		char* tempo = strtok(array[3], "/");
		sprintf(res, "LE TEXTE LE PLUS LONG DU MONDE heloo world je sui la ");
		lcd_message(res);
		sleep(1);
		fclose(fd);
	}

	/* Release the GPIO memory mapping */
	gpio_teardown();

	return 0;
}
