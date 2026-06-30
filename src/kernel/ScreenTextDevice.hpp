#pragma once

#include "serial.hpp"
#include <bootdefs.h>
#include <stdbool.h>
#include <stdint.h>
#include <font.hpp>

static constexpr uint16_t SERIAL_COM1 = 0x3F8;
static constexpr uint32_t CLEAR_COLOR = 0x00000000;
static constexpr uint32_t PSF1_CHARWIDTH = 8;

class ScreenTextDevice
{
public:
    ScreenTextDevice(Framebuffer *fb, PsF1Header *font)
        : m_fb(fb), m_font(font)
    {
        outb(SERIAL_COM1 + 1, 0x00);
        outb(SERIAL_COM1 + 3, 0x80);
        outb(SERIAL_COM1 + 0, 0x01);
        outb(SERIAL_COM1 + 1, 0x00);
        outb(SERIAL_COM1 + 3, 0x03);
        outb(SERIAL_COM1 + 2, 0xC7);
        outb(SERIAL_COM1 + 4, 0x0B);
    }

    void setColor(uint32_t value) { m_color = value; }

    void putPix(uint32_t x, uint32_t y, uint32_t color)
    {
        if (x >= m_fb->Width || y >= m_fb->Height)
            return;
        uint32_t *pixPtr = (uint32_t *)m_fb->BaseAddress;
        pixPtr[y * m_fb->PixelsPerScanLine + x] = color;
    }

    void clear()
    {
        uint32_t *pixPtr = (uint32_t *)m_fb->BaseAddress;
        uint64_t count = (uint64_t)m_fb->Height * m_fb->PixelsPerScanLine;
        for (uint64_t i = 0; i < count; i++)
            pixPtr[i] = CLEAR_COLOR;
        cursor_x = 0;
        cursor_y = 0;
    }

    void printc(char c, uint32_t xOff, uint32_t yOff, bool useSerial = false)
    {
        if (useSerial)
        {
            serialWrite((uint8_t)c);
            return;
        }

        unsigned char *glyph = (unsigned char *)m_font + sizeof(PsF1Header) + ((unsigned char)c * m_font->charsize);

        for (uint32_t y = 0; y < m_font->charsize; y++)
        {
            for (uint32_t x = 0; x < PSF1_CHARWIDTH; x++)
            {
                uint32_t px = (glyph[y] & (0x80 >> x)) ? m_color : CLEAR_COLOR;
                putPix(xOff + x, yOff + y, px);
            }
        }
    }

    void printf(const char *fmt,  ... )
    {
        __builtin_va_list args;
        __builtin_va_start(args, fmt);

        for (const char *ptr = fmt; *ptr != '\0'; ptr++)
        {
            if (*ptr == '%')
            {
                ptr++;
                switch (*ptr)
                {
                case 's':
                {
                    const char *s = __builtin_va_arg(args, const char *);
                    if (!s)
                        s = "(null)";
                    print(s, false);
                    break;
                }
                case 'd':
                case 'i':
                {
                    int64_t i = __builtin_va_arg(args, int64_t);
                    if (i < 0)
                    {
                        print("-", false);
                        printuint((uint64_t)-i, false);
                    }
                    else
                    {
                        printuint((uint64_t)i, false);
                    }
                    break;
                }
                case 'u':
                {
                    uint64_t u = __builtin_va_arg(args, uint64_t);
                    printuint(u, false);
                    break;
                }
                case 'x':
                case 'p':
                {
                    uint64_t x = __builtin_va_arg(args, uint64_t);
                    printh(x, false);
                    break;
                }
                case 'l':
                {
                    ptr++;
                    switch (*ptr)
                    {
                    case 'u':
                    {
                        uint64_t u = __builtin_va_arg(args, uint64_t);
                        printuint(u, false);
                        break;
                    }
                    case 'x':
                    case 'p':
                    {
                        uint64_t x = __builtin_va_arg(args, uint64_t);
                        printh(x, false);
                        break;
                    }
                    case 'd':
                    case 'i':
                    {
                        int64_t i = __builtin_va_arg(args, int64_t);
                        if (i < 0)
                        {
                            print("-", false);
                            printuint((uint64_t)-i, false);
                        }
                        else
                        {
                            printuint((uint64_t)i, false);
                        }
                        break;
                    }
                    default:
                    {
                        char str[3] = {'l', *ptr, '\0'};
                        print(str, false);
                        break;
                    }
                    }
                    break;
                }
                case 'c':
                {
                    // char is promoted to int in variadic functions
                    char c = (char)__builtin_va_arg(args, int);
                    char str[2] = {c, '\0'};
                    print(str, false);
                    break;
                }
                case '%':
                {
                    print("%", false);
                    break;
                }
                default:
                {
                    char str[2] = {*ptr, '\0'};
                    print(str, false);
                    break;
                }
                }
            }
            else
            {
                char str[2] = {*ptr, '\0'};
                print(str, false);
            }
        }

        __builtin_va_end(args);
    }

    void print(const char *str, bool useSerial = false)
    {
        int prevCX = 0, prevCY = 0;

        if(useSerial){
            prevCX = cursor_x;
            prevCY = cursor_y;
        }

        for (; *str != '\0'; str++)
        {
            switch (*str)
            {
            case '\n':
                cursor_x = 0;
                cursor_y += m_font->charsize;
                break;

            case '\r':
                cursor_x = 0;
                break;

            case '\t':
                cursor_x += TAB_WIDTH * PSF1_CHARWIDTH;
                if (cursor_x >= m_fb->Width)
                {
                    cursor_x = 0;
                    cursor_y += m_font->charsize;
                }
                break;

            default:
                printc(*str, cursor_x, cursor_y, useSerial);
                cursor_x += PSF1_CHARWIDTH;
                if (cursor_x + PSF1_CHARWIDTH > m_fb->Width)
                {
                    cursor_x = 0;
                    cursor_y += m_font->charsize;
                }
                break;
            }

            if(useSerial){
                cursor_x = prevCX;
                cursor_y = prevCY;
            }

            if (cursor_y + m_font->charsize > m_fb->Height)
                scroll();
        }
    }
    void printd(const char* msg){
        printf("K: %s", true, msg);
    }

    void printh(uint64_t value, bool useSerial = false)
    {
        static char buf[19];
        buf[0] = '0';
        buf[1] = 'x';
        buf[18] = '\0';

        static constexpr char hex[] = "0123456789ABCDEF";
        for (int i = 17; i >= 2; i--)
        {
            buf[i] = hex[value & 0xF];
            value >>= 4;
        }

        print(buf, useSerial);
    }

    void printh16(uint16_t value, bool useSerial = false)
    {
        static char buf[7];
        buf[0] = '0';
        buf[1] = 'x';
        buf[6] = '\0';
        static constexpr char hex[] = "0123456789ABCDEF";
        for (int i = 5; i >= 2; i--)
        {
            buf[i] = hex[value & 0xF];
            value >>= 4;
        }
        print(buf, useSerial);
    }

    void printuint(uint64_t value, bool useSerial = false)
    {
        if (value == 0)
        {
            print("0", useSerial);
            return;
        }

        char buf[21];
        buf[20] = '\0';
        int i = 19;

        while (value > 0 && i >= 0)
        {
            buf[i--] = '0' + (value % 10);
            value /= 10;
        }

        print(&buf[i + 1], useSerial);
    }

    void backspace()
    {
        if (cursor_x >= PSF1_CHARWIDTH)
        {
            cursor_x -= PSF1_CHARWIDTH;
        }
        else if (cursor_y >= m_font->charsize)
        {
            cursor_y -= m_font->charsize;
            cursor_x = m_fb->Width - ((m_fb->Width % PSF1_CHARWIDTH) + PSF1_CHARWIDTH);
        }
        else
        {
            return;
        }

        for (uint32_t y = 0; y < m_font->charsize; y++)
            for (uint32_t x = 0; x < PSF1_CHARWIDTH; x++)
                putPix(cursor_x + x, cursor_y + y, CLEAR_COLOR);
    }

private:
    void serialWrite(uint8_t c)
    {
        while ((inb(SERIAL_COM1 + 5) & 0x20) == 0)
            ;
        outb(SERIAL_COM1, c);
    }

    void scroll()
    {
        uint32_t *base = (uint32_t *)m_fb->BaseAddress;
        uint32_t linePixels = m_fb->PixelsPerScanLine;
        uint32_t charHeight = m_font->charsize;

        uint64_t sizeToCopy = (uint64_t)(m_fb->Height - charHeight) * linePixels;
        for (uint64_t i = 0; i < sizeToCopy; i++)
        {
            base[i] = base[i + (charHeight * linePixels)];
        }

        uint64_t clearStart = (uint64_t)(m_fb->Height - charHeight) * linePixels;
        uint64_t totalPixels = (uint64_t)m_fb->Height * linePixels;
        for (uint64_t i = clearStart; i < totalPixels; i++)
        {
            base[i] = CLEAR_COLOR;
        }

        cursor_y -= charHeight;
    }

    PsF1Header *m_font;
    Framebuffer *m_fb;

    uint32_t m_color{0xFFFFFFFF};
    uint32_t cursor_x{0};
    uint32_t cursor_y{0};

    static constexpr uint32_t TAB_WIDTH = 4;
};