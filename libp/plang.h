#ifndef __PLANG_H_
#define __PLANG_H_

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t len;
    wchar_t str[];
} __p_string_t;

#define __P_STRING_CONST(NAME, LEN, VALUE) \
__p_string_t __##NAME = { LEN, VALUE }; \
const wchar_t * const NAME = __##NAME.str

#define __p_alloc malloc
#define __p_free free

// Printers.

void __p_printBool(bool _value);
void __p_printWChar(wchar_t _value);
void __p_printInt8(int8_t _value);
void __p_printInt16(int16_t _value);
void __p_printInt32(int32_t _value);
void __p_printInt64(int64_t _value);
void __p_printUInt8(uint8_t _value);
void __p_printUInt16(uint16_t _value);
void __p_printUInt32(uint32_t _value);
void __p_printUInt64(uint64_t _value);
void __p_printWString(const wchar_t * _value);
void __p_printPtr(const void * _value);

#endif // __PLANG_H_
