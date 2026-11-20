/* Three exact metadata methods only. Never invoke, patch, follow or unprotect. */
#include <stddef.h>
#define NATIVE_CODE_CAP 512u
#define NATIVE_UNKNOWN_PREFIX 256u
#define NATIVE_ABI_SHA "FA65649ED1800F460316BD66C9D176D22C8FE5E12BECE51F4EC74E4FF8ABF6AB"
/* Declared prefix from the workspace integration il2cpp-class-internals.h:341.
 * Runtime class/name/return pointers must match independent exported APIs. */
typedef struct NativeMethodPrefix {
    Il2CppMethodPointer entry, virtual_entry;
    void* invoker;
    const char* name;
    Il2CppClass* klass;
    const Il2CppType* result;
} NativeMethodPrefix;
_Static_assert(sizeof(void*) == 8 && offsetof(NativeMethodPrefix, entry) == 0 &&
    offsetof(NativeMethodPrefix, name) == 24 && offsetof(NativeMethodPrefix, klass) == 32 &&
    offsetof(NativeMethodPrefix, result) == 40, "Reviewed x64 MethodInfo prefix required");
#ifdef SURVEY_NATIVE_TEST
static SIZE_T (WINAPI *native_query)(LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T) = VirtualQuery;
static BOOL (WINAPI *native_read)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*) = ReadProcessMemory;
#else
#define native_query VirtualQuery
#define native_read ReadProcessMemory
#endif
typedef struct NativeRead { size_t copied; const char* stop; DWORD error; } NativeRead;
static bool native_readable(DWORD protection, bool code) {
    if (protection & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    DWORD p = protection & 0xff;
    bool readable = p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
        p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
    return readable && (!code || p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY);
}
static NativeRead native_copy(uintptr_t address, void* output, size_t requested, bool code) {
    NativeRead r = {0, "complete", 0};
    if (!address || !output || !requested || requested > NATIVE_CODE_CAP || address > UINTPTR_MAX - requested) { r.stop = "invalid_or_over_cap_range"; return r; }
    SYSTEM_INFO system; GetSystemInfo(&system); size_t page = system.dwPageSize;
    if (!page) { r.stop = "page_size_unavailable"; return r; }
    while (r.copied < requested) {
        uintptr_t current = address + r.copied; MEMORY_BASIC_INFORMATION region;
        if (native_query((void*)current, &region, sizeof(region)) != sizeof(region)) { r.stop = "query_failed"; r.error = GetLastError(); break; }
        uintptr_t base = (uintptr_t)region.BaseAddress;
        if (region.State != MEM_COMMIT || !native_readable(region.Protect, code)) { r.stop = "not_committed_readable_page"; break; }
        if (base > current || region.RegionSize > UINTPTR_MAX - base || base + region.RegionSize <= current) { r.stop = "invalid_region"; break; }
        size_t chunk = requested - r.copied, available = base + region.RegionSize - current;
        if (chunk > available) chunk = available;
        size_t page_left = page - current % page; if (chunk > page_left) chunk = page_left;
        SIZE_T got = 0; BOOL ok = native_read(GetCurrentProcess(), (void*)current, (char*)output + r.copied, chunk, &got);
        if (got > chunk) { r.stop = "invalid_read_count"; break; }
        r.copied += got;
        if (!ok || got != chunk) { r.stop = "read_failed_or_partial"; r.error = GetLastError(); break; }
    }
    return r;
}
static bool native_exact(uintptr_t at, void* out, size_t bytes) { return native_copy(at, out, bytes, false).copied == bytes; }
typedef struct NativeImage { uintptr_t base; DWORD size, timestamp, checksum, pdata_rva, pdata_size; WORD machine; bool valid; } NativeImage;
static NativeImage native_image(HMODULE module) {
    NativeImage image = {0}; image.base = (uintptr_t)module;
    if (!module) return image;
    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
    if (!native_exact(image.base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < (LONG)sizeof(dos) || dos.e_lfanew > 65536 ||
        image.base > UINTPTR_MAX - (uintptr_t)dos.e_lfanew || !native_exact(image.base + dos.e_lfanew, &nt, sizeof(nt)) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || !nt.OptionalHeader.SizeOfImage) return image;
    if (image.base > UINTPTR_MAX - nt.OptionalHeader.SizeOfImage) return image;
    image.size = nt.OptionalHeader.SizeOfImage; image.timestamp = nt.FileHeader.TimeDateStamp;
    image.checksum = nt.OptionalHeader.CheckSum; image.machine = nt.FileHeader.Machine; image.valid = true; return image;
}
/* Runtime-loaded PE exception directory only, through the same guarded reader.
 * No Rtl unwind lookup can implicitly touch an unchecked/guarded .pdata page. */
static bool native_function_span(NativeImage* image, uintptr_t entry, DWORD* begin, DWORD* end) {
    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
    if (!image->valid || entry < image->base || entry - image->base >= image->size ||
        !native_exact(image->base, &dos, sizeof(dos)) || dos.e_lfanew < (LONG)sizeof(dos) || dos.e_lfanew > 65536 ||
        !native_exact(image->base + dos.e_lfanew, &nt, sizeof(nt)) || nt.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXCEPTION) return false;
    IMAGE_DATA_DIRECTORY directory = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (!directory.VirtualAddress || !directory.Size || directory.Size % sizeof(RUNTIME_FUNCTION) || directory.VirtualAddress >= image->size || directory.Size > image->size - directory.VirtualAddress) return false;
    image->pdata_rva = directory.VirtualAddress; image->pdata_size = directory.Size;
    size_t low = 0, high = directory.Size / sizeof(RUNTIME_FUNCTION); DWORD rva = (DWORD)(entry - image->base);
    for (unsigned step = 0; low < high && step < 32; ++step) {
        size_t mid = low + (high - low) / 2; RUNTIME_FUNCTION f;
        if (!native_exact(image->base + directory.VirtualAddress + mid * sizeof(f), &f, sizeof(f)) || f.BeginAddress >= f.EndAddress || f.EndAddress > image->size) return false;
        if (rva < f.BeginAddress) high = mid;
        else if (rva >= f.EndAddress) low = mid + 1;
        else { *begin = f.BeginAddress; *end = f.EndAddress; return true; }
    }
    return false;
}

