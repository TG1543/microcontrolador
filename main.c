#include <18f4680.h>
#device ADC=10
#fuses NOWDT, HS, NODEBUG, BROWNOUT, NOLVP
#use delay(internal=32M)
#use rs232(baud=9600,parity=N,bits=8,xmit=PIN_C6,rcv=PIN_C7)

#define on 1
#define off 0


#include <stdlibm.h>
#include "instrument.h"

//Global							// bit menos significativo
const long dac_pins[] = {	pin_b5,pin_b4,pin_b3,pin_b2,
							pin_b1,pin_b0,pin_d7,pin_d6,
							pin_d5,pin_d4,pin_c5,pin_c4,
							pin_d3,pin_d2,pin_d1,pin_d0}; // bit más significativo

const signed long dac_max = 32767;
const signed long dac_min = -32768;
int *samples;

int time_of_chart;
int run_time;
int voltage;
int charts;
int current_chart;

//set pins of control
void set_cin1(short state){output_bit(pin_a1,state);}
void set_cin2(short state){output_bit(pin_a3,state);}
void set_cin3(short state){output_bit(pin_a5,state);}
void set_dac_a0(short state){output_bit(pin_c3,state);}
void set_dac_clr(short state){output_bit(pin_c1,state);}
void set_dac_wr(short state){output_bit(pin_c2,state);}

void set_dac_pins(long i){
    long dac_pin = 1;
    int pin;
    for (pin = 0; pin < 16; dac_pin*=2, ++pin){
        output_bit(dac_pins[pin],dac_pin&i);
    }
}

void set_off_all_pines(){
	set_dac_pins(off);
	set_dac_a0(off);
	set_dac_clr(off);
	set_dac_wr(off);
	set_cin1(off);
	set_cin2(off);
	set_cin3(off);
}

//functions
void _setup_adc(){
	setup_adc(ADC_CLOCK_INTERNAL);
	//setup_adc_ports(AN0 | VSS_VREF);
	setup_adc_ports(AN0);
	set_adc_channel(0);
	delay_us(100);
}

//initialize the pic
void init (){
	//speed
	setup_oscillator(OSC_32MHZ);
	//Signals
	set_tris_a(0x01);
	set_tris_b(0xc0);
	set_tris_c(0xc0);
	set_tris_d(0x00);
	//setup of the device
	if (read_eeprom(0) != 1){
		write_eeprom(0,1);
		write_eeprom(1,10);	// voltage
		write_eeprom(2,60); // time of chart
		write_eeprom(3,1);  // number of chart
		write_eeprom(4,0);  // current chart
		write_eeprom(5,24); // time of run
	}
	voltage = read_eeprom(1);
	time_of_chart = read_eeprom(2);
	charts = read_eeprom(3);
	current_chart = read_eeprom(4);
	run_time = read_eeprom(5);
	set_off_all_pines();
	delay_ms(1000);
}

void print_setup(){
	printf(":%u:%u:%u:%u:%u",voltage,time_of_chart,charts,current_chart,run_time);
}

short comunication(){
	if (kbhit()){
		char value = getc();
		if (value == 'i'){
				putc('i');
				while (kbhit());
				voltage = getc();
				time_of_chart = getc();
				charts = getc();
				run_time = getc();
				putc('f');
				write_eeprom(0,1);
				if (voltage > 10) voltage = 10;
				write_eeprom(1,voltage);  //max voltage
				write_eeprom(2,time_of_chart); // time of each chart
				write_eeprom(3,charts); // number of charts
				write_eeprom(4,0); // current chart -> start in 0
				write_eeprom(5,run_time); // time of work of tht device
		}else if (value == 'p'){
			print_setup();
		}else if (value == 'd'){
				putc('d');
				while (kbhit());
				//Enviar datos
		}
		return true;
	}
	return false;
}

short stop (){
	if (kbhit()){
		int value = getc();
		if (value == 's'){
			putc('y');
			return true;
		}
	}
	return false;
}

long get_value_adc(){
	long long value = 0;
	long i=128;
	while (i--){
		delay_us(20);
		value+= read_adc();
	}
	value = value>>8;
	return (long)value;
}

void dac(long start_value,long end_value, long delay){
	_setup_adc();
	samples = malloc(100*8);
	short scale = 1;
	set_cin1(on);
	set_cin2(on);
	set_cin3(scale);
	set_dac_wr(on);
	set_dac_clr(on);
	set_dac_wr(off);
	delay_ms(100);
	signed long i;
	long long take_sample = 0;
	int nsample = 0;
	int take_sample_2 = (end_value-start_value-1)/59;
	long sample = get_value_adc();
	*(samples + nsample) = ((sample>>3)&(scale<<7));
	nsample++;
	for (i = start_value; i <= end_value; i=i+1){
		long dac_pin = 1;
		int pin;
		for (pin = 0; pin < 16; dac_pin*=2, ++pin){
			output_bit(dac_pins[pin],dac_pin&i);
		}
		long value = read_adc();
		if (scale == 0 && value <= 15) { scale=1;set_cin3(1);delay_us(delay); i=i+1; }//switchx1
		if (scale == 1 && value >= 800){ scale=0;set_cin3(0);delay_us(delay); i=i+1; }//switchx10
		delay_ms(delay);
		//printf(":%Lu",value);
		if (take_sample >= take_sample_2){
		  sample = get_value_adc();
			*(samples + nsample) = ((sample>>3)&(scale<<7));
			nsample++;
			take_sample = 0;
		}
		take_sample++;
		if(i == end_value){ break;}
	}
	*(samples + nsample) = get_value_adc();
	set_off_all_pines();
	setup_adc(ADC_OFF);
}

void set_in_memory(long factor){
		long start_position = 100 ;
		free(samples);
}

short run (){
	long unit_run_time = (run_time*60)/(charts - 1); //m
	long current_run_time = 0;
	while (current_chart < charts){
		while ((current_run_time/60.0)/unit_run_time >= 1.0){
			delay_ms(1000);
			++current_run_time;
			if (stop()) return false;
		}
		current_run_time = time_of_chart*60+1;
		signed long start_dac = dac_min;
		long steps = dac_max-dac_min+1;
		signed long end_dac = ((steps)/10.0)*voltage + dac_min;
		long long delay = time_of_chart*6*voltage;

		dac(start_dac,end_dac, delay);
		set_in_memory(current_chart++);
		write_eeprom(4,current_chart);
	}
	return true;
}

void main() {
	init();
	//if (read_eeprom(0) == 1)	s = run();
	set_dac_wr(on);
	set_dac_clr(on);
	set_dac_wr(off);
	delay_ms(1000);
	_setup_adc();
	//short result = run();
	while (1){
	 	comunication ();
		long i = get_value_adc();
		printf(":%Lu",i);
	}
}
