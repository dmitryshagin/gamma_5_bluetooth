#include <util/delay.h>
#include <avr/io.h>
#include "i2c_slave.h"


#define BT_OFF 			(PORTB |=  (1<<0))
#define BT_ON 			(PORTB &= ~(1<<0))


//call recieved:
// 0x00 0x0A NUM 0x00 0x0A 0x00 0x0A +380632427542 0x00 0x0A
//+2.3S MP
//+0.7S - NUM bla-bla-bla


#define RELEASE_CRQ  (PORTD |=  (1<<7))
#define SET_CRQ_LOW  (PORTD &= ~(1<<7))

#define TO_HEX(i) (i <= 9 ? 0x30 + i : 0x37 + i)

#define NACK TWCR = (1<<TWIE) | (1<<TWINT) | (1<<TWEN)
#define ACK  TWCR = (1<<TWIE) | (1<<TWEA) | (1<<TWINT) | (1<<TWEN)

#define BAUD 115200        //The baudrate that we want to use
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
	DDRB |= (1 << 0); 
	DDRB |= (1 << 5); 

	DDRD |= (1 << 7); 
	DDRD |= (1 << 6); 
	DDRD |= (1 << 5); 
	DDRD |= (1 << 4); 
	DDRD |= (1 << 2); 
	RELEASE_CRQ;
	is_playing = 0;
	I2C_setCallbacks(I2C_received, I2C_requested);
  	I2C_init(I2C_ADDR);
}


int main(void){
	init();
	USART_init();
	_delay_ms(25);
	seq_number = 0;
	seq_pos = 0;
	booted = 0;
	BT_OFF;
	SET_CRQ_LOW;
	while(1){

		if(need_next_byte){
			RELEASE_CRQ;
			_delay_us(200);
			ACK;
			SET_CRQ_LOW;
			seq_number+=1;	
			need_next_byte = 0;	
		}

		if(need_switch_side>0){
			_delay_ms(75);//можно поставить меньше, но при переключении слышно остаток предыдущего трека
			if(need_switch_side==2){
				reply_index = 5;
			}
			if(need_switch_side==1){
				reply_index = 1;
			}
			need_switch_side=0;
			SET_CRQ_LOW;
		}

		if(need_reply){
			ACK;
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
				_delay_ms(10);
				USART_Print("AT#VU\r\n"); //vol up. just in case :)
			}
			if(data==0x19){
				is_playing = 0;
				reply_index = 2;
				BT_OFF;
			}
			if(data==0x15){
				reply_index = 3;
				USART_Print("AT#MD\r\n");
				_delay_ms(25);
				USART_Print("AT#CE\r\n");
				_delay_ms(25);
				need_switch_side = 1;
			}
			if(data==0x16){
				reply_index = 4;
				USART_Print("AT#ME\r\n");
				_delay_ms(15);
				USART_Print("AT#CF\r\n");
				_delay_ms(15);
				USART_Print("AT#CG\r\n");
				_delay_ms(15);
				need_switch_side = 2;
			}
			// if(data==0x11){ //eject - IGNORE
				// reply_index = 5; 
			// }
		}
			
	}

}