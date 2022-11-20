#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/************************************************************************/
/* defines                                                              */
/************************************************************************/

#define LED_PIO           PIOC                
#define LED_PIO_ID        ID_PIOC                 
#define LED_PIO_IDX       8                 
#define LED_PIO_IDX_MASK  (1 << LED_PIO_IDX)  

//Configuracoes dos LEDs do display
#define LED_ONE PIOA
#define LED_ONE_ID ID_PIOA
#define LED_ONE_IDX 0
#define LED_ONE_IDX_MASK (1u << LED_ONE_IDX)

#define LED_TWO PIOC
#define LED_TWO_ID ID_PIOC
#define LED_TWO_IDX 30
#define LED_TWO_IDX_MASK (1u << LED_TWO_IDX)

#define LED_THREE PIOB
#define LED_THREE_ID ID_PIOB
#define LED_THREE_IDX 2
#define LED_THREE_IDX_MASK (1u << LED_THREE_IDX)

#define BUT_ONE	PIOD
#define BUT_ONE_ID ID_PIOD
#define BUT_ONE_IDX 28
#define BUT_ONE_IDX_MASK (1u << BUT_ONE_IDX)


typedef struct  {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t week;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
} calendar;

volatile char but_flag = 0;
volatile char flag_rtc_second = 0;
volatile char counter = 0;

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/

void init(void);
void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);
void pin_toggle(Pio *pio, uint32_t mask);
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type);
void but_one_callback(void);
void gfx_mono_draw_string(const char *str, const gfx_coord_t x, const gfx_coord_t y, const struct font *font);

/************************************************************************/
/* Handlers                                                             */
/************************************************************************/

/**
*  Interrupt handler for TC1 interrupt.
*/
void TC1_Handler(void) {
	/**
	* Devemos indicar ao TC que a interrup��o foi satisfeita.
	* Isso � realizado pela leitura do status do perif�rico
	**/
	volatile uint32_t status = tc_get_status(TC0, 1);

	/** Muda o estado do LED (pisca) **/
	pin_toggle(LED_PIO, LED_PIO_IDX_MASK);  
}

void TC2_Handler(void) {
	/**
	* Devemos indicar ao TC que a interrup��o foi satisfeita.
	* Isso � realizado pela leitura do status do perif�rico
	**/
	volatile uint32_t status = tc_get_status(TC0, 2);

	/** Muda o estado do LED (pisca) **/
	pin_toggle(LED_ONE, LED_ONE_IDX_MASK);  
}

void TC3_Handler(void) {

	volatile uint32_t status = tc_get_status(TC1, 0);
	counter++;
	
}

void TC4_Handler(void) {
	/**
	* Devemos indicar ao TC que a interrup��o foi satisfeita.
	* Isso � realizado pela leitura do status do perif�rico
	**/
	volatile uint32_t status = tc_get_status(TC1, 1);

	/** Muda o estado do LED (pisca) **/
	pin_toggle(LED_THREE, LED_THREE_IDX_MASK);  
}

void RTT_Handler(void) {
	uint32_t ul_status;

	/* Get RTT status - ACK */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		RTT_init(0, 4, RTT_MR_RTTINCIEN);
	}
	
	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
		pin_toggle(LED_TWO, LED_TWO_IDX_MASK);    // BLINK Led
	}

}

void RTC_Handler(void) {
	uint32_t ul_status = rtc_get_status(RTC);
	
	/* second tick */
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		// o c�digo para irq de segundo vem aqui
		flag_rtc_second = 1;
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
	}

	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

void but_one_callback(void){
	but_flag = 1;
}

/************************************************************************/
/* Funcoes                                                              */
/************************************************************************/

void LED_init(int estado, Pio *pio, int id, uint32_t mask) {
	pmc_enable_periph_clk(id);
	pio_set_output(pio, mask, estado, 0, 0);
};

void pin_toggle(Pio *pio, uint32_t mask) {
	if(pio_get_output_data_status(pio, mask))
	pio_clear(pio, mask);
	else
	pio_set(pio,mask);
}

void init(void){
	sysclk_init();

	WDT->WDT_MR = WDT_MR_WDDIS;	
	
	pmc_enable_periph_clk(BUT_ONE_ID);
	
	pio_configure(BUT_ONE, PIO_INPUT, BUT_ONE_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT_ONE, BUT_ONE_IDX_MASK, 60);
	
	pio_handler_set(BUT_ONE,
	BUT_ONE_ID,
	BUT_ONE_IDX_MASK,
	PIO_IT_RISE_EDGE,
	but_one_callback);
	
	pio_enable_interrupt(BUT_ONE, BUT_ONE_IDX_MASK);
	pio_get_interrupt_status(BUT_ONE);
	
	NVIC_EnableIRQ(BUT_ONE_ID);
	NVIC_SetPriority(BUT_ONE_ID, 4); // Prioridade 4
}
;

static float get_time_rtt(){
	uint ul_previous_time = rtt_read_timer_value(RTT);
}

void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	uint32_t channel = 1;

	/* Configura o PMC */
	/* O TimerCounter � meio confuso
	o uC possui 3 TCs, cada TC possui 3 canais
	TC0 : ID_TC0, ID_TC1, ID_TC2
	TC1 : ID_TC3, ID_TC4, ID_TC5
	TC2 : ID_TC6, ID_TC7, ID_TC8
	*/
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  freq hz e interrup�c�o no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrup�c�o no TC canal 0 */
	/* Interrup��o no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);
}

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);
	
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
	
}

void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type) {
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.second);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 4);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc,  irq_type);
}

/************************************************************************/
/* Main                                                                 */
/************************************************************************/

int main (void)
{
	char str[25];
	
	board_init();
	sysclk_init();
	delay_init();
	init();
	 gfx_mono_ssd1306_init();

	LED_init(0,LED_PIO,LED_PIO_ID,LED_PIO_IDX_MASK);
	LED_init(0,LED_ONE,LED_ONE_ID,LED_ONE_IDX_MASK);
	LED_init(0,LED_TWO,LED_TWO_ID,LED_TWO_IDX_MASK);
	LED_init(1,LED_THREE,LED_THREE_ID,LED_THREE_IDX_MASK);
	
	 TC_init(TC0, ID_TC1, 1, 5);
	 TC_init(TC0, ID_TC2, 2, 4);
	 
	 tc_start(TC0, 1);
	 tc_start(TC0, 2);  
	 
	 RTT_init(4, 16, RTT_MR_ALMIEN);
	 
	  calendar rtc_initial = {2018, 3, 19, 12, 15, 45 ,1};
	  RTC_init(RTC, ID_RTC, rtc_initial, RTC_SR_SEC);
	  
	  uint32_t current_hour, current_min, current_sec;
	  uint32_t current_year, current_month, current_day, current_week;
	  rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
	  rtc_get_date(RTC, &current_year, &current_month, &current_day, &current_week);	  

	while(1) {
		if (flag_rtc_second == 1){
			rtc_get_date(RTC, &current_year, &current_month, &current_day, &current_week);
			rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
			sprintf(str, "%02d:%02d:%02d", current_hour, current_min, current_sec);
			gfx_mono_draw_string(str, 0, 0, &sysfont);
		}
		
		if (but_flag == 1){
			TC_init(TC1, ID_TC3, 0, 1);
			tc_start(TC1, 0);
			
			while(counter < 21){
				rtc_get_date(RTC, &current_year, &current_month, &current_day, &current_week);
				rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
				sprintf(str, "%02d:%02d:%02d", current_hour, current_min, current_sec);
				gfx_mono_draw_string(str, 0, 0, &sysfont);
			}
			tc_stop(TC1, 0);
			TC_init(TC1, ID_TC4, 1, 10);
			tc_start(TC1, 1);
			
			but_flag = 0;
		}
		
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
	}
}
