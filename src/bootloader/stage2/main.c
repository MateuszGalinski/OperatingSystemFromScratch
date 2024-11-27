#include "stdint.h"
#include "stdio.h"

void _cdecl cstart_(uint16_t bootDrive){
    puts("Hello world from C!\r\n");

    printf("Formatted %% %c %s\r\n", 'a', "string");
    // printf("Formatted %d %d\r\n", 1234, -5678);
    // printf("Formatted %u\r\n", 121);
    for (;;);
}
