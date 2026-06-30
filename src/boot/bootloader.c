#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <stddef.h>
#include <UEFI.h>
#include "elf64.h"

typedef void (*KernelStart)(BootInfo*);
BootInfo binfo;
static UINT8 mmap_static_buf[64 * 1024];

Framebuffer* getFrameBuffer(EFI_SYSTEM_TABLE *SystemTable, EFI_GUID gopGuid){
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    static Framebuffer fb;
    EFI_STATUS status = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3, &gopGuid, NULL, (void**)&gop);
    if (status == EFI_SUCCESS) {
        fb.BaseAddress       = gop->Mode->FrameBufferBase;
        fb.BufferSize        = gop->Mode->FrameBufferSize;
        fb.Width             = gop->Mode->Info->HorizontalResolution;
        fb.Height            = gop->Mode->Info->VerticalResolution;
        fb.PixelsPerScanLine = gop->Mode->Info->PixelsPerScanLine;
    } else {
        Print(L"ERROR: Could not locate GOP!\r\n");
        return NULL;
    }
    return &fb;
}

void* getFontFile(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *root, EFI_GUID file_info_guid){
    EFI_FILE_PROTOCOL *font_file;
    EFI_STATUS status = uefi_call_wrapper(root->Open, 5, root, &font_file,
        L"\\Ultima\\fonts\\zap-light16.psf", EFI_FILE_MODE_READ, 0);

    if (status != EFI_SUCCESS) {
        Print(L"Open failed, status: %r\n", status);
        return NULL;
    }

    UINTN f_info_size = sizeof(EFI_FILE_INFO) + 1024;
    EFI_FILE_INFO *f_info;
    uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, f_info_size, (void**)&f_info);
    uefi_call_wrapper(font_file->GetInfo, 4, font_file, &file_info_guid, &f_info_size, f_info);

    UINTN font_size = f_info->FileSize;
    EFI_PHYSICAL_ADDRESS font_buffer;
    uefi_call_wrapper(SystemTable->BootServices->AllocatePages, 4,
        AllocateAnyPages, EfiLoaderData, (font_size + 4095) / 4096, &font_buffer);
    uefi_call_wrapper(font_file->Read, 3, font_file, &font_size, (void*)font_buffer);
    FreePool(f_info);
    return (void*)font_buffer;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable){
    InitializeLib(ImageHandle, SystemTable);
    Print(L"NeoOS Bootloader: Initializing Handover...\r\n");

    EFI_GUID vfs_guid          = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID gopGuid           = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GUID acpi_guid         = ACPI_20_TABLE_GUID;

    EFI_LOADED_IMAGE               *loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *vfs;
    EFI_FILE_PROTOCOL              *root, *kernel_file;
    EFI_STATUS status = EFI_SUCCESS;

    Framebuffer* fb = getFrameBuffer(SystemTable, gopGuid);
    if (fb == (void*)0) status = EFI_DEVICE_ERROR;

    uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3, ImageHandle, &loaded_image_guid, (void**)&loaded_image);
    uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3, loaded_image->DeviceHandle, &vfs_guid, (void**)&vfs);
    uefi_call_wrapper(vfs->OpenVolume, 2, vfs, &root);

    void* rsdt_ptr = NULL;
    for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++) {
        if (CompareGuid(&SystemTable->ConfigurationTable[i].VendorGuid, &acpi_guid) == 0) {
            rsdt_ptr = SystemTable->ConfigurationTable[i].VendorTable;
            Print(L"ACPI Table Found at: 0x%lx\r\n", rsdt_ptr);
            break;
        }
    }

    status = uefi_call_wrapper(root->Open, 5, root, &kernel_file, L"\\Ultima\\KERNEL.ELF", EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) {
        Print(L"ERROR: Kernel not found!\r\n");
        return status;
    }

    EFI_GUID file_info_guid = EFI_FILE_INFO_ID;
    EFI_FILE_INFO *info;
    UINTN info_size = sizeof(EFI_FILE_INFO) + 1024;
    uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, info_size, (void**)&info);
    uefi_call_wrapper(kernel_file->GetInfo, 4, kernel_file, &file_info_guid, &info_size, info);
    UINTN kernel_size = info->FileSize;

    EFI_PHYSICAL_ADDRESS kernel_buffer = 0;
    UINTN pages = (kernel_size + 4095) / 4096;
    status = uefi_call_wrapper(SystemTable->BootServices->AllocatePages, 4,
        AllocateAnyPages, EfiLoaderData, pages, &kernel_buffer);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Could not allocate memory for ELF buffer!\r\n");
        return status;
    }

    UINTN read_size = kernel_size;
    uefi_call_wrapper(kernel_file->Read, 3, kernel_file, &read_size, (void*)kernel_buffer);
    Print(L"Kernel ELF loaded at 0x%lx, size: %lu bytes\r\n", kernel_buffer, kernel_size);

    Elf64_Ehdr* elf = (Elf64_Ehdr*)kernel_buffer;
    if (*(uint32_t*)elf->e_ident != ELF_MAGIC) {
        Print(L"ERROR: Invalid ELF magic!\r\n");
        return EFI_LOAD_ERROR;
    }

    Elf64_Phdr* phdrs = (Elf64_Phdr*)(kernel_buffer + elf->e_phoff);
    for (int i = 0; i < elf->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            EFI_PHYSICAL_ADDRESS seg_addr = phdrs[i].p_paddr;
            UINTN seg_pages = (phdrs[i].p_memsz + 4095) / 4096;
            status = uefi_call_wrapper(SystemTable->BootServices->AllocatePages, 4,
                AllocateAddress, EfiLoaderCode, seg_pages, &seg_addr);
            if (EFI_ERROR(status)) {
                Print(L"ERROR: Could not allocate segment %d at 0x%lx\r\n", i, phdrs[i].p_paddr);
                return status;
            }
            CopyMem((void*)phdrs[i].p_paddr,
                    (void*)(kernel_buffer + phdrs[i].p_offset),
                    phdrs[i].p_filesz);
            SetMem((void*)(phdrs[i].p_paddr + phdrs[i].p_filesz),
                   phdrs[i].p_memsz - phdrs[i].p_filesz, 0);
            Print(L"  Loaded segment %d: 0x%lx (%lu bytes)\r\n", i, phdrs[i].p_paddr, phdrs[i].p_memsz);
        }
    }

    Print(L"ELF loaded. Entry point: 0x%lx\r\n", elf->e_entry);
    binfo.font_address = getFontFile(SystemTable, root, file_info_guid);

    Print(L"offsetof framebuffer:     %lu\r\n", __builtin_offsetof(BootInfo, framebuffer));
    Print(L"sizeof BootInfo:          %lu\r\n", sizeof(BootInfo));
    Print(L"rsdt_ptr value:           0x%lx\r\n", rsdt_ptr);
    Print(L"Jumping to kernel at 0x%lx...\r\n", elf->e_entry);

    binfo.kernel_entry = (void*)elf->e_entry;
    binfo.kernel_size  = kernel_size;
    binfo.framebuffer  = *fb;

    Print(L"Handoff to kernel started!\r\n");

    UINTN mmap_size = 0;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 desc_version = 0;
    EFI_MEMORY_DESCRIPTOR* mmap = NULL;

    uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
        &mmap_size, NULL, &map_key, &descriptor_size, &desc_version);

    mmap_size += 2 * descriptor_size;

    uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3,
        EfiLoaderData, mmap_size, (void**)&mmap);

    status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
        &mmap_size, mmap, &map_key, &descriptor_size, &desc_version);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2,
        ImageHandle, map_key);
    if (EFI_ERROR(status)) return status;

    binfo.mmap_address    = (void*)mmap;
    binfo.mmap_size       = mmap_size;
    binfo.descriptor_size = descriptor_size;

    KernelStart kernel_start = (KernelStart)elf->e_entry;
    //__asm__ volatile("cli");
    kernel_start(&binfo);

    while (1) __asm__ volatile("hlt");
    return EFI_SUCCESS;
}