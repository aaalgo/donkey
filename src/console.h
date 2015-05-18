#ifndef AAALGO_DONKEY_CONSOLE
#define AAALGO_DONKEY_CONSOLE
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <iostream>

#define CC_CONSOLE_COLOR_DEFAULT "\033[0m"
#define CC_FORECOLOR(C) "\033[" #C "m"
#define CC_BACKCOLOR(C) "\033[" #C "m"
#define CC_ATTR(A) "\033[" #A "m"

namespace console
{
    enum Color
    {
        Black,
        Red,
        Green,
        Yellow,
        Blue,
        Magenta,
        Cyan,
        White,
        Default = 9,
        Light = 60 
    };

    enum Attributes
    {
        Reset,
        Bright,
        Dim,
        Underline,
        Blink,
        Reverse,
        Hidden
    };

    static inline std::string color(int attr, int fg, int bg)
    {
        char command[BUFSIZ];
        /* Command is the control command to the terminal */
        sprintf(command, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
        return std::string(command);
    }

    static char const *reset = CC_CONSOLE_COLOR_DEFAULT;
    static char const *underline = CC_ATTR(4);
    static char const *bold = CC_ATTR(1);
}

#endif
