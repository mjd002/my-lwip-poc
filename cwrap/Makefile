# MinGW Makefile to build a simple wrapper DLL
CC = gcc
CFLAGS = -O2 -fPIC
LDFLAGS = -shared
TARGET_DLL = lwip_wrapper.dll
SRC = wrapper.c

all: $(TARGET_DLL)

$(TARGET_DLL): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET_DLL) $(SRC) -Wl,--out-implib,liblwip_wrapper.a

clean:
	del /Q $(TARGET_DLL) liblwip_wrapper.a 2>nul || true
