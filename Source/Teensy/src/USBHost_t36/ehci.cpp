/* USB EHCI Host for Teensy 3.6
 * Copyright 2017 Paul Stoffregen (paul@pjrc.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <Arduino.h>
#include "USBHost_t36.h" // Read this header first for key info

// All USB EHCI controller hardware access is done from this file's code.
// Hardware services are made available to the rest of this library by
// three structures:
//
//   Pipe_t: Every USB endpoint is accessed by a pipe.  new_Pipe()
//     sets up the EHCI to support the pipe/endpoint, and delete_Pipe()
//     removes this configuration.
//
//   Transfer_t: These are used for all communication.  Data transfers
//     are placed into work queues, to be executed by the EHCI in
//     the future.  Transfer_t only manages data.  The actual data
//     is stored in a separate buffer (usually from a device driver)
//     which is referenced from Transfer_t.  All data transfer is queued,
//     never done with blocking functions that wait.  When transfers
//     complete, a driver-supplied callback function is called to notify
//     the driver.
//
//   USBDriverTimer: Some drivers require timers.  These allow drivers
//     to share the hardware timer, with each USBDriverTimer object
//     able to schedule a callback function a configurable number of
//     microseconds in the future.
//
// In addition to these 3 services, the EHCI interrupt also responds
// to changes on the main port, creating and deleting the root device.
// See enumeration.cpp for all device-level code.

// Size of the periodic list, in milliseconds.  This determines the
// slowest rate we can poll interrupt endpoints.  Each entry uses
// 12 bytes (4 for a pointer, 8 for bandwidth management).
// Supported values: 8, 16, 32, 64, 128, 256, 512, 1024
#if defined(USBHS_PERIODIC_LIST_SIZE)
#define PERIODIC_LIST_SIZE (USBHS_PERIODIC_LIST_SIZE)
#else
#define PERIODIC_LIST_SIZE  32
#endif

// The EHCI periodic schedule, used for interrupt pipes/endpoints
static uint32_t periodictable[PERIODIC_LIST_SIZE] __attribute__ ((aligned(4096), used));
static uint8_t  uframe_bandwidth[PERIODIC_LIST_SIZE*8];

// State of the 1 and only physical USB host port on Teensy 3.6
static uint8_t  port_state;
#define PORT_STATE_DISCONNECTED   0
#define PORT_STATE_DEBOUNCE       1
#define PORT_STATE_RESET          2
#define PORT_STATE_RECOVERY       3
#define PORT_STATE_ACTIVE         4

// The device currently connected, or NULL when no device
static Device_t   *rootdev=NULL;

// List of all queued transfers in the asychronous schedule (control & bulk).
// When the EHCI completes these transfers, this list is how we locate them
// in memory.
static Transfer_t *async_followup_first=NULL;
static Transfer_t *async_followup_last=NULL;

// List of all queued transfers in the asychronous schedule (interrupt endpoints)
// When the EHCI completes these transfers, this list is how we locate them
// in memory.
static Transfer_t *periodic_followup_first=NULL;
static Transfer_t *periodic_followup_last=NULL;

// List of all pending timers.  This double linked list is stored in
// chronological order.  Each timer is stored with the number of
// microseconds which need to elapsed from the prior timer on this
// list, to allow efficient servicing from the timer interrupt.
static USBDriverTimer *active_timers=NULL;


static void init_qTD(volatile Transfer_t *t, void *buf, uint32_t len,
              uint32_t pid, uint32_t data01, bool irq);
static void add_to_async_followup_list(Transfer_t *first, Transfer_t *last);
static void remove_from_async_followup_list(Transfer_t *transfer);
static void add_to_periodic_followup_list(Transfer_t *first, Transfer_t *last);
static void remove_from_periodic_followup_list(Transfer_t *transfer);

#define print   USBHost::print_
#define println USBHost::println_

void USBHost::begin()
{
#if defined(__MK66FX1M0__)
	// Teensy 3.6 has USB host power controlled by PTE6
	PORTE_PCR6 = PORT_PCR_MUX(1);
	GPIOE_PDDR |= (1<<6);
	GPIOE_PSOR = (1<<6); // turn on USB host power
	delay(10);
	println("sizeof Device = ", sizeof(Device_t));
	println("sizeof Pipe = ", sizeof(Pipe_t));
	println("sizeof Transfer = ", sizeof(Transfer_t));
	if ((sizeof(Pipe_t) & 0x1F) || (sizeof(Transfer_t) & 0x1F)) {
		println("ERROR: Pipe_t & Transfer_t must be multiples of 32 bytes!");
		while (1) ; // die here
	}

	// configure the MPU to allow USBHS DMA to access memory
	MPU_RGDAAC0 |= 0x30000000;
	//println("MPU_RGDAAC0 = ", MPU_RGDAAC0, HEX);

	// turn on clocks
	MCG_C1 |= MCG_C1_IRCLKEN;  // enable MCGIRCLK 32kHz
	OSC0_CR |= OSC_ERCLKEN;
	SIM_SOPT2 |= SIM_SOPT2_USBREGEN; // turn on USB regulator
	SIM_SOPT2 &= ~SIM_SOPT2_USBSLSRC; // use IRC for slow clock
	println("power up USBHS PHY");
	SIM_USBPHYCTL |= SIM_USBPHYCTL_USBDISILIM; // disable USB current limit
	//SIM_USBPHYCTL = SIM_USBPHYCTL_USBDISILIM | SIM_USBPHYCTL_USB3VOUTTRG(6); // pg 237
	SIM_SCGC3 |= SIM_SCGC3_USBHSDCD | SIM_SCGC3_USBHSPHY | SIM_SCGC3_USBHS;
	USBHSDCD_CLOCK = 33 << 2;
	//print("init USBHS PHY & PLL");
	// init process: page 1681-1682
	USBPHY_CTRL_CLR = (USBPHY_CTRL_SFTRST | USBPHY_CTRL_CLKGATE); // // CTRL pg 1698
	USBPHY_CTRL_SET = USBPHY_CTRL_ENUTMILEVEL2 | USBPHY_CTRL_ENUTMILEVEL3;
	//USBPHY_CTRL_SET = USBPHY_CTRL_FSDLL_RST_EN; // TODO: what does this do??
	USBPHY_TRIM_OVERRIDE_EN_SET = 1;
	USBPHY_PLL_SIC = USBPHY_PLL_SIC_PLL_POWER | USBPHY_PLL_SIC_PLL_ENABLE |
		USBPHY_PLL_SIC_PLL_DIV_SEL(1) | USBPHY_PLL_SIC_PLL_EN_USB_CLKS;
	// wait for the PLL to lock
	int pll_count=0;
	while ((USBPHY_PLL_SIC & USBPHY_PLL_SIC_PLL_LOCK) == 0) {
		pll_count++;
	}
	//println("PLL locked, waited ", pll_count);

	// turn on power to PHY
	USBPHY_PWD = 0;

	// sanity check, connect 470K pullup & 100K pulldown and watch D+ voltage change
	//USBPHY_ANACTRL_CLR = (1<<10); // turn off both 15K pulldowns... works! :)

	// sanity check, output clocks on pin 9 for testing
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(3); // LPO 1kHz
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(2); // Flash
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(6); // XTAL
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(7); // IRC 48MHz
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(4); // MCGIRCLK
	//CORE_PIN9_CONFIG = PORT_PCR_MUX(5);  // CLKOUT on PTC3 Alt5 (Arduino pin 9)


#elif defined(__IMXRT1052__) || defined(__IMXRT1062__)
	// Teensy 4.0 PLL & USB PHY powerup
	while (1) {
		uint32_t n = CCM_ANALOG_PLL_USB2;
		if (n & CCM_ANALOG_PLL_USB2_DIV_SELECT) {
			CCM_ANALOG_PLL_USB2_CLR = 0xC000; // get out of 528 MHz mode
			CCM_ANALOG_PLL_USB2_SET = CCM_ANALOG_PLL_USB2_BYPASS;
			CCM_ANALOG_PLL_USB2_CLR = CCM_ANALOG_PLL_USB2_POWER |
				CCM_ANALOG_PLL_USB2_DIV_SELECT |
				CCM_ANALOG_PLL_USB2_ENABLE |
				CCM_ANALOG_PLL_USB2_EN_USB_CLKS;
			continue;
		}
		if (!(n & CCM_ANALOG_PLL_USB2_ENABLE)) {
			CCM_ANALOG_PLL_USB2_SET = CCM_ANALOG_PLL_USB2_ENABLE; // enable
			continue;
		}
		if (!(n & CCM_ANALOG_PLL_USB2_POWER)) {
			CCM_ANALOG_PLL_USB2_SET = CCM_ANALOG_PLL_USB2_POWER; // power up
			continue;
		}
		if (!(n & CCM_ANALOG_PLL_USB2_LOCK)) {
			continue; // wait for lock
		}
		if (n & CCM_ANALOG_PLL_USB2_BYPASS) {
			CCM_ANALOG_PLL_USB2_CLR = CCM_ANALOG_PLL_USB2_BYPASS; // turn off bypass
			continue;
		}
		if (!(n & CCM_ANALOG_PLL_USB2_EN_USB_CLKS)) {
			CCM_ANALOG_PLL_USB2_SET = CCM_ANALOG_PLL_USB2_EN_USB_CLKS; // enable
			continue;
		}
		println("USB2 PLL running");
		break; // USB2 PLL up and running
	}
	// turn on USB clocks (should already be on)
	CCM_CCGR6 |= CCM_CCGR6_USBOH3(CCM_CCGR_ON);
	// turn on USB2 PHY
	USBPHY2_CTRL_CLR = USBPHY_CTRL_SFTRST | USBPHY_CTRL_CLKGATE;
	USBPHY2_CTRL_SET = USBPHY_CTRL_ENUTMILEVEL2 | USBPHY_CTRL_ENUTMILEVEL3;
	USBPHY2_PWD = 0;
	#ifdef ARDUINO_TEENSY41
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_40 = 5;
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_40 = 0x0008; // slow speed, weak 150 ohm drive
	GPIO8_GDIR |= 1<<26;
	GPIO8_DR_SET = 1<<26;
	#endif
#endif
	delay(10);

	// now with the PHY up and running, start up USBHS
	//print("begin ehci reset");
	USBHS_USBCMD |= USBHS_USBCMD_RST;
	int reset_count = 0;
	while (USBHS_USBCMD & USBHS_USBCMD_RST) {
		reset_count++;
	}
	println(" reset waited ", reset_count);

	init_Device_Pipe_Transfer_memory();
	for (int i=0; i < PERIODIC_LIST_SIZE; i++) {
		periodictable[i] = 1;
	}
	memset(uframe_bandwidth, 0, sizeof(uframe_bandwidth));
	port_state = PORT_STATE_DISCONNECTED;

	USBHS_USB_SBUSCFG = 1; //  System Bus Interface Configuration

	// turn on the USBHS controller
	//USBHS_USBMODE = USBHS_USBMODE_TXHSD(5) | USBHS_USBMODE_CM(3); // host mode
	USBHS_USBMODE = USBHS_USBMODE_CM(3); // host mode
	USBHS_USBINTR = 0;
	USBHS_PERIODICLISTBASE = (uint32_t)periodictable;
	USBHS_FRINDEX = 0;
	USBHS_ASYNCLISTADDR = 0;
	USBHS_USBCMD = USBHS_USBCMD_ITC(1) | USBHS_USBCMD_RS |
		USBHS_USBCMD_ASP(3) | USBHS_USBCMD_ASPE | USBHS_USBCMD_PSE |
		#if PERIODIC_LIST_SIZE == 8
		USBHS_USBCMD_FS2 | USBHS_USBCMD_FS(3);
		#elif PERIODIC_LIST_SIZE == 16
		USBHS_USBCMD_FS2 | USBHS_USBCMD_FS(2);
		#elif PERIODIC_LIST_SIZE == 32
		USBHS_USBCMD_FS2 | USBHS_USBCMD_FS(1);
		#elif PERIODIC_LIST_SIZE == 64
		USBHS_USBCMD_FS2 | USBHS_USBCMD_FS(0);
		#elif PERIODIC_LIST_SIZE == 128
		USBHS_USBCMD_FS(3);
		#elif PERIODIC_LIST_SIZE == 256
		USBHS_USBCMD_FS(2);
		#elif PERIODIC_LIST_SIZE == 512
		USBHS_USBCMD_FS(1);
		#elif PERIODIC_LIST_SIZE == 1024
		USBHS_USBCMD_FS(0);
		#else
		#error "Unsupported PERIODIC_LIST_SIZE"
		#endif

	// turn on the USB port
	//USBHS_PORTSC1 = USBHS_PORTSC_PP;
	USBHS_PORTSC1 |= USBHS_PORTSC_PP;
	//USBHS_PORTSC1 |= USBHS_PORTSC_PFSC; // force 12 Mbit/sec
	//USBHS_PORTSC1 |= USBHS_PORTSC_PHCD; // phy off

	println("USBHS_ASYNCLISTADDR = ", USBHS_ASYNCLISTADDR, HEX);
	println("USBHS_PERIODICLISTBASE = ", USBHS_PERIODICLISTBASE, HEX);
	println("periodictable = ", (uint32_t)periodictable, HEX);

	// enable interrupts, after this point interruts to all the work
	attachInterruptVector(IRQ_USBHS, isr);
	NVIC_ENABLE_IRQ(IRQ_USBHS);
	USBHS_USBINTR = USBHS_USBINTR_PCE | USBHS_USBINTR_TIE0 | USBHS_USBINTR_TIE1;
	USBHS_USBINTR |= USBHS_USBINTR_UEE | USBHS_USBINTR_SEE;
	USBHS_USBINTR |= USBHS_USBINTR_UPIE | USBHS_USBINTR_UAIE;

}


// EHCI registers         page  default
// --------------         ----  -------
// USBHS_USBCMD           1599  00080000  USB Command
// USBHS_USBSTS           1602  00000000  USB Status
// USBHS_USBINTR          1606  00000000  USB Interrupt Enable
// USBHS_FRINDEX          1609  00000000  Frame Index Register
// USBHS_PERIODICLISTBASE 1610  undefine  Periodic Frame List Base Address
// USBHS_ASYNCLISTADDR    1612  undefine  Asynchronous List Address
// USBHS_PORTSC1          1619  00002000  Port Status and Control
// USBHS_USBMODE          1629  00005000  USB Mode
// USBHS_GPTIMERnCTL      1591  00000000  General Purpose Timer n Control

// PORT_STATE_DISCONNECTED   0
// PORT_STATE_DEBOUNCE       1
// PORT_STATE_RESET          2
// PORT_STATE_RECOVERY       3
// PORT_STATE_ACTIVE         4


void USBHost::isr()
{
	uint32_t stat = USBHS_USBSTS;
	USBHS_USBSTS = stat; // clear pending interrupts
	//stat &= USBHS_USBINTR; // mask away unwanted interrupts
#if 0
	println();
	println("ISR: ", stat, HEX);
	//if (stat & USBHS_USBSTS_UI)  println(" USB Interrupt");
	if (stat & USBHS_USBSTS_UEI) println(" USB Error");
	if (stat & USBHS_USBSTS_PCI) println(" Port Change");
	//if (stat & USBHS_USBSTS_FRI) println(" Frame List Rollover");
	if (stat & USBHS_USBSTS_SEI) println(" System Error");
	//if (stat & USBHS_USBSTS_AAI) println(" Async Advance (doorbell)");
	if (stat & USBHS_USBSTS_URI) println(" Reset Recv");
	//if (stat & USBHS_USBSTS_SRI) println(" SOF");
	if (stat & USBHS_USBSTS_SLI) println(" Suspend");
	if (stat & USBHS_USBSTS_HCH) println(" Host Halted");
	//if (stat & USBHS_USBSTS_RCL) println(" Reclamation");
	//if (stat & USBHS_USBSTS_PS)  println(" Periodic Sched En");
	//if (stat & USBHS_USBSTS_AS)  println(" Async Sched En");
	if (stat & USBHS_USBSTS_NAKI) println(" NAK");
	if (stat & USBHS_USBSTS_UAI) println(" USB Async");
	if (stat & USBHS_USBSTS_UPI) println(" USB Periodic");
	if (stat & USBHS_USBSTS_TI0) println(" Timer0");
	if (stat & USBHS_USBSTS_TI1) println(" Timer1");
#endif

	if (stat & USBHS_USBSTS_UAI) { // completed qTD(s) from the async schedule
		//println("Async Followup");
		//print(async_followup_first, async_followup_last);
		Transfer_t *p = async_followup_first;
		while (p) {
			if (followup_Transfer(p)) {
				// transfer completed
				Transfer_t *next = p->next_followup;
				remove_from_async_followup_list(p);
				free_Transfer(p);
				p = next;
			} else {
				// transfer still pending
				p = p->next_followup;
			}
		}
		//print(async_followup_first, async_followup_last);
	}
	if (stat & USBHS_USBSTS_UPI) { // completed qTD(s) from the periodic schedule
		//println("Periodic Followup");
		Transfer_t *p = periodic_followup_first;
		while (p) {
			if (followup_Transfer(p)) {
				// transfer completed
				Transfer_t *next = p->next_followup;
				remove_from_periodic_followup_list(p);
				free_Transfer(p);
				p = next;
			} else {
				// transfer still pending
				p = p->next_followup;
			}
		}
	}
	if (stat & USBHS_USBSTS_UEI) {
		followup_Error();
	}

	if (stat & USBHS_USBSTS_PCI) { // port change detected
		const uint32_t portstat = USBHS_PORTSC1;
		println("port change: ", portstat, HEX);
		USBHS_PORTSC1 = portstat | (USBHS_PORTSC_OCC|USBHS_PORTSC_PEC|USBHS_PORTSC_CSC);
		if (portstat & USBHS_PORTSC_OCC) {
			println("  overcurrent change");
		}
		if (portstat & USBHS_PORTSC_CSC) {
			if (portstat & USBHS_PORTSC_CCS) {
				println("    connect");
				if (port_state == PORT_STATE_DISCONNECTED
				  || port_state == PORT_STATE_DEBOUNCE) {
					// 100 ms debounce (USB 2.0: TATTDB, page 150 & 188)
					port_state = PORT_STATE_DEBOUNCE;
					USBHS_GPTIMER0LD = 100000; // microseconds
					USBHS_GPTIMER0CTL =
						USBHS_GPTIMERCTL_RST | USBHS_GPTIMERCTL_RUN;
					stat &= ~USBHS_USBSTS_TI0;
				}
			} else {
				println("    disconnect");
				port_state = PORT_STATE_DISCONNECTED;
				USBPHY_CTRL_CLR = USBPHY_CTRL_ENHOSTDISCONDETECT;
				disconnect_Device(rootdev);
				rootdev = NULL;
			}
		}
		if (portstat & USBHS_PORTSC_PEC) {
			// PEC bit only detects disable
			println("  disable");
		} else if (port_state == PORT_STATE_RESET && portstat & USBHS_PORTSC_PE) {
			println("  port enabled");
			port_state = PORT_STATE_RECOVERY;
			// 10 ms reset recover (USB 2.0: TRSTRCY, page 151 & 188)
			USBHS_GPTIMER0LD = 10000; // microseconds
			USBHS_GPTIMER0CTL = USBHS_GPTIMERCTL_RST | USBHS_GPTIMERCTL_RUN;
			if (USBHS_PORTSC1 & USBHS_PORTSC_HSP) {
				// turn on high-speed disconnect detector
				USBPHY_CTRL_SET = USBPHY_CTRL_ENHOSTDISCONDETECT;
			}
		}
		if (portstat & USBHS_PORTSC_FPR) {
			println("  force resume");

		}
	}
	if (stat & USBHS_USBSTS_TI0) { // timer 0 - used for built-in port events
		//println("timer0");
		if (port_state == PORT_STATE_DEBOUNCE) {
			port_state = PORT_STATE_RESET;
			// Since we have only 1 port, no other device can
			// be in reset or enumeration.  If multiple ports
			// are ever supported, we would need to remain in
			// debounce if any other port was resetting or
			// enumerating a device.
			USBHS_PORTSC1 |= USBHS_PORTSC_PR; // begin reset sequence
			println("  begin reset");
		} else if (port_state == PORT_STATE_RECOVERY) {
			port_state = PORT_STATE_ACTIVE;
			println("  end recovery");
			//  HCSPARAMS  TTCTRL  page 1671
			uint32_t speed = (USBHS_PORTSC1 >> 26) & 3;
			rootdev = new_Device(speed, 0, 0);
		}
	}
	if (stat & USBHS_USBSTS_TI1) { // timer 1 - used for USBDriverTimer
		//println("timer1");
		USBDriverTimer *list = NULL, *last = NULL;
		USBDriverTimer *timer = active_timers;
		while (timer) {
			USBDriverTimer *next = timer->next;
			active_timers = next;
			// create a list of all timer_event() callbacks to do
			if (list == NULL) {
				list = timer;
				last = timer;
			} else {
				last->next = timer;
				last = timer;
			}
			timer->next = NULL;
			if (!next) break;
			next->prev = NULL;
			if (next->usec >= 5) { // TODO: is 5us a safe minimum?
				USBHS_GPTIMER1LD = next->usec - 1;
				USBHS_GPTIMER1CTL = USBHS_GPTIMERCTL_RST | USBHS_GPTIMERCTL_RUN;
				break;
			} else {
				// next timer will be too soon for the hardware timer, so
				// allow the next loop iteration to add it to the list
				if (next->usec > 0 && next->next != NULL) {
					// when we "lose" microseconds by calling early,
					// adjust future timers so errors don't accumulate
					next->next->usec += next->usec;
				}
				timer = next;
			}
		}
		// do all callbacks after hardware timer is started, so time spent by
		// the callback functions can't delay starting the hardware timer
		while (list) {
			USBDriverTimer *next = list->next;
			list->prev = NULL;
			list->next = NULL;
			list->driver->timer_event(list);
			list = next;
		}
	}
}

void USBDriverTimer::start(uint32_t microseconds)
{
#if 0
	USBHost::print_("start_timer, us = ");
	USBHost::print_(microseconds);
	USBHost::print_(", driver = ");
	USBHost::print_((uint32_t)driver, HEX);
	USBHost::print_(", this = ");
	USBHost::println_((uint32_t)this, HEX);
#endif
	if (!driver) return;
	if (microseconds < 100) return; // minimum timer duration
	started_micros = micros();
	if (active_timers == NULL) {
		// schedule is empty, just add this timer
		usec = microseconds;
		next = NULL;
		prev = NULL;
		active_timers = this;
		USBHS_GPTIMER1LD = microseconds - 1;
		USBHS_GPTIMER1CTL = USBHS_GPTIMERCTL_RST | USBHS_GPTIMERCTL_RUN;
		return;
	}
	uint32_t remain = USBHS_GPTIMER1CTL & 0xFFFFFF;
	//USBHDBGSerial.print("remain = ");
	//USBHDBGSerial.println(remain);
	if (microseconds < remain) {
		// this timer event is before any on the schedule
		__disable_irq();
		USBHS_GPTIMER1CTL = 0;
		USBHS_USBSTS = USBHS_USBSTS_TI1; // TODO: UPI & UAI safety?!
		usec = microseconds;
		next = active_timers;
		prev = NULL;
		active_timers->usec = remain - microseconds;
		active_timers->prev = this;
		active_timers = this;
		USBHS_GPTIMER1LD = microseconds - 1;
		USBHS_GPTIMER1CTL = USBHS_GPTIMERCTL_RST | USBHS_GPTIMERCTL_RUN;
		__enable_irq();
		return;
	}
	// add this timer to the schedule, somewhere after the first timer
	microseconds -= remain;
	USBDriverTimer *list = active_timers;
	while (list->next) {
		list = list->next;
		if (microseconds < list->usec) {
			// add timer into middle of list
			list->usec -= microseconds;
			usec = microseconds;
			next = list;
			prev = list->prev;
			list->prev = this;
			prev->next = this;
			return;
		}
		microseconds -= list->usec;
	}
	// add timer to the end of the schedule
	usec = microseconds;
	next = NULL;
	prev = list;
	list->next = this;
}

void USBDriverTimer::stop()
{
	__disable_irq();
	if (active_timers) {
		if (active_timers == this) {
			USBHS_GPTIMER1CTL = 0;
			if (next) {
				uint32_t usec_til_next = USBHS_GPTIMER1CTL & 0xFFFFFF;
				usec_til_next += next->usec;
				next->usec = usec_til_next;
				USBHS_GPTIMER1LD = usec_til_next;
				USBHS_GPTIMER1CTL = USBHS_GPTIMERCTL_RST | USBHS_GPTIMERCTL_RUN;
				next->prev = NULL;
				active_timers = next;
			} else {
				active_timers = NULL;
			}
		} else {
			for (USBDriverTimer *t = active_timers->next; t; t = t->next) {
				if (t == this) {
					t->prev->next = t->next;
					if (t->next) {
						t->next->usec += t->usec;
						t->next->prev = t->prev;
					}
					break;
				}
			}
		}
	}
	__enable_irq();
}


static uint32_t QH_capabilities1(uint32_t nak_count_reload, uint32_t control_endpoint_flag,
	uint32_t max_packet_length, uint32_t head_of_list, uint32_t data_toggle_control,
	uint32_t speed, uint32_t endpoint_number, uint32_t inactivate, uint32_t address)
{
	return ( (nak_count_reload << 28) | (control_endpoint_flag << 27) |
		(max_packet_length << 16) | (head_of_list << 15) |
		(data_toggle_control << 14) | (speed << 12) | (endpoint_number << 8) |
		(inactivate << 7) | (address << 0) );
}

static uint32_t QH_capabilities2(uint32_t high_bw_mult, uint32_t hub_port_number,
	uint32_t hub_address, uint32_t split_completion_mask, uint32_t interrupt_schedule_mask)
{
        return ( (high_bw_mult << 30) | (hub_port_number << 23) | (hub_address << 16) |
		(split_completion_mask << 8) | (interrupt_schedule_mask << 0) );
}



// Create a new pipe.  It's QH is added to the async or periodic schedule,
// and a halt qTD is added to the QH, so we can grow the qTD list later.
//   dev:       device owning this pipe/endpoint
//   type:      0=control, 2=bulk, 3=interrupt
//   endpoint:  0 for control, 1-15 for bulk or interrupt
//   direction: 0=OUT, 1=IN  (unused for control)
//   maxlen:    maximum packet size
//   interval:  polling interval for interrupt, power of 2, unused if control or bulk
//
Pipe_t * USBHost::new_Pipe(Device_t *dev, uint32_t type, uint32_t endpoint,
	uint32_t direction, uint32_t maxlen, uint32_t interval)
{
	Pipe_t *pipe;
	Transfer_t *halt;
	uint32_t c=0, dtc=0;

	println("new_Pipe");
	pipe = allocate_Pipe();
	if (!pipe) return NULL;
	halt = allocate_Transfer();
	if (!halt) {
		free_Pipe(pipe);
		return NULL;
	}
	memset(pipe, 0, sizeof(Pipe_t));
	memset(halt, 0, sizeof(Transfer_t));
	halt->qtd.next = 1;
	halt->qtd.token = 0x40;
	pipe->device = dev;
	pipe->qh.next = (uint32_t)halt;
	pipe->qh.alt_next = 1;
	pipe->direction = direction;
	pipe->type = type;
	if (type == 3) {
		// interrupt transfers require bandwidth & microframe scheduling
		if (!allocate_interrupt_pipe_bandwidth(pipe, maxlen, interval)) {
			free_Transfer(halt);
			free_Pipe(pipe);
			return NULL;
		}
	}
	if (endpoint > 0) {
		// if non-control pipe, update dev->data_pipes list
		Pipe_t *p = dev->data_pipes;
		if (p == NULL) {
			dev->data_pipes = pipe;
		} else {
			while (p->next) p = p->next;
			p->next = pipe;
		}
	}
	if (type == 0) {
		// control
		if (dev->speed < 2) c = 1;
		dtc = 1;
	} else if (type == 2) {
		// bulk
	} else if (type == 3) {
		// interrupt
		//pipe->qh.token = 0x80000000; // TODO: OUT starts with DATA0 or DATA1?
	}
	pipe->qh.capabilities[0] = QH_capabilities1(15, c, maxlen, 0,
		dtc, dev->speed, endpoint, 0, dev->address);
	pipe->qh.capabilities[1] = QH_capabilities2(1, dev->hub_port,
		dev->hub_address, pipe->complete_mask, pipe->start_mask);

	if (type == 0 || type == 2) {
		// control or bulk: add to async queue
		Pipe_t *list = (Pipe_t *)USBHS_ASYNCLISTADDR;
		if (list == NULL) {
			pipe->qh.capabilities[0] |= 0x8000; // H bit
			pipe->qh.horizontal_link = (uint32_t)&(pipe->qh) | 2; // 2=QH
			USBHS_ASYNCLISTADDR = (uint32_t)&(pipe->qh);
			USBHS_USBCMD |= USBHS_USBCMD_ASE; // enable async schedule
			//println("  first in async list");
		} else {
			// EHCI 1.0: section 4.8.1, page 72
			pipe->qh.horizontal_link = list->qh.horizontal_link;
			list->qh.horizontal_link = (uint32_t)&(pipe->qh) | 2;
			//println("  added to async list");
		}
	} else if (type == 3) {
		// interrupt: add to periodic schedule
		add_qh_to_periodic_schedule(pipe);
	}
	return pipe;
}



// Fill in the qTD fields (token & data)
//   t       the Transfer qTD to initialize
//   buf     data to transfer
//   len     length of data
//   pid     type of packet: 0=OUT, 1=IN, 2=SETUP
//   data01  value of DATA0/DATA1 toggle on 1st packet
//   irq     whether to generate an interrupt when transfer complete
//
static void init_qTD(volatile Transfer_t *t, void *buf, uint32_t len,
              uint32_t pid, uint32_t data01, bool irq)
{
	t->qtd.alt_next = 1; // 1=terminate
	if (data01) data01 = 0x80000000;
	t->qtd.token = data01 | (len << 16) | (irq ? 0x8000 : 0) | (pid << 8) | 0x80;
	uint32_t addr = (uint32_t)buf;
	t->qtd.buffer[0] = addr;
	addr &= 0xFFFFF000;
	t->qtd.buffer[1] = addr + 0x1000;
	t->qtd.buffer[2] = addr + 0x2000;
	t->qtd.buffer[3] = addr + 0x3000;
	t->qtd.buffer[4] = addr + 0x4000;
}



// Create a Control Transfer and queue it
//
bool USBHost::queue_Control_Transfer(Device_t *dev, setup_t *setup, void *buf, USBDriver *driver)
{
	Transfer_t *transfer, *data, *status;
	uint32_t status_direction;

	//println("new_Control_Transfer");
	if (setup->wLength > 16384) return false; // max 16K data for control
	transfer = allocate_Transfer();
	if (!transfer) {
		println("  error allocating setup transfer");
		return false;
	}
	status = allocate_Transfer();
	if (!status) {
		println("  error allocating status transfer");
		free_Transfer(transfer);
		return false;
	}
	if (setup->wLength > 0) {
		data = allocate_Transfer();
		if (!data) {
			println("  error allocating data transfer");
			free_Transfer(transfer);
			free_Transfer(status);
			return false;
		}
		uint32_t pid = (setup->bmRequestType & 0x80) ? 1 : 0;
		init_qTD(data, buf, setup->wLength, pid, 1, false);
		transfer->qtd.next = (uint32_t)data;
		data->qtd.next = (uint32_t)status;
		status_direction = pid ^ 1;
	} else {
		transfer->qtd.next = (uint32_t)status;
		status_direction = 1; // always IN, USB 2.0 page 226
	}
	//println("setup address ", (uint32_t)setup, HEX);
	init_qTD(transfer, setup, 8, 2, 0, false);
	init_qTD(status, NULL, 0, status_direction, 1, true);
	status->pipe = dev->control_pipe;
	status->buffer = buf;
	status->length = setup->wLength;
	status->setup.word1 = setup->word1;
	status->setup.word2 = setup->word2;
	status->driver = driver;
	status->qtd.next = 1;
	return queue_Transfer(dev->control_pipe, transfer);
}


// Create a Bulk or Interrupt Transfer and queue it
//
bool USBHost::queue_Data_Transfer(Pipe_t *pipe, void *buffer, uint32_t len, USBDriver *driver)
{
	Transfer_t *transfer, *data, *next;
	uint8_t *p = (uint8_t *)buffer;
	uint32_t count;
	bool last = false;
	
	// We always want to do this while the interrupt is disabled. 
	// But only re-enable if it was enabled coming in. 
	bool irq_was_enabled = NVIC_IS_ENABLED(IRQ_USBHS);
	NVIC_DISABLE_IRQ(IRQ_USBHS);

	// TODO: option for zero length packet?  Maybe in Pipe_t fields?

	//println("new_Data_Transfer");
	// allocate qTDs
	transfer = allocate_Transfer();
	if (!transfer) {
		if (irq_was_enabled) NVIC_ENABLE_IRQ(IRQ_USBHS);
		return false;
	}
	data = transfer;
	for (count=((len-1) >> 14); count; count--) {
		next = allocate_Transfer();
		if (!next) {
			// free already-allocated qTDs
			while (1) {
				next = (Transfer_t *)transfer->qtd.next;
				free_Transfer(transfer);
				if (transfer == data) break;
				transfer = next;
			}
			if (irq_was_enabled) NVIC_ENABLE_IRQ(IRQ_USBHS);
            return false;
        }
		data->qtd.next = (uint32_t)next;
		data = next;
	}
	// last qTD needs info for followup
	data->qtd.next = 1;
	data->pipe = pipe;
	data->buffer = buffer;
	data->length = len;
	data->setup.word1 = 0;
	data->setup.word2 = 0;
	data->driver = driver;
	// initialize all qTDs
	data = transfer;
	while (1) {
		uint32_t count = len;
		if (count > 16384) {
			count = 16384;
		} else {
			last = true;
		}
		init_qTD(data, p, count, pipe->direction, 0, last);
		if (last) break;
		p += count;
		len -= count;
		data = (Transfer_t *)(data->qtd.next);
	}
	bool return_value = queue_Transfer(pipe, transfer);
	if (irq_was_enabled) NVIC_ENABLE_IRQ(IRQ_USBHS);
	return return_value;
}


bool USBHost::queue_Transfer(Pipe_t *pipe, Transfer_t *transfer)
{
	// find halt qTD
	Transfer_t *halt = (Transfer_t *)(pipe->qh.next);
	while (!(halt->qtd.token & 0x40)) halt = (Transfer_t *)(halt->qtd.next);
	// transfer's token
	uint32_t token = transfer->qtd.token;
	// transfer becomes new halt qTD
	transfer->qtd.token = 0x40;
	// copy transfer non-token fields to halt
	halt->qtd.next = transfer->qtd.next;
	halt->qtd.alt_next = transfer->qtd.alt_next;
	halt->qtd.buffer[0] = transfer->qtd.buffer[0]; // TODO: optimize memcpy, all
	halt->qtd.buffer[1] = transfer->qtd.buffer[1]; //       fields except token
	halt->qtd.buffer[2] = transfer->qtd.buffer[2];
	halt->qtd.buffer[3] = transfer->qtd.buffer[3];
	halt->qtd.buffer[4] = transfer->qtd.buffer[4];
	halt->pipe = pipe;
	halt->buffer = transfer->buffer;
	halt->length = transfer->length;
	halt->setup = transfer->setup;
	halt->driver = transfer->driver;
	// find the last qTD we're adding
	Transfer_t *last = halt;
	while ((uint32_t)(last->qtd.next) != 1) last = (Transfer_t *)(last->qtd.next);
	// last points to transfer (which becomes new halt)
	last->qtd.next = (uint32_t)transfer;
	transfer->qtd.next = 1;
	// link all the new qTD by next_followup & prev_followup
	Transfer_t *prev = NULL;
	Transfer_t *p = halt;
	while (p->qtd.next != (uint32_t)transfer) {
		Transfer_t *next = (Transfer_t *)p->qtd.next;
		p->prev_followup = prev;
		p->next_followup = next;
		prev = p;
		p = next;
	}
	p->prev_followup = prev;
	p->next_followup = NULL;
	//print(halt, p);
	// add them to a followup list
	if (pipe->type == 0 || pipe->type == 2) {
		// control or bulk
		add_to_async_followup_list(halt, p);
	} else {
		// interrupt
		add_to_periodic_followup_list(halt, p);
	}
	// old halt becomes new transfer, this commits all new qTDs to QH
	halt->qtd.token = token;
	return true;
}

bool USBHost::followup_Transfer(Transfer_t *transfer)
{
	//print("  Followup ", (uint32_t)transfer, HEX);
	//println("    token=", transfer->qtd.token, HEX);

	if (!(transfer->qtd.token & 0x80)) {
		// TODO: check error status
		if (transfer->qtd.token & 0x8000) {
			// this transfer caused an interrupt
			if (transfer->pipe->callback_function) {
				// do the callback
				(*(transfer->pipe->callback_function))(transfer);
			}
		}
		// do callback function...
		//println("    completed");
		return true;
	}
	return false;
}

void USBHost::followup_Error(void)
{
	println("ERROR Followup");
	Transfer_t *p = async_followup_first;
	while (p) {
		if (followup_Transfer(p)) {
			// transfer completed
			Transfer_t *next = p->next_followup;
			remove_from_async_followup_list(p);
			println("    remove from followup list");
			if (p->qtd.token & 0x40) {
				Pipe_t *haltedpipe = p->pipe;
				free_Transfer(p);
				// traverse the rest of the list for unfinished work
				// from this halted pipe.  Remove from the followup
				// list and put onto our own temporary list
				Transfer_t *first = NULL;
				Transfer_t *last = NULL;
				p = next;
				while (p) {
					Transfer_t *next2 = p->next_followup;
					if (p->pipe == haltedpipe) {
						println("    stray halted ", (uint32_t)p, HEX);
						remove_from_async_followup_list(p);
						if (first == NULL) {
							first = p;
							last = p;
						} else {
							last->next_followup = p;
						}
						p->next_followup = NULL;
						if (next == p) next = next2;
					}
					p = next2;
				}
				// halted pipe (probably) still has unfinished transfers
				// find the halted pipe's dummy halt transfer
				p = (Transfer_t *)(haltedpipe->qh.next & ~0x1F);
				while (p && ((p->qtd.token & 0x40) == 0)) {
					print("  qtd: ", (uint32_t)p, HEX);
					print(", token=", (uint32_t)p->qtd.token, HEX);
					println(", next=", (uint32_t)p->qtd.next, HEX);
					p = (Transfer_t *)(p->qtd.next & ~0x1F);
				}
				if (p) {
					// unhalt the pipe, "forget" unfinished transfers
					// hopefully they're all on the list we made!
					println("  dummy halt: ", (uint32_t)p, HEX);
					haltedpipe->qh.next = (uint32_t)p;
					haltedpipe->qh.current = 0;
					haltedpipe->qh.token = 0;
				} else {
					println("  no dummy halt found, yikes!");
					// TODO: this should never happen, but what if it does?
				}

				// Do any driver callbacks belonging to the unfinished
				// transfers.  This is done last, after retoring the
				// pipe to a working state (if possible) so the driver
				// callback can use the pipe.
				p = first;
				while (p) {
					uint32_t token = p->qtd.token;
					if (token & 0x8000 && haltedpipe->callback_function) {
						// driver expects a callback
						p->qtd.token = token | 0x40;
						(*(p->pipe->callback_function))(p);
					}
					Transfer_t *next2 = p->next_followup;
					free_Transfer(p);
					p = next2;
				}
			} else {
				free_Transfer(p);
			}
			p = next;
		} else {
			// transfer still pending
			println("    remain on followup list");
			p = p->next_followup;
		}
	}
	// TODO: handle errors from periodic schedule!
}

static void add_to_async_followup_list(Transfer_t *first, Transfer_t *last)
{
	last->next_followup = NULL; // always add to end of list
	if (async_followup_last == NULL) {
		first->prev_followup = NULL;
		async_followup_first = first;
	} else {
		first->prev_followup = async_followup_last;
		async_followup_last->next_followup = first;
	}
	async_followup_last = last;
}

static void remove_from_async_followup_list(Transfer_t *transfer)
{
	Transfer_t *next = transfer->next_followup;
	Transfer_t *prev = transfer->prev_followup;
	if (prev) {
		prev->next_followup = next;
	} else {
		async_followup_first = next;
	}
	if (next) {
		next->prev_followup = prev;
	} else {
		async_followup_last = prev;
	}
}

static void add_to_periodic_followup_list(Transfer_t *first, Transfer_t *last)
{
	last->next_followup = NULL; // always add to end of list
	if (periodic_followup_last == NULL) {
		first->prev_followup = NULL;
		periodic_followup_first = first;
	} else {
		first->prev_followup = periodic_followup_last;
		periodic_followup_last->next_followup = first;
	}
	periodic_followup_last = last;
}

static void remove_from_periodic_followup_list(Transfer_t *transfer)
{
	Transfer_t *next = transfer->next_followup;
	Transfer_t *prev = transfer->prev_followup;
	if (prev) {
		prev->next_followup = next;
	} else {
		periodic_followup_first = next;
	}
	if (next) {
		next->prev_followup = prev;
	} else {
		periodic_followup_last = prev;
	}
}


static uint32_t max4(uint32_t n1, uint32_t n2, uint32_t n3, uint32_t n4)
{
	if (n1 > n2) {
		// can't be n2
		if (n1 > n3) {
			// can't be n3
			if (n1 > n4) return n1;
		} else {
			// can't be n1
			if (n3 > n4) return n3;
		}
	} else {
		// can't be n1
		if (n2 > n3) {
			// can't be n3
			if (n2 > n4) return n2;
		} else {
			// can't be n2
			if (n3 > n4) return n3;
		}
	}
	return n4;
}

static uint32_t round_to_power_of_two(uint32_t n, uint32_t maxnum)
{
	for (uint32_t pow2num=1; pow2num < maxnum; pow2num <<= 1) {
		if (n <= (pow2num | (pow2num >> 1))) return pow2num;
	}
	return maxnum;
}

// Allocate bandwidth for an interrupt pipe.  Given the packet size
// and other parameters, find the best place to schedule this pipe.
// Returns true if enough bandwidth is available, and the best
// frame offset, smask and cmask.  Or returns false if no group
// of microframes has enough bandwidth available.
//
//   pipe:
//     device->speed      [in]   0=full speed, 1=low speed, 2=high speed
//     direction          [in]   0=OUT, 1=IN
//     start_mask         [out]  uframes to start transfer
//     complete_mask      [out]  uframes to complete transfer (FS & LS only)
//     periodic_interval  [out]  fream repeat level: 1, 2, 4, 8... PERIODIC_LIST_SIZE
//     periodic_offset    [out]  frame repeat offset: 0 to periodic_interval-1
//   maxlen:              [in]   maximum packet length
//   interval:            [in]   polling interval: LS+FS: frames, HS: 2^(n-1) uframes
//
bool USBHost::allocate_interrupt_pipe_bandwidth(Pipe_t *pipe, uint32_t maxlen, uint32_t interval)
{
	println("allocate_interrupt_pipe_bandwidth");
	if (interval == 0) interval = 1;
	maxlen = (maxlen * 76459) >> 16; // worst case bit stuffing
	if (pipe->device->speed == 2) {
		// high speed 480 Mbit/sec
		println("  ep interval = ", interval);
		if (interval > 15) interval = 15;
		interval = 1 << (interval - 1);
		if (interval > PERIODIC_LIST_SIZE*8) interval = PERIODIC_LIST_SIZE*8;
		println("  interval = ", interval);
		uint32_t pinterval = interval >> 3;
		pipe->periodic_interval = (pinterval > 0) ? pinterval : 1;
		uint32_t stime = (55 + 32 + maxlen) >> 5; // time units: 32 bytes or 533 ns
		uint32_t best_offset = 0xFFFFFFFF;
		uint32_t best_bandwidth = 0xFFFFFFFF;
		for (uint32_t offset=0; offset < interval; offset++) {
			// for each possible uframe offset, find the worst uframe bandwidth
			uint32_t max_bandwidth = 0;
			for (uint32_t i=offset; i < PERIODIC_LIST_SIZE*8; i += interval) {
				uint32_t bandwidth = uframe_bandwidth[i] + stime;
				if (bandwidth > max_bandwidth) max_bandwidth = bandwidth;
			}
			// remember which uframe offset is the best
			if (max_bandwidth < best_bandwidth) {
				best_bandwidth = max_bandwidth;
				best_offset = offset;
			}
		}
		print(" best_bandwidth = ", best_bandwidth);
		//print(best_bandwidth);
		println(", at offset = ", best_offset);
		//println(best_offset);
		// a 125 us micro frame can fit 7500 bytes, or 234 of our 32-byte units
		// fail if the best found needs more than 80% (234 * 0.8) in any uframe
		if (best_bandwidth > 187) return false;
		// save essential bandwidth specs, for cleanup in delete_Pipe
		pipe->bandwidth_interval = interval;
		pipe->bandwidth_offset = best_offset;
		pipe->bandwidth_stime = stime;
		for (uint32_t i=best_offset; i < PERIODIC_LIST_SIZE*8; i += interval) {
			uframe_bandwidth[i] += stime;
		}
		if (interval == 1) {
			pipe->start_mask = 0xFF;
		} else if (interval == 2) {
			pipe->start_mask = 0x55 << (best_offset & 1);
		} else if (interval <= 4) {
			pipe->start_mask = 0x11 << (best_offset & 3);
		} else {
			pipe->start_mask = 0x01 << (best_offset & 7);
		}
		pipe->periodic_offset = best_offset >> 3;
		pipe->complete_mask = 0;
	} else {
		// full speed 12 Mbit/sec or low speed 1.5 Mbit/sec
		interval = round_to_power_of_two(interval, PERIODIC_LIST_SIZE);
		pipe->periodic_interval = interval;
		uint32_t stime, ctime;
		if (pipe->direction == 0) {
			// for OUT direction, SSPLIT will carry the data payload
			// TODO: how much time to SSPLIT & CSPLIT actually take?
			// they're not documented in 5.7 or 5.11.3.
			stime = (100 + 32 + maxlen) >> 5;
			ctime = (55 + 32) >> 5;
		} else {
			// for IN direction, data payload in CSPLIT
			stime = (40 + 32) >> 5;
			ctime = (70 + 32 + maxlen) >> 5;
		}
		// TODO: should we take Single-TT hubs into account, avoid
		// scheduling overlapping SSPLIT & CSPLIT to the same hub?
		// TODO: even if Multi-TT, do we need to worry about packing
		// too many into the same uframe?
		uint32_t best_shift = 0;
		uint32_t best_offset = 0xFFFFFFFF;
		uint32_t best_bandwidth = 0xFFFFFFFF;
		for (uint32_t offset=0; offset < interval; offset++) {
			// for each 1ms frame offset, compute the worst uframe usage
			uint32_t max_bandwidth = 0;
			for (uint32_t i=offset; i < PERIODIC_LIST_SIZE; i += interval) {
				for (uint32_t j=0; j <= 3; j++) { // max 3 without FSTN
					// at each location, find worst uframe usage
					// for SSPLIT+CSPLITs
					uint32_t n = (i << 3) + j;
					uint32_t bw1 = uframe_bandwidth[n+0] + stime;
					uint32_t bw2 = uframe_bandwidth[n+2] + ctime;
					uint32_t bw3 = uframe_bandwidth[n+3] + ctime;
					uint32_t bw4 = uframe_bandwidth[n+4] + ctime;
					max_bandwidth = max4(bw1, bw2, bw3, bw4);
					// remember the best usage found
					if (max_bandwidth < best_bandwidth) {
						best_bandwidth = max_bandwidth;
						best_offset = i;
						best_shift = j;
					}
				}
			}
		}
		print(" best_bandwidth = ", best_bandwidth);
		//println(best_bandwidth);
		print(", at offset = ", best_offset);
		//print(best_offset);
		println(", shift= ", best_shift);
		//println(best_shift);
		// a 125 us micro frame can fit 7500 bytes, or 234 of our 32-byte units
		// fail if the best found needs more than 80% (234 * 0.8) in any uframe
		if (best_bandwidth > 187) return false;
		// save essential bandwidth specs, for cleanup in delete_Pipe
		pipe->bandwidth_interval = interval;
		pipe->bandwidth_offset = best_offset;
		pipe->bandwidth_shift = best_shift;
		pipe->bandwidth_stime = stime;
		pipe->bandwidth_ctime = ctime;
		for (uint32_t i=best_offset; i < PERIODIC_LIST_SIZE; i += interval) {
			uint32_t n = (i << 3) + best_shift;
			uframe_bandwidth[n+0] += stime;
			uframe_bandwidth[n+2] += ctime;
			uframe_bandwidth[n+3] += ctime;
			uframe_bandwidth[n+4] += ctime;
		}
		pipe->start_mask = 0x01 << best_shift;
		pipe->complete_mask = 0x1C << best_shift;
		pipe->periodic_offset = best_offset;
	}
	return true;
}

// put a new pipe into the periodic schedule tree
// according to periodic_interval and periodic_offset
//
void USBHost::add_qh_to_periodic_schedule(Pipe_t *pipe)
{
	// quick hack for testing, just put it into the first table entry
	//println("add_qh_to_periodic_schedule: ", (uint32_t)pipe, HEX);
#if 0
	pipe->qh.horizontal_link = periodictable[0];
	periodictable[0] = (uint32_t)&(pipe->qh) | 2; // 2=QH
	println("init periodictable with ", periodictable[0], HEX);
#else
	uint32_t interval = pipe->periodic_interval;
	uint32_t offset = pipe->periodic_offset;
	//println("  interval = ", interval);
	//println("  offset =   ", offset);

	// By an interative miracle, hopefully make an inverted tree of EHCI figure 4-18, page 93
	for (uint32_t i=offset; i < PERIODIC_LIST_SIZE; i += interval) {
		//print("    old slot ", i);
		//print(": ");
		//print_qh_list((Pipe_t *)(periodictable[i] & 0xFFFFFFE0));
		uint32_t num = periodictable[i];
		Pipe_t *node = (Pipe_t *)(num & 0xFFFFFFE0);
		if ((num & 1) || ((num & 6) == 2 && node->periodic_interval < interval)) {
			//println("  add to slot ", i);
			pipe->qh.horizontal_link = num;
			periodictable[i] = (uint32_t)&(pipe->qh) | 2; // 2=QH
		} else {
			//println("  traverse list ", i);
			// TODO: skip past iTD, siTD when/if we support isochronous
			while (node->periodic_interval >= interval) {
				if (node == pipe) goto nextslot;
				//print("  num ", num, HEX);
				//print("  node ", (uint32_t)node, HEX);
				//println("->", node->qh.horizontal_link, HEX);
				if (node->qh.horizontal_link & 1) break;
				num = node->qh.horizontal_link;
				node = (Pipe_t *)(num & 0xFFFFFFE0);
			}
			Pipe_t *n = node;
			do {
				if (n == pipe) goto nextslot;
				n = (Pipe_t *)(n->qh.horizontal_link & 0xFFFFFFE0);
			} while (n != NULL);
			//print("  adding at node ", (uint32_t)node, HEX);
			//print(", num=", num, HEX);
			//println(", node->qh.horizontal_link=", node->qh.horizontal_link, HEX);
			pipe->qh.horizontal_link = node->qh.horizontal_link;
			node->qh.horizontal_link = (uint32_t)pipe | 2; // 2=QH
			// TODO: is it really necessary to keep doing the outer
			// loop?  Does adding it here satisfy all cases?  If so
			// we could avoid extra work by just returning here.
		}
		nextslot:
		//print("    new slot ", i);
		//print(": ");
		//print_qh_list((Pipe_t *)(periodictable[i] & 0xFFFFFFE0));
		{}
	}
#endif
#if 0
	println("Periodic Schedule:");
	for (uint32_t i=0; i < PERIODIC_LIST_SIZE; i++) {
		if (i < 10) print(" ");
		print(i);
		print(": ");
		print_qh_list((Pipe_t *)(periodictable[i] & 0xFFFFFFE0));
	}
#endif
}


void USBHost::delete_Pipe(Pipe_t *pipe)
{
	println("delete_Pipe ", (uint32_t)pipe, HEX);

	// halt pipe, find and free all Transfer_t

	// EHCI 1.0, 4.8.2 page 72: "Software should first deactivate
	// all active qTDs, wait for the queue head to go inactive"
	//
	// http://www.spinics.net/lists/linux-usb/msg131607.html
	// http://www.spinics.net/lists/linux-usb/msg131936.html
	//
	// In practice it's not feasible to wait for an active QH to become
	// inactive before removing it, for several reasons.  For one, the QH may
	// _never_ become inactive (if the endpoint NAKs indefinitely).  For
	// another, the procedure given in the spec (deactivate the qTDs on the
	// queue) is racy, since the controller can perform a new overlay or
	// writeback at any time.

	bool isasync = (pipe->type == 0 || pipe->type == 2);
	if (isasync) {
		// find the next QH in the async schedule loop
		Pipe_t *next = (Pipe_t *)(pipe->qh.horizontal_link & 0xFFFFFFE0);
		if (next == pipe) {
			// removing the only QH, so just shut down the async schedule
			println("  shut down async schedule");
			USBHS_USBCMD &= ~USBHS_USBCMD_ASE; // disable async schedule
			while (USBHS_USBSTS & USBHS_USBSTS_AS) ; // busy loop wait
			USBHS_ASYNCLISTADDR = 0;
		} else {
			// find the previous QH in the async schedule loop
			println("  remove QH from async schedule");
			Pipe_t *prev = next;
			while (1) {
				Pipe_t *n = (Pipe_t *)(prev->qh.horizontal_link & 0xFFFFFFE0);
				if (n == pipe) break;
				prev = n;
			}
			// if removing the one with H bit, set another
			if (pipe->qh.capabilities[0] & 0x8000) {
				prev->qh.capabilities[0] |= 0x8000; // set H bit
			}
			// link the previous QH, we're no longer in the loop
			prev->qh.horizontal_link = pipe->qh.horizontal_link;
			// do the Async Advance Doorbell handshake to wait to be
			// sure the EHCI no longer references the removed QH
			USBHS_USBCMD |= USBHS_USBCMD_IAA;
			while (!(USBHS_USBSTS & USBHS_USBSTS_AAI)) ; // busy loop wait
			USBHS_USBSTS = USBHS_USBSTS_AAI;
			// TODO: does this write interfere UPI & UAI (bits 18 & 19) ??
		}
		// find & free all the transfers which completed
		println("  Free transfers");
		Transfer_t *t = async_followup_first;
		while (t) {
			print("    * ", (uint32_t)t);
			Transfer_t *next = t->next_followup;
			if (t->pipe == pipe) {
				print(" * remove");
				remove_from_async_followup_list(t);

				// Only free if not in QH list
				Transfer_t *tr = (Transfer_t *)(pipe->qh.next);
				while (((uint32_t)tr & 0xFFFFFFE0) && (tr != t)){
					tr  = (Transfer_t *)(tr->qtd.next);
				}
				if (tr == t) {
					println(" * defer free until QH");
				} else {
					println(" * free");
					free_Transfer(t);  // The later code should actually free it...
				}
			} else {
				println("");
			}
			t = next;
		}
	} else {
		// remove from the periodic schedule
		for (uint32_t i=0; i < PERIODIC_LIST_SIZE; i++) {
			uint32_t num = periodictable[i];
			if (num & 1) continue;
			Pipe_t *node = (Pipe_t *)(num & 0xFFFFFFE0);
			if (node == pipe) {
				periodictable[i] = pipe->qh.horizontal_link;
				continue;
			}
			Pipe_t *prev = node;
			while (1) {
				num = node->qh.horizontal_link;
				if (num & 1) break;
				node = (Pipe_t *)(num & 0xFFFFFFE0);
				if (node == pipe) {
					prev->qh.horizontal_link = node->qh.horizontal_link;
					break;
				}
				prev = node;
			}
		}
		// subtract bandwidth from uframe_bandwidth array
		if (pipe->device->speed == 2) {
			uint32_t interval = pipe->bandwidth_interval;
			uint32_t offset = pipe->bandwidth_offset;
			uint32_t stime = pipe->bandwidth_stime;
			for (uint32_t i=offset; i < PERIODIC_LIST_SIZE*8; i += interval) {
				uframe_bandwidth[i] -= stime;
			}
		} else {
			uint32_t interval = pipe->bandwidth_interval;
			uint32_t offset = pipe->bandwidth_offset;
			uint32_t shift = pipe->bandwidth_shift;
			uint32_t stime = pipe->bandwidth_stime;
			uint32_t ctime = pipe->bandwidth_ctime;
			for (uint32_t i=offset; i < PERIODIC_LIST_SIZE; i += interval) {
				uint32_t n = (i << 3) + shift;
				uframe_bandwidth[n+0] -= stime;
				uframe_bandwidth[n+2] -= ctime;
				uframe_bandwidth[n+3] -= ctime;
				uframe_bandwidth[n+4] -= ctime;
			}
		}

		// find & free all the transfers which completed
		println("  Free transfers");
		Transfer_t *t = periodic_followup_first;
		while (t) {
			print("    * ", (uint32_t)t);
			Transfer_t *next = t->next_followup;
			if (t->pipe == pipe) {
				print(" * remove");
				remove_from_periodic_followup_list(t);

				// Only free if not in QH list
				Transfer_t *tr = (Transfer_t *)(pipe->qh.next);
				while (((uint32_t)tr & 0xFFFFFFE0) && (tr != t)){
					tr  = (Transfer_t *)(tr->qtd.next);
				}
				if (tr == t) {
					println(" * defer free until QH");
				} else {
					println(" * free");
					free_Transfer(t);  // The later code should actually free it...
				}
			} else {
				println("");
			}
			t = next;
		}
	}
	//
	// TODO: do we need to look at pipe->qh.current ??
	//
	// free all the transfers still attached to the QH
	println("  Free transfers attached to QH");
	Transfer_t *tr = (Transfer_t *)(pipe->qh.next);
	while ((uint32_t)tr & 0xFFFFFFE0) {
		println("    * ", (uint32_t)tr);
		Transfer_t *next = (Transfer_t *)(tr->qtd.next);
		free_Transfer(tr);
		tr = next;
	}
	// hopefully we found everything...
	free_Pipe(pipe);
	println("* Delete Pipe completed");
}


