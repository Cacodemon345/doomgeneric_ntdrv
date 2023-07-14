
#ifndef _WINNT_KERNEL_ENV
#define WIN32_NO_STATUS
#include <windows.h>
#include <kernelspecs.h>
#include "ntddkbd.h"
#ifndef _IRQL_requires_max_
#define _IRQL_requires_max_(x)
#endif
#include <ntndk.h>
#include "doomkeys.h"
#include "doomgeneric.h"
#include <immintrin.h>
#else
#include <ntifs.h>
#include <ntddk.h>
#include <wdm.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ntddkbd.h>
#include <ntddvdeo.h>
#include "doomkeys.h"
#include "doomgeneric.h"
#endif

#include "d_event.h"
#include <ntddmou.h>

#define INBVSHIM_TYPE 40001

#define IOCTL_INBVSHIM_SOLID_COLOR_FILL \
	CTL_CODE( INBVSHIM_TYPE, 0x850, METHOD_IN_DIRECT, FILE_ANY_ACCESS )
	
#define IOCTL_INBVSHIM_RESET_DISPLAY \
	CTL_CODE( INBVSHIM_TYPE, 0x851, METHOD_NEITHER, FILE_ANY_ACCESS )


typedef enum _INBV_DISPLAY_STATE
{
    INBV_DISPLAY_STATE_OWNED,
    INBV_DISPLAY_STATE_DISABLED,
    INBV_DISPLAY_STATE_LOST
} INBV_DISPLAY_STATE;

//
// Function Callbacks
//
typedef BOOLEAN (NTAPI *INBV_RESET_DISPLAY_PARAMETERS)(ULONG Cols, ULONG Rows);

typedef VOID (NTAPI *INBV_DISPLAY_STRING_FILTER)(PCHAR *Str);

VOID NTAPI InbvAcquireDisplayOwnership(VOID);

BOOLEAN NTAPI InbvCheckDisplayOwnership(VOID);

VOID NTAPI InbvNotifyDisplayOwnershipLost(IN INBV_RESET_DISPLAY_PARAMETERS Callback);

//
// Installation Functions
//
VOID NTAPI InbvEnableBootDriver(IN BOOLEAN Enable);

VOID NTAPI InbvInstallDisplayStringFilter(IN INBV_DISPLAY_STRING_FILTER DisplayFilter);

BOOLEAN NTAPI InbvIsBootDriverInstalled(VOID);

//
// Display Functions
//
BOOLEAN NTAPI InbvDisplayString(IN PCHAR String);

BOOLEAN NTAPI InbvEnableDisplayString(IN BOOLEAN Enable);

BOOLEAN NTAPI InbvResetDisplay(VOID);

VOID NTAPI InbvSetScrollRegion(IN ULONG Left, IN ULONG Top, IN ULONG Width, IN ULONG Height);

VOID NTAPI InbvSetTextColor(IN ULONG Color);

VOID NTAPI InbvSolidColorFill(IN ULONG Left, IN ULONG Top, IN ULONG Width, IN ULONG Height, IN ULONG Color);

extern __declspec(dllimport) double floor(double arg);
extern __declspec(dllimport) long _ftol(double arg);
//long __cdecl _ftol2_sse(double val) { return _ftol(val); }

#define BV_COLOR_BLACK          0
#define BV_COLOR_RED            1
#define BV_COLOR_GREEN          2
#define BV_COLOR_BROWN          3
#define BV_COLOR_BLUE           4
#define BV_COLOR_MAGENTA        5
#define BV_COLOR_CYAN           6
#define BV_COLOR_DARK_GRAY      7
#define BV_COLOR_LIGHT_GRAY     8
#define BV_COLOR_LIGHT_RED      9
#define BV_COLOR_LIGHT_GREEN    10
#define BV_COLOR_YELLOW         11
#define BV_COLOR_LIGHT_BLUE     12
#define BV_COLOR_LIGHT_MAGENTA  13
#define BV_COLOR_LIGHT_CYAN     14
#define BV_COLOR_WHITE          15
#define BV_COLOR_NONE           16

int nt_errno = 0;
int* _imp___errno()
{
	return &nt_errno;
}

static PVOID heap = NULL;
static BOOLEAN InhibitGraphicsPrint = FALSE;

#define strace(s, x, ...) nt_printf(x": 0x%X", __VA_ARGS__, s)
#define etrace(x, ...) nt_printf(x, __VA_ARGS__)
#define mtrace()
#define DbgCheck1(x, y) if (!x) return y;
#define DbgCheck2(x, y, z) if (!x || !y) return z;

HANDLE hHeap = NULL;
typedef struct NT_FILE
{
	HANDLE hFile;             /* file handle */
	LARGE_INTEGER offset;    /* offset. */
} NT_FILE;

// Taken from PCDoom-v2.
unsigned char scantokey[128] =
{
	//  0           1       2       3       4       5       6       7
	//  8           9       A       B       C       D       E       F
		0  ,    KEY_ESCAPE,     '1',    '2',    '3',    '4',    '5',    '6',
		'7',    '8',    '9',    '0',    '-',    '=',    KEY_BACKSPACE, 9, // 0
		'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
		'o',    'p',    '[',    ']',    13 ,    KEY_FIRE,'a',  's',      // 1
		'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
		39 ,    '`',    KEY_RSHIFT,92,  'z',    'x',    'c',    'v',      // 2
		'b',    'n',    'm',    ',',    '.',    '/',    KEY_RSHIFT,'*',
		KEY_RALT,KEY_USE,   0 ,    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,   // 3
		KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,0  ,    0  , KEY_HOME,
		KEY_UPARROW,KEY_PGUP,'-',KEY_LEFTARROW,'5',KEY_RIGHTARROW,'+',KEY_END, //4
		KEY_DOWNARROW,KEY_PGDN,KEY_INS,KEY_DEL,0,0,             0,              KEY_F11,
		KEY_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5
		0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
		0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6
		0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
		0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7
};

int __cdecl nt_printf (const char* fmt, ...)
{
	va_list MessageList;
	PCHAR MessageBuffer;
	UNICODE_STRING MessageString;
	ANSI_STRING astr;
	NTSTATUS Status;
	int printchars;

	//
	// Allocate Memory for the String Buffer
	//
	MessageBuffer = ExAllocatePool(PagedPool, 4096);

	//
	// First, combine the message
	//
	va_start(MessageList, fmt);
	printchars = _vsnprintf(MessageBuffer, 4096, fmt, MessageList);
	va_end(MessageList);

	//
	// Now make it a unicode string
	//

	RtlInitAnsiString(&astr, MessageBuffer);
	RtlAnsiStringToUnicodeString(&MessageString, &astr, TRUE);

	//
	// Display it on screen
	//
	if (!InhibitGraphicsPrint)
		Status = ZwDisplayString(&MessageString);

	DbgPrint("%ls", MessageString.Buffer);

	//
	// Free Memory
	//
	ExFreePool(MessageBuffer);
	RtlFreeUnicodeString(&MessageString);

	//
	// Return to the caller
	//
	return printchars;
}

int __cdecl nt_fprintf(const NT_FILE* file, const char* fmt, ...)
{
	va_list MessageList;
	PCHAR MessageBuffer;
	NTSTATUS Status;
	IO_STATUS_BLOCK statblock;
	int printchars;

	memset(&statblock, 0, sizeof(IO_STATUS_BLOCK));
	//
	// Allocate Memory for the String Buffer
	//
	MessageBuffer = ExAllocatePool(PagedPool, 4096);

	//
	// First, combine the message
	//
	va_start(MessageList, fmt);
	printchars = _vsnprintf(MessageBuffer, 4096, fmt, MessageList);
	va_end(MessageList);

	if (printchars < 0)
	{
		goto cleanup;
	}
	//
	// Write to file.
	//
	if (file == stdout || file == stderr)
	{
		return nt_printf("%s", MessageBuffer);
	}
	Status = NtWriteFile(file->hFile, NULL, NULL, NULL, &statblock, MessageBuffer, printchars, NULL, NULL);
	if (!NT_SUCCESS(Status))
	{
		printchars = -1;
		goto cleanup;
	}
	else
	{
		printchars = (int)statblock.Information;
	}

cleanup:
	//
	// Free Memory
	//
	ExFreePool(MessageBuffer);

	//
	// Return to the caller
	//
	return printchars;
}

/* Rest of the file functions are taken from ZenWINX library but modified. */

static HANDLE hCurrentDirectory = NULL; // Needed for relative paths support.

static BOOLEAN nt_open_current_directory()
{
// FIXME: Broken as hell.
#if 0
	UNICODE_STRING us;
	NTSTATUS status;
	OBJECT_ATTRIBUTES oa;
	IO_STATUS_BLOCK iosb;
	WCHAR curDir[260];

	RtlZeroMemory(curDir, 260 * sizeof(WCHAR));
	RtlGetCurrentDirectory_U(260, curDir);
	RtlCreateUnicodeString(&us, curDir);
	InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = NtCreateFile(&hCurrentDirectory,
						  FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_OPEN_FOR_BACKUP_INTENT,
						  &oa,
						  &iosb,
						  NULL,
						  FILE_ATTRIBUTE_NORMAL,
						  FILE_SHARE_READ | FILE_SHARE_WRITE,
						  FILE_OPEN,
						  FILE_SYNCHRONOUS_IO_NONALERT | FILE_DIRECTORY_FILE,
						  NULL,
						  0
	);
	return NT_SUCCESS(status);
#else
	return FALSE;
#endif
}

/**
 * @brief fopen() native equivalent.
 * @note
 * - Accepts native paths only,
 * like \\??\\C:\\file.txt
 * - Only r, w, a, r+, w+, a+
 * modes are supported.
 */
NT_FILE* nt_fopen(const wchar_t* filename, const char* mode)
{
	UNICODE_STRING us;
	NTSTATUS status;
	HANDLE hFile;
	OBJECT_ATTRIBUTES oa;
	IO_STATUS_BLOCK iosb;
	char copiedmode[4] = { 0, 0, 0, 0 };
	ACCESS_MASK access_mask = FILE_GENERIC_READ;
	ULONG disposition = FILE_OPEN;
	NT_FILE* f;
	UINT32 i = 0, j = 0;

	DbgCheck2(filename, mode, NULL);
	if (!hCurrentDirectory)
	{
		nt_open_current_directory();
	}
	RtlInitUnicodeString(&us, filename);
	InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, hCurrentDirectory, NULL);
	for (i = 0; i < strlen(mode) && i < 4; i++)
	{
		if (mode[i] == 'b') continue;
		copiedmode[j] = mode[i];
		j++;
	}

	if (!strcmp(copiedmode, "r")) {
		access_mask = FILE_GENERIC_READ;
		disposition = FILE_OPEN;
	}
	else if (!strcmp(copiedmode, "w")) {
		access_mask = FILE_GENERIC_WRITE;
		disposition = FILE_OVERWRITE_IF;
	}
	else if (!strcmp(copiedmode, "r+")) {
		access_mask = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
		disposition = FILE_OPEN;
	}
	else if (!strcmp(copiedmode, "w+")) {
		access_mask = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
		disposition = FILE_OVERWRITE_IF;
	}
	else if (!strcmp(copiedmode, "a")) {
		access_mask = FILE_APPEND_DATA;
		disposition = FILE_OPEN_IF;
	}
	else if (!strcmp(copiedmode, "a+")) {
		access_mask = FILE_GENERIC_READ | FILE_APPEND_DATA;
		disposition = FILE_OPEN_IF;
	}
	access_mask |= SYNCHRONIZE;

	status = NtCreateFile(&hFile,
						  access_mask,
						  &oa,
						  &iosb,
						  NULL,
						  FILE_ATTRIBUTE_NORMAL,
						  FILE_SHARE_READ | FILE_SHARE_WRITE,
						  disposition,
						  FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
						  NULL,
						  0
	);
	if (status != STATUS_SUCCESS) {
		strace(status, "cannot open %ws", filename);
		if (status == STATUS_FILE_IS_A_DIRECTORY) nt_errno = EBADF;
		return NULL;
	}

	/* use winx_tmalloc just for winx_flush_dbg_log */
	f = (NT_FILE*)nt_malloc(sizeof(NT_FILE));
	if (f == NULL) {
		mtrace();
		NtClose(hFile);
		return NULL;
	}

	f->hFile = hFile;
	return f;
}

/**
 * @brief fread() native equivalent.
 */
size_t nt_fread(void* buffer, size_t size, size_t count, NT_FILE* f)
{
	NTSTATUS status;
	IO_STATUS_BLOCK iosb;

	DbgCheck2(buffer, f, 0);

	status = NtReadFile(f->hFile, NULL, NULL, NULL, &iosb,
						buffer, size * count, NULL, NULL);
	if (NT_SUCCESS(status)) {
		status = ZwWaitForSingleObject(f->hFile, FALSE, NULL);
		if (NT_SUCCESS(status)) status = iosb.Status;
	}
	if (status != STATUS_SUCCESS) {
		//strace(status, "cannot read from a file");
		return 0;
	}
	if (iosb.Information == 0) { /* encountered on x64 XP */
		return count;
	}
	return ((size_t)iosb.Information / size);
}

/**
 * @internal
 * @brief nt_fwrite helper.
 * @details Writes to file directly
 * regardless of whether it's opened
 * for buffered i/o or not.
 */
size_t nt_fwrite(const void* buffer, size_t size, size_t count, NT_FILE* f)
{
	NTSTATUS status;
	IO_STATUS_BLOCK iosb;

	DbgCheck2(buffer, f, 0);

	status = NtWriteFile(f->hFile, NULL, NULL, NULL, &iosb,
						 (void*)buffer, size * count, NULL, NULL);
	if (NT_SUCCESS(status)) {
		/*trace(D"waiting for %p at %I64u started",f,f->woffset.QuadPart);*/
		status = ZwWaitForSingleObject(f->hFile, FALSE, NULL);
		/*trace(D"waiting for %p at %I64u completed",f,f->woffset.QuadPart);*/
		if (NT_SUCCESS(status)) status = iosb.Status;
	}
	if (status != STATUS_SUCCESS) {
		//strace(status, "cannot write to a file");
		return 0;
	}
	if (iosb.Information == 0) { /* encountered on x64 XP */
		return count;
	}
	return ((size_t)iosb.Information / size);
}

/**
 * @brief Sends an I/O control code to the specified device.
 * @param[in] f the device handle.
 * @param[in] code the IOCTL code.
 * @param[in] description a string describing request's
 * meaning, intended for use by the error handling code.
 * @param[in] in_buffer the input buffer.
 * @param[in] in_size size of the input buffer, in bytes.
 * @param[out] out_buffer the output buffer.
 * @param[in] out_size size of the output buffer, in bytes.
 * @param[out] pbytes_returned pointer to variable receiving
 * the number of bytes written to the output buffer.
 * @return Zero for success, a negative value otherwise.
 */
int nt_ioctl(NT_FILE* f,
			   int code, char* description,
			   void* in_buffer, int in_size,
			   void* out_buffer, int out_size,
			   int* pbytes_returned)
{
	IO_STATUS_BLOCK iosb;
	NTSTATUS status;

	DbgCheck1(f, -1);

	/* required by x64 system, otherwise it may trash stack */
	if (out_buffer) RtlZeroMemory(out_buffer, out_size);

	if (pbytes_returned) *pbytes_returned = 0;
	if ((code >> 16) == FILE_DEVICE_FILE_SYSTEM) {
		status = NtFsControlFile(f->hFile, NULL, NULL, NULL,
								 &iosb, code, in_buffer, in_size, out_buffer, out_size);
	}
	else {
		status = NtDeviceIoControlFile(f->hFile, NULL, NULL, NULL,
									   &iosb, code, in_buffer, in_size, out_buffer, out_size);
	}
	if (NT_SUCCESS(status)) {
		status = ZwWaitForSingleObject(f->hFile, FALSE, NULL);
		if (NT_SUCCESS(status)) status = iosb.Status;
	}
	if (!NT_SUCCESS(status)) {
		if (description) {
			//strace(status, "%s failed", description);
		}
		else {
			//strace(status, "IOCTL %u failed", code);
		}
		return (-1);
	}
	if (pbytes_returned) *pbytes_returned = (int)iosb.Information;
	return 0;
}

/**
 * @brief fflush() native equivalent.
 * @return Zero for success,
 * a negative value otherwise.
 */
int nt_fflush(NT_FILE* f)
{
#ifndef _WINNT_KERNEL_ENV
	NTSTATUS status;
	IO_STATUS_BLOCK iosb;

	DbgCheck1(f, -1);

	status = ZwFlushBuffersFile(f->hFile, &iosb);
	if (!NT_SUCCESS(status)) {
		//strace(status, "cannot flush file buffers");
		return (-1);
	}
#endif
	return 0;
}

/**
 * @brief Retrieves size of a file.
 */
ULONGLONG nt_fsize(NT_FILE* f)
{
	NTSTATUS status;
	IO_STATUS_BLOCK iosb;
	FILE_STANDARD_INFORMATION fsi;

	DbgCheck1(f, 0);

	memset(&fsi, 0, sizeof(FILE_STANDARD_INFORMATION));
	status = NtQueryInformationFile(f->hFile, &iosb,
									&fsi, sizeof(FILE_STANDARD_INFORMATION),
									FileStandardInformation);
	if (!NT_SUCCESS(status)) {
		//strace(status, "cannot get standard file information");
		return 0;
	}
	return fsi.EndOfFile.QuadPart;
}

void nt_free(void* ptr);
/**
 * @brief fclose() native equivalent.
 */
void nt_fclose(NT_FILE* f)
{
	if (f == NULL)
		return;

	if (f->hFile) NtClose(f->hFile);
	nt_free(f);
}

/* End of ZenWINX file functions. */

int nt_ftell(NT_FILE* file)
{
	IO_STATUS_BLOCK sIoStatus;
	FILE_POSITION_INFORMATION sFilePosition;
	NTSTATUS ntStatus = 0;
	int pRetCurrentPosition = -1;

	memset(&sIoStatus, 0, sizeof(IO_STATUS_BLOCK));
	memset(&sFilePosition, 0, sizeof(FILE_POSITION_INFORMATION));

	ntStatus = NtQueryInformationFile(file->hFile, &sIoStatus, &sFilePosition,
									  sizeof(FILE_POSITION_INFORMATION), FilePositionInformation);
	if (ntStatus == STATUS_SUCCESS)
	{
		pRetCurrentPosition = (int)(sFilePosition.CurrentByteOffset.QuadPart);
	}

	return pRetCurrentPosition;
}

int nt_fseek(NT_FILE* stream, long offset, int origin)
{
	IO_STATUS_BLOCK sIoStatus;
	FILE_POSITION_INFORMATION sFilePosition;
	NTSTATUS ntStatus = 0;
	ULONGLONG curPos = offset;

	memset(&sIoStatus, 0, sizeof(IO_STATUS_BLOCK));
	if (origin == SEEK_CUR) curPos = nt_ftell(stream) + offset;
	else if (origin == SEEK_END) curPos = nt_fsize(stream) - 1ull + (ULONGLONG)offset;
	sFilePosition.CurrentByteOffset.QuadPart = curPos;
	ntStatus = NtSetInformationFile(stream->hFile, &sIoStatus, &sFilePosition,
									sizeof(FILE_POSITION_INFORMATION), FilePositionInformation);
	if (NT_SUCCESS(ntStatus))
	{
		return 0;
	}
	return -1;
}

void* nt_malloc(size_t size)
{
	void* ptr = NULL;
	if (!heap)
		heap = RtlCreateHeap(HEAP_GROWABLE, NULL, 1 << 24, 0, NULL, NULL);
	ptr = RtlAllocateHeap(heap, HEAP_ZERO_MEMORY, size);
	if (!ptr) nt_printf("nt_malloc failed.\n");
	return ptr;
}

void* nt_calloc(size_t size1, size_t size2)
{
	void* ptr = nt_malloc(size1 * size2);
	if (!ptr) return NULL;
	memset(ptr, size1 * size2, 0);
	return ptr;
}

void nt_free(void* ptr)
{
	if (ptr)
		RtlFreeHeap(heap, 0, ptr);
}

NTSYSAPI
SIZE_T
NTAPI
RtlSizeHeap(
    PVOID HeapHandle,
    ULONG Flags,
    PVOID MemoryPointer
);

void* nt_realloc(void* ptr, size_t newsize)
{
	void* new_ptr;
	size_t oldsize;
	if (ptr == NULL)
		return nt_malloc(newsize);
	
	oldsize = RtlSizeHeap(heap, 0, ptr);

	new_ptr = nt_malloc(newsize);
	if (!new_ptr) {
		nt_free(ptr);
		return NULL;
	}
	if (newsize <= oldsize)
		memcpy(new_ptr, ptr, newsize);
	else
		memcpy(new_ptr, ptr, oldsize);
	
	nt_free(ptr);
	return new_ptr;
}

// TODO: Implement.
char* nt_getenv(const char* name)
{
	return NULL;
}

NT_FILE* nt_fopena(const char* filename, const char* mode)
{
	ANSI_STRING astr;
	UNICODE_STRING ustr;
	NT_FILE* file = NULL;
	RtlInitAnsiString(&astr, filename);
	RtlAnsiStringToUnicodeString(&ustr, &astr, TRUE);
	file = nt_fopen(ustr.Buffer, mode);
	RtlFreeUnicodeString(&ustr);
	return file;
}

static HANDLE pDevice;
static HANDLE pKeyboardDevice;
static HANDLE hKeyboardEvent;
static HANDLE pMouseDevice = NULL;
static HANDLE hMouseEvent = NULL;

static BOOLEAN IsWindowsXP()
{
	RTL_OSVERSIONINFOW verInfo;
	RtlZeroMemory(&verInfo, sizeof(OSVERSIONINFOW));
	verInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
	RtlGetVersion(&verInfo);
	return verInfo.dwMajorVersion == 5 && verInfo.dwMinorVersion == 1;
}

static BOOLEAN IsWindows8OrLater()
{
	RTL_OSVERSIONINFOW verInfo;
	RtlZeroMemory(&verInfo, sizeof(OSVERSIONINFOW));
	verInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
	RtlGetVersion(&verInfo);
	return verInfo.dwMajorVersion >= 6 && verInfo.dwMinorVersion >= 2;
}

static byte DGNT_ScrBuf[DOOMGENERIC_RESX * DOOMGENERIC_RESY];
extern ULONG_PTR VgaBase;

extern BOOLEAN NTAPI VidInitialize(BOOLEAN SetMode);
extern int init_graph_vga(int width, int height,int chain4);
extern VOID TextMode(VOID);

#define LO						 85
#define MD						170
#define HI						255

typedef struct tagRGBQUAD
{
    UCHAR rgbBlue;
    UCHAR rgbGreen;
    UCHAR rgbRed;
    UCHAR rgbReserved;
} RGBQUAD, *LPRGBQUAD;

MOUSE_INPUT_DATA mouseData[256];
volatile unsigned int mouseDataWrite = 0, mouseDataRead = 0;
volatile int Quit = 0;
static HANDLE MouseThreadHandle;

NTSTATUS
NTAPI
ZwCancelIoFile(
	HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock
);

void MouseThread(void* parm)
{
	NTSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	UNICODE_STRING ustr;
	OBJECT_ATTRIBUTES ObjectAttributes;
	LARGE_INTEGER ByteOffset;

	ByteOffset.QuadPart = 0;
	RtlInitUnicodeString(&ustr, L"\\Device\\PointerClass0");
	InitializeObjectAttributes(&ObjectAttributes,
							   &ustr,
							   OBJ_CASE_INSENSITIVE,
							   NULL,
							   NULL);

	Status = ZwCreateFile(&pMouseDevice, SYNCHRONIZE | GENERIC_READ | FILE_READ_ATTRIBUTES, &ObjectAttributes, &Iosb, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_DIRECTORY_FILE, NULL, 0);
	if (!NT_SUCCESS(Status))
	{
		nt_printf("Failed to open \\Device\\PointerClass0: 0x%X.\n", Status);
	}
	while (!Quit)
	{
		while ((mouseDataWrite - mouseDataRead) >= 16)
		{
			DG_SleepMs(1);
		}

		if (Quit)
			break;
		Status = ZwReadFile(pMouseDevice, hMouseEvent, NULL, NULL, &Iosb, &mouseData[mouseDataWrite & 0xFF], sizeof(MOUSE_INPUT_DATA), &ByteOffset, 0);
		if (Status == STATUS_PENDING)
		{
			ZwCancelIoFile(pMouseDevice, &Iosb);
		}
		else if (NT_SUCCESS(Status))
		{
			mouseDataWrite++;
		}
	}
}

extern VOID NTAPI SetPaletteEntryRGB(ULONG Id, RGBQUAD Rgb);
void DG_Init()
{
	IO_STATUS_BLOCK iosb;
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjectAttributes;
	UNICODE_STRING ustr;
	IO_STATUS_BLOCK Iosb;
	MOUSE_ATTRIBUTES mouseAttr;
	//ULONG solidcolfillparams[5] = { 0, 0, 639, 479, 0 };
	unsigned int i = 0;

	if (IsWindows8OrLater())
	{
		nt_printf("Windows 8 and later is unsupported.\n");
		while (1)
		{
			DG_SleepMs(1);
		}
	}
	//nt_printf("%s\n", __func__);
	//InbvResetDisplay();
	//InbvSetTextColor(BV_COLOR_WHITE);
	//InbvInstallDisplayStringFilter(NULL);
	//InbvEnableDisplayString(TRUE);
	//InbvSetScrollRegion(0, 0, 679, 449);
	
	//nt_printf("%s 4\n", __func__);
	RtlInitUnicodeString(&ustr, L"\\Device\\KeyboardClass0");
	InitializeObjectAttributes(&ObjectAttributes,
							   &ustr,
							   OBJ_CASE_INSENSITIVE,
							   NULL,
							   NULL);
	//nt_printf("%s 5\n", __func__);
	Status = NtCreateFile(&pKeyboardDevice, SYNCHRONIZE | GENERIC_READ | FILE_READ_ATTRIBUTES, &ObjectAttributes, &Iosb, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_DIRECTORY_FILE, NULL, 0);
	if (!NT_SUCCESS(Status))
	{
		nt_printf("Failed to open \\Device\\KeyboardClass0: 0x%X.\n", Status);
		while (1)
		{
			DG_SleepMs(1);
		}
	}

	RtlInitUnicodeString(&ustr, L"\\Device\\PointerClass0");
	InitializeObjectAttributes(&ObjectAttributes,
							   &ustr,
							   OBJ_CASE_INSENSITIVE,
							   NULL,
							   NULL);
	//nt_printf("%s 5\n", __func__);
	Status = ZwCreateFile(&pMouseDevice, SYNCHRONIZE | GENERIC_READ | FILE_READ_ATTRIBUTES, &ObjectAttributes, &Iosb, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_DIRECTORY_FILE, NULL, 0);
	if (!NT_SUCCESS(Status))
	{
		nt_printf("Failed to open \\Device\\PointerClass0: 0x%X.\n", Status);
	}
	else
	{
		InitializeObjectAttributes(&ObjectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
		Status = PsCreateSystemThread(&MouseThreadHandle, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &ObjectAttributes, NULL, NULL, MouseThread, NULL);

		if (!NT_SUCCESS(Status)) {
			nt_printf("Failed to create mouse thread: 0x%X.\n", Status);
		}
	}

	//nt_printf("%s 5\n", __func__);

#if 0
	Status = NtCreateFile(&pVideoDevice, SYNCHRONIZE | GENERIC_READ | FILE_READ_ATTRIBUTES, &ObjectAttributes, &Iosb, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_DIRECTORY_FILE, NULL, 0);
	if (!NT_SUCCESS(Status))
	{
		nt_printf("Failed to open \\Device\\Video0: 0x%X.\n", Status);
		while (1)
		{
			DG_SleepMs(1);
		}
	}

	//ZwDeviceIoControlFile(pVideoDevice, NULL, NULL, NULL, &Iosb, IOCTL_VIDEO_RESET_DEVICE, NULL, 0, NULL, 0);
nt_ioctl((NT_FILE*)&pVideoDevice, IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES, "doomgeneric_nt", NULL, 0, (void*)&numModes, sizeof(VIDEO_NUM_MODES), NULL);
	nt_printf("%d, %lu\n", numModes.NumModes, numModes.ModeInformationLength);

	modes = nt_calloc(numModes.NumModes, numModes.ModeInformationLength);

	nt_ioctl((NT_FILE*)&pVideoDevice, IOCTL_VIDEO_QUERY_AVAIL_MODES, "doomgeneric_nt", NULL, 0, (void*)modes, numModes.ModeInformationLength * numModes.NumModes, NULL);

	for (i = 0; i < numModes.NumModes; i++)
	{
		nt_printf("%dx%dx%d\n", modes[i].VisScreenWidth, modes[i].VisScreenHeight, modes[i].BitsPerPlane * modes[i].NumberOfPlanes);
	}
	
	for (i = 0; i < numModes.NumModes; i++)
	{
		if (modes[i].VisScreenWidth == 320 && modes[i].VisScreenHeight == 200)
		{
			VIDEO_MODE mode;
			mode.RequestedMode = i;
			nt_ioctl((NT_FILE*)&pVideoDevice, IOCTL_VIDEO_SET_CURRENT_MODE, "doomgeneric_nt", (void*)&mode, sizeof(VIDEO_MODE), NULL, 0, NULL);
			nt_printf("%dx%dx%d set\n", modes[i].VisScreenWidth, modes[i].VisScreenHeight, modes[i].BitsPerPlane * modes[i].NumberOfPlanes);
		}
		//nt_printf("%dx%dx%d\n", modes[i].VisScreenWidth, modes[i].VisScreenHeight, modes[i].BitsPerPlane * modes[i].NumberOfPlanes);
	}

	memReq.RequestedVirtualAddress = NULL;
	RtlZeroMemory(&memInfo, sizeof(VIDEO_MEMORY_INFORMATION));
	nt_ioctl((NT_FILE*)&pVideoDevice, IOCTL_VIDEO_MAP_VIDEO_MEMORY, "doomgeneric_nt", (void*)&memReq, sizeof(VIDEO_MEMORY), (void*)&memInfo, sizeof(VIDEO_MEMORY_INFORMATION), NULL);
	if (memInfo.VideoRamBase)
	{
		uint16_t* buffer = (uint16_t*)memInfo.FrameBufferBase;
		buffer[0] = 'T' | (0x0f00);
		buffer[1] = 'e' | (0x0f00);
		buffer[2] = 's' | (0x0f00);
		buffer[3] = 't' | (0x0f00);
		while (1)
		{
			DG_SleepMs(1);
		}
	}
#endif

	//nt_printf("%s 6\n", __func__);
	InitializeObjectAttributes(&ObjectAttributes, NULL, 0, NULL, NULL);
	ZwCreateEvent(&hKeyboardEvent, EVENT_ALL_ACCESS, &ObjectAttributes, 1, 0);
	ZwCreateEvent(&hMouseEvent, EVENT_ALL_ACCESS, &ObjectAttributes, 1, 0);
	
	//nt_printf("%s 7\n", __func__);
}
struct tickCountStruct
{
	union
	{
		void* funcptr;
		uint32_t(*funcGetTickCount)();
	};
};

uint32_t DG_GetTicksMs()
{
	LARGE_INTEGER systime = { 0 };

	KeQuerySystemTime(&systime);
	return ((int)systime.QuadPart / 10000);
}

void DG_SleepMs(uint32_t ms)
{
	LARGE_INTEGER sleepAmount;
	sleepAmount.QuadPart = -((LONGLONG)(ms) * 10000);
	KeDelayExecutionThread(KernelMode, FALSE, &sleepAmount);
}

static const RGBQUAD TextModePalette[16] =
{
	{  0,  0,  0, 0 },		// 0 black
	{ MD,  0,  0, 0 },		// 1 blue
	{  0, MD,  0, 0 },		// 2 green
	{ MD, MD,  0, 0 },		// 3 cyan
	{  0,  0, MD, 0 },		// 4 red
	{ MD,  0, MD, 0 },		// 5 magenta
	{  0, LO, MD, 0 },		// 6 brown
	{ MD, MD, MD, 0 },		// 7 light gray

	{ LO, LO, LO, 0 },		// 8 dark gray
	{ HI, LO, LO, 0 },		// 9 light blue
	{ LO, HI, LO, 0 },		// A light green
	{ HI, HI, LO, 0 },		// B light cyan
	{ LO, LO, HI, 0 },		// C light red
	{ HI, LO, HI, 0 },		// D light magenta
	{ LO, HI, HI, 0 },		// E yellow
	{ HI, HI, HI, 0 }		// F white
};
static int GetPaletteIndex(int r, int g, int b)
{
	int best, best_diff, diff;
	int i;
	RGBQUAD color;

	printf("I_GetPaletteIndex\n");

	best = 0;
	best_diff = INT_MAX;

	for (i = 0; i < 16; ++i)
	{
		color.rgbRed = TextModePalette[i].rgbRed;
		color.rgbGreen = TextModePalette[i].rgbGreen;
		color.rgbBlue = TextModePalette[i].rgbBlue;

		diff = (r - color.rgbBlue) * (r - color.rgbBlue)
			+ (g - color.rgbGreen) * (g - color.rgbGreen)
			+ (b - color.rgbRed) * (b - color.rgbRed);

		if (diff < best_diff)
		{
			best = i;
			best_diff = diff;
		}

		if (diff == 0)
		{
			break;
		}
	}

	return best;
}
extern RGBQUAD* I_GetPalPtr(void);
static void RGBtoHSV(float r, float g, float b, float* h, float* s, float* v)
{
	float min, max, delta, foo;

	if (r == g && g == b)
	{
		*h = 0;
		*s = 0;
		*v = r;
		return;
	}

	foo = r < g ? r : g;
	min = (foo < b) ? foo : b;
	foo = r > g ? r : g;
	max = (foo > b) ? foo : b;

	*v = max;									// v

	delta = max - min;

	*s = delta / max;							// s

	if (r == max)
		*h = (g - b) / delta;					// between yellow & magenta
	else if (g == max)
		*h = 2 + (b - r) / delta;				// between cyan & yellow
	else
		*h = 4 + (r - g) / delta;				// between magenta & cyan

	*h *= 60;									// degrees
	if (*h < 0)
		*h += 360;
}
float lerp(float v0, float v1, float t) {
	return (1 - t) * v0 + t * v1;
}
void HSVtoRGB(float* r, float* g, float* b, float h, float s, float v)
{
	int i;
	float f, p, q, t;

	if (s == 0)
	{ // achromatic (grey)
		*r = *g = *b = v;
		return;
	}

	h /= 60;									// sector 0 to 5
	i = (int)floor(h);
	f = h - i;									// factorial part of h
	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));

	switch (i)
	{
	case 0:		*r = v; *g = t; *b = p; break;
	case 1:		*r = q; *g = v; *b = p; break;
	case 2:		*r = p; *g = v; *b = t; break;
	case 3:		*r = p; *g = q; *b = v; break;
	case 4:		*r = t; *g = p; *b = v; break;
	default:	*r = v; *g = p; *b = q; break;
	}
}
static int keysRead = 0;
struct ColFillParams
{
	ULONG Left;
	ULONG Top;
	ULONG Right;
	ULONG Bottom;
	ULONG color;
};
#define UNDETAILLEVELX 2
#define UNDETAILLEVELY 2
extern void I_ReadScreen (byte* scr);
void DG_DrawFrame()
{
#if 0
	UINT32 x = 0, y = 0;
	uint8_t* dgPixBuf = (void*)DG_ScreenBuffer;
	for (y = 0; y < DOOMGENERIC_RESY; y += UNDETAILLEVELX)
	{
		for (x = 0; x < DOOMGENERIC_RESX; x += UNDETAILLEVELY)
		{

			float h = 0, s = 0, v = 0;
			float r = dgPixBuf[y * DOOMGENERIC_RESX * 4 + x * 4 + 2] / 255.f;
			float g = dgPixBuf[y * DOOMGENERIC_RESX * 4 + x * 4 + 1] / 255.f;
			float b = dgPixBuf[y * DOOMGENERIC_RESX * 4 + x * 4] / 255.f;
			struct ColFillParams fillcol;
			uint8_t attrib = 0;
			
			RGBtoHSV(r, g, b, &h, &s, &v);
			if (s != 0)
			{ // color
				HSVtoRGB(&r, &g, &b, h, 1, 1);
				if (r == 1)  attrib = BV_COLOR_RED;
				if (g == 1)  attrib |= BV_COLOR_GREEN;
				if (b == 1)  attrib |= BV_COLOR_BLUE;
				if (v > 0.6) attrib |= attrib == 0 ? BV_COLOR_DARK_GRAY : BV_COLOR_LIGHT_GRAY;
			}
			else
			{ // gray
				if (v < 0.33) attrib = BV_COLOR_DARK_GRAY;
				else if (v < 0.90) attrib = BV_COLOR_LIGHT_GRAY;
				else			   attrib = BV_COLOR_WHITE;
			}
			if (DGNT_ScrBuf[y * DOOMGENERIC_RESX + x] == (attrib & 0xF)) continue;
			fillcol.Left = fillcol.Right = x;
			fillcol.Top = fillcol.Bottom = y;
			fillcol.Right += UNDETAILLEVELX - 1;
			fillcol.Bottom += UNDETAILLEVELY - 1;
			fillcol.color = attrib & 0xF;
			DGNT_ScrBuf[y * DOOMGENERIC_RESX + x] = attrib;
			//nt_ioctl((NT_FILE*)&pDevice, IOCTL_INBVSHIM_SOLID_COLOR_FILL, "doomgeneric_nt", &fillcol, sizeof(struct ColFillParams), NULL, 0, NULL);
			//InbvSolidColorFill(fillcol.Left, fillcol.Top, fillcol.Right, fillcol.Bottom, fillcol.color);


		}
	}
#endif
	int i = 0;
	RGBQUAD* palette;

	if (!InhibitGraphicsPrint) {
		InbvEnableBootDriver(FALSE);
		VidInitialize(TRUE);
		init_graph_vga(320, 200, 1);
		memset((void*)VgaBase, 0x0, 0x10000);
		InhibitGraphicsPrint = 1;
	}

	palette = I_GetPalPtr();
	for (i = 0; i < 256; i++)
	{
		SetPaletteEntryRGB(i, palette[i]);
	}
	I_ReadScreen((byte*)VgaBase);
	keysRead = 0;
}

static int IncAndReturn()
{
	keysRead++;
	return keysRead >= 16 ? 0 : 1;
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
	IO_STATUS_BLOCK Iosb;
	LARGE_INTEGER ByteOffset;
	NTSTATUS Status;
	KEYBOARD_INPUT_DATA inputData;
	MOUSE_INPUT_DATA mouseInputData;

	RtlZeroMemory(&Iosb, sizeof(Iosb));
	RtlZeroMemory(&ByteOffset, sizeof(ByteOffset));
	RtlZeroMemory(&inputData, sizeof(KEYBOARD_INPUT_DATA));
	RtlZeroMemory(&mouseInputData, sizeof(MOUSE_INPUT_DATA));

#if 0
	Status = NtReadFile(pMouseDevice, hMouseEvent, NULL, NULL, &Iosb, &mouseInputData, sizeof(MOUSE_INPUT_DATA), &ByteOffset, NULL);
	if (Status == STATUS_PENDING)
	{
		// No input to read at the moment, cancel read.
		ZwCancelIoFile(pMouseDevice, &Iosb);
	}
	else if (NT_SUCCESS(Status))
	{
		event_t event;
        event.type = ev_mouse;
        event.data1 = 0;
        event.data2 = mouseInputData.LastX;
        event.data3 = mouseInputData.LastY;
        D_PostEvent(&event);
	}
#endif

	while (mouseDataRead != mouseDataWrite)
	{
		static event_t mouseEvent = { ev_mouse, 0, 0, 0, 0 };
		MOUSE_INPUT_DATA data = mouseData[mouseDataRead & 0xFF];
		mouseDataRead++;

		if (!(data.Flags & MOUSE_MOVE_ABSOLUTE)) {
			mouseEvent.data2 = data.LastX * 2;
			mouseEvent.data3 = -data.LastY * 2;
		}
		
		if (data.ButtonFlags & MOUSE_BUTTON_1_DOWN)
			mouseEvent.data1 |= 1;
		if (data.ButtonFlags & MOUSE_BUTTON_1_UP)
			mouseEvent.data1 &= ~1;

		D_PostEvent(&mouseEvent);
	}

	Status = NtReadFile(pKeyboardDevice, hKeyboardEvent, NULL, NULL, &Iosb, &inputData, sizeof(KEYBOARD_INPUT_DATA), &ByteOffset, NULL);
	if (Status == STATUS_PENDING)
	{
		// No input to read at the moment, cancel read and return.
		ZwCancelIoFile(pKeyboardDevice, &Iosb);
		return 0;
	}
	else if (!NT_SUCCESS(Status))
	{
		return 0;
	}
	if (inputData.MakeCode > 0x7F) return 0;

	*pressed = !(inputData.Flags & KEY_BREAK);
	if (inputData.Flags & KEY_E1)
	{
		*doomKey = KEY_PAUSE;
		return IncAndReturn();
	}
	switch (inputData.MakeCode)
	{
	case 0x48: // Up arrow
		*doomKey = KEY_UPARROW;
		break;
	case 0x50: // Down arrow
		*doomKey = KEY_DOWNARROW;
		break;
	case 0x4b: // Left arrow
		*doomKey = KEY_LEFTARROW;
		break;
	case 0x4d: // Right arrow
		*doomKey = KEY_RIGHTARROW;
		break;
	default:
		*doomKey = scantokey[inputData.MakeCode & 0x7F];
		break;
	}
	return IncAndReturn();
}

void DG_SetWindowTitle(const char* title)
{

}

#ifdef _M_X64
#define HAL_HalFindBusAddressTranslation_OFFSET 0x50
#else
#define HAL_HalFindBusAddressTranslation_OFFSET 0x28
#endif

#pragma pack(push, 1)
typedef struct _HAL_PRIVATE_DISPATCH_TABLE
{
	union
	{
		ULONG Version;
        struct {
            char pad[HAL_HalFindBusAddressTranslation_OFFSET];
            BOOLEAN (NTAPI *HalFindBusAddressTranslation)(PHYSICAL_ADDRESS, ULONG*, PHYSICAL_ADDRESS*, ULONG_PTR*, BOOLEAN);
            BOOLEAN (NTAPI *HalResetDisplay)(VOID);
        };
    };
} HAL_PRIVATE_DISPATCH_TABLE;
#pragma pack(pop)

extern NTSYSAPI HAL_PRIVATE_DISPATCH_TABLE HalPrivateDispatchTable;

void nt_exit(int code)
{
	//NtTerminateProcess(NtCurrentProcess(), code);
	HalPrivateDispatchTable.HalResetDisplay();
	InbvEnableBootDriver(TRUE);
	InbvResetDisplay();
	InbvSolidColorFill(0, 0, 639, 479, 0);
	InhibitGraphicsPrint = 0;
	Quit = 1;
	ZwSetEvent(hMouseEvent, NULL);
	nt_printf("Close not implemented!\n");

	while (1) {
		DG_SleepMs(1);
	}
}

int nt_puts(const char* string)
{
	return nt_printf("%s", string);
}

int nt_putchar(int chr)
{
	return nt_printf("%c", (unsigned char)chr);
}

int nt_mkdir(const char* directory)
{
	UNICODE_STRING us;
	NTSTATUS status;
	OBJECT_ATTRIBUTES oa;
	IO_STATUS_BLOCK iosb;
	HANDLE hDirectory;
	ANSI_STRING orig_str;

	RtlInitAnsiString(&orig_str, directory);
	RtlAnsiStringToUnicodeString(&us, &orig_str, TRUE);
	InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, hCurrentDirectory, NULL);
	status = NtCreateFile(&hDirectory,
						  FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_OPEN_FOR_BACKUP_INTENT,
						  &oa,
						  &iosb,
						  NULL,
						  FILE_ATTRIBUTE_NORMAL,
						  FILE_SHARE_READ | FILE_SHARE_WRITE,
						  FILE_CREATE,
						  FILE_SYNCHRONOUS_IO_NONALERT | FILE_DIRECTORY_FILE,
						  NULL,
						  0
	);
	RtlFreeUnicodeString(&us);
	return NT_SUCCESS(status);
}

int nt_remove(const char* path)
{
	UNICODE_STRING us;
	NTSTATUS status;
	OBJECT_ATTRIBUTES oa;
	ANSI_STRING orig_str;

	RtlInitAnsiString(&orig_str, path);
	RtlAnsiStringToUnicodeString(&us, &orig_str, TRUE);
	InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, hCurrentDirectory, NULL);
	status = ZwDeleteFile(&oa);
	RtlFreeUnicodeString(&us);
	return NT_SUCCESS(status) ? 0 : -1;
}

int nt_rename(const char* oldfilename, const char* newfilename)
{
	PFILE_RENAME_INFORMATION FileRenameInfo;
	UNICODE_STRING us;
	ANSI_STRING orig_str;
	NTSTATUS status;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES oa;
	IO_STATUS_BLOCK iosb;
	WCHAR* ptrToFileName;
	UINT32 i;

	RtlInitAnsiString(&orig_str, oldfilename);
	RtlAnsiStringToUnicodeString(&us, &orig_str, TRUE);
	InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, hCurrentDirectory, NULL);
	status = NtCreateFile(&FileHandle,
						  FILE_ALL_ACCESS,
						  &oa,
						  &iosb,
						  NULL,
						  FILE_ATTRIBUTE_NORMAL,
						  FILE_SHARE_READ | FILE_SHARE_WRITE,
						  FILE_OPEN,
						  FILE_SYNCHRONOUS_IO_NONALERT,
						  NULL,
						  0);
	RtlFreeUnicodeString(&us);
	if (!NT_SUCCESS(status))
	{
		return -1;
	}
	FileRenameInfo = nt_calloc(sizeof(FILE_RENAME_INFORMATION) + strlen(newfilename) * sizeof(WCHAR), 1);
	if (!FileRenameInfo)
	{
		return -1;
	}
	ptrToFileName = FileRenameInfo->FileName;
	for (i = 0; i < strlen(newfilename); i++)
	{
		ptrToFileName[i] = newfilename[i];
	}
	FileRenameInfo->FileNameLength = strlen(newfilename) * sizeof(WCHAR);
	FileRenameInfo->RootDirectory = hCurrentDirectory;
	FileRenameInfo->ReplaceIfExists = 1;

	status = NtSetInformationFile(
		FileHandle,
		&iosb,
		FileRenameInfo,
		sizeof(FILE_RENAME_INFORMATION) + FileRenameInfo->FileNameLength,
		FileRenameInformation);

	nt_free(FileRenameInfo);

	return NT_SUCCESS(status) ? 0 : -1;
}