// $Id: mios32_config.h 1136 2011-01-08 23:06:59Z tk $
/*
 * Local MIOS32 configuration file
 *
 * this file allows to disable (or re-configure) default functions of MIOS32
 * available switches are listed in $MIOS32_PATH/modules/mios32/MIOS32_CONFIG.txt
 *
 */

#ifndef _MIOS32_CONFIG_H
#define _MIOS32_CONFIG_H

// The boot message which is print during startup and returned on a SysEx query
#define MIOS32_LCD_BOOT_MSG_LINE1 "MIDIbox LC V2.0Beta2"
#define MIOS32_LCD_BOOT_MSG_LINE2 "(C) 2010 T.Klose"

// define a unique VID/PID for this application
#define MIOS32_USB_VENDOR_ID    0x16c0        // sponsored by voti.nl! see http://www.voti.nl/pids
#define MIOS32_USB_VENDOR_STR   "midibox.org" // you will see this in the USB device description
#define MIOS32_USB_PRODUCT_STR  "MIDIbox LC V2" // you will see this in the MIDI device list
#define MIOS32_USB_PRODUCT_ID   1006          // 1000..1009 are free for lab use

#define MIOS32_USB_MIDI_NUM_PORTS 4           // we provide 4 USB ports

// enable third UART for communication with MBHP_MF_V3 module
// it's available at Port J5B.A6 and A7
#define MIOS32_UART_NUM 3


#endif /* _MIOS32_CONFIG_H */
