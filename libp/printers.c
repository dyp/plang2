/// \file printers.c
///

#include <plang.h>
#include <stdio.h>
#include <inttypes.h>

void __p_printBool(bool _value) {
    printf("%s\n", (_value ? "true" : "false"));
}

void __p_printWChar(wchar_t _value) {
    printf("%lc\n", _value);
}

void __p_printInt8(int8_t _value) {
    printf("%hhd\n", _value);
}

void __p_printInt16(int16_t _value) {
    printf("%hd\n", _value);
}

void __p_printInt32(int32_t _value) {
   printf("%d\n", _value);
}

void __p_printInt64(int64_t _value) {
    printf("%" PRId64 "\n", _value);
}

void __p_printUInt8(uint8_t _value) {
    printf("%hhu\n", _value);
}

void __p_printUInt16(uint16_t _value) {
    printf("%hu\n", _value);
}

void __p_printUInt32(uint32_t _value) {
    printf("%u\n", _value);
}

void __p_printUInt64(uint64_t _value) {
    printf("%" PRIu64 "\n", _value);
}

void __p_printWString(const wchar_t * _value) {
    printf("%ls\n", _value);
}

void __p_printPtr(const void * _value) {
    printf("0x%p\n", _value);
}
