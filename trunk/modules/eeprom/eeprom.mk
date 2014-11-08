# $Id: eeprom.mk 712 2009-08-16 15:49:21Z tk $

# enhance include path
C_INCLUDE += -I $(MIOS32_PATH)/modules/eeprom


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(MIOS32_PATH)/modules/eeprom/eeprom.c


# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/modules/eeprom
