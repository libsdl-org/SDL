#include <windows.h>
#include <dbghelp.h>

#include <stdio.h>
#include <string.h>

#define APPNAME "[SDLPROCDUMP]"
#define DUMP_FOLDER "minidumps"

typedef BOOL (WINAPI *MiniDumpWriteDumpFuncType)(
    HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    MINIDUMP_TYPE DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

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

static const char *exceptionCode_to_string(DWORD dwCode) {
#define SWITCH_CODE_STR(V) case V: return #V;
    switch (dwCode) {
        FOREACH_EXCEPTION_CODES(SWITCH_CODE_STR)
    default: {
        static const char unknown[] = "unknown";
        return unknown;
    }
    }
#undef SWITCH_CODE_STR
}

static int IsFatalExceptionCode(DWORD dwCode) {
    switch (dwCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_STACK_OVERFLOW:
    case STATUS_HEAP_CORRUPTION:
    case STATUS_STACK_BUFFER_OVERRUN:
    case EXCEPTION_GUARD_PAGE:
    case EXCEPTION_INVALID_HANDLE:
        return 1;
    default:
        return 0;
    }
}

static void format_windows_error_message(const char *message) {
    char win_msg[512];
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        win_msg, sizeof(win_msg)/sizeof(*win_msg),
        NULL);
    size_t win_msg_len = strlen(win_msg);
    while (win_msg[win_msg_len-1] == '\r' || win_msg[win_msg_len-1] == '\n' || win_msg[win_msg_len-1] == ' ') {
        win_msg[win_msg_len-1] = '\0';
        win_msg_len--;
    }
    fprintf(stderr, "%s %s (%s)\n", APPNAME, message, win_msg);
}

static void create_minidump(const char *child_file_path, const LPPROCESS_INFORMATION process_information, DWORD dwThreadId) {
    BOOL success;
    char dump_file_path[MAX_PATH];
    char child_file_name[64];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HMODULE dbghelp_module = NULL;
    MiniDumpWriteDumpFuncType MiniDumpWriteDumpFunc = NULL;
    MINIDUMP_EXCEPTION_INFORMATION minidump_exception_information;
    SYSTEMTIME system_time;

    success = CreateDirectoryA(DUMP_FOLDER, NULL);
    if (!success && GetLastError() != ERROR_ALREADY_EXISTS) {
        format_windows_error_message("Failed to create minidump directory");
        goto post_dump;
    }
    _splitpath_s(child_file_path, NULL, 0, NULL, 0, child_file_name, sizeof(child_file_name), NULL, 0);
    GetLocalTime(&system_time);

    snprintf(dump_file_path, sizeof(dump_file_path), "minidumps/%s_%04d-%02d-%02d_%d-%02d-%02d.dmp",
             child_file_name,
             system_time.wYear, system_time.wMonth, system_time.wDay,
             system_time.wHour, system_time.wMinute, system_time.wSecond);
    fprintf(stderr, "%s Writing minidump to \"%s\"\n", APPNAME, dump_file_path);
    hFile = CreateFileA(
        dump_file_path,
        GENERIC_WRITE,
        FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        format_windows_error_message("Failed to open file for minidump");
        goto post_dump;
    }
    dbghelp_module = LoadLibraryA("dbghelp.dll");
    if (!dbghelp_module) {
        format_windows_error_message("Failed to load dbghelp.dll");
        goto post_dump;
    }
    MiniDumpWriteDumpFunc = (MiniDumpWriteDumpFuncType)GetProcAddress(dbghelp_module, "MiniDumpWriteDump");
    if (!MiniDumpWriteDumpFunc) {
        format_windows_error_message("Failed to find MiniDumpWriteDump in dbghelp.dll");
        goto post_dump;
    }
    minidump_exception_information.ClientPointers = FALSE;
    minidump_exception_information.ExceptionPointers = FALSE;
    minidump_exception_information.ThreadId = dwThreadId;
    success = MiniDumpWriteDumpFunc(
        process_information->hProcess,      /* HANDLE                            hProcess */
        process_information->dwProcessId,   /* DWORD                             ProcessId */
        hFile,                              /* HANDLE                            hFile */
        MiniDumpWithFullMemory,             /* MINIDUMP_TYPE                     DumpType */
        &minidump_exception_information,    /* PMINIDUMP_EXCEPTION_INFORMATION   ExceptionParam */
        NULL,                               /* PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam */
        NULL);                              /* PMINIDUMP_CALLBACK_INFORMATION    CallbackParam */
    if (!success) {
        format_windows_error_message("Failed to write minidump");
    }
post_dump:
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
    if (dbghelp_module != NULL) {
        FreeLibrary(dbghelp_module);
    }
}

int main(int argc, char *argv[]) {
    int i;
    size_t command_line_len = 0;
    char *command_line;
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_information;
    BOOL success;
    BOOL debugger_present;
    DWORD exit_code;
    DWORD creation_flags;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s PROGRAM [ARG1 [ARG2 [ARG3 ... ]]]\n", argv[0]);
        return 1;
    }

    for (i = 1; i < argc; i++) {
        command_line_len += strlen(argv[1]) + 1;
    }
    command_line = malloc(command_line_len + 1);
    if (!command_line) {
        fprintf(stderr, "%s Failed to allocate memory for command line\n", APPNAME);
        return 1;
    }
    command_line[0] = '\0';
    for (i = 1; i < argc; i++) {
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
        argv[1],                /* LPCSTR                lpApplicationName, */
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
        fprintf(stderr, "%s Failed to start application\n", APPNAME);
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
                fprintf(stderr, "%s Failed to get a debug event\n", APPNAME);
                return 1;
            }
            switch (event.dwDebugEventCode) {
            case EXCEPTION_DEBUG_EVENT:
                if (IsFatalExceptionCode(event.u.Exception.ExceptionRecord.ExceptionCode) || (event.u.Exception.ExceptionRecord.ExceptionFlags & EXCEPTION_NONCONTINUABLE)) {
                    fprintf(stderr, "%s EXCEPTION_DEBUG_EVENT ExceptionCode: 0x%08lx (%s) ExceptionFlags: 0x%08lx\n",
                            APPNAME,
                            event.u.Exception.ExceptionRecord.ExceptionCode,
                            exceptionCode_to_string(event.u.Exception.ExceptionRecord.ExceptionCode),
                            event.u.Exception.ExceptionRecord.ExceptionFlags);
                    fprintf(stderr, "%s Non-continuable exception debug event\n", APPNAME);
                    create_minidump(argv[1], &process_information, event.dwThreadId);
                    DebugActiveProcessStop(event.dwProcessId);
                    process_alive = 0;
                }
                continue_status = DBG_EXCEPTION_HANDLED;
                break;
            case EXIT_PROCESS_DEBUG_EVENT:
                exit_code = event.u.ExitProcess.dwExitCode;
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

    exit_code = 1;
    success = GetExitCodeProcess(process_information.hProcess, &exit_code);

    if (!success) {
        fprintf(stderr, "%s Failed to get process exit code\n", APPNAME);
        return 1;
    }

    CloseHandle(process_information.hThread);
    CloseHandle(process_information.hProcess);

    return exit_code;
}
