#include <util/delay.h>
#include <avr/io.h>

static const uint8_t C[4][15] = {
			{0x00, 0x00, 0xFF, 0x5D, 0x55, 0x77, 0x77, 0x5D, 0x57, 0x75, 0xD5, 0x55, 0xD7, 0x77, 0x77}, //up
			{0x00, 0x00, 0xFF, 0x5D, 0x55, 0x77, 0x77, 0x5D, 0x55, 0xD7, 0x55, 0x75, 0xD7, 0x77, 0x77}, //down 
			{0x00, 0x00, 0xFF, 0x5D, 0x55, 0x77, 0x77, 0x5D, 0x57, 0x55, 0x55, 0x77, 0x77, 0x77, 0x77}, //vol+
			{0x00, 0x00, 0xFF, 0x5D, 0x55, 0x77, 0x77, 0x5D, 0x55, 0x55, 0x57, 0x77, 0x77, 0x77, 0x77}}; //vol-

#define OUT_ON 			(PORTB |=  (1<<1))
#define OUT_OFF 		(PORTB &= ~(1<<1))

#define LED_ON 			(PORTB |=  (1<<5))
#define LED_OFF 		(PORTB &= ~(1<<5))


#define BUTTON_UP_PRESSED 	(!(PIND & (1 << 5)))
#define BUTTON_DN_PRESSED 	(!(PIND & (1 << 4)))
#define BUTTON_VOL_UP_PRESSED 	(!(PIND & (1 << 3)))
#define BUTTON_VOL_DN_PRESSED 	(!(PIND & (1 << 2)))


// void pwm_gen(){
// 	TCCR1A |= (1<<COM1A1)|(1<<WGM11);
// 	TCCR1B |= (1<<WGM13)|(1<<WGM12)|(1<<CS10);
// 	OCR1A = 0;
// 	ICR1 = 255;
// 	TCNT1 = 0;
// }


void send_byte(uint8_t byte){
	uint8_t bit;
	for(bit=0;bit<8;bit++){
		if((byte >> (7-bit))  & 0x01){
			OUT_ON;
			LED_ON;
		}else{
			OUT_OFF;
			LED_OFF;
		}
		_delay_us(444);
	}

}
void send_command(uint8_t number){
	uint8_t byte=0;
	for(byte=0;byte<15;byte++){
		send_byte(C[number][byte]);
	}
}

void send_repeat(){
	send_byte(0x00);
	send_byte(0x00);
	send_byte(0xF7);
}


int main(void){
	DDRD = 0x00; //PORTD - all input
	DDRB  = (1<<1)|(1<<2)|(1<<5); //output pin - internal led
	PORTD |= (1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5);
	uint8_t pressed = 0;
	// pwm_gen();
    // OCR1A = 255;
	while(1){
		if(BUTTON_UP_PRESSED || BUTTON_DN_PRESSED || BUTTON_VOL_UP_PRESSED || BUTTON_VOL_DN_PRESSED){
			if(pressed==0){
				if(BUTTON_UP_PRESSED){
					send_command(0);
					// OCR1A=100;
				}else if(BUTTON_DN_PRESSED){
					send_command(1);
					// OCR1A=125;
				}else if(BUTTON_VOL_UP_PRESSED){
					send_command(2);
					// OCR1A=150;
				}else{
					send_command(3);
					// OCR1A=75;
				}	
				_delay_ms(33);
				pressed = 1;
			}else{
				send_repeat();
				_delay_ms(75);
			}
		
		}else{
			// OCR1A=255;
			pressed = 0;
			OUT_ON;
			LED_ON;
		}
	}

}