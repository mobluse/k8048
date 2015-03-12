// File: hippocampusgame.c
/////////////////////////////////////////////////////////////////////
//
// Hippocampus Game by M.O.B for K8048.
// Copyright (C) 2008 by Mikael O. Bonnier, Lund, Sweden.
// All rights reserved. This program is free software licensed under 
// the terms of "GNU General Public License" v3 or later, see
// <http://www.gnu.org/copyleft/gpl.html>. Donations are welcome.
// The source code is at <http://www.df.lth.se.orbin.se/~mikaelb/micro/stk500/>.
//
// This program lets the user play Brain Game (a.k.a. Simon) using 
// LD1-LD4 and by pressing the switches SW1-SW4.
// The goal is to emulate the Velleman MK112.
//
// First press SW1 or SW2 to start and select sound or mute, respectively 
// (sound is currently not supported). Then press one of SW1-SW4 to select
// level. Higher level gives longer maximum sequence to repeat. The micro 
// controller plays a sequence. Repeat the sequence by pressing the switches
// corresponding to the leds. The sequence increased by one is then played. Repeat. 
// Eventually you will win, or make a mistake or timeout and loose. You will
// be celebrated or mocked, and the game is restarted, and you select sound 
// or mute again.
//
// Target platform is Velleman K8048/VM111 with Microchip PIC 16F627 micro 
// controller with original configuration (JP3-JP4 on, all other jumpers off). 
// It was developed in C using Piklab in Kubuntu Linux 8.04 and MPLAB IDE v8.10 
// in Windows 2000 with CC5X v3.3A (FREE edition) from B Knudsen Data.
// In MPLAB IDE with CC5X it is necessary to set Build Options For 
// Project/CC5X C Compiler/Additional Command-Line Options 
//  to -p- in order to remove the effect of  MPLAB's chip definition. 
//
// In Windows I programmed the chip using Velleman ProgPIC2 V2.6.0 and 
// PicProg2006 V2.2.0.0, and in Linux: K14 and Piklab (with delay 10 on my computer). 
// In Linux you can usually install Piklab from the distribution, but it is a KDE 
// program. You can download K14 from <http://dev.kewl.org/k8048/Doc/>.
//
// Road map:
// * port to other evaluation boards using other micro controllers
// * improve debouncing
// * implement sleep
// * make it interrupt driven
// * use more power saving features
// * implement sound
// * optimize.
//
// Revision history:
// 2008-Nov-14:     v.0.0.1
//
// Suggestions, improvements, and bug-reports
// are always welcome to:
//                  Mikael Bonnier
//                  Osten Undens gata 88
//                  SE-227 62  LUND
//                  SWEDEN
//
// Or use my electronic addresses:
//     Web: http://www.df.lth.se.orbin.se/~mikaelb/
//     E-mail/MSN: mikael.bonnier@gmail.com
//     ICQ # 114635318
//     Skype: mikael4u
//              _____
// :           /   / \           :
// ***********/   /   \***********
// :         /   /     \         :
// *********/   /       \*********
// :       /   /   / \   \       :
// *******/   /   /   \   \*******
// :     /   /   / \   \   \     :
// *****/   /   /***\   \   \*****
// :   /   /__ /_____\   \   \   :
// ***/               \   \   \***
// : /_________________\   \   \ :
// **\                      \  /**
// :  \______________________\/  :
//
// Mikael O. Bonnier
/////////////////////////////////////////////////////////////////////

#include <16f627.h> // Comes with CC5X.
#include "config16f627.h" // In the same directory as this file.

#pragma config = _BODEN_ON & _CP_OFF & _DATA_CP_OFF & _PWRTE_ON & _WDT_OFF & _LVP_OFF & _MCLRE_ON & _XT_OSC

void init(void);
bit play(void);
void celebrate(void);
void mock(void);
void gen_seq(unsigned int level);
bit standby(void);
void while_key(unsigned char key);
void while_key_not(unsigned char key);
void while_keypattern_not(unsigned char keypattern);
bit wait_for_key(unsigned char key);
bit delay_long(unsigned int count, bit interruptible);
bit delay(bit interruptible);
unsigned char number2bits(unsigned int number);
unsigned int bits2number(unsigned char bits);

unsigned long random(); // TODO: Implement!

bit sleeping;
unsigned char keyboard;
unsigned long seq; // TODO: Fix longer sequences.
unsigned int seq_len;

// These constants can be adjusted
const unsigned int lengths[4] = {5, 6, 7, 8}; // Max length is 8
#define DELAYTIME 5000
#define STANDBYTIMEOUT 20
#define KEYTIMEOUT 20L


void main(void)
{
	sleeping = 1;

	CMCON = 0b00000111; // Disable Comparator module's
		// Disable pull-ups
		// INT on rising edge
		// TMR0 to CLKOUT
		// TMR0 Incr low2high trans.
		// Prescaler assign to Timer0
		// Prescaler rate is 1:256
	OPTION_REG = 0b11010111; // Set PIC options (see datasheet).
	INTCON = 0; // Disable interrupts.
	TRISB = 0b11000000; // RB7 & RB6 are inputs. 
						// RB5...RB0 are outputs.
	TRISA = 0b11111111; // All RA ports are inputs.
	PORTB = 0b0000; // switch off all leds
	while_key(0b0000); // wait for user to press a key

	for(;;)
	{
		init();
		if(sleeping)
			continue;
		bit win = play();
		if(win)
			celebrate();
		else
			mock();
		while_key_not(0b0000);
	}
	return;
}

void init(void)
{
	while_keypattern_not(0b0011); // wait for SW0 (sound) or SW1 (mute)
	if(sleeping)
		return;
	PORTB = keyboard;
	while_key_not(0b0000); // wait for release of all keys

	while_keypattern_not(0b1111); // wait for level 0-3
	if(sleeping)
		return;
	PORTB = keyboard;
	unsigned int level = bits2number(keyboard);
	while_key_not(0b0000);
	PORTB = keyboard;
	gen_seq(level);
}

bit play(void)
{
	unsigned int i, j;
	for(i = 1; i <= seq_len; ++i)
	{
		delay_long(12, 0);
		for(j = 0; j < i; ++j)
		{
			unsigned long led = seq;
			unsigned int j2 = j<<1;
			led >>= j2;
			led &= 0b11;
			PORTB = number2bits(led);
			delay_long(4, 0);
			PORTB = 0b0000;
			delay_long(4, 0);
		}
		for(j = 0; j < i; ++j)
		{
			bit _error = 0;
			unsigned long led = seq;
			unsigned int j2 = j<<1;
			led >>= j2;
			led &= 0b11;
			unsigned char keypattern = number2bits(led);
			_error = wait_for_key(keypattern);
			if(_error)
				return 0;
		}
		
	}
	return 1;
}

void celebrate(void)
{
	unsigned int i;
	for(i = 0; i < 3; ++i)
	{
		PORTB = 0b0001;
		delay(0);
		PORTB = 0b0000;
		delay(0);
		PORTB = 0b0001;
		delay(0);
		PORTB = 0b0000;
		delay(0);
		PORTB = 0b0010;
		delay(0);
		PORTB = 0b0000;
		delay(0);
		PORTB = 0b0010;
		delay(0);
		PORTB = 0b0000;
		delay(0);
		PORTB = 0b0100;
		delay(0);
		PORTB = 0b0000;
		delay(0);
		PORTB = 0b0100;
		delay(0);
		PORTB = 0b0000;
		delay(0);
		PORTB = 0b1000;
		delay(0);
		PORTB = 0b0000;
		delay(0);
		PORTB = 0b1000;
		delay(0);
		PORTB = 0b0000;
		delay(0);
	}
}

void mock(void)
{
	unsigned int i;
	for(i = 0; i < 15; ++i)
	{
		PORTB = 0xF;
		delay(0);
		PORTB = 0x0;
		delay(0);
	}
}

void gen_seq(unsigned int level)
{
	seq_len = lengths[level];
	seq = random();
}

unsigned long random()
{
	return 0b1110010011100111L;
}

bit standby(void)
{
	PORTB = 0b0000;
	unsigned int count;
	for(count = 0; count < STANDBYTIMEOUT; ++count)
	{
		unsigned char leds = 0b0001;
		unsigned int i;
		for(i = 0; i < 4; ++i)
		{
			if(!sleeping)
				PORTB = leds;
			if(delay(1))
			{
				PORTB = 0b0000;
				return 0;
			}
			leds <<= 1;
			leds &= 0b1110;
		}
	}
	PORTB = 0b0000;
	return 1;
}

void while_key(unsigned char key)
{
	do 
	{
		keyboard = PORTA & 0b00001111;
	} while(keyboard == key);
}

void while_key_not(unsigned char key)
{
	do 
	{
		keyboard = PORTA & 0b00001111;
	} while(keyboard != key);
}

void while_keypattern_not(unsigned char keypattern)
{
	do 
	{
		bit timeout = standby();
		if(timeout)
		{
			sleeping = 1;
			return;
		}
	} while(!(keyboard & keypattern));
	sleeping = 0;
}

bit wait_for_key(unsigned char key)
{
	unsigned long count = 0;
	while_key_not(0b0000);
	do 
	{
		if(count > KEYTIMEOUT)
			break;
		++count;
		delay(1);
		// keyboard = PORTA & 0b00001111;
	} while(keyboard == 0b0000);
	PORTB = keyboard;
	bit _error = keyboard != key;
	while_key_not(0b0000);
	PORTB = keyboard;
	return _error;
}

bit delay_long(unsigned int count, bit interruptible)
{
	unsigned int i;
	for(i = 0; i < count; ++i)
		if(delay(interruptible))
			return 1;
	return 0;
}

bit delay(bit interruptible)
{
	unsigned long i;
	for(i = 0; i < DELAYTIME; ++i)
	{
		keyboard = PORTA & 0b00001111;
		if(interruptible && keyboard != 0b0000) {
			PORTB = keyboard;
			return 1;
		}
		random();
	}
	return 0;
}

unsigned char number2bits(unsigned int number)
{
	unsigned char retval = 0b1 << number;
	return retval;
}

unsigned int bits2number(unsigned char bits)
{
	switch(bits)
	{
		case 0b0001:
			return 0;
		case 0b0010:
			return 1;
		case 0b0100:
			return 2;
		case 0b1000:
			return 3;
	}
	return 8;
}
