#include <util/delay.h>
#include <avr/io.h>
#include "i2c_slave.h"


#define LED_ON 			(PORTB |=  (1<<5))
#define LED_OFF 		(PORTB &= ~(1<<5))

#define BT_OFF 			(PORTD |=  (1<<4))
#define BT_ON 			(PORTD &= ~(1<<4))


#define BACK_OFF		(PORTD |=  (1<<7))
#define BACK_ON 		(PORTD &= ~(1<<7))
#define FWD_OFF 		(PORTD |=  (1<<6))
#define FWD_ON 			(PORTD &= ~(1<<6))
#define PLAY_OFF 		(PORTD |=  (1<<5))
#define PLAY_ON 		(PORTD &= ~(1<<5))

// #define INPUT_LOW 		(!(PINB & (1 << 0)))


#define CRST 			(!(PIND  & (1<<PD3)))


#define RELEASE_CRQ  (PORTD |=  (1<<2))
#define SET_CRQ_LOW  (PORTD &= ~(1<<2))

#define TO_HEX(i) (i <= 9 ? 0x30 + i : 0x37 + i)

#define NACK TWCR = (1<<TWIE) | (1<<TWINT) | (1<<TWEN)
#define ACK  TWCR = (1<<TWIE) | (1<<TWEA) | (1<<TWINT) | (1<<TWEN)

#define BAUD 57600        //The baudrate that we want to use
#define BAUD_PRESCALLER ((F_CPU / 16UL / BAUD ) - 1)    //The formula that does all the required maths

#define I2C_ADDR 0x18


volatile uint8_t data;


uint8_t boot_seq[3][5] = {{0x04, 0x11, 0x10, 0x92, 0xB3},
						  {0x04, 0x20, 0x01, 0x03, 0x24},
						  {0x03, 0x41, 0x12, 0x53, 0x00}};

volatile uint8_t seq_number, seq_pos, booted, need_next_byte, 
					default_reply_pos, need_reply, is_playing, reply_index, need_switch_side;


uint8_t replies[6][5] = {  
							{0x04, 0x20, 0x04, 0x03, 0x27}, //default reply
							{0x04, 0x20, 0x05, 0x03, 0x28}, //playing reply
							{0x04, 0x20, 0x01, 0x03, 0x24}, //turn off
							{0x04, 0x20, 0x07, 0x03, 0x2A}, //FWD reply
							{0x04, 0x20, 0x08, 0x03, 0x2B}, //BACK reply
							{0x04, 0x20, 0x06, 0x03, 0x29}  //after side switched
						}; 


void USART_init(void){
 
 UBRR0H = (uint8_t)(BAUD_PRESCALLER>>8);
 UBRR0L = (uint8_t)(BAUD_PRESCALLER);
 UCSR0B = (1<<TXEN0);
 UCSR0C = ((1<<UCSZ00)|(1<<UCSZ01));
}

void USART_send( unsigned char data){
 
 while(!(UCSR0A & (1<<UDRE0)));
 UDR0 = data;
 
}


void USART_Print(const char *USART_String) {
 uint8_t c;
 while ((c=*USART_String++))
 {
   USART_send(c);
 }
}

void I2C_received(uint8_t received_data)
{
  	data = received_data;
  	need_reply = 1;
}

void I2C_requested()
{
	if(!booted){
		TWDR = boot_seq[seq_number][seq_pos];
		if(seq_number<2){
			if(seq_pos==4){
				NACK;
			}else{
				ACK;
			}	
		}else{
			if(seq_pos==3){
				NACK;
			}else{
				ACK;
			}
		}
		
		seq_pos+=1;

		if(seq_pos>4){
			seq_pos=0;	
			if(seq_number<2){
				need_next_byte = 1;
			}
		}

		if(seq_number==2&&seq_pos==4){
			booted = 1;
			RELEASE_CRQ;
		}
	}else{
		TWDR = replies[reply_index][default_reply_pos];
		default_reply_pos+=1;
		if(default_reply_pos==5){
			default_reply_pos=0;
			NACK;
			RELEASE_CRQ;
		}else{
			ACK;
		}	
	}
}


void init(){
	
	I2C_stop();
	DDRB |= (1 << 5); 
	DDRD |= (1 << 7); 
	DDRD |= (1 << 6); 
	DDRD |= (1 << 5); 
	DDRD |= (1 << 4); 
	DDRD |= (1 << 2); 
	RELEASE_CRQ;
	BT_OFF;
	is_playing = 0;
	I2C_setCallbacks(I2C_received, I2C_requested);
  	I2C_init(I2C_ADDR);
  	LED_OFF;
  	FWD_OFF;
  	PLAY_OFF;
  	BACK_OFF;
    
}


int main(void){
	USART_init();
	USART_Print("Hello. I'm simulator.");
    USART_send('\n');
	init();
	BT_ON;    
	while(1){
		if(CRST){
			USART_send('R');
			while(CRST){};
			init();
			_delay_ms(25);
			seq_number = 0;
			seq_pos = 0;
			booted = 0;
			SET_CRQ_LOW;
			USART_send('S');
		}

		if(need_next_byte){
			RELEASE_CRQ;
			//TODO - wait for SCL & SCK to set high
			_delay_us(200); //todo - make it 100us when upper todo is dones
			ACK;
			SET_CRQ_LOW;
			seq_number+=1;	
			need_next_byte = 0;	
		}

		if(need_switch_side>0){
			_delay_ms(250);//можно поставить меньше, но при переключении слышно остаток предыдущего трека
			if(need_switch_side==2){
				reply_index = 5;
			}
			if(need_switch_side==1){
				reply_index = 1;
			}
			need_switch_side=0;
			USART_send('S');
			SET_CRQ_LOW;
		}

		if(need_reply){
		  	USART_send('0');
			USART_send('x');
			USART_send(TO_HEX(((data&0xF0)>>4)));
			USART_send(TO_HEX((data&0xF)));
			USART_send('\n');
			ACK;
			// _delay_us(200);
			SET_CRQ_LOW;
			need_reply = 0;
			
			if(data==0x13){
				if(is_playing){
					reply_index = 1;
				}else{
					reply_index = 0;
				}
				is_playing = 1;
				BT_ON;
				LED_ON;
				USART_send('+');
			}
			if(data==0x19){
				is_playing = 0;
				reply_index = 2;
				LED_OFF;
				BT_OFF;
				USART_send('-');
			}
			if(data==0x15){
				USART_send('>');
				reply_index = 3;
				FWD_ON;
				_delay_ms(200);
				FWD_OFF;
				need_switch_side = 1;
			}
			if(data==0x16){
				USART_send('<');
				reply_index = 4;
				BACK_ON;
				_delay_ms(200);
				BACK_OFF;
				need_switch_side = 2;
			}
			if(data==0x11){ //eject - show to emulate?
				USART_send('^');
				reply_index = 1; 
			}
		}
			
	}

}