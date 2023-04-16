//
//	MODULE:		TextViewParser.cpp
//
//	PURPOSE:	Parser for the Syntax-Description-Language
//
//	NOTES:		www.catch22.net
//
#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

typedef struct {
    TCHAR ruleName[32];
    COLORREF fg;
    COLORREF bg;
} Rule;

typedef enum {
    testNone,
    testEqual,
    testNotEqual,
    testInRange,
    testOutRange
} TEST;

typedef struct {
    TCHAR arg1;
    TCHAR arg2;
    TEST test;
    int next;
} State;

BOOL Init(TCHAR *buf, int len) { return TRUE; }

void Machine(TCHAR *str, int len, int initialState)
{
    State machine[] = {{'a', 0, testEqual}, {'b', 0, testEqual}};

    int state = initialState;

    // match string character-by-character
    for (int i = 0; i < len; i++) {
        TEST status = testNone;

        switch (machine[state].test) {
        case testEqual:
            if (str[i] == machine[state].arg1)
                status = testEqual;

            break;

        case testNotEqual:
            if (str[i] != machine[state].arg1)
                status = testNotEqual;
            break;

        case testInRange:
            if (str[i] >= machine[state].arg1 && str[i] <= machine[state].arg2)
                status = testInRange;

            break;

        case testOutRange:
            if (str[i] < machine[state].arg1 || str[i] > machine[state].arg2)
                status = testOutRange;

            break;

        default:
            break;
        }

        if (status == testNone)
            state++;
        else
            state = machine[state].test;
    }
}
