
DEVICE     = atmega328p
CLOCK      = 20000000UL
PROGRAMMER = -c arduino
OBJECTS    = main.o
BAUD	   = 57600
PORT       = /dev/tty.wchusbserial1410


AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE) -P $(PORT) -b $(BAUD) -v


COMPILE = avr-gcc -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE)

all:	main.hex main.eep

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:main.hex:i

eeprom:	all
	$(AVRDUDE) -U eeprom:w:main.eep


install: flash fuse

load: all
	bootloadHID main.hex

clean:
	rm -f main.hex main.elf main.eep $(OBJECTS)

main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS)

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	avr-size --format=avr --mcu=$(DEVICE) main.elf

main.eep: main.elf
	rm -f main.eep
	avr-objcopy -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@


disasm:	main.elf
	avr-objdump -d main.elf

cpp:
	$(COMPILE) -E main.c
