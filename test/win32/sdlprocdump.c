#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <dbghelp.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DUMP_FOLDER "minidumps"
#define APPNAME "SDLPROCDUMP"

#define PRODCUMP_MIN(A,B) (((A) < (B)) ? (A) : (B))

#if defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) ||defined( __i386) || defined(_M_IX86)
#define SDLPROCDUMP_CPU_X86 1
#elif defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
#define SDLPROCDUMP_CPU_X64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define SDLPROCDUMP_CPU_ARM64 1
#elif defined(__arm__) || defined(_M_ARM)
#define SDLPROCDUMP_CPU_ARM32 1
#endif

#if defined(SDLPROCDUMP_CPU_X86) || defined(SDLPROCDUMP_CPU_X64) || defined(SDLPROCDUMP_CPU_ARM32) || defined(SDLPROCDUMP_CPU_ARM64)
#define SDLPROCDUMP_PRINTSTACK
#else
#pragma message("Unsupported architecture: don't know how to StackWalk")
#endif

#ifndef EXCEPTION_SOFTWARE_ORIGINATE
#define EXCEPTION_SOFTWARE_ORIGINATE 0x80
#endif

static void printf_message(const char *format, ...) {
    va_list ap;
    fprintf(stderr, "[" APPNAME "] ");
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

static void printf_windows_message(const char *format, ...) {
    va_list ap;
    char win_msg[512];
    size_t win_msg_len;

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        win_msg, sizeof(win_msg)/sizeof(*win_msg),
        NULL);
    win_msg_len = strlen(win_msg);
    while (win_msg[win_msg_len-1] == '\r' || win_msg[win_msg_len-1] == '\n' || win_msg[win_msg_len-1] == ' ') {
        win_msg[win_msg_len-1] = '\0';
        win_msg_len--;
    }
    fprintf(stderr, "[" APPNAME "] ");
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, " (%s)\n", win_msg);
}

struct {
    HMODULE module;
    BOOL (WINAPI *pSymInitialize)(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess);
    BOOL (WINAPI *pSymCleanup)(HANDLE hProcess);
    BOOL (WINAPI *pMiniDumpWriteDump)(
        HANDLE hProcess,
        DWORD ProcessId,
        HANDLE hFile,
        MINIDUMP_TYPE DumpType,
        PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
        PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
        PMINIDUMP_CALLBACK_INFORMATION CallbackParam);
    BOOL (WINAPI *pSymFromAddr)(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol);
    BOOL (WINAPI *pSymGetLineFromAddr64)(HANDLE hProcess, DWORD64 qwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line);
    BOOL (WINAPI *pStackWalk64)(DWORD MachineType, HANDLE hProcess, HANDLE hThread, LPSTACKFRAME64 StackFrame,
        PVOID ContextRecord, PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
        PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
        PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine, PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);
    PVOID (WINAPI *pSymFunctionTableAccess64)(HANDLE hProcess, DWORD64 AddrBase);
    DWORD64 (WINAPI *pSymGetModuleBase64)(HANDLE hProcess, DWORD64 qwAddr);
    BOOL (WINAPI *pSymGetModuleInfo64)(HANDLE hProcess, DWORD64 qwAddr, PIMAGEHLP_MODULE64 ModuleInfo);
    BOOL (WINAPI *pSymRefreshModuleList)(HANDLE hProcess);
} dyn_dbghelp;

static void load_dbghelp(void) {
    if (dyn_dbghelp.module) {
        return;
    }
    dyn_dbghelp.module = LoadLibraryA("dbghelp.dll");
    if (!dyn_dbghelp.module) {
        printf_message("Failed to load dbghelp.dll");
        goto failed;
    }
    dyn_dbghelp.pSymInitialize = (void *)GetProcAddress(dyn_dbghelp.module, "SymInitialize");
    dyn_dbghelp.pSymCleanup = (void *)GetProcAddress(dyn_dbghelp.module, "SymCleanup");
    dyn_dbghelp.pMiniDumpWriteDump = (void *)GetProcAddress(dyn_dbghelp.module, "MiniDumpWriteDump");
    dyn_dbghelp.pSymFromAddr = (void *)GetProcAddress(dyn_dbghelp.module, "SymFromAddr");
    dyn_dbghelp.pStackWalk64 = (void *)GetProcAddress(dyn_dbghelp.module, "StackWalk64");
    dyn_dbghelp.pSymGetLineFromAddr64 = (void *)GetProcAddress(dyn_dbghelp.module, "SymGetLineFromAddr64");
    dyn_dbghelp.pSymFunctionTableAccess64 = (void *)GetProcAddress(dyn_dbghelp.module, "SymFunctionTableAccess64");
    dyn_dbghelp.pSymGetModuleBase64 = (void *)GetProcAddress(dyn_dbghelp.module, "SymGetModuleBase64");
    dyn_dbghelp.pSymGetModuleInfo64 = (void *)GetProcAddress(dyn_dbghelp.module, "SymGetModuleInfo64");
    dyn_dbghelp.pSymRefreshModuleList = (void *)GetProcAddress(dyn_dbghelp.module, "SymRefreshModuleList");
    return;
failed:
    if (dyn_dbghelp.module) {
        FreeLibrary(dyn_dbghelp.module);
        dyn_dbghelp.module = NULL;
    }
}

static void unload_dbghelp(void) {
    if (!dyn_dbghelp.module) {
        return;
    }
    FreeLibrary(dyn_dbghelp.module);
    memset(&dyn_dbghelp, 0, sizeof(dyn_dbghelp));
}

#define FOREACH_EXCEPTION_CODES(X) \
    X(EXCEPTION_ACCESS_VIOLATION) \
    X(EXCEPTION_DATATYPE_MISALIGNMENT) \
    X(EXCEPTION_BREAKPOINT) \
    X(EXCEPTION_SINGLE_STEP) \
    X(EXCEPTION_ARRAY_BOUNDS_EXCEEDED) \
    X(EXCEPTION_FLT_DENORMAL_OPERAND) \
    X(EXCEPTION_FLT_DIVIDE_BY_ZERO) \
    X(EXCEPTION_FLT_INEXACT_RESULT) \
    X(EXCEPTION_FLT_INVALID_OPERATION) \
    X(EXCEPTION_FLT_OVERFLOW) \
    X(EXCEPTION_FLT_STACK_CHECK) \
    X(EXCEPTION_FLT_UNDERFLOW) \
    X(EXCEPTION_INT_DIVIDE_BY_ZERO) \
    X(EXCEPTION_INT_OVERFLOW) \
    X(EXCEPTION_PRIV_INSTRUCTION) \
    X(EXCEPTION_IN_PAGE_ERROR) \
    X(EXCEPTION_ILLEGAL_INSTRUCTION) \
    X(EXCEPTION_NONCONTINUABLE_EXCEPTION) \
    X(EXCEPTION_STACK_OVERFLOW) \
    X(EXCEPTION_INVALID_DISPOSITION) \
    X(EXCEPTION_GUARD_PAGE) \
    X(EXCEPTION_INVALID_HANDLE) \
    X(STATUS_HEAP_CORRUPTION)

#define FOREACH_EXCEPTION_FLAGS(X) \
    X(EXCEPTION_NONCONTINUABLE) \
    X(EXCEPTION_UNWINDING) \
    X(EXCEPTION_EXIT_UNWIND) \
    X(EXCEPTION_STACK_INVALID) \
    X(EXCEPTION_NESTED_CALL) \
    X(EXCEPTION_TARGET_UNWIND) \
    X(EXCEPTION_COLLIDED_UNWIND) \
    X(EXCEPTION_SOFTWARE_ORIGINATE)

static const char *exceptionCode_to_string(DWORD dwCode) {
#define SWITCH_CODE_STR(V) case V: return #V;
    switch (dwCode) {
    case 0xe06d7363: return "MS Visual C++ Exception";
        FOREACH_EXCEPTION_CODES(SWITCH_CODE_STR)
    default: {
        return "unknown";
    }
    }
#undef SWITCH_CODE_STR
}

static const char *exceptionFlags_to_string(DWORD dwFlags, char *buffer, size_t buffer_length) {
    buffer[0] = '\0';

#define APPEND_OR_STR(CODE)                       \
    if (dwFlags & (CODE)) {                       \
        if (buffer[0]) {                          \
            strcat_s(buffer, buffer_length, "|"); \
        }                                         \
        strcat_s(buffer, buffer_length, #CODE);   \
    }

    FOREACH_EXCEPTION_FLAGS(APPEND_OR_STR)
#undef APPEND_OR_STR
    return buffer;
}

static BOOL IsCXXException(DWORD dwCode) {
    /* https://devblogs.microsoft.com/oldnewthing/20100730-00/?p=13273 */
    return dwCode == 0xe06d7363; /* FOURCC(0xe0, 'm', 's', 'c') */
}

static BOOL IsFatalExceptionCode(DWORD dwCode) {
    switch (dwCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_STACK_OVERFLOW:
    case STATUS_HEAP_CORRUPTION:
    case STATUS_STACK_BUFFER_OVERRUN:
    case EXCEPTION_GUARD_PAGE:
    case EXCEPTION_INVALID_HANDLE:
        return TRUE;
    default:
        return FALSE;
    }
}

static const char *get_simple_basename(const char *path) {
    const char *pos = strrchr(path, '\\');
    if (pos) {
        return pos + 1;
    }
    pos = strrchr(path, '/');
    if (pos) {
        return pos + 1;
    }
    return path;
}

static void write_minidump(const char *child_file_path, const LPPROCESS_INFORMATION process_information, DWORD dwThreadId, PEXCEPTION_RECORD exception_record, PCONTEXT context) {
    BOOL success;
    char dump_file_path[MAX_PATH];
    char child_file_name[64];
    EXCEPTION_POINTERS exception_pointers;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    MINIDUMP_EXCEPTION_INFORMATION minidump_exception_information;
    SYSTEMTIME system_time;

    if (!dyn_dbghelp.pMiniDumpWriteDump) {
        printf_message("Cannot find pMiniDumpWriteDump in dbghelp.dll: no minidump");
        return;
    }

    success = CreateDirectoryA(DUMP_FOLDER, NULL);
    if (!success && GetLastError() != ERROR_ALREADY_EXISTS) {
        printf_windows_message("Failed to create minidump directory");
        goto post_dump;
    }
    _splitpath_s(child_file_path, NULL, 0, NULL, 0, child_file_name, sizeof(child_file_name), NULL, 0);
    GetLocalTime(&system_time);

    snprintf(dump_file_path, sizeof(dump_file_path), "minidumps/%s_%04d-%02d-%02d_%02d-%02d-%02d.dmp",
             child_file_name,
             system_time.wYear, system_time.wMonth, system_time.wDay,
             system_time.wHour, system_time.wMinute, system_time.wSecond);
    printf_message("");
    printf_message("Writing minidump to \"%s\"", dump_file_path);
    hFile = CreateFileA(
        dump_file_path,
        GENERIC_WRITE,
        FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf_windows_message("Failed to open file for minidump");
        goto post_dump;
    }
    memset(&exception_pointers, 0, sizeof(exception_pointers));
    exception_pointers.ContextRecord = context;
    exception_pointers.ExceptionRecord = exception_record;
    minidump_exception_information.ClientPointers = FALSE;
    minidump_exception_information.ExceptionPointers = &exception_pointers;
    minidump_exception_information.ThreadId = dwThreadId;
    success = dyn_dbghelp.pMiniDumpWriteDump(
        process_information->hProcess,      /* HANDLE                            hProcess */
        process_information->dwProcessId,   /* DWORD                             ProcessId */
        hFile,                              /* HANDLE                            hFile */
        MiniDumpWithFullMemory,             /* MINIDUMP_TYPE                     DumpType */
        &minidump_exception_information,    /* PMINIDUMP_EXCEPTION_INFORMATION   ExceptionParam */
        NULL,                               /* PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam */
        NULL);                              /* PMINIDUMP_CALLBACK_INFORMATION    CallbackParam */
    if (!success) {
        printf_windows_message("Failed to write minidump");
    }
post_dump:
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
}

static void print_stacktrace(const LPPROCESS_INFORMATION process_information, LPVOID address, PCONTEXT context) {
    STACKFRAME64 stack_frame;
    DWORD machine_type;

    if (!context) {
        printf_message("Cannot create a stacktrace without a context");
        return;
    }
    if (!dyn_dbghelp.pStackWalk64) {
        printf_message("Cannot find StackWalk64 in dbghelp.dll: no stacktrace");
        return;
    }
    if (!dyn_dbghelp.pSymFunctionTableAccess64) {
        printf_message("Cannot find SymFunctionTableAccess64 in dbghelp.dll: no stacktrace");
        return;
    }
    if (!dyn_dbghelp.pSymGetModuleBase64) {
        printf_message("Cannot find SymGetModuleBase64 in dbghelp.dll: no stacktrace");
        return;
    }
    if (!dyn_dbghelp.pSymFromAddr) {
        printf_message("Cannot find pSymFromAddr in dbghelp.dll: no stacktrace");
        return;
    }
    if (!dyn_dbghelp.pSymGetLineFromAddr64) {
        printf_message("Cannot find SymGetLineFromAddr64 in dbghelp.dll: no stacktrace");
        return;
    }
    if (!dyn_dbghelp.pSymGetModuleInfo64) {
        printf_message("Cannot find SymGetModuleInfo64 in dbghelp.dll: no stacktrace");
        return;
    }

    if (!dyn_dbghelp.pSymRefreshModuleList || !dyn_dbghelp.pSymRefreshModuleList(process_information->hProcess)) {
        printf_windows_message("SymRefreshModuleList failed: maybe no stacktrace");
    }

    memset(&stack_frame, 0, sizeof(stack_frame));

    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Mode = AddrModeFlat;

#if defined(SDLPROCDUMP_CPU_X86)
    machine_type = IMAGE_FILE_MACHINE_I386;
    stack_frame.AddrFrame.Offset = context->Ebp;
    stack_frame.AddrStack.Offset = context->Esp;
    stack_frame.AddrPC.Offset = context->Eip;
#elif defined(SDLPROCDUMP_CPU_X64)
    machine_type = IMAGE_FILE_MACHINE_AMD64;
    stack_frame.AddrFrame.Offset = context->Rbp;
    stack_frame.AddrStack.Offset = context->Rsp;
    stack_frame.AddrPC.Offset = context->Rip;
#elif defined(SDLPROCDUMP_CPU_ARM32)
    machine_type = IMAGE_FILE_MACHINE_ARM;
    stack_frame.AddrFrame.Offset = context->Lr;
    stack_frame.AddrStack.Offset = context->Sp;
    stack_frame.AddrPC.Offset = context->Pc;
#elif defined(SDLPROCDUMP_CPU_ARM64)
    machine_type = IMAGE_FILE_MACHINE_ARM64;
    stack_frame.AddrFrame.Offset = context->Fp;
    stack_frame.AddrStack.Offset = context->Sp;
    stack_frame.AddrPC.Offset = context->Pc;
#endif
    while (dyn_dbghelp.pStackWalk64(machine_type,                          /* DWORD                            MachineType */
                                    process_information->hProcess,         /* HANDLE                           hProcess */
                                    process_information->hThread,          /* HANDLE                           hThread */
                                    &stack_frame,                          /* LPSTACKFRAME64                   StackFrame */
                                    context,                               /* PVOID                            ContextRecord */
                                    NULL,                                  /* PREAD_PROCESS_MEMORY_ROUTINE64   ReadMemoryRoutine */
                                    dyn_dbghelp.pSymFunctionTableAccess64, /* PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine */
                                    dyn_dbghelp.pSymGetModuleBase64,       /* PGET_MODULE_BASE_ROUTINE64       GetModuleBaseRoutine */
                                    NULL)) {                               /* PTRANSLATE_ADDRESS_ROUTINE64     TranslateAddress */
        IMAGEHLP_MODULE64 module_info;
        union {
            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(CHAR)];
            SYMBOL_INFO symbol_info;
        } symbol;
        DWORD64 dwDisplacement;
        DWORD lineColumn = 0;
        IMAGEHLP_LINE64 line;
        const char *image_file_name;
        const char *symbol_name;
        const char *file_name;
        char line_number[16];

        if (stack_frame.AddrPC.Offset == stack_frame.AddrReturn.Offset) {
            printf_message("PC == Return Address => Possible endless callstack");
            break;
        }

        memset(&module_info, 0, sizeof(module_info));
        module_info.SizeOfStruct = sizeof(module_info);
        if (!dyn_dbghelp.pSymGetModuleInfo64(process_information->hProcess, stack_frame.AddrPC.Offset, &module_info)) {
            image_file_name = "?";
        } else {
            image_file_name = get_simple_basename(module_info.ImageName);
        }

        memset(&symbol, 0, sizeof(symbol));
        symbol.symbol_info.SizeOfStruct = sizeof(symbol.symbol_info);
        symbol.symbol_info.MaxNameLen = MAX_SYM_NAME;
        if (!dyn_dbghelp.pSymFromAddr(process_information->hProcess, (DWORD64)(uintptr_t)stack_frame.AddrPC.Offset, &dwDisplacement, &symbol.symbol_info)) {
            symbol_name = "???";
            dwDisplacement = 0;
        } else {
            symbol_name = symbol.symbol_info.Name;
        }

        line.SizeOfStruct = sizeof(line);
        if (!dyn_dbghelp.pSymGetLineFromAddr64(process_information->hProcess, (DWORD64)(uintptr_t)stack_frame.AddrPC.Offset, &lineColumn, &line)) {
            file_name = "";
            line_number[0] = '\0';
        } else {
            file_name = line.FileName;
            snprintf(line_number, sizeof(line_number), "Line %u", (unsigned int)line.LineNumber);
        }
        printf_message("%s!%s+0x%x %s %s", image_file_name, symbol_name, dwDisplacement, file_name, line_number);
    }
}

static PCONTEXT FillInThreadContext(LPPROCESS_INFORMATION process_information, PCONTEXT context_buffer) {
    HANDLE thread_handle = NULL;

    thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE, process_information->dwThreadId);
    if (!thread_handle) {
        printf_windows_message("OpenThread failed: no stacktrace");
        return NULL;
    }

    memset(context_buffer, 0, sizeof(*context_buffer));
    context_buffer->ContextFlags = CONTEXT_ALL;
    if (!GetThreadContext(thread_handle, context_buffer)) {
        printf_windows_message("GetThreadContext failed: no stacktrace");
        CloseHandle(thread_handle);
        return NULL;
    }
    CloseHandle(thread_handle);
    return context_buffer;
}

static void GetMSCExceptionName(HANDLE hProcess, ULONG_PTR *parameters, DWORD count_parameters, char *buffer, size_t buffer_size) {

#define FIXUP_DWORD_POINTER(ADDR) ((sizeof(void *) == 8) ? (parameters[3] + (ADDR)) : (ADDR))
#define CHECKED_ReadProcessMemory(PROCESS, ADDRESS, BUFFER, COUNT, WHAT) \
    do {                                                                                                \
        SIZE_T actual_count;                                                                            \
        BOOL res = ReadProcessMemory((PROCESS), (ADDRESS), (BUFFER), (COUNT), &actual_count);           \
        if (!res) {                                                                                     \
            printf_windows_message(WHAT ": ReadProcessMemory failed");                                  \
            strncpy_s(buffer, buffer_size, "<error>", buffer_size);                                     \
            return;                                                                                     \
        }                                                                                               \
        if ((COUNT) != (actual_count)) {                                                                \
            printf_message(WHAT ": ReadProcessMemory did not read enough data actual=%lu expected=%lu", \
                           (unsigned long) (actual_count), (unsigned long) (COUNT));                    \
            strncpy_s(buffer, buffer_size, "<error>", buffer_size);                                     \
            return;                                                                                     \
        }                                                                                               \
    } while (0)

    DWORD depth0;
    char *ptr_depth0;
    DWORD depth1;
    char *ptr_depth1;
    DWORD depth2;
    char *ptr_depth2;

    CHECKED_ReadProcessMemory(hProcess, (void *)(parameters[2] + 3 * sizeof(DWORD)), &depth0, sizeof(depth0), "depth 0");
    ptr_depth0 = (char *)FIXUP_DWORD_POINTER(depth0);
    CHECKED_ReadProcessMemory(hProcess, ptr_depth0 + 1 * sizeof(DWORD), &depth1, sizeof(depth1), "depth 1");
    ptr_depth1 = (char *)FIXUP_DWORD_POINTER(depth1);
    CHECKED_ReadProcessMemory(hProcess, ptr_depth1 + 1 * sizeof(DWORD), &depth2, sizeof(depth2), "depth 2");
    ptr_depth2 = (char *)FIXUP_DWORD_POINTER(depth2);
    CHECKED_ReadProcessMemory(hProcess, ptr_depth2 + 2 * sizeof(void*), buffer, buffer_size, "data");
    buffer[buffer_size - 1] = '\0';

#undef FIXUP_DWORD_POINTER
#undef CHECKED_ReadProcessMemory
}

static void log_usage(const char *argv0) {
    fprintf(stderr, "Usage: %s [--help] [--debug-stream] [--] PROGRAM [ARG1 [ARG2 [ARG3 ... ]]]\n", argv0);
}

int main(int argc, char *argv[]) {
    int i;
    int cmd_start;
    size_t command_line_len = 0;
    char *command_line;
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_information;
    BOOL success;
    BOOL debugger_present;
    DWORD exit_code;
    DWORD creation_flags;
    BOOL log_debug_stream = FALSE;

    cmd_start = -1;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            cmd_start = i + 1;
            break;
        } else if (strcmp(argv[i], "--debug-stream") == 0) {
            log_debug_stream = TRUE;
            continue;
        } else if (strcmp(argv[i], "--help") == 0) {
            log_usage(argv[0]);
            return 0;
        } else {
            cmd_start = i;
            break;
        }
    }
    if (cmd_start < 0 || cmd_start >= argc) {
        log_usage(argv[0]);
        return 1;
    }

    for (i = cmd_start; i < argc; i++) {
        command_line_len += strlen(argv[i]) + 1;
    }
    command_line = malloc(command_line_len + 1);
    if (!command_line) {
        printf_message("Failed to allocate memory for command line");
        return 1;
    }
    command_line[0] = '\0';
    for (i = cmd_start; i < argc; i++) {
        strcat_s(command_line, command_line_len, argv[i]);
        if (i != argc - 1) {
            strcat_s(command_line, command_line_len, " ");
        }
    }

    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);

    debugger_present = IsDebuggerPresent();
    creation_flags = NORMAL_PRIORITY_CLASS;
    if (!debugger_present) {
        creation_flags |= DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS;
    }
    success = CreateProcessA(
        argv[cmd_start],        /* LPCSTR                lpApplicationName, */
        command_line,           /* LPSTR                 lpCommandLine, */
        NULL,                   /* LPSECURITY_ATTRIBUTES lpProcessAttributes, */
        NULL,                   /* LPSECURITY_ATTRIBUTES lpThreadAttributes, */
        TRUE,                   /* BOOL                  bInheritHandles, */
        creation_flags,         /* DWORD                 dwCreationFlags, */
        NULL,                   /* LPVOID                lpEnvironment, */
        NULL,                   /* LPCSTR                lpCurrentDirectory, */
        &startup_info,          /* LPSTARTUPINFOA        lpStartupInfo, */
        &process_information);  /* LPPROCESS_INFORMATION lpProcessInformation */

    if (!success) {
        printf_windows_message("Failed to start application \"%s\"", argv[cmd_start]);
        return 1;
    }

    if (debugger_present) {
        WaitForSingleObject(process_information.hProcess, INFINITE);
    } else {
        int process_alive = 1;
        DEBUG_EVENT event;
        while (process_alive) {
            DWORD continue_status = DBG_CONTINUE;
            success = WaitForDebugEvent(&event, INFINITE);
            if (!success) {
                printf_windows_message("Failed to get a debug event");
                return 1;
            }
            switch (event.dwDebugEventCode) {
            case OUTPUT_DEBUG_STRING_EVENT:
            {
                if (log_debug_stream) {
                    SIZE_T bytes_read = 0;
                    union {
                        char char_buffer[512];
                        WCHAR wchar_buffer[256];
                    } buffer;
                    if (ReadProcessMemory(process_information.hProcess, event.u.DebugString.lpDebugStringData, buffer.char_buffer, PRODCUMP_MIN(sizeof(buffer), event.u.DebugString.nDebugStringLength), &bytes_read) && bytes_read) {
                        if (event.u.DebugString.fUnicode) {
                            size_t len = bytes_read / 2;
                            buffer.wchar_buffer[255] = '\0';
                            while (len > 0 && (buffer.wchar_buffer[len - 1] == '\0' || buffer.wchar_buffer[len - 1] == '\n' || buffer.wchar_buffer[len - 1] == '\r')) {
                                buffer.wchar_buffer[len - 1] = '\0';
                                len -= 1;
                            }
                            if (len > 0) {
                                printf("[" APPNAME "] (debug) %S\n", buffer.wchar_buffer);
                            }
                        } else {
                            size_t len = bytes_read;
                            buffer.char_buffer[511] = '\0';
                            while (len > 0 && (buffer.char_buffer[len - 1] == '\0' || buffer.char_buffer[len - 1] == '\n' || buffer.char_buffer[len - 1] == '\r')) {
                                buffer.char_buffer[len - 1] = '\0';
                                len -= 1;
                            }
                            if (len > 0) {
                                printf("[" APPNAME "] (debug) %s\n", buffer.char_buffer);
                            }
                        }
                    }
                }
                break;
            }
            case EXCEPTION_DEBUG_EVENT:
            {
                const BOOL cxx_exception = IsCXXException(event.u.Exception.ExceptionRecord.ExceptionCode);
                const BOOL is_fatal = !cxx_exception && (IsFatalExceptionCode(event.u.Exception.ExceptionRecord.ExceptionCode) || (event.u.Exception.ExceptionRecord.ExceptionFlags & EXCEPTION_NONCONTINUABLE));
                if (cxx_exception || is_fatal) {
                    char flag_buffer[256];
                    printf_message("EXCEPTION_DEBUG_EVENT");
                    printf_message("       ExceptionCode: 0x%08lx (%s)",
                                   event.u.Exception.ExceptionRecord.ExceptionCode,
                                   exceptionCode_to_string(event.u.Exception.ExceptionRecord.ExceptionCode));
                    printf_message("      ExceptionFlags: 0x%08lx (%s)",
                                   event.u.Exception.ExceptionRecord.ExceptionFlags,
                                    exceptionFlags_to_string(event.u.Exception.ExceptionRecord.ExceptionFlags, flag_buffer, sizeof(flag_buffer)));

                    printf_message("         FirstChance: %ld", event.u.Exception.dwFirstChance);
                    printf_message("    ExceptionAddress: 0x%08lx",
                                   event.u.Exception.ExceptionRecord.ExceptionAddress);
                }
                if (cxx_exception) {
                    char exception_name[256];
                    GetMSCExceptionName(process_information.hProcess, event.u.Exception.ExceptionRecord.ExceptionInformation, event.u.Exception.ExceptionRecord.NumberParameters,
                                        exception_name, sizeof(exception_name));
                    printf_message("      Exception name: %s", exception_name);
                } else if (is_fatal) {
                    CONTEXT context_buffer;
                    PCONTEXT context;

                    printf_message("    (Non-continuable exception debug event)");
                    context = FillInThreadContext(&process_information, &context_buffer);
                    write_minidump(argv[cmd_start], &process_information, event.dwThreadId, &event.u.Exception.ExceptionRecord, context);
                    printf_message("");
#ifdef SDLPROCDUMP_PRINTSTACK
                    print_stacktrace(&process_information, event.u.Exception.ExceptionRecord.ExceptionAddress, context);
#else
                    printf_message("No support for printing stacktrack for current architecture");
#endif
                    DebugActiveProcessStop(event.dwProcessId);
                    process_alive = FALSE;
                }
                continue_status = DBG_EXCEPTION_NOT_HANDLED;
                break;
            }
            case CREATE_PROCESS_DEBUG_EVENT:
                load_dbghelp();
                if (!dyn_dbghelp.pSymInitialize) {
                    printf_message("Cannot find pSymInitialize in dbghelp.dll: no stacktrace");
                    break;
                }
                /* Don't invade process on CI: downloading symbols will cause test timeouts */
                if (!dyn_dbghelp.pSymInitialize(process_information.hProcess, NULL, FALSE)) {
                    printf_windows_message("SymInitialize failed: no stacktrace");
                    break;
                }
                break;
            case EXIT_PROCESS_DEBUG_EVENT:
                if (event.dwProcessId == process_information.dwProcessId) {
                    process_alive = 0;
                    DebugActiveProcessStop(event.dwProcessId);
                }
                break;
            }
            success = ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continue_status);
            if (!process_alive) {
                DebugActiveProcessStop(event.dwProcessId);
            }
        }
    }
    if (dyn_dbghelp.pSymCleanup) {
        dyn_dbghelp.pSymCleanup(process_information.hProcess);
    }
    unload_dbghelp();

    exit_code = 1;
    success = GetExitCodeProcess(process_information.hProcess, &exit_code);

    if (!success) {
        printf_message("Failed to get process exit code");
        return 1;
    }

    CloseHandle(process_information.hThread);
    CloseHandle(process_information.hProcess);

    return exit_code;
}
