#ifndef plc_int
#define plc_int
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <Windows.h>
#include <io.h>
#define MSG_BUFFER 256
DWORD WINAPI PLC_Interface(LPVOID arg);
#endif