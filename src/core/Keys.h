#pragma once

typedef enum
{
    MouseButton_Left = 0,
    MouseButton_Right,
    MouseButton_Middle,
    MouseButton_XButton1,
    MouseButton_XButton2
} MouseButton;

typedef enum
{
    Key_Invalid = 0x00,
    Key_LeftCtrl,
    Key_RightCtrl,
    Key_LeftShift,
    Key_RightShift,
    Key_LeftAlt,
    Key_RightAlt,
    Key_Space,
    Key_Apostrophe,
    Key_Comma,
    Key_Minus,
    Key_Period,
    Key_Slash,
    Key_0 = 0x30,
    Key_1,
    Key_2,
    Key_3,
    Key_4,
    Key_5,
    Key_6,
    Key_7,
    Key_8,
    Key_9,
    Key_Semicolon,
    Key_Equal,
    Key_A = 0x41,
    Key_B,
    Key_C,
    Key_D,
    Key_E,
    Key_F,
    Key_G,
    Key_H,
    Key_I,
    Key_J,
    Key_K,
    Key_L,
    Key_M,
    Key_N,
    Key_O,
    Key_P,
    Key_Q,
    Key_R,
    Key_S,
    Key_T,
    Key_U,
    Key_V,
    Key_W,
    Key_X,
    Key_Y,
    Key_Z,
    Key_LeftBracket,
    Key_BackSlash,
    Key_RightBracket,
    Key_Tilde,
    Key_Escape,
    Key_Enter,
    Key_Tab,
    Key_Backspace,
    Key_Insert,
    Key_Delete,
    Key_Right,
    Key_Left,
    Key_Down,
    Key_Up,
    Key_PageUp,
    Key_PageDown,
    Key_Home,
    Key_End,
    Key_CapsLock,
    Key_ScrollLock,
    Key_NumLock,
    Key_PrintScreen,
    Key_Pause,
    Key_F1 = 114,
    Key_F2,
    Key_F3,
    Key_F4,
    Key_F5,
    Key_F6,
    Key_F7,
    Key_F8,
    Key_F9,
    Key_F10,
    Key_F11,
    Key_F12,
    Key_F13,
    Key_F14,
    Key_F15,
    Key_F16,
    Key_F17,
    Key_F18,
    Key_F19,
    Key_F20,
    Key_F21,
    Key_F22,
    Key_F23,
    Key_F24,
    Key_Num0,
    Key_Num1,
    Key_Num2,
    Key_Num3,
    Key_Num4,
    Key_Num5,
    Key_Num6,
    Key_Num7,
    Key_Num8,
    Key_Num9,
    Key_NumDecimal,
    Key_NumDivide,
    Key_NumMultiply,
    Key_NumSubtract,
    Key_NumAdd,
    Key_LeftSuper,
    Key_Menu,
    Key_RightSuper,
    Key_Clear,
    Key_NumEnter,
} Key;

inline const char* ToString_Key(Key keycode)
{
    switch (keycode)
    {
    case Key_Backspace: { return "Backspace";  }
    case Key_Tab: { return "Tab";  }
    case Key_Clear: { return "Clear";  }
    case Key_Enter: { return "Enter";  }
    case Key_LeftShift: { return "LeftShift";  }
    case Key_LeftCtrl: { return "LeftCtrl";  }
    case Key_LeftAlt: { return "LeftAlt";  }
    case Key_RightShift: { return "RightShift";  }
    case Key_RightCtrl: { return "RightCtrl";  }
    case Key_RightAlt: { return "RightAlt";  }
    case Key_Pause: { return "Pause";  }
    case Key_CapsLock: { return "CapsLock";  }
    case Key_Escape: { return "Escape";  }
    case Key_Space: { return "Space";  }
    case Key_PageUp: { return "PageUp";  }
    case Key_PageDown: { return "PageDown";  }
    case Key_End: { return "End";  }
    case Key_Home: { return "Home";  }
    case Key_Left: { return "Left";  }
    case Key_Up: { return "Up";  }
    case Key_Right: { return "Right";  }
    case Key_Down: { return "Down";  }
    case Key_PrintScreen: { return "PrintScreen";  }
    case Key_Insert: { return "Insert";  }
    case Key_Delete: { return "Delete";  }
    case Key_0: { return "0";  }
    case Key_1: { return "1";  }
    case Key_2: { return "2";  }
    case Key_3: { return "3";  }
    case Key_4: { return "4";  }
    case Key_5: { return "5";  }
    case Key_6: { return "6";  }
    case Key_7: { return "7";  }
    case Key_8: { return "8";  }
    case Key_9: { return "9";  }
    case Key_A: { return "A";  }
    case Key_B: { return "B";  }
    case Key_C: { return "C";  }
    case Key_D: { return "D";  }
    case Key_E: { return "E";  }
    case Key_F: { return "F";  }
    case Key_G: { return "G";  }
    case Key_H: { return "H";  }
    case Key_I: { return "I";  }
    case Key_J: { return "J";  }
    case Key_K: { return "K";  }
    case Key_L: { return "L";  }
    case Key_M: { return "M";  }
    case Key_N: { return "N";  }
    case Key_O: { return "O";  }
    case Key_P: { return "P";  }
    case Key_Q: { return "Q";  }
    case Key_R: { return "R";  }
    case Key_S: { return "S";  }
    case Key_T: { return "T";  }
    case Key_U: { return "U";  }
    case Key_V: { return "V";  }
    case Key_W: { return "W";  }
    case Key_X: { return "X";  }
    case Key_Y: { return "Y";  }
    case Key_Z: { return "Z";  }
    case Key_LeftSuper: { return "LeftSuper";  }
    case Key_RightSuper: { return "RightSuper";  }
    case Key_Num0: { return "Num0";  }
    case Key_Num1: { return "Num1";  }
    case Key_Num2: { return "Num2";  }
    case Key_Num3: { return "Num3";  }
    case Key_Num4: { return "Num4";  }
    case Key_Num5: { return "Num5";  }
    case Key_Num6: { return "Num6";  }
    case Key_Num7: { return "Num7";  }
    case Key_Num8: { return "Num8";  }
    case Key_Num9: { return "Num9";  }
    case Key_NumMultiply: { return "NumMultiply";  }
    case Key_NumAdd: { return "NumAdd";  }
    case Key_NumSubtract: { return "NumSubtract";  }
    case Key_NumDecimal: { return "NumDecimal";  }
    case Key_NumDivide: { return "NumDivide";  }
    case Key_F1: { return "F1";  }
    case Key_F2: { return "F2";  }
    case Key_F3: { return "F3";  }
    case Key_F4: { return "F4";  }
    case Key_F5: { return "F5";  }
    case Key_F6: { return "F6";  }
    case Key_F7: { return "F7";  }
    case Key_F8: { return "F8";  }
    case Key_F9: { return "F9";  }
    case Key_F10: { return "F10";  }
    case Key_F11: { return "F11";  }
    case Key_F12: { return "F12";  }
    case Key_F13: { return "F13";  }
    case Key_F14: { return "F14";  }
    case Key_F15: { return "F15";  }
    case Key_F16: { return "F16";  }
    case Key_F17: { return "F17";  }
    case Key_F18: { return "F18";  }
    case Key_F19: { return "F19";  }
    case Key_F20: { return "F20";  }
    case Key_F21: { return "F21";  }
    case Key_F22: { return "F22";  }
    case Key_F23: { return "F23";  }
    case Key_F24: { return "F24";  }
    case Key_NumLock: { return "NumLock";  }
    case Key_ScrollLock: { return "ScrollLock";  }
    case Key_Menu: { return "Menu";  }
    case Key_Semicolon: { return "Semicolon";  }
    case Key_Equal: { return "Equal";  }
    case Key_Comma: { return "Comma";  }
    case Key_Minus: { return "Minus";  }
    case Key_Period: { return "Period";  }
    case Key_Slash: { return "Slash";  }
    case Key_Tilde: { return "Tilde";  }
    case Key_LeftBracket: { return "LeftBracket";  }
    case Key_BackSlash: { return "BackSlash";  }
    case Key_RightBracket: { return "RightBracket";  }
    case Key_Apostrophe: { return "Apostrophe";  }
    case Key_NumEnter: { return "NumEnter";  }
    default: { return "Invalid"; }
    }
}

inline const char* ToString(MouseButton button)
{
    switch (button)
    {
    case MouseButton_Left:     return "Left";
    case MouseButton_Right:    return "Right";
    case MouseButton_Middle:   return "Middle";
    case MouseButton_XButton1: return "XButton1";
    case MouseButton_XButton2: return "XButton2";
        InvalidDefault();
    }
    // Dummy return for the compiler
    return "Unknown";
}
