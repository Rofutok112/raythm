#pragma once

#include "raylib.h"

// raylib の KeyboardKey 列挙値を表示用文字列に変換する。
inline const char* get_key_name(int key) {
    switch (key) {
        case KEY_A: return "A";
        case KEY_B: return "B";
        case KEY_C: return "C";
        case KEY_D: return "D";
        case KEY_E: return "E";
        case KEY_F: return "F";
        case KEY_G: return "G";
        case KEY_H: return "H";
        case KEY_I: return "I";
        case KEY_J: return "J";
        case KEY_K: return "K";
        case KEY_L: return "L";
        case KEY_M: return "M";
        case KEY_N: return "N";
        case KEY_O: return "O";
        case KEY_P: return "P";
        case KEY_Q: return "Q";
        case KEY_R: return "R";
        case KEY_S: return "S";
        case KEY_T: return "T";
        case KEY_U: return "U";
        case KEY_V: return "V";
        case KEY_W: return "W";
        case KEY_X: return "X";
        case KEY_Y: return "Y";
        case KEY_Z: return "Z";
        case KEY_ZERO: return "0";
        case KEY_ONE: return "1";
        case KEY_TWO: return "2";
        case KEY_THREE: return "3";
        case KEY_FOUR: return "4";
        case KEY_FIVE: return "5";
        case KEY_SIX: return "6";
        case KEY_SEVEN: return "7";
        case KEY_EIGHT: return "8";
        case KEY_NINE: return "9";
        case KEY_SPACE: return "SPACE";
        case KEY_LEFT_SHIFT: return "L-SHIFT";
        case KEY_RIGHT_SHIFT: return "R-SHIFT";
        case KEY_LEFT_CONTROL: return "L-CTRL";
        case KEY_RIGHT_CONTROL: return "R-CTRL";
        case KEY_LEFT_ALT: return "L-ALT";
        case KEY_RIGHT_ALT: return "R-ALT";
        case KEY_TAB: return "TAB";
        case KEY_COMMA: return ",";
        case KEY_PERIOD: return ".";
        case KEY_SLASH: return "/";
        case KEY_SEMICOLON: return ";";
        case KEY_APOSTROPHE: return "'";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_RIGHT_BRACKET: return "]";
        case KEY_BACKSLASH: return "\\";
        case KEY_MINUS: return "-";
        case KEY_EQUAL: return "=";
        case KEY_GRAVE: return "`";
        case KEY_F1: return "F1";
        case KEY_F2: return "F2";
        case KEY_F3: return "F3";
        case KEY_F4: return "F4";
        case KEY_F5: return "F5";
        case KEY_F6: return "F6";
        case KEY_F7: return "F7";
        case KEY_F8: return "F8";
        case KEY_F9: return "F9";
        case KEY_F10: return "F10";
        case KEY_F11: return "F11";
        case KEY_F12: return "F12";
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        default: return "???";
    }
}
