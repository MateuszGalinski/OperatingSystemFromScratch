#pragma once
#include "stdint.h"

void _cdecl x86_printCharWithInterrupt(char c, uint8_t page);
void _cdecl x86_div64_32(uint64_t dividend, uint32_t divisor, uint64_t* quotientOut, uint32_t* remainderOut);
bool _cdecl x86_resetDisk(uint8_t drive);
bool _cdecl x86_readDisk(uint8_t drive,
                         uint16_t cylinder,
                         uint16_t sector,
                         uint16_t head,
                         uint8_t count,
                         void far * dataOut);

bool _cdecl x86_getDriveParams(uint8_t drive,
                               uint8_t* driveTypeOut,
                               uint16_t* cylindersOut,
                               uint16_t* sectorsOut,
                               uint16_t* headsOut);
