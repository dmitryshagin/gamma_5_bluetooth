#include <util/delay.h>
#include <avr/io.h>

static const uint8_t C[4][10] = {
			{0x77, 0x77, 0x5D, 0x57, 0x55, 0x55, 0x77, 0x77, 0x77, 0x77}, //vol+
			{0x77, 0x77, 0x5D, 0x55, 0x55, 0x57, 0x77, 0x77, 0x77, 0x77}, //vol-
			{0x77, 0x77, 0x5D, 0x57, 0x75, 0xD5, 0x55, 0xD7, 0x77, 0x77}, //up
			{0x77, 0x77, 0x5D, 0x55, 0xD7, 0x55, 0x75, 0xD7, 0x77, 0x77}}; //down 

#define INPUT_LOW 	(!(PIND & (1 << 2)))


#define LED_ON 			(PORTB |=  (1<<5))
#define LED_OFF 		(PORTB &= ~(1<<5))

#define TO_HEX(i) (i <= 9 ? 0x30 + i : 0x37 + i)

#define RESOLUTION 2 //delay in uS to calculate bit length. just to avoid timers
// #define RESOLUTION 10 //delay in uS to calculate bit length. just to avoid timers

uint8_t pulse_length[8]={0,0,0,0,0,0,0};
uint8_t calibration_index = 0;
uint16_t calibrated_pulse = 0;
uint8_t command[10];
uint8_t pwm_val[4]={10,40,70,100};


void pwm_gen(){
	TCCR1A |= (1<<COM1A1)|(1<<WGM11);
	TCCR1B |= (1<<WGM13)|(1<<WGM12)|(1<<CS10);
	OCR1A = 0;
	ICR1 = 255;
	TCNT1 = 0;
}



#define BAUD 38400        //The baudrate that we want to use
#define BAUD_PRESCALLER ((F_CPU / 16UL / BAUD ) - 1)    //The formula that does all the required maths
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

void delay_cycle(uint8_t cycles){
	while(cycles--) {
    	_delay_us(RESOLUTION);
    	pulse_length[calibration_index]++; //just to make delay equal to measure.
  	}

}


void init(){
	USART_init();
}
void look_for_start(){
	uint8_t waiting_for = 0;
	while(!INPUT_LOW){
		_delay_ms(1);
		waiting_for++;
		if(waiting_for>250){
			OCR1A = 0xFF;//turn off key press if no ney pressed for 100ms
		}
	}; //waiting for signal to go low
	OCR1A = 0xFF;
	_delay_ms(2);
}


void skip_start(){
	// we don't need to be bit-perfect - just split one button from another, so we'll be rough
	//so we'll skip several bytes and move to frequency calibration
	while(INPUT_LOW){}; //wait for 1
	_delay_ms(2);
	while(!INPUT_LOW){}; //wait for 0
	_delay_us(100);
	while(INPUT_LOW){}; //wait for 1
	_delay_us(100);
	while(!INPUT_LOW){}; //wait for 0
	_delay_us(100);
	while(INPUT_LOW){}; //wait for 1
	_delay_us(100);
	while(!INPUT_LOW){}; //wait for 0
	_delay_us(100);
	while(INPUT_LOW){}; //wait for 1
	_delay_us(100);
	while(!INPUT_LOW){}; //skip 1	
	_delay_us(100);
}
void clean_data(){
	for(calibration_index=0;calibration_index<8;calibration_index++){
		pulse_length[calibration_index]=0;
	}	
	for(calibration_index=0; calibration_index<10; calibration_index++){
		command[calibration_index]=0;
	}	
}

void calculate_frequency(){
	for(calibration_index=0;calibration_index<4;calibration_index++){
		while(INPUT_LOW){
			_delay_us(RESOLUTION);
			pulse_length[calibration_index]++;
			asm("nop");
		}
		// _delay_us(1);
		while(!INPUT_LOW){
			_delay_us(RESOLUTION);
			pulse_length[calibration_index+4]++;
			asm("nop");
		}
		// _delay_us(1);
	}
	calibrated_pulse=0;
	for(calibration_index=1;calibration_index<7;calibration_index++){
		calibrated_pulse += pulse_length[calibration_index];
	}
	calibrated_pulse=(calibrated_pulse+1)/6; //we'll cut first and last one
	// calibrated_pulse = 0x60;
	
}

void read_data(){
	uint8_t bytes = 0;
	uint8_t bits = 0;
	uint8_t byte = 0;
	delay_cycle(calibrated_pulse/2);//a little offset to compensate frequency deviation
	for(bytes=0; bytes<10; bytes++){
		byte = 0xFF;
		for(bits=0;bits<8;bits++){
			if(INPUT_LOW){
				byte &= ~(1 << (7-bits));
			}
			delay_cycle(calibrated_pulse);
		}
		command[bytes]=byte;
	}
}

int define_cmd(){
	uint8_t idx = 0;
	uint8_t pos = 0;
	uint8_t found;
	for(idx=0;idx<4;idx++){
		found = 1;
		for(pos=0;pos<8;pos++){
			if(C[idx][pos]!=command[pos]){
				found = 0;
			}
		}
		if(found==1){
			return idx;
		}
	}
	return -1;
}

int main(void){
	init();
    LED_OFF;
    DDRB  = (1<<1)|(1<<2)|(1<<5); //output pin - internal led
	USART_Print("Hello");
    uint8_t i;
    pwm_gen();
    OCR1A = 255;

	while(1){
		clean_data();
		look_for_start();
		skip_start();
		
		calculate_frequency();
		read_data();

		int cmd = define_cmd();

		USART_Print("Calibration pulse: ");
		USART_send(TO_HEX(((calibrated_pulse&0xF0)>>4)));
		USART_send(TO_HEX((calibrated_pulse&0xF)));
		USART_send('\n');
		USART_Print("Calibration info: ");
		for(i=0;i<8;i++){
			USART_send(TO_HEX(((pulse_length[i]&0xF0)>>4)));
			USART_send(TO_HEX((pulse_length[i]&0xF)));
			USART_send(',');
		}	
		USART_send('\n');
		if(cmd<0){
			USART_Print("No command found :(");
			OCR1A=0xFF;
		}else{
			OCR1A=pwm_val[cmd];
			USART_Print("Command: ");
			USART_send(TO_HEX(cmd));
			LED_ON;
		}
		USART_send('\n');

		uint8_t i=0;
		for(i=0;i<10;i++){
			USART_send('0');
			USART_send('x');
			USART_send(TO_HEX(((command[i]&0xF0)>>4)));
			USART_send(TO_HEX((command[i]&0xF)));
			USART_send(',');
			USART_send(' ');
		}
		USART_send('\n');
		uint8_t repeat_count = 0; 
		if(cmd>=0){
			//TODO - apply TAP. Exception - if we are recieving a call?
			uint8_t i=0;
			for(i=0;i<100;i++){
				if(INPUT_LOW){
					repeat_count+=1;
					i=0;
					if(repeat_count>5){
						USART_send('=');
						//TODO - apply LONGTAP
					}else{
						USART_send('!');
					}
					_delay_ms(20);
				}
				_delay_ms(1);
			}
		}

		LED_OFF;
	}

}