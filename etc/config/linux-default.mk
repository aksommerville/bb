DRIVERS_ENABLE:=pulse
CC:=gcc -c -MMD -O2 -Isrc -I$(MIDDIR) -Werror -Wimplicit
AR:=ar rc
LD:=gcc
LDPOST:=-lpulse -lpulse-simple -lpthread -lm
OPT_ENABLE:=
