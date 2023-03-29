#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/BlockIo.h>
#include <Guid/FileInfo.h>

struct MemoryMap
{
  UINTN buffer_size;
  VOID* buffer;
  UINTN map_size;
  UINTN map_key;
  UINTN descriptor_size;
  UINT32 descriptor_version;
};

EFI_STATUS GetMemoryMap(struct MemoryMap* map)
{
  if(map->buffer == NULL)
    return EFI_BUFFER_TOO_SMALL;
  
  map->map_size = map->buffer_size;
  return gBS->GetMemoryMap(&map->map_size, (EFI_MEMORY_DESCRIPTOR*)map->buffer, &map->map_key, &map->descriptor_size, &map->descriptor_version);
}

const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
  switch (type) {
    case EfiReservedMemoryType: return L"EfiReservedMemoryType";
    case EfiLoaderCode: return L"EfiLoaderCode";
    case EfiLoaderData: return L"EfiLoaderData";
    case EfiBootServicesCode: return L"EfiBootServicesCode";
    case EfiBootServicesData: return L"EfiBootServicesData";
    case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
    case EfiConventionalMemory: return L"EfiConventionalMemory";
    case EfiUnusableMemory: return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode: return L"EfiPalCode";
    case EfiPersistentMemory: return L"EfiPersistentMemory";
    case EfiMaxMemoryType: return L"EfiMaxMemoryType";
    default: return L"InvalidMemoryType";
  }
}

const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt) {
  switch (fmt) {
    case PixelRedGreenBlueReserved8BitPerColor: return L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor: return L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask: return L"PixelBitMask";
    case PixelBltOnly: return L"PixelBltOnly";
    case PixelFormatMax: return L"PixelFormatMax";
    default: return L"InvalidPixelFormat";
  }
}

void Halt(void)
{
  while (1) __asm__("hlt");
}

EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file)
{
  EFI_STATUS status;
  CHAR8 buf[256];
  UINTN len;

  CHAR8* header = "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
  len = AsciiStrLen(header);
  status = file->Write(file, &len, header);
  if(EFI_ERROR(status)) return status;
  
  Print(L"map->buffer = %08lx, map->map_size = %08lx\n", map->buffer, map->map_size);

  EFI_PHYSICAL_ADDRESS iter;
  int i;
  for (iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0; iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size; iter += map->descriptor_size, i++)
  {
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
    len = AsciiSPrint(buf, sizeof(buf), "%u, %x, %-ls, %08lx, %lx, %lx\n", i, desc->Type, GetMemoryTypeUnicode(desc->Type), desc->PhysicalStart, desc->NumberOfPages, desc->Attribute & 0xffffflu);
    status = file->Write(file, &len, buf);
    if(EFI_ERROR(status)) return status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root)
{
  EFI_STATUS status;
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;

  status = gBS->OpenProtocol(image_handle, &gEfiLoadedImageProtocolGuid, (VOID**)&loaded_image, image_handle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if(EFI_ERROR(status)) return status;
  status = gBS->OpenProtocol(loaded_image->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&fs, image_handle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if(EFI_ERROR(status)) return status;
  return fs->OpenVolume(fs, root);
}

EFI_STATUS OpenGOP(EFI_HANDLE image_handle, EFI_GRAPHICS_OUTPUT_PROTOCOL** gop)
{
  EFI_STATUS status;
  UINTN num_gop_handles = 0;
  EFI_HANDLE* gop_handles = NULL;
  status = gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &num_gop_handles, &gop_handles);
  if(EFI_ERROR(status)) return status;
  status = gBS->OpenProtocol(gop_handles[0], &gEfiGraphicsOutputProtocolGuid, (VOID**)gop, image_handle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if(EFI_ERROR(status)) return status;
  FreePool(gop_handles);
  return EFI_SUCCESS;
}

void CheckErrorAndDisplayMessge(EFI_STATUS status, CHAR16* msg, BOOLEAN halt)
{
  CHAR16 buf[128];
  UnicodeSPrint(buf, sizeof(buf), L"%s: %r\n", msg, status);
  if(EFI_ERROR(status))
  {
    Print(buf);
    if(halt) Halt();
  }
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
  EFI_STATUS status;
  Print(L"Hello, LemonadeOS! inspired by MikanOS\n\n");

  //Load memory map and save it into file
  CHAR8 memmap_buf[4096 * 4];
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
  status = GetMemoryMap(&memmap);
  CheckErrorAndDisplayMessge(status, L"Failed to get memory map", TRUE);

  EFI_FILE_PROTOCOL* root_dir;
  status = OpenRootDir(image_handle, &root_dir);
  CheckErrorAndDisplayMessge(status, L"Failed to open root directory", TRUE);

  EFI_FILE_PROTOCOL* memmap_file;
  status = root_dir->Open(root_dir, &memmap_file, L"\\memmap", EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  CheckErrorAndDisplayMessge(status, L"Failed to open file \\memmap, but ignored", FALSE);
  status = SaveMemoryMap(&memmap, memmap_file);
  CheckErrorAndDisplayMessge(status, L"Failed to save \\memmap, but ignored", FALSE);
  status = memmap_file->Close(memmap_file);
  CheckErrorAndDisplayMessge(status, L"Failed to close file \\memmap, but ignored", FALSE);

  //Open GOP
  EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
  status = OpenGOP(image_handle, &gop);
  CheckErrorAndDisplayMessge(status, L"Failed to open GOP", TRUE);
  Print(L"Resolution: %ux%u\n", gop->Mode->Info->HorizontalResolution, gop->Mode->Info->VerticalResolution);
  Print(L"Pixels format: %s\n", GetPixelFormatUnicode(gop->Mode->Info->PixelFormat));
  Print(L"Pixels per line: %u\n\n", gop->Mode->Info->PixelsPerScanLine);

  //Load kernel
  EFI_FILE_PROTOCOL* kernel_file;
  status = root_dir->Open(root_dir, &kernel_file, L"\\kernel.elf", EFI_FILE_MODE_READ, 0);
  CheckErrorAndDisplayMessge(status, L"Failed to Open file \\kernel.elf", TRUE);

  UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
  UINT8 file_info_buffer[file_info_size];
  status = kernel_file->GetInfo(kernel_file, &gEfiFileInfoGuid, &file_info_size, file_info_buffer);
  CheckErrorAndDisplayMessge(status, L"Failed to get kernel file information", TRUE);

  EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
  UINTN kernel_file_size = file_info->FileSize;

  EFI_PHYSICAL_ADDRESS kernel_base_addr = 0x100000;
  UINT8 allocated_pages = (kernel_file_size + 0xfff) / 0x1000;
  status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, allocated_pages, &kernel_base_addr);
  CheckErrorAndDisplayMessge(status, L"Failed to allocate pages", TRUE);
  status = kernel_file->Read(kernel_file, &kernel_file_size, (VOID*)kernel_base_addr);
  CheckErrorAndDisplayMessge(status, L"Failed to read kernel file", TRUE);
  UINT64 entry_addr = *(UINT64*)(kernel_base_addr + 24);

  Print(L"Kernel load addr: 0x%01x\n", kernel_base_addr);
  Print(L"Kernel entry addr:0x%01x\n", entry_addr);
  Print(L"Kernel file size: %lu\n", kernel_file_size);
  Print(L"Kernel allocated pages: %u\n\n", allocated_pages);

  gBS->Stall(5000000);

  status = gBS->ExitBootServices(image_handle, memmap.map_key);
  if(EFI_ERROR(status))
  {
    Print(L"Key of memory map has changed, try to get it again: %r\n", status);
    status = GetMemoryMap(&memmap);
    CheckErrorAndDisplayMessge(status, L"Failed to get memory map", TRUE);
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    CheckErrorAndDisplayMessge(status, L"Failed to exit boot service", TRUE);
  }

  typedef void EntryPointType(UINT64, UINT64);
  EntryPointType* entry_point = (EntryPointType*)entry_addr;
  entry_point(gop->Mode->FrameBufferBase, gop->Mode->FrameBufferSize);

  return EFI_SUCCESS;
}