#pragma once
#include <stdint.h>

struct Framebuffer{
    uint64_t BaseAddress;
    uint64_t BufferSize;
    uint32_t Width;
    uint32_t Height;
    uint32_t PixelsPerScanLine;
};

typedef struct Framebuffer Framebuffer;

typedef struct {
    void* kernel_entry;
    void* rsdt_address;
    void* mmap_address;
    uint64_t mmap_size;
    uint64_t descriptor_size;
    void* font_address;
    Framebuffer framebuffer;
    uint64_t kernel_size;
} BootInfo;

typedef struct __attribute__((packed)) {
    char     Signature[8];
    uint8_t  Checksum;
    char     OEMID[6];
    uint8_t  Revision;
    uint32_t RsdtAddress;
} RSDPDescriptor;

typedef struct __attribute__((packed)) {
    RSDPDescriptor base;
    uint32_t       Length;
    uint64_t       XsdtAddress;
    uint8_t        ExtendedChecksum;
    uint8_t        Reserved[3];
} RSDPDescriptor20;