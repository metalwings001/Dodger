/*
 * joystick.c
 *
 * Created: 5/22/2019 10:19:42 PM
 * Author : Justin De Leon
 * The ADC init and read was found at http://maxembedded.com/2011/06/the-adc-of-the-avr/
 */ 
#include <stdlib.h> 
#include <time.h> 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>
//#define F_CPU 8000000UL
//#include <util/delay.h>
//#include <stdio.h>

#include "nokia5110.c"
#include "io.c"

volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.
volatile unsigned char blinker, three;
unsigned int i = 0;

struct gameObject
{
	unsigned char objectArr[48][84];
	int type;
};

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A = 125;	// Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR() {
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}


void adc_init()
{
	// AREF = AVcc
	ADMUX = (1<<REFS0);
	
	// ADC Enable and prescaler of 128
	// 16000000/128 = 125000
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}

uint16_t ADC_Read(uint8_t ch)
{
	// select the corresponding channel 0~7
	// ANDing with ’7? will always keep the value
	// of ‘ch’ between 0 and 7
	ch &= 0b00000111;  // AND operation with 7
	ADMUX = (ADMUX & 0xF8)|ch; // clears the bottom 3 bits before ORing
	
	// start single convertion
	// write ’1? to ADSC
	ADCSRA |= (1<<ADSC);
	
	// wait for conversion to complete
	// ADSC becomes ’0? again
	// till then, run loop continuously
	while(ADCSRA & (1<<ADSC));
	
	return (ADC);
}

void startScreen(){
	TimerSet(1);
	TimerOn();
	unsigned long totTime = 0;
	unsigned char button = 0x00;
	nokia_lcd_clear();
	nokia_lcd_set_cursor(20,0);
	nokia_lcd_write_string("Dodger!",1);
	nokia_lcd_set_cursor(0,10);
	nokia_lcd_write_string("Press A to ",1);
	nokia_lcd_set_cursor(0,20);
	nokia_lcd_write_string("start!",1);
	nokia_lcd_render();
	while(button == 0x00) {
		while (!TimerFlag){};
		TimerFlag = 0;
		button = ~PINC & 0xF0;
		totTime += 5;
	}
	srand(totTime + 1);
	nokia_lcd_clear();
	nokia_lcd_render();
}

void gameOver(unsigned long totalTime) {
	unsigned char button = 0x00;
	totalTime /= 1000;
	int score = totalTime * 25;
	unsigned char time[20];
	unsigned char cScore[20];
	sprintf(time, "%d", totalTime);
	sprintf(cScore, "%d", score);
	
	nokia_lcd_clear();
	nokia_lcd_set_cursor(5,0);
	nokia_lcd_write_string("Game Over!",1);
	nokia_lcd_set_cursor(5,10);
	nokia_lcd_write_string("Score:",1);
	nokia_lcd_write_string(cScore,1);
	nokia_lcd_set_cursor(5,20);
	nokia_lcd_write_string("Time:",1);
	nokia_lcd_write_string(time,1);
	nokia_lcd_set_cursor(5,30);
	nokia_lcd_write_string("Press B to",1);
	nokia_lcd_set_cursor(5,40);
	nokia_lcd_write_string("Play Again!",1);
	
	nokia_lcd_render();
	while(button == 0x00) {
		button = ~PINC & 0x0F;
	}
}
void forLoopGen(unsigned char matrixTracker[48][84], int x, int y, int length) {
	for(unsigned k = 0; k < length; ++k) {
		matrixTracker[x][y+k] = 1;
	}
}
void eraseForLoopGen(unsigned char matrixTracker[48][84], int x, int y, int length) {
	for(unsigned k = 0; k < length; ++k) {
		matrixTracker[x][y+k] = 0;
	}
}
void wideBomb(unsigned char matrixTracker[48][84], int xPos, int yPos) {
	int z = 0;
	for(unsigned x = 0; x < 13; ++x) {
		for(unsigned y = 0; y < 13; ++y) {
			if((x == 0 || x == 12) && z == 0) {
				forLoopGen(matrixTracker, x + xPos, y + 4 + yPos, 5);
			}
			if(x == 1 || x == 11) {
				forLoopGen(matrixTracker, x + xPos, y + 2 + yPos, 9);
			}
			if(x == 2 || x == 3 || x == 9 || x == 10) {
				forLoopGen(matrixTracker, x + xPos,  y + 1 + yPos, 11);
			}
			if(x == 4 || x == 5 || x == 6 || x == 7 || x == 8){
				forLoopGen(matrixTracker, x + xPos, y + yPos, 13);
			}
		}	
	}
}
void drawBomb(unsigned char matrixTracker[48][84], int xPos, int yPos) {
	unsigned y = 0;
	for(unsigned x = 0; x < 13; ++x) {
		if(x == 0 || x == 12) {
			forLoopGen(matrixTracker, xPos + x, 4 + yPos, 5);
		}
		if(x == 1 || x == 11) {
			forLoopGen(matrixTracker, xPos + x, 2 + yPos, 9);
		}
		if(x == 2 || x == 3 || x == 9 || x == 10) {
			forLoopGen(matrixTracker, xPos + x, 1 + yPos, 11);
		}
		if(x == 4 || x == 5 || x == 6 || x == 7 || x == 8){
			forLoopGen(matrixTracker, xPos + x, yPos, 13);
		}
		++y;
	}
}
void drawBombErase(unsigned char matrixTracker[48][84], int xPos, int yPos) {
	unsigned y = 0;
	for(unsigned x = 0; x < 13; ++x) {
		if(x == 0 || x == 12) {
			eraseForLoopGen(matrixTracker, xPos + x, 4 + yPos, 5);
		}
		if(x == 1 || x == 11) {
			eraseForLoopGen(matrixTracker, xPos + x, 2 + yPos, 9);
		}
		if(x == 2 || x == 3 || x == 9 || x == 10) {
			eraseForLoopGen(matrixTracker, xPos + x, 1 + yPos, 11);
		}
		if(x == 4 || x == 5 || x == 6 || x == 7 || x == 8){
			eraseForLoopGen(matrixTracker, xPos + x, yPos, 13);
		}
		++y;
	}
}
void imageGenerator( unsigned char player[48][84], unsigned char bomb[48][84] ) {
	nokia_lcd_clear();
	for(unsigned i = 0; i < 48; ++i) {
		for(unsigned j = 0; j < 84; ++j) {
			if(player[i][j] == 1) {
				for(unsigned k = 0; k < 3; ++k) {
					nokia_lcd_set_pixel(j,i+k,1);
					nokia_lcd_set_pixel(j+1,i+k,1);
					nokia_lcd_set_pixel(j+2,i+k,1);
				}
			}
			
		}
	}
	for(unsigned i = 0; i < 48; ++i) {
		for(unsigned j = 0; j < 84; ++j) {
			if(bomb[i][j] == 1) {
				nokia_lcd_set_pixel(j,i,1);
			}
		}
	}
	nokia_lcd_render();
}
int collisionDetection(unsigned char player[48][84], unsigned char bomb[48][84], unsigned long totalTime){
		for(unsigned i = 0; i < 48; ++i) {
			for(unsigned j = 0; j < 84; ++j) {
				if(player[i][j] == 1 && bomb[i][j] == 1) {
					gameOver(totalTime);
					return 1;
				}
			}
		}
		return 0;
}
void smallBomb(unsigned char objectArr[48][84], int xPos, int yPos) {
	unsigned y = 0;
	for(unsigned x = 0; x < 5; ++x) {
		if(x == 0 || x == 4){
			forLoopGen(objectArr, xPos + x, yPos + 1, 3);
		}
		else{
			objectArr[xPos + x][yPos] = 1;
			objectArr[xPos + x][yPos+4] = 1;
			if(x == 2) {
				objectArr[xPos + x][yPos+2] = 1;
			}
		}
	}
	
}
void eraseSmallBomb(unsigned char objectArr[48][84], int xPos, int yPos) {
	unsigned y = 0;
	for(unsigned x = 0; x < 5; ++x) {
		if(x == 0 || x == 4){
			eraseForLoopGen(objectArr, xPos + x, yPos + 1, 3);
		}
		else{
			objectArr[xPos + x][yPos] = 0;
			objectArr[xPos + x][yPos+4] = 0;
			if(x == 2) {
				objectArr[xPos + x][yPos+2] = 0;
			}
		}
	}
	
}
int main(void)
{
	DDRA = 0xFF; PORTA = 0x00; //output set to 0
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0x00; PORTC = 0xFF;
	DDRD = 0xFF; PORTD = 0x00;
	unsigned char buttonB = 0x00;
	
	
		const unsigned long TIMER_PERIOD = 50;
	
		time_t start, end;
		struct gameObject player, bomb, miniBomb;
	
		int ADC_Value, xPos, yPos;
		char buffer[20];
	
		LCD_init();
		adc_init();
		nokia_lcd_init();
		nokia_lcd_clear();
	
		int xAxis = 22;
		int yAxis = 39;
	
		player.objectArr[xAxis][yAxis] = 1;
		player.type = 1;
		unsigned long movementTimeElapsed = 50;
		unsigned long totalTimeElapsed = 0;
		unsigned long smallBombTime = 0;
		unsigned long largeBombTime = 0;
		unsigned long eraseTime = 0;
		unsigned long explosionTime = 0;
		unsigned long difficultyTime = 0;
		unsigned long collisionTime = 0;
		startScreen();
		TimerSet(TIMER_PERIOD);
		TimerOn();
		imageGenerator(player.objectArr, bomb.objectArr);
		unsigned long difficulty = 1300;
		unsigned char flag = 0x00;
		unsigned int numBombs = 1;
		unsigned int bombTracker[50];
		int z = 0;
		while(1){
			buttonB = ~PINC & 0x0F;
			int x, y;
			char xResult[10];
			char yResult[10];
			sprintf(xResult, "%d", xAxis);
			sprintf(yResult, "%d", yAxis);
	
			xPos = ADC_Read(2);
			yPos = ADC_Read(3);
				if(xPos > 120) {
					if(yAxis <= 78) {
						player.objectArr[xAxis][yAxis] = 0;
						yAxis += 3;
						player.objectArr[xAxis][yAxis] = 1;
						movementTimeElapsed = 0;
					}
		
				}
				else if(xPos <= 1) {
					if(yAxis >= 3) {
						player.objectArr[xAxis][yAxis] = 0;
						yAxis -= 3;
						player.objectArr[xAxis][yAxis] = 1;
						movementTimeElapsed = 0;
					}
		
				}
				else if(yPos > 120) {
					if(xAxis <= 41) {	
						player.objectArr[xAxis][yAxis] = 0;
						xAxis += 3;
						player.objectArr[xAxis][yAxis] = 1;
						movementTimeElapsed = 0;
					}
				}
				else if(yPos <= 1) {
					if(xAxis >= 3) {
						player.objectArr[xAxis][yAxis] = 0;
						xAxis -= 3;
						player.objectArr[xAxis][yAxis] = 1;
						movementTimeElapsed = 0;
					}
				}
		
			if(smallBombTime >= difficulty) {
				for(unsigned i = 0; i < numBombs; ++i){
					x = rand() % 48;
					y = rand() % 84;
					if(x <= 3) {
						x += 2;
					}
					if(x >= 44) {
						x -= 1;
					}
					if(y <= 5) {
						y+= 4;
					} 
					if(y >= 80) {
						y -= 4;
					}
					bombTracker[i+i] = x;
					bombTracker[i+i+1] = y;
				}
			
				for(unsigned i = 0; i < numBombs; ++i){
					smallBomb(bomb.objectArr,bombTracker[i+i],bombTracker[i+i+1]);
				}
				smallBombTime = 0;
			}
			if(largeBombTime >= difficulty + 700) {
				for(unsigned i = 0; i < numBombs; ++i){
					eraseSmallBomb(bomb.objectArr,bombTracker[i+i],bombTracker[i+i+1]);
					drawBomb(bomb.objectArr,bombTracker[i+i]-5,bombTracker[i+i+1]-4);
				}
				if(numBombs < 24) {
					++numBombs;
				}
				explosionTime = 0;
				largeBombTime = 0;
				smallBombTime = 0;
				flag = 1;
			}
			/*if(difficultyTime >= 2500) {
				if(numBombs < 24) {
					++numBombs;
				}
				difficultyTime = 0;
			}*/
			if(explosionTime >= 650 && flag == 1) {
				for(unsigned i = 0; i < numBombs; ++i){
					drawBombErase(bomb.objectArr,bombTracker[i+i]-5,bombTracker[i+i+1]-4);
				}
				explosionTime = 0;
				flag = 0;
			}
			if(collisionTime >= 51) {
				z = collisionDetection(player.objectArr,bomb.objectArr,totalTimeElapsed);
				collisionTime = 0;
			}
			if(z == 1) { //reset the game
				for(unsigned i = 0; i < numBombs; ++i){
					eraseSmallBomb(bomb.objectArr,bombTracker[i+i],bombTracker[i+i+1]);
					drawBombErase(bomb.objectArr,bombTracker[i+i]-5,bombTracker[i+i+1]-4);
				}
				numBombs = 1;
				player.objectArr[xAxis][yAxis] = 0;
				xAxis = 22;
				yAxis = 39;
				player.objectArr[xAxis][yAxis] = 1;
				collisionTime = 0;
				explosionTime = 0;
				movementTimeElapsed = 0;
				totalTimeElapsed = 0;
				smallBombTime = 0;
				largeBombTime = 0;
				difficultyTime = 0;
				z = 0;
				startScreen();
				TimerSet(TIMER_PERIOD);
			}
			imageGenerator(player.objectArr, bomb.objectArr);
			while (!TimerFlag){};
			TimerFlag = 0;
		
			collisionTime += TIMER_PERIOD;
			explosionTime += TIMER_PERIOD;
			movementTimeElapsed += TIMER_PERIOD;
			totalTimeElapsed += TIMER_PERIOD;
			smallBombTime += TIMER_PERIOD;
			largeBombTime += TIMER_PERIOD;
			difficultyTime += TIMER_PERIOD;
			
			/*ADC_Value = ADC_Read(2);//Read the status on X-OUT pin using channel 2
			sprintf(buffer, "X=%d   ", ADC_Value);
			LCD_DisplayString(1,buffer);
			//LCD_String_xy(1, 0, buffer);
	
			ADC_Value = ADC_Read(3);// Read the status on Y-OUT pin using channel 3
			sprintf(buffer, "Y=%d   ", ADC_Value);
			LCD_DisplayString(17,buffer);
	
			//LCD_DisplayString(21, xResult);
			LCD_DisplayString(27, xResult);
			collisionDetection(player.objectArr, bomb.objectArr,totalTimeElapsed);
			*/
			}
			
		
	return 0;
}

