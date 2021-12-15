DRIVERS_ENABLE:=pulse ossmidi
CC:=gcc -c -MMD -O2 -Isrc -I$(MIDDIR) -Werror -Wimplicit
AR:=ar rc
LD:=gcc
LDPOST:=-lpulse -lpulse-simple -lpthread -lm

# Disable drivers, eg if you want to get a clearer picture of memory usage.
#DRIVERS_ENABLE:=
#LDPOST:=-lm
