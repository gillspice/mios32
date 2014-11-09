// $Id: app.c 1109 2010-10-11 23:14:49Z tk $
/*
 * Example for a "fastscan button matrix"
 *
 * ==========================================================================
 *
 *  Copyright (C) 2010 Thorsten Klose(tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include "app.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>


/////////////////////////////////////////////////////////////////////////////
// for optional debugging messages
/////////////////////////////////////////////////////////////////////////////

// level >= 1: print warnings (recommented default value)
// level >= 2: print debug messages for Robin's Fatar Keyboard
// level >= 3: print row/column messages in addition for initial testing of matrix scan for other usecases
#define DEBUG_VERBOSE_LEVEL 3
#define DEBUG_MSG MIOS32_MIDI_SendDebugMessage

/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#define PRIORITY_TASK_MATRIX_SCAN	( tskIDLE_PRIORITY + 2 )

// scan 16 rows
#define MATRIX_NUM_ROWS 17

// sink drivers used? (no for Fatar keyboard)
#define MATRIX_DOUT_HAS_SINK_DRIVERS 0

// non velocity sensitive version
//#define NO_VELOCITY 0

// maximum number of supported keys (rowsxcolumns = 24*16)
#define KEYBOARD_NUM_PINS (24*16)

// used MIDI port and channel (DEFAULT, USB0, UART0 or UART1)
#define KEYBOARD_MIDI_PORT DEFAULT
#define KEYBOARD_MIDI_CHN  Chn1

// initial minimum/maximum delay to calculate velocity
// (will be copied into variables, so that values could be changed during runtime)
#define INITIAL_KEYBOARD_DELAY_FASTEST 0
#define INITIAL_KEYBOARD_DELAY_SLOWEST 127


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////
static void TASK_MatrixScan(void *pvParameters);


/////////////////////////////////////////////////////////////////////////////
// Local Variables
/////////////////////////////////////////////////////////////////////////////

static u32 din_value[MATRIX_NUM_ROWS];

static u32 last_timestamp[KEYBOARD_NUM_PINS];

static u32 keyboard_delay_fastest;
static u32 keyboard_delay_slowest;

int selected_midi_port;

int last_velocity;
int channel = KEYBOARD_MIDI_CHN;
int no_velocity;

// this table scales the velocity counter (0..127) to the velocity value which will be sent via MIDI
// it has been generated with "perl utils/velocity_table.pl"
const int velocity_table[128] = {
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02,
  0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
  0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04,
  0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x06,
  0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x08,
  0x08, 0x08, 0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0b,
  0x0b, 0x0b, 0x0c, 0x0c, 0x0d, 0x0d, 0x0e, 0x0f,
  0x0f, 0x10, 0x10, 0x11, 0x12, 0x12, 0x13, 0x14,
  0x15, 0x16, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
  0x1c, 0x1d, 0x1f, 0x20, 0x21, 0x22, 0x24, 0x25,
  0x27, 0x28, 0x2a, 0x2b, 0x2d, 0x2f, 0x31, 0x33,
  0x35, 0x37, 0x39, 0x3b, 0x3d, 0x40, 0x42, 0x45,
  0x48, 0x4a, 0x4d, 0x50, 0x54, 0x57, 0x5a, 0x5e,
  0x61, 0x65, 0x69, 0x6d, 0x72, 0x76, 0x7b, 0x7f,
};

// this table maps a note number to a pin (or key) number
const int note_table[192] = {
  39,33,40,35,37,36,38,33,
  35,37,36,38,33,39,34,40,
  38,34,39,34,40,35,37,36,
  93,86,79,72,65,58,51,44,
  90,83,76,69,62,55,48,41,
  94,87,80,73,66,59,52,45,
  91,84,77,70,63,56,49,42,
  95,88,81,74,67,60,53,46,
  92,85,78,71,64,57,50,43,
  96,89,82,75,68,61,54,47,
  93,86,79,72,65,58,51,44,
  90,83,76,69,62,55,48,41,
  94,87,80,73,66,59,52,45,
  91,84,77,70,63,56,49,42,
  95,88,81,74,67,60,53,46,
  92,85,78,71,64,57,50,43,
  96,89,82,75,68,61,54,47,
  93,86,79,72,65,58,51,44,
  90,83,76,69,62,55,48,41,
  94,87,80,73,66,59,52,45,
  91,84,77,70,63,56,49,42,
  95,88,81,74,67,60,53,46,
  92,85,78,71,64,57,50,43,
  96,89,82,75,68,61,54,47,
};

const int channel_table[16] = {
  7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8
};

/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  // initialize all LEDs
  MIOS32_BOARD_LED_Init(0xffffffff);


  // initialize DIN arrays
  int row;
  for(row=0; row<MATRIX_NUM_ROWS; ++row) {
    din_value[row] = 0xffffffff; // default state: buttons depressed
  }

  // initialize timestamps
  int i;
  for(i=0; i<KEYBOARD_NUM_PINS; ++i) {
    last_timestamp[i] = 0;
  }

    // initialize all pins of J5A, J5B and J5C as inputs with internal Pull-Up
  int pin;
  for(pin=0; pin<12; ++pin)
    MIOS32_BOARD_J5_PinInit(pin, MIOS32_BOARD_PIN_MODE_INPUT_PU);
	
  //Initialize pin 7 as an output to be row 17
  MIOS32_BOARD_J5_PinInit(7, MIOS32_BOARD_PIN_MODE_OUTPUT_PP);
	
  // initialize keyboard delay values
  keyboard_delay_fastest = INITIAL_KEYBOARD_DELAY_FASTEST;
  keyboard_delay_slowest = INITIAL_KEYBOARD_DELAY_SLOWEST;
  
  no_velocity=MIOS32_BOARD_J5_PinGet(1);
  MIOS32_MIDI_SendDebugMessage("no_velocity:%02x",no_velocity);
  
  if (MIOS32_BOARD_J5_PinGet(0)){
	selected_midi_port=0x20;
	MIOS32_LCD_CursorSet(0, 0); // X, Y
    MIOS32_LCD_PrintFormattedString("UART");
	MIOS32_MIDI_SendDebugMessage("UART");
	}
  else {
	selected_midi_port=0x10;
	MIOS32_LCD_CursorSet(0, 0); // X, Y
    MIOS32_LCD_PrintFormattedString("USB");
	MIOS32_MIDI_SendDebugMessage("USB");
	}

  // start matrix scan task
  xTaskCreate(TASK_MatrixScan, (signed portCHAR *)"MatrixScan", configMINIMAL_STACK_SIZE, NULL, PRIORITY_TASK_MATRIX_SCAN, NULL);
}


/////////////////////////////////////////////////////////////////////////////
// This task is running endless in background
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void)
{
	u8 old_state[12]; // to store the state of 12 pins
	 // initialize pin state to inactive value
  int pin;
  for(pin=0; pin<12; ++pin)
    old_state[pin] = 1;

  // endless loop
  while( 1 ) {
    // toggle the state of all LEDs (allows to measure the execution speed with a scope)
    MIOS32_BOARD_LED_Set(0xffffffff, ~MIOS32_BOARD_LED_Get());
	
	no_velocity=MIOS32_BOARD_J5_PinGet(1);
	
	
	
/*	// check each J5 pin for changes
    for(pin=0; pin<12; ++pin) {
      u8 new_state = MIOS32_BOARD_J5_PinGet(pin);

      // pin changed?
      if( new_state != old_state[pin] ) {
        // store new state
        old_state[pin] = new_state;

        switch (pin) {
		case 0:  
		break;
		case 1:
		break;
		case 2:
		break;
		case 3:
		break;
		default:
		  break;
		}
      }
    }*/
}
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a MIDI package has been received
/////////////////////////////////////////////////////////////////////////////
void APP_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{
/*	
// forward USB0->UART0 and UART0->USB0
  switch( port ) {
    case 0x10:  MIOS32_MIDI_SendPackage(0x20, midi_package);
				MIOS32_LCD_CursorSet(0, 0); // X, Y
				MIOS32_LCD_PrintFormattedString("%02X %X %02X %02X %02X",
					port,
                   midi_package.type,
                   midi_package.evnt0, midi_package.evnt1, midi_package.evnt2);
	break;
	
    case 0x20: MIOS32_MIDI_SendPackage(0x10,  midi_package); break;
  }*/
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called before the shift register chain is scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServicePrepare(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called after the shift register chain has been scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServiceFinish(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a button has been toggled
// pin_value is 1 when button released, and 0 when button pressed
/////////////////////////////////////////////////////////////////////////////
void APP_DIN_NotifyToggle(u32 pin, u32 pin_value)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when an encoder has been moved
// incrementer is positive when encoder has been turned clockwise, else
// it is negative
/////////////////////////////////////////////////////////////////////////////
void APP_ENC_NotifyChange(u32 encoder, s32 incrementer)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a pot has been moved
/////////////////////////////////////////////////////////////////////////////
void APP_AIN_NotifyChange(u32 pin, u32 pin_value)
{
#if 1
  // convert 12bit value to 7bit value
  u8 value_7bit = 127-(pin_value >> 5);

  // send MIDI event
  MIOS32_MIDI_SendCC(selected_midi_port, Chn1, pin, value_7bit);
#else

  // convert 12bit value to 14bit value
  u16 value_14bit = pin_value << 2;

  // send Pitch Bender event
  MIOS32_MIDI_SendPitchBend(selected_midi_port, pin, value_14bit);
#endif
}



/////////////////////////////////////////////////////////////////////////////
// This task is called each mS to scan the button matrix
/////////////////////////////////////////////////////////////////////////////

// will be called on button pin changes (see TASK_BLM_Check)
void BUTTON_NotifyToggle(u8 row, u8 column, u8 pin_value, u32 timestamp)
{
  // determine pin number based on row/column
  // based on pin map for fadar keyboard provided by Robin (see doc/ directory)
  // tested with utils/test_pinmap.pl

#if DEBUG_VERBOSE_LEVEL >= 3
    DEBUG_MSG("row=0x%02x, column=0x%02x, pin_value=%d\n",
	      row, column, pin_value);
#endif

  int pin = -1;

  // pin number (counted from 0) consists of:
  //   bit #0 if row -> pin bit #0
  //int bit0 = row & 1;
  //   bit #2..0 of column -> pin bit #3..1
  int bit3to1 = row & 0x7;
  //   bit #3..1 of row -> pin bit #6..4
  //int bit6to4 = (row & 0xe) >> 1;

  // combine to pin value
//  if( column < 8 ) {
    // left half
//    if( row >= 0 && row <= 0x9 ) {
      pin = (column << 3) | bit3to1;
 //   }
 // } else {
 //   // right half
 //   if( row >= 0 && row <= 0xb ) {
 //     pin = 80 + (bit0 | (bit6to4 << 4) | (bit3to1 << 1));
 //   }
 // }

  // following check ensures that we never continue with an unexpected/invalid pin number.
  // e.g. this could address a memory location outside the last_timestamp[] array!
  // print a warning message in this case for analysis purposes
  if( pin < 0 || pin >= KEYBOARD_NUM_PINS ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("WARNING: row=0x%02x, column=0x%02x, pin_value=%d -> pin=%d NOT MAPPED!\n",
	      row, column, pin_value, pin);
#endif
    return;
  }

  // first or second switch if a key?
  u8 second_switch = ((row & 0x8) >> 3); // 0 if first switch, 1 if second switch

  // the note number lookup
  int note_number = note_table[pin];
  if( note_number > 127 ) // just another check to ensure that no invalid note will be sent
    note_number = 127;

  // we have three transitions which are for interest:
  // a) first switch changes from 1->0 (pin_value == 0):
  //    - store the current timestamp
  // b) second switch changes from 1->0 (pin_value == 0):
  //    - calculate delay between current timestamp and timestamp captured during a)
  //    - do this only if the captured timestamp is != 0 (see also c)
  //    - calculate velocity depending on the delay
  //    - send Note On event
  // c) first switch changes from 0->1 (pin_value == 1): 
  //    - send Note Off event (resp. Note On with velocity 0)
  //    - clear captured timestamp (allows to check for valid delay on next transition)

  unsigned key_ix = pin & 0xfffffffe;
  int delay = -1;
  u8 send_note_on = 0;
  u8 send_note_off = 0;

  if( pin_value == 0 ) {
    if( second_switch == 0 ) { // first switch
      last_timestamp[key_ix] = timestamp;
 //     if (no_velocity)
 //      send_note_on = 1;

    } else { // second switch
      if( last_timestamp[key_ix] ) {
	delay = timestamp - last_timestamp[key_ix];
	send_note_on = 1;
      }
    }
  } else {
    if( second_switch == 0 ) { // first switch
      last_timestamp[key_ix] = 0;
      send_note_off = 1;
    }
  }


  // now we know:
  // - if a note on or off event should be sent
  // - the measured delay (note on only)

  if( send_note_on ) {
    // determine velocity depending on delay
    int velocity = 127 - (((delay-keyboard_delay_fastest) * 127) / (keyboard_delay_slowest-keyboard_delay_fastest));
     if (no_velocity)
       velocity = 127;

    // saturate to ensure that range 1..127 won't be exceeded
    if( velocity < 1 )
      velocity = 1;
    if( velocity > 127 )
      velocity = 127;

    last_velocity=velocity_table[velocity];
    MIOS32_MIDI_SendNoteOn(0x20, channel, note_number, last_velocity);
	MIOS32_MIDI_SendNoteOn(0x10, channel, note_number, last_velocity);
	
#if DEBUG_VERBOSE_LEVEL >= 2
    DEBUG_MSG("row=0x%02x, column=0x%02x, pin_value=%d -> pin=%d, timestamp=%u -> NOTE ON (delay=%d); velocity=%d\n",
	      row, column, pin_value, pin, timestamp, delay, velocity);
#endif
  } else if( send_note_off ) {
    // send Note On with velocity 0
    MIOS32_MIDI_SendNoteOn(0x20, channel, note_number, 0x00);
	MIOS32_MIDI_SendNoteOn(0x10, channel, note_number, 0x00);
	

#if DEBUG_VERBOSE_LEVEL >= 2
    DEBUG_MSG("row=0x%02x, column=0x%02x, pin_value=%d -> pin=%d, timestamp=%u -> NOTE OFF\n",
	      row, column, pin_value, pin, timestamp);
#endif
  } else {
#if DEBUG_VERBOSE_LEVEL >= 2
    DEBUG_MSG("row=0x%02x, column=0x%02x, pin_value=%d -> pin=%d, timestamp=%u -> IGNORE\n",
	      row, column, pin_value, pin, timestamp);
#endif
  }
}


static void TASK_MatrixScan(void *pvParameters)
{
  while( 1 ) {
    // wait for next timesplice (1 mS)
    vTaskDelay(1 / portTICK_RATE_MS);

    // determine timestamp (we need it for delay measurements)
    mios32_sys_time_t t = MIOS32_SYS_TimeGet();
    u32 timestamp = 1000*t.seconds + t.fraction_ms;

    // loop:
    //   - latch DIN/DOUT values
    //   - shift selection pattern for *next* row to DOUT registers
    //   - read DIN values of previously selected row
    // since we need to select the first row before the first DIN values are latched, we loop from -1
    // to handle the initial state
    int row;
    for(row=-1; row<MATRIX_NUM_ROWS; ++row) {
      if( row >= 0 ) { // not required for initial scan
	// latch DIN values
	MIOS32_SPI_RC_PinSet(MIOS32_SRIO_SPI, MIOS32_SRIO_SPI_RC_PIN, 0); // spi, rc_pin, pin_value
	MIOS32_DELAY_Wait_uS(1);
	MIOS32_SPI_RC_PinSet(MIOS32_SRIO_SPI, MIOS32_SRIO_SPI_RC_PIN, 1); // spi, rc_pin, pin_value
      }

      // determine selection mask for next row (written into DOUT registers while reading DIN registers)
	  u16 select_row_pattern = ~(1 << (row+1));
	  //if (row == 16) {select_row_pattern = 0xffff;}
   
#if MATRIX_DOUT_HAS_SINK_DRIVERS
      select_row_pattern ^= 0xffff; // invert selection pattern if sink drivers are connected to DOUT pins
#endif

      // read DIN, write DOUT
      u8 din0 = MIOS32_SPI_TransferByte(MIOS32_SRIO_SPI,  0xff);
      u8 din1 = MIOS32_SPI_TransferByte(MIOS32_SRIO_SPI, (select_row_pattern >> 8) & 0xff);
      u8 din2 = MIOS32_SPI_TransferByte(MIOS32_SRIO_SPI, (select_row_pattern >> 0) & 0xff);

      // latch new DOUT value
      MIOS32_SPI_RC_PinSet(MIOS32_SRIO_SPI, MIOS32_SRIO_SPI_RC_PIN, 0); // spi, rc_pin, pin_value
      MIOS32_DELAY_Wait_uS(1);
      MIOS32_SPI_RC_PinSet(MIOS32_SRIO_SPI, MIOS32_SRIO_SPI_RC_PIN, 1); // spi, rc_pin, pin_value
	  if ( row == 15 ) {MIOS32_BOARD_J5_PinSet(7, 0);}
	  else {MIOS32_BOARD_J5_PinSet(7, 1);}

      if( row >= 0 ) {
	// combine read DIN bytes to 24bit value
	u32 din_pattern = 0xff000000 | (din2 << 16) | (din1 << 8) | din0;

	// check if values have been changed via XOR combination with previously scanned value
	u32 changed = din_pattern ^ din_value[row];
	if( changed ) {
	  // store changed value
	  din_value[row] = din_pattern;

	  // notify changed value
	  int column;
	  for(column=0; column<24; ++column) {
	    u32 mask = 1 << column;
	    if( changed & mask ){
		  if (row == 16) {
			channel = channel_table[column - 8];
			MIOS32_LCD_CursorSet (0,1);
			MIOS32_LCD_PrintFormattedString("%02d", channel + 1);
			#if DEBUG_VERBOSE_LEVEL >= 1
			DEBUG_MSG("WARNING: row=0x%02x, column=0x%02x,  pin_value=%d, channel=%02x /n",row, column, (din_pattern & mask) ? 1 : 0, channel);
			#endif
			}
		  else {
	      BUTTON_NotifyToggle(row, column, (din_pattern & mask) ? 1 : 0, timestamp);}
		  }
	  }
	}
      }
    }
  }
}
