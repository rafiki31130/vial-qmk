VIA_ENABLE = yes
VIAL_ENABLE = yes
VIALRGB_ENABLE = yes

ENCODER_MAP_ENABLE = yes

# Activate the Dynamic macro features
DYNAMIC_MACRO_ENABLE = yes

# EEPROM reduction
KEY_OVERRIDE_ENABLE = no

# Instrumentation temporaire tap/hold, désactivée par défaut.
# make splitkb/kyria/rev3:vial CONVERT_TO=liatris TAPHOLD_TRACE=yes
# Voir docs/instrumentation.md avant usage.
ifeq ($(strip $(TAPHOLD_TRACE)), yes)
    CONSOLE_ENABLE = yes
    OPT_DEFS += -DTAPHOLD_TRACE -DUSER_PRINT
    SRC += taphold_trace.c
endif
