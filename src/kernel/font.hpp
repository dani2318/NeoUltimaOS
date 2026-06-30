#pragma once

struct PsF1Header
{
    unsigned char magic[2];
    unsigned char mode;
    unsigned char charsize;
};

class Font {
    public:
        Font(){}

        void Load(void* fontAddress){
            fontHeader = LoadFont(fontAddress);
        }

        PsF1Header* GetFontPs1Header(){
            return this->fontHeader;
        }

    private:
        PsF1Header* fontHeader;
        PsF1Header *LoadFont(void* fontAddress)
        {
            PsF1Header *font = (PsF1Header *)fontAddress;
            if (font->magic[0] != 0x36 || font->magic[1] != 0x04)
            {
                while (1) __asm__ volatile("hlt");
            }
            return font;
        };
};