#include "stdio.h"
#include "x86.h"

void putc(char c){
    x86_printCharWithInterrupt(c, 0);
}

void puts(const char* str){
    while(*str){
        putc(*str);
        str++;
    }
}

#define RADIX_DEC 10

// typedef enum printState{
//     PRINT_NORMAL = 0,
//     PRINT_SPEC = 1,
// } printState;

#define PRINT_NORMAL 0
#define PRINT_SPEC 1

// int* printf_number(int* argp, bool sign);

void _cdecl printf(const char* fmt, ...){
    int* argp = (int*)&fmt;
    uint8_t currentState = PRINT_NORMAL;
    bool sign = False;

    argp++;

    while(*fmt){
        switch (currentState) {
            case PRINT_NORMAL:
                if (*fmt == '%'){
                    currentState = PRINT_SPEC;
                } else {
                    putc(*fmt);
                }
                break;
            case PRINT_SPEC:
                switch (*fmt) {
                    case 'c':
                        putc((char)*argp);
                        argp++;
                        break;
                    case 's':
                        puts(*(const char **)argp);
                        argp++;
                        break;
                    // case 'd':
                    //     sign = True;
                    //     printf_number(argp, sign);
                    //     break;
                    // case 'u':
                    //     sign = False;
                    //     printf_number(argp, sign);
                    //     break;
                    case '%': 
                        putc('%');
                        break;    
                    default:
                        putc('?');
                        break;
                }
                currentState = PRINT_NORMAL;
                sign = False;
                break;
            default:
                break;
        }

        fmt++;
    }
}

// int* printf_number(int* argp, bool sign){
//     const char g_HexChars[] = "0123456789abcdef";
//     char buffer[32];
//     unsigned long long number;
//     int number_sign = 1;
//     int pos = 0;

//     if (sign){
//         int n = *argp;
//         if (n < 0)
//         {
//             n = -n;
//             number_sign = -1;
//         }
//         number = (unsigned long long)n;
//     }
//     else
//     {
//         number = *(unsigned int*)argp;
//     }
//     argp++;

//     do {
//         uint32_t rem;
//         x86_div64_32(number, RADIX_DEC, &number, &rem);
//         buffer[pos++] = g_HexChars[rem];
//     } while (number > 0);

//     if (sign && number_sign < 0) {
//         buffer[pos++] = '-';
//     }

//     while (--pos >= 0) {
//         putc(buffer[pos]);
//     }

//     return argp;
// }
