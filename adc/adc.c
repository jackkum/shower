/*
 * adc.c
 *
 * Created: 08.03.2015 18:42:16
 *  Author: Ирина
 */ 

#define F_CPU 8000000UL

#define BIT_1 0x01
#define BIT_2 0x02
#define BIT_3 0x04
#define BIT_4 0x08
#define BIT_5 0x10
#define BIT_6 0x20	

enum CHANELS {
	ADC1,
	ADC2,
	ADC3,
	ADC4,
	ADC5,
	ADC6
};

#define  GET_COLUMN(V) (10UL * (V * 100UL / 1023UL) / 100UL)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdio.h>

char str[100];
uint8_t selected = 0;

// пороговое значение для сравнения длинн импульсов и пауз
static const uint8_t IrPulseThershold = 9;// 1024/8000 * 9 = 1.152 msec
// определяет таймаут приема посылки
// и ограничивает максимальную длину импульса и паузы
static const uint8_t TimerReloadValue = 150;
static const uint8_t TimerClock = (1 << CS02) | (1 << CS00);// 8 MHz / 1024

volatile struct ir_t
{
	// флаг начала приема полылки
	uint8_t rx_started;
	// принятый код
	uint32_t code,
	// буфер приёма
	rx_buffer;
} ir;

volatile uint16_t c1 = 0;
volatile uint16_t c2 = 0;
volatile uint16_t c3 = 0;
volatile uint16_t c4 = 0;
volatile uint16_t c5 = 0;
volatile uint16_t c6 = 0;

volatile uint16_t v1 = 0;
volatile uint16_t v2 = 0;
volatile uint16_t v3 = 0;
volatile uint16_t v4 = 0;
volatile uint16_t v5 = 0;
volatile uint16_t v6 = 0;

void checkChanel(const unsigned char index, const unsigned char mask, const unsigned int level);
void reDraw(void);

static void ir_start_timer()
{
	TIMSK |= (1 << TOIE0);
	TCNT0 = 0;
	TCCR0 = TimerClock;
}

void RS_putc(char byte)
{
	while (!(UCSRA&(1<<5)));
	UDR = byte;
}

void RS_puts(char * str)
{
	char c;
	while((c = *str++) != 0) RS_putc(c);
}

/*
ISR(USART_RXC_vect)
{
	
}
*/

ISR(TIMER1_OVF_vect)
{
	PORTD |= (1<<4);
	asm("nop");
	checkChanel(0, BIT_1, c1);
	checkChanel(1, BIT_2, c2);
	checkChanel(2, BIT_3, c3);
	checkChanel(3, BIT_4, c4);
	checkChanel(4, BIT_5, c5);
	checkChanel(5, BIT_6, c6);
	PORTD &= ~(1<<4);
	
	reDraw();
}

ISR(TIMER0_OVF_vect)
{
	ir.code = ir.rx_buffer;
	ir.rx_buffer = 0;
	ir.rx_started = 0;
	if(ir.code == 0)
	TCCR0 = 0;
	TCNT0 = TimerReloadValue;
}

// внешнее прерывание по фронту и спаду
ISR(INT0_vect)
{
	uint8_t delta;
	if(ir.rx_started)
	{
		// если длительность импульса/паузы больше пороговой
		// сдвигаем в буфер единицу иначе ноль.
		delta = TCNT0 - TimerReloadValue;
		ir.rx_buffer <<= 1;
		if(delta > IrPulseThershold) ir.rx_buffer |= 1;
	}
	else{
		ir.rx_started = 1;
		ir_start_timer();
	}
	TCNT0 = TimerReloadValue;
}

static inline void ir_init()
{
	GIMSK |= _BV(INT0);
	MCUCR |= (1 << ISC00) | (0 <<ISC01);
	ir_start_timer();
}


void checkChanel(const unsigned char index, const unsigned char mask, const unsigned int level)
{
	if(level == 0xFFFF){
		return;
	}
	
	ADMUX = index;
	ADCSR |= (1<<ADSC);
	
	while(ADCSR & (1<<ADSC));
	
	uint16_t current = ADCW;
	
	if(current >= level){
		PORTB |= (mask);
	} else {
		PORTB &= ~(mask);
	}
	
	switch(index){
		case ADC1: v1 = current; break;
		case ADC2: v2 = current; break;
		case ADC3: v3 = current; break;
		case ADC4: v4 = current; break;
		case ADC5: v5 = current; break;
		case ADC6: v6 = current; break;
	}
}

void printValue(const uint16_t v)
{
	uint8_t l = 5 - sprintf(str, "%u", v);
	while(l--) RS_putc(' ');
	RS_puts(str);
	RS_puts("  ");
}

void reDraw(void)
{
	static uint8_t inProcess = 0;
	
	if(inProcess) return;
	
	inProcess = 1;
	
	RS_putc(0x0C);
	RS_puts("---------------------------------------------\r\n");
	
	RS_puts("| ");
	for(uint8_t i = 0; i < 6; i++){
		RS_puts("   ");
		RS_putc(selected == i ? '*' : ' ');
		sprintf(str, "%u  ", i+1);
		RS_puts(str);
	}
	
	RS_puts("|\r\n");
	
	RS_puts("---------------------------------------------\r\n");
	
	uint8_t values[6] = {
		GET_COLUMN(c1),
		GET_COLUMN(c2),
		GET_COLUMN(c3),
		GET_COLUMN(c4),
		GET_COLUMN(c5),
		GET_COLUMN(c6)
	};
	
	for(uint8_t l = 10; l > 0; l--){
		RS_puts("| ");
		
		for(uint8_t i = 0; i < 6; i++){
			RS_puts("    ");
			//sprintf(str, "%u", values[i]);
			//RS_puts(str);
			RS_putc(values[i] >= l ? '#' : ' ');
			RS_puts("  ");
		}
		
		RS_puts("|\r\n");
	}
	
	RS_puts("---------------------------------------------\r\n");
	
	RS_puts("| ");
	printValue(c1);
	printValue(c2);
	printValue(c3);
	printValue(c4);
	printValue(c5);
	printValue(c6);
	RS_puts("|\r\n");
	
	RS_puts("---------------------------------------------\r\n");
	
	RS_puts("| ");
	printValue(v1);
	printValue(v2);
	printValue(v3);
	printValue(v4);
	printValue(v5);
	printValue(v6);
	RS_puts("|\r\n");
	
	RS_puts("---------------------------------------------\r\n");
	
	inProcess = 0;
}

int main(void)
{
	
	PORTB = 0x00;
	PORTC = 0xFF;
	
	DDRC  = 0x00;
	DDRB  = 0xFF;
	DDRD |= (1<<4)|(0<<2);
	
	UBRRH = 0;
	UBRRL = 51; //скорость обмена 9600 бод
	UCSRB = (1<<RXEN)|(1<<TXEN); //разр. прерыв при приеме и передачи, разр приема, разр передачи.
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0); //размер слова 8 разрядов
	
	TCCR1B  = (1<<CS02)|(0<<CS01)|(1<<CS00);
	TIMSK  |= (1<<TOIE1);
	
	ir_init();
	
	ACSR  = 0x80;
	SFIOR = 0x00;

	ADCSR = 0x85;
	
	c1 = (uint16_t) eeprom_read_word((uint16_t *)0x00);
	c2 = (uint16_t) eeprom_read_word((uint16_t *)0x02);
	c3 = (uint16_t) eeprom_read_word((uint16_t *)0x04);
	c4 = (uint16_t) eeprom_read_word((uint16_t *)0x06);
	c5 = (uint16_t) eeprom_read_word((uint16_t *)0x08);
	c6 = (uint16_t) eeprom_read_word((uint16_t *)0x0A);

	c1 = c1 > 1023 ? 0 : c1;
	c2 = c2 > 1023 ? 0 : c2;
	c3 = c3 > 1023 ? 0 : c3;
	c4 = c4 > 1023 ? 0 : c4;
	c5 = c5 > 1023 ? 0 : c5;
	c6 = c6 > 1023 ? 0 : c6;

	reDraw();
	
	asm("sei");
	
    while(1){
		if(ir.code)
		{
			switch(ir.code){
				case 142778920UL:
					// left select adc
					if(selected){
						selected--;
						reDraw();
					}
				break;
				
				case 33728680UL:
					// right select adc
					if(selected < 5) {
						selected++;
						reDraw();
					}
				break;
				
				case 167944360UL:
					// adc lower
					switch(selected){
						case ADC1: c1 = (c1 > 10 ? c1-10 : 0); eeprom_write_word((uint16_t *)0x00, c1); break;
						case ADC2: c2 = (c2 > 10 ? c2-10 : 0); eeprom_write_word((uint16_t *)0x02, c2); break;
						case ADC3: c3 = (c3 > 10 ? c3-10 : 0); eeprom_write_word((uint16_t *)0x04, c3); break;
						case ADC4: c4 = (c4 > 10 ? c4-10 : 0); eeprom_write_word((uint16_t *)0x06, c4); break;
						case ADC5: c5 = (c5 > 10 ? c5-10 : 0); eeprom_write_word((uint16_t *)0x08, c5); break;
						case ADC6: c6 = (c6 > 10 ? c6-10 : 0); eeprom_write_word((uint16_t *)0x0A, c6); break;
					}
					
					reDraw();
				break;
				
				case 8563240UL:
					// adc up
					switch(selected){
						case ADC1: c1 = (c1 < 1013 ? c1+10 : 1023); eeprom_write_word((uint16_t *)0x00, c1); break;
						case ADC2: c2 = (c2 < 1013 ? c2+10 : 1023); eeprom_write_word((uint16_t *)0x02, c2); break;
						case ADC3: c3 = (c3 < 1013 ? c3+10 : 1023); eeprom_write_word((uint16_t *)0x04, c3); break;
						case ADC4: c4 = (c4 < 1013 ? c4+10 : 1023); eeprom_write_word((uint16_t *)0x06, c4); break;
						case ADC5: c5 = (c5 < 1013 ? c5+10 : 1023); eeprom_write_word((uint16_t *)0x08, c5); break;
						case ADC6: c6 = (c6 < 1013 ? c6+10 : 1023); eeprom_write_word((uint16_t *)0x0A, c6); break;
					}
					
					reDraw();
					
				break;
				
				case 174760UL:
					// set io out
					switch(selected){
						case ADC1: PORTB |= (1<<0); break;
						case ADC2: PORTB |= (1<<1); break;
						case ADC3: PORTB |= (1<<2); break;
						case ADC4: PORTB |= (1<<3); break;
						case ADC5: PORTB |= (1<<4); break;
						case ADC6: PORTB |= (1<<5); break;
					}
					reDraw();
				break;
				
				case 134390440UL:
					// clear io out
					switch(selected){
						case ADC1: PORTB &= ~(1<<0); break;
						case ADC2: PORTB &= ~(1<<1); break;
						case ADC3: PORTB &= ~(1<<2); break;
						case ADC4: PORTB &= ~(1<<3); break;
						case ADC5: PORTB &= ~(1<<4); break;
						case ADC6: PORTB &= ~(1<<5); break;
					}
					reDraw();
				break;

			}
			
			ir.code = 0;
		}

	}		
	
	return 1;
}