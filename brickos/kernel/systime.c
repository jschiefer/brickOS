/*! \file systime.c
    \brief  Implementation: system time services
    \author Markus L. Noga <markus@noga.de>
*/

/*
 *  The contents of this file are subject to the Mozilla Public License
 *  Version 1.0 (the "License"); you may not use this file except in
 *  compliance with the License. You may obtain a copy of the License at
 *  http://www.mozilla.org/MPL/
 *
 *  Software distributed under the License is distributed on an "AS IS"
 *  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 *  License for the specific language governing rights and limitations
 *  under the License.
 *
 *  The Original Code is legOS code, released October 17, 1999.
 *
 *  The Initial Developer of the Original Code is Markus L. Noga.
 *  Portions created by Markus L. Noga are Copyright (C) 1999
 *  Markus L. Noga. All Rights Reserved.
 *
 *  Contributor(s): Markus L. Noga <markus@noga.de>
 *                  David Van Wagner <davevw@alumni.cse.ucsc.edu>
 */

/*
 *  2000.05.01 - Paolo Masetti <paolo.masetti@itlug.org>
 *
 * - Added battery indicator handler
 *
 *  2000.08.12 - Rossz V�mos-Wentworth <rossw@jps.net>
 *
 * - Added idle shutdown handler
 *
 */

#include <config.h>

#ifdef CONF_TIME

#include <sys/time.h>
#include <sys/h8.h>
#include <sys/irq.h>
#include <sys/dmotor.h>
#include <sys/dsound.h>
#include <sys/battery.h>
#include <sys/critsec.h>
#ifdef CONF_AUTOSHUTOFF
#include <sys/timeout.h>
#endif

///////////////////////////////////////////////////////////////////////////////
//
// Global Variables
//
///////////////////////////////////////////////////////////////////////////////

//! current system time in ms
/*! \warning This is a 32 bit value which will overflow after 49.7 days
             of continuous operation.
*/
volatile time_t sys_time;

///////////////////////////////////////////////////////////////////////////////
//
// Internal Variables
//
///////////////////////////////////////////////////////////////////////////////

#ifdef CONF_TM
volatile unsigned char tm_timeslice;            //!< task time slice
volatile unsigned char tm_current_slice;        //!< current time remaining

void* tm_switcher_vector;                       //!< pointer to task switcher
#endif


///////////////////////////////////////////////////////////////////////////////
//
// Functions
//
///////////////////////////////////////////////////////////////////////////////

//! clock handler triggered on the WDT overflow (every msec) on the NMI
/*! this is the system clock
 */
extern void clock_handler(void);
#ifndef DOXYGEN_SHOULD_SKIP_THIS
__asm__("\n\
.text\n\
.align 1\n\
.global _clock_handler\n\
        _clock_handler:\n\
                mov.w #0x5a07,r6                ; reset wd timer to 6\n\
                mov.w r6,@0xffa8\n\
\n\
                mov.w @_sys_time+2,r6           ; lower 8 bits\n\
                add.b #0x1,r6l                  ; inc lower 4 bits\n\
                addx  #0x0,r6h                  ; add carry to top 4 bits\n\
                mov.w r6,@_sys_time+2\n\
                bcc sys_nohigh                  ; if carry, inc upper 8 bits\n\
                  mov.w @_sys_time,r6           ; \n\
                  add.b #0x1,r6l                ; inc lower 4 bits\n\
                  addx  #0x0,r6h                ; add carry to top 4 bits\n\
                  mov.w r6,@_sys_time\n\
              sys_nohigh: \n\
                rts\n\
       ");
#endif // DOXYGEN_SHOULD_SKIP_THIS

//! subsystem handler for every 2nd msec
/*! this is the pulse of the system (subsystems).
    sound, motor and lcd driver calls are initiated here.
    task_switch_handler is called from here as well.
*/
extern void subsystem_handler(void);

//! task switch handler called every msec
/*! handles swapping between tasks
 */
extern void task_switch_handler(void);
#ifndef DOXYGEN_SHOULD_SKIP_THIS
__asm__("\n\
.text\n\
.align 1\n\
.global _subsystem_handler\n\
.global _task_switch_handler\n\
.global _systime_tm_return\n\
_subsystem_handler:\n\
               ; r6 saved by ROM\n\
\n\
                push r0                         ; both motors & task\n\
                                                ; switcher need this reg.\n\
        "

#ifdef CONF_DSOUND
        "\n\
                jsr _dsound_handler             ; call sound handler\n\
        "
#endif // CONF_DSOUND

#ifdef CONF_LNP
        "\n\
                mov.w @_lnp_timeout_counter,r6  ; check LNP timeout counter\n\
                subs #0x1,r6\n\
                mov.w r6,r6                     ; subs doesn't change flags!\n\
                bne sys_noreset\n\
                \n\
                  jsr _lnp_integrity_reset\n\
                  mov.w @_lnp_timeout,r6        ; reset timeout\n\
\n\
              sys_noreset:\n\
                mov.w r6,@_lnp_timeout_counter\n\
        "
#endif // CONF_LNP

#ifdef CONF_DKEY
        "\n\
                jsr _dkey_handler\n\
        "
#endif // CONF_DKEY

#ifndef CONF_TM
#ifdef CONF_BATTERY_INDICATOR
        "\n\
                mov.w @_battery_refresh_counter,r6\n\
                subs #0x1,r6\n\
                bne batt_norefresh\n\
\n\
                  jsr _battery_refresh\n\
                  mov.w @_battery_refresh_period,r6\n\
\n\
              batt_norefresh:\n\
                mov.w r6,@_battery_refresh_counter\n\
        "
#endif // CONF_BATTERY_INDICATOR
#endif // CONF_TM

#ifdef CONF_AUTOSHUTOFF
        "\n\
                mov.w @_auto_shutoff_counter,r6\n\
                subs  #0x1,r6\n\
                bne auto_notshutoff\n\
\n\
                  jsr _autoshutoff_check\n\
                  mov.w @_auto_shutoff_period,r6\n\
                  \n\
              auto_notshutoff:\n\
                  mov.w r6,@_auto_shutoff_counter\n\
        "
#endif // CONF_AUTOSHUTOFF

#ifdef CONF_VIS
        "\n\
                mov.b @_vis_refresh_counter,r6l\n\
                dec r6l\n\
                bne vis_norefresh\n\
                \n\
                  jsr _vis_handler\n\
                  mov.b @_vis_refresh_period,r6l\n\
                  \n\
              vis_norefresh:\n\
                mov.b r6l,@_vis_refresh_counter\n\
        "
#endif // CONF_VIS

#ifdef CONF_LCD_REFRESH
        "\n\
                mov.b @_lcd_refresh_counter,r6l\n\
                dec r6l\n\
                bne lcd_norefresh\n\
                \n\
                  jsr _lcd_refresh_next_byte\n\
                  mov.b @_lcd_refresh_period,r6l\n\
                  \n\
              lcd_norefresh:\n\
                mov.b r6l,@_lcd_refresh_counter\n\
        "
#endif // CONF_LCD_REFRESH
        "\n\
                bclr  #2,@0x91:8                ; reset compare B IRQ flag\n\
        "
#ifdef CONF_TM
        "\n\
                pop r0                          ; if fallthrough, pop r0\n\
              _task_switch_handler:\n\
                push r0                         ; save r0\n\
\n\
                mov.b @_tm_current_slice,r6l\n\
                dec r6l\n\
                bne sys_noswitch                ; timeslice elapsed?\n\
\n\
                  mov.w @_kernel_critsec_count,r6 ; check critical section\n\
                  beq sys_switch                ; ok to switch\n\
                  mov.b #1,r6l                  ; wait another tick\n\
                  jmp sys_noswitch              ; don't switch\n\
\n\
                sys_switch:\n\
                  mov.w @_tm_switcher_vector,r6\n\
                  jsr @r6                       ; call task switcher\n\
                  \n\
              _systime_tm_return:\n\
                mov.b @_tm_timeslice,r6l        ; new timeslice\n\
\n\
              sys_noswitch:\n\
                mov.b r6l,@_tm_current_slice\n\
        "
#endif // CONF_TM

#ifdef CONF_DMOTOR
        "\n\
                jsr _dm_handler                 ; call motor driver\n\
        "
#endif // CONF_DMOTOR

        "\n\
                pop r0\n\
                bclr  #3,@0x91:8                ; reset compare A IRQ flag\n\
                rts\n\
        "
);
#endif // DOXYGEN_SHOULD_SKIP_THIS


//! initialize system timer
/*! task switcher initialized to empty handler
    motors turned off
*/
void systime_init(void) {
  systime_shutdown();                           // shutdown hardware

  sys_time=0l;                                  // init timer

#ifdef CONF_TM
  tm_current_slice=tm_timeslice=TM_DEFAULT_SLICE;
  tm_switcher_vector=&rom_dummy_handler;        // empty handler
#endif

#ifdef CONF_DMOTOR
  dm_shutdown();
#endif

  // configure 16-bit timer
  // compare B IRQ will fire after one msec
  // compare A IRQ will fire after another msec
  // counter is then reset
  //
  T_CSR  = TCSR_RESET_ON_A;
  T_CR   = TCR_CLOCK_32;
  T_OCR &= ~TOCR_OCRB;
  T_OCRA = 1000;
  T_OCR &= ~TOCR_OCRA;
  T_OCR |= TOCR_OCRB; 
  T_OCRB = 500;

#if defined(CONF_TM)
  ocia_vector = &task_switch_handler;
#else // CONF_TM
  ocia_vector = &subsystem_handler;
#endif // CONF_TM
  ocib_vector = &subsystem_handler;
  T_IER |= (TIER_ENABLE_OCB | TIER_ENABLE_OCA);

  nmi_vector = &clock_handler;
  WDT_CSR = WDT_CNT_PASSWORD | WDT_CNT_MSEC_64;   // trigger every msec 
  WDT_CSR = WDT_CSR_PASSWORD
        | WDT_CSR_CLOCK_64
	| WDT_CSR_WATCHDOG_NMI
        | WDT_CSR_ENABLE
        | WDT_CSR_MODE_WATCHDOG; 
}

//! shutdown system timer
/*! will also stop task switching and motors.
*/
void systime_shutdown(void) {
  T_IER &= ~(TIER_ENABLE_OCA | TIER_ENABLE_OCB);  // unhook compare A/B IRQs
  WDT_CSR &= ~WDT_CSR_ENABLE;                     // disable wd timer
}

#ifdef CONF_TM
//! set task switcher vector
/*! \param switcher the switcher
*/
void systime_set_switcher(void* switcher) {
  tm_switcher_vector=switcher;
}

//! set multitasking timeslice in ms
/*! \param slice the timeslice. must be at least 5ms.
*/
void systime_set_timeslice(unsigned char slice) {
  if(slice>5) {                    // some minimum value
    tm_timeslice=slice;
    if(tm_current_slice>tm_timeslice)
      tm_current_slice=tm_timeslice;
  }
}

#endif

//! retrieve the current system time
/*! \return number of msecs the system has been running
 *  Since sys_time is 32bits, it takes more than one
 *  instruction to retrieve; the NMI can fire mid
 *  retrieval; causing the upper and lower 16bits to be
 *  unmatched (lower 16bits could overflow and reset to
 *  0, while upper 16bits were already read)
 */
extern time_t get_system_up_time(void);
__asm__("\n\
.text\n\
.align 1\n\
.global _get_system_up_time\n\
_get_system_up_time:\n\
    push  r2\n\
  try_again:\n\
    mov.w @_sys_time+2, r1\n\
    mov.w @_sys_time,   r0\n\
    mov.w @_sys_time+2, r2\n\
    cmp   r2, r1\n\
    bne try_again\n\
    pop   r2\n\
    rts\n\
");

#endif // CONF_TIME
