#include <asf.h>

#include "PIO_OLED.h"
#include "PIO_FUNCTIONS.h"
#include "TC-RTT-RTC.h"

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

//PINO ECHO
#define ECHO_PI				PIOD
#define	ECHO_PI_ID			ID_PIOD
#define ECHO_PI_IDX			11
#define ECHO_PI_IDX_MASK	(1 << ECHO_PI_IDX)	

//PINO TRIGGER
#define TRIG_PI				PIOC
#define	TRIG_PI_ID			ID_PIOC
#define TRIG_PI_IDX			13
#define TRIG_PI_IDX_MASK	(1 << TRIG_PI_IDX)

//Variaveis globais
volatile char subida = 0;
volatile char descida = 0;
volatile double tempo = 0;
double freq = (float) 1/(2*0.000058);

volatile double echo_flag = 0;

volatile double dist = 0;
char str[300];


void echo_callback(void){
		
	if (!echo_flag) {
		RTT_init(freq, 0, 0);	
		echo_flag = 1;
		
	} 
	
	else {
		echo_flag = 0;
		
		tempo = rtt_read_timer_value(RTT);
	}
}

void init(void) {
	sysclk_init();
	oled_init();
	
	WDT->WDT_MR = WDT_MR_WDDIS;
	
	pmc_enable_periph_clk(ECHO_PI_ID);
	pio_set_input(ECHO_PI,ECHO_PI_IDX_MASK,PIO_DEFAULT);
	
	pmc_enable_periph_clk(TRIG_PI_ID);
	pio_configure(TRIG_PI, PIO_OUTPUT_0,TRIG_PI_IDX_MASK, PIO_DEFAULT);
	
	pio_handler_set(ECHO_PI,
		ECHO_PI_ID,
		ECHO_PI_IDX_MASK,
		PIO_IT_EDGE,
		echo_callback);
		
	pio_enable_interrupt(ECHO_PI, ECHO_PI_IDX_MASK);
	pio_get_interrupt_status(ECHO_PI);
	
	NVIC_EnableIRQ(ECHO_PI_ID);
	NVIC_SetPriority(ECHO_PI_ID, 4); // Prioridade 4

}

void io_init(){
	board_init();
	sysclk_init();
	init();
	delay_init();
	gfx_mono_ssd1306_init();
}

int main (void)
{

	io_init();
	while(1) {
			
			if (!pio_get(BUT_PI1,PIO_INPUT, BUT_PI1_IDX_MASK)) {
					
					pio_set(TRIG_PI,TRIG_PI_IDX_MASK);
					delay_us(10);
					pio_clear(TRIG_PI,TRIG_PI_IDX_MASK);
					
					pin_toggle(LED_PI1, LED_PI1_IDX_MASK);
					delay_ms(300);
					pin_toggle(LED_PI1, LED_PI1_IDX_MASK);
				}
			
			
			if (tempo != 0) {
				
				double  t = (float) tempo/freq;
				float dist = (340*t*100.0)/2.0;
				gfx_mono_ssd1306_init();
				sprintf(str, "%6.2lf", dist);
				gfx_mono_draw_string(str, 0,16, &sysfont);
				tempo = 0;				
			}
			
	}
}