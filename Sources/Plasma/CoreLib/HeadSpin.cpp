/*==LICENSE==*

CyanWorlds.com Engine - MMOG client, server and tools
Copyright (C) 2011  Cyan Worlds, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Additional permissions under GNU GPL version 3 section 7

If you modify this Program, or any covered work, by linking or
combining it with any of RAD Game Tools Bink SDK, Autodesk 3ds Max SDK,
NVIDIA PhysX SDK, Microsoft DirectX SDK, OpenSSL library, Independent
JPEG Group JPEG library, Microsoft Windows Media SDK, or Apple QuickTime SDK
(or a modified version of those libraries),
containing parts covered by the terms of the Bink SDK EULA, 3ds Max EULA,
PhysX SDK EULA, DirectX SDK EULA, OpenSSL and SSLeay licenses, IJG
JPEG Library README, Windows Media SDK EULA, or QuickTime SDK EULA, the
licensors of this Program grant you additional
permission to convey the resulting work. Corresponding Source for a
non-source form of such a combination shall include the source code for
the parts of OpenSSL and IJG JPEG Library used as well as that of the covered
work.

You can contact Cyan Worlds, Inc. by email legal@cyan.com
 or by snail mail at:
      Cyan Worlds, Inc.
      14617 N Newport Hwy
      Mead, WA   99021

*==LICENSE==*/
#include "HeadSpin.h"
#include "hsWindows.h"
#include <wchar.h>

#ifdef _MSC_VER
#   include <crtdbg.h>
#endif

#if defined(HS_BUILD_FOR_UNIX)
#   include <cstring>
#   include <sys/stat.h>
#   include <sys/utsname.h>
#   include <fcntl.h>
#   include <unistd.h>
#   include <signal.h>
#endif

#include <string_theory/format>


///////////////////////////////////////////////////////////////////////////
/////////////////// For Status Messages ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////
hsDebugMessageProc gHSStatusProc = nil;

hsDebugMessageProc hsSetStatusMessageProc(hsDebugMessageProc newProc)
{
    hsDebugMessageProc oldProc = gHSStatusProc;

    gHSStatusProc = newProc;

    return oldProc;
}

//////////////////////////////////////////////////////////////////////////

hsDebugMessageProc gHSDebugProc = nil;

hsDebugMessageProc hsSetDebugMessageProc(hsDebugMessageProc newProc)
{
    hsDebugMessageProc oldProc = gHSDebugProc;

    gHSDebugProc = newProc;

    return oldProc;
}

#ifdef HS_DEBUGGING
void hsDebugMessage (const char* message, long val)
{
    char    s[1024];

    if (val)
        s[0] = snprintf(&s[1], 1022, "%s: %ld", message, val);
    else
        s[0] = snprintf(&s[1], 1022, "%s", message);

    if (gHSDebugProc)
        gHSDebugProc(&s[1]);
    else
#if HS_BUILD_FOR_WIN32
    {
        OutputDebugString(&s[1]);
        OutputDebugString("\n");
    }
#else
    {
        fprintf(stderr, "%s\n", &s[1]);
    }
#endif
}
#endif

static bool s_GuiAsserts = true;
void ErrorEnableGui(bool enabled)
{
    s_GuiAsserts = enabled;
}

NORETURN void ErrorAssert(int line, const char* file, const char* fmt, ...)
{
#if defined(HS_DEBUGGING) || !defined(PLASMA_EXTERNAL_RELEASE)
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, std::size(msg), fmt, args);
#if defined(HS_DEBUGGING)
#if defined(_MSC_VER)
    if (s_GuiAsserts)
    {
        if (_CrtDbgReport(_CRT_ASSERT, file, line, NULL, msg))
            DebugBreakAlways();
    } else
#endif // _MSC_VER
    {
        DebugMsg("-------\nASSERTION FAILED:\nFile: %s   Line: %i\nMessage: %s\n-------",
                 file, line, msg);
        fflush(stderr);

        DebugBreakAlways();
    }
#endif // HS_DEBUGGING
#else
    DebugBreakIfDebuggerPresent();
#endif // defined(HS_DEBUGGING) || !defined(PLASMA_EXTERNAL_RELEASE)

    // If no debugger break occurred, just crash.
    std::abort();
}

bool DebugIsDebuggerPresent()
{
#if defined(HS_BUILD_FOR_WIN32)
    return IsDebuggerPresent();
#elif defined(HS_BUILD_FOR_LINUX)
    // From http://google-perftools.googlecode.com/svn/trunk/src/heap-checker.cc
    char buf[256];   // TracerPid comes relatively earlier in status output
    int fd = open("/proc/self/status", O_RDONLY);
    if (fd == -1) {
        return false;  // Can't tell for sure.
    }
    const int len = read(fd, buf, sizeof(buf));
    bool rc = false;
    if (len > 0) {
        const char* const kTracerPid = "TracerPid:\t";
        buf[len - 1] = '\0';
        const char* p = strstr(buf, kTracerPid);
        if (p) {
            rc = (strncmp(p + strlen(kTracerPid), "0\n", 2) != 0);
        }
    }
    close(fd);
    return rc;
#else
    // FIXME
    return false;
#endif
}

void DebugBreakIfDebuggerPresent()
{
#if defined(_MSC_VER)
    __try
    {
        __debugbreak();
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        // Debugger not present or some such shwiz.
        // Whatever. Don't crash here.
    }
#elif defined(HS_BUILD_FOR_UNIX)
    if (DebugIsDebuggerPresent())
        raise(SIGTRAP);
#else
    // FIXME
#endif // _MSC_VER
}

void DebugBreakAlways()
{
#if defined(_MSC_VER)
    DebugBreak();
#elif defined(HS_BUILD_FOR_UNIX)
    raise(SIGTRAP);
#else
    // FIXME
    abort();
#endif // _MSC_VER
}

void DebugMsg(const char* fmt, ...)
{
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, std::size(msg), fmt, args);
    fprintf(stderr, "%s\n", msg);

#ifdef _MSC_VER
    if (DebugIsDebuggerPresent())
    {
        // Also print to the MSVC Output window
        OutputDebugStringA(msg);
        OutputDebugStringA("\n");
    }
#endif
}

////////////////////////////////////////////////////////////////////////////

#ifndef PLASMA_EXTERNAL_RELEASE

void hsStatusMessage(const char* message)
{
  if (gHSStatusProc) {
    gHSStatusProc(message);
  } else {
#if HS_BUILD_FOR_UNIX
    printf("%s",message);
    size_t len = strlen(message);
    if (len>0 && message[len-1]!='\n')
        printf("\n");
#elif HS_BUILD_FOR_WIN32
    OutputDebugString(message);
    size_t len = strlen(message);
    if (len>0 && message[len-1]!='\n')
        OutputDebugString("\n");
#endif
  }
}

void hsStatusMessageV(const char * fmt, va_list args)
{
    char  buffer[2000];
    vsnprintf(buffer, std::size(buffer), fmt, args);
    hsStatusMessage(buffer);
}

void hsStatusMessageF(const char * fmt, ...)
{
    va_list args;
    va_start(args,fmt);
    hsStatusMessageV(fmt,args);
    va_end(args);
}

#endif

class hsMinimizeClientGuard
{
    hsWindowHndl fWnd;

public:
    hsMinimizeClientGuard()
    {
#ifdef HS_BUILD_FOR_WIN32
        fWnd = GetActiveWindow();
        // If the application's topmost window is fullscreen, minimize it before displaying an error
        if ((GetWindowLong(fWnd, GWL_STYLE) & WS_POPUP) != 0)
            ShowWindow(fWnd, SW_MINIMIZE);
#endif // HS_BUILD_FOR_WIN32
    }

    ~hsMinimizeClientGuard()
    {
#ifdef HS_BUILD_FOR_WIN32
        ShowWindow(fWnd, SW_RESTORE);
#endif // HS_BUILD_FOR_WIN32
    }
};

bool hsMessageBox_SuppressPrompts = false;

int hsMessageBoxWithOwner(hsWindowHndl owner, const char* message, const char* caption, int kind, int icon)
{
    if (hsMessageBox_SuppressPrompts)
        return hsMBoxOk;

#if HS_BUILD_FOR_WIN32
    uint32_t flags = 0;

    if (kind == hsMessageBoxNormal)
        flags |= MB_OK;
    else if (kind == hsMessageBoxAbortRetyIgnore)
        flags |= MB_ABORTRETRYIGNORE;
    else if (kind == hsMessageBoxOkCancel)
        flags |= MB_OKCANCEL;
    else if (kind == hsMessageBoxRetryCancel)
        flags |= MB_RETRYCANCEL;
    else if (kind == hsMessageBoxYesNo)
        flags |= MB_YESNO;
    else if (kind == hsMessageBoxYesNoCancel)
        flags |= MB_YESNOCANCEL;
    else
        flags |= MB_OK;

    if (icon == hsMessageBoxIconError)
        flags |= MB_ICONERROR;
    else if (icon == hsMessageBoxIconQuestion)
        flags |= MB_ICONQUESTION;
    else if (icon == hsMessageBoxIconExclamation)
        flags |= MB_ICONEXCLAMATION;
    else if (icon == hsMessageBoxIconAsterisk)
        flags |= MB_ICONASTERISK;
    else
        flags |= MB_ICONERROR;

    hsMinimizeClientGuard guard;
    int ans = MessageBox(owner, message, caption, flags);

    switch (ans)
    {
    case IDOK:          return hsMBoxOk;
    case IDCANCEL:      return hsMBoxCancel;
    case IDABORT:       return hsMBoxAbort;
    case IDRETRY:       return hsMBoxRetry;
    case IDIGNORE:      return hsMBoxIgnore;
    case IDYES:         return hsMBoxYes;
    case IDNO:          return hsMBoxNo;
    default:            return hsMBoxCancel;
    }

#endif
    return hsMBoxCancel;
}

int hsMessageBoxWithOwner(hsWindowHndl owner, const wchar_t* message, const wchar_t* caption, int kind, int icon)
{
    if (hsMessageBox_SuppressPrompts)
        return hsMBoxOk;

#if HS_BUILD_FOR_WIN32
    uint32_t flags = 0;

    if (kind == hsMessageBoxNormal)
        flags |= MB_OK;
    else if (kind == hsMessageBoxAbortRetyIgnore)
        flags |= MB_ABORTRETRYIGNORE;
    else if (kind == hsMessageBoxOkCancel)
        flags |= MB_OKCANCEL;
    else if (kind == hsMessageBoxRetryCancel)
        flags |= MB_RETRYCANCEL;
    else if (kind == hsMessageBoxYesNo)
        flags |= MB_YESNO;
    else if (kind == hsMessageBoxYesNoCancel)
        flags |= MB_YESNOCANCEL;
    else
        flags |= MB_OK;

    if (icon == hsMessageBoxIconError)
        flags |= MB_ICONERROR;
    else if (icon == hsMessageBoxIconQuestion)
        flags |= MB_ICONQUESTION;
    else if (icon == hsMessageBoxIconExclamation)
        flags |= MB_ICONEXCLAMATION;
    else if (icon == hsMessageBoxIconAsterisk)
        flags |= MB_ICONASTERISK;
    else
        flags |= MB_ICONERROR;

    hsMinimizeClientGuard guard;
    int ans = MessageBoxW(owner, message, caption, flags);

    switch (ans)
    {
    case IDOK:          return hsMBoxOk;
    case IDCANCEL:      return hsMBoxCancel;
    case IDABORT:       return hsMBoxAbort;
    case IDRETRY:       return hsMBoxRetry;
    case IDIGNORE:      return hsMBoxIgnore;
    case IDYES:         return hsMBoxYes;
    case IDNO:          return hsMBoxNo;
    default:            return hsMBoxCancel;
    }

#endif
    return hsMBoxCancel;
}

int hsMessageBox(const char* message, const char* caption, int kind, int icon)
{
    return hsMessageBoxWithOwner((hsWindowHndl)nil,message,caption,kind,icon);
}

int hsMessageBox(const wchar_t* message, const wchar_t* caption, int kind, int icon)
{
    return hsMessageBoxWithOwner((hsWindowHndl)nil,message,caption,kind,icon);
}

/**************************************/
char* hsStrcpy(char* dst, const char* src)
{
    if (src)
    {
        if (dst == nil)
        {
            size_t count = strlen(src);
            dst = new char[count + 1];
            memcpy(dst, src, count);
            dst[count] = 0;
            return dst;
        }

        int32_t i;
        for (i = 0; src[i] != 0; i++)
            dst[i] = src[i];
        dst[i] = 0;
    }

    return dst;
}

void hsStrLower(char *s)
{
    if (s)
    {
        size_t len = strlen(s);
        for (size_t i = 0; i < len; i++)
            s[i] = tolower(s[i]);
    }
}

//// IStringToWString /////////////////////////////////////////////////////////
// Converts a char * string to a wchar_t * string

wchar_t *hsStringToWString( const char *str )
{
    // convert the char string to a wchar_t string
    size_t len = strlen(str);
    wchar_t *wideString = new wchar_t[len+1];
    for (size_t i = 0; i < len; i++)
        wideString[i] = btowc(str[i]);
    wideString[len] = L'\0';
    return wideString;
}

//// IWStringToString /////////////////////////////////////////////////////////
// Converts a wchar_t * string to a char * string

char    *hsWStringToString( const wchar_t *str )
{
    // convert the wchar_t string to a char string
    size_t len = wcslen(str);
    char *sStr = new char[len+1];

    for (size_t i = 0; i < len; i++)
    {
        char temp = wctob(str[i]);
        if (temp == EOF)
        {
            sStr[i] = '\0';
            i = len;
        }
        else
            sStr[i] = temp;
    }
    sStr[len] = '\0';

    return sStr;
}

//
// Microsoft SAMPLE CODE
// returns array of allocated version info strings or nil
//
std::vector<ST::string> DisplaySystemVersion()
{
    std::vector<ST::string> versionStrs;
#if HS_BUILD_FOR_WIN32
#ifndef VER_SUITE_PERSONAL
#define VER_SUITE_PERSONAL 0x200
#endif
    const RTL_OSVERSIONINFOEXW& version = hsGetWindowsVersion();

    switch (version.dwPlatformId)
    {
    case VER_PLATFORM_WIN32_NT:

        // Test for the product.

        if ( version.dwMajorVersion <= 4 )
            versionStrs.push_back("Microsoft Windows NT ");

        if ( version.dwMajorVersion == 5 && version.dwMinorVersion == 0 )
            versionStrs.push_back("Microsoft Windows 2000 ");

        if ( version.dwMajorVersion == 5 && version.dwMinorVersion == 1 )
            versionStrs.push_back("Microsoft Windows XP ");

        if ( version.dwMajorVersion == 6 && version.dwMinorVersion == 0 )
            versionStrs.push_back("Microsoft Windows Vista ");

        if ( version.dwMajorVersion == 6 && version.dwMinorVersion == 1 )
            versionStrs.push_back("Microsoft Windows 7 ");

        if ( version.dwMajorVersion == 6 && version.dwMinorVersion == 2 )
            versionStrs.push_back("Microsoft Windows 8 ");

        if ( version.dwMajorVersion == 6 && version.dwMinorVersion == 3 )
            versionStrs.push_back("Microsoft Windows 8.1 ");

        if ( version.dwMajorVersion == 10 && version.dwMinorVersion == 0 )
            versionStrs.push_back("Microsoft Windows 10 ");

        // Test for product type.

        if ( version.wProductType == VER_NT_WORKSTATION )
        {
            if( version.wSuiteMask & VER_SUITE_PERSONAL )
                versionStrs.push_back("Personal ");
            else
                versionStrs.push_back("Professional ");
        }
        else if ( version.wProductType == VER_NT_SERVER )
        {
            if( version.wSuiteMask & VER_SUITE_DATACENTER )
                versionStrs.push_back("DataCenter Server ");
            else if( version.wSuiteMask & VER_SUITE_ENTERPRISE )
                versionStrs.push_back("Advanced Server ");
            else
                versionStrs.push_back("Server ");
        }

        // Display version, service pack (if any), and build number.

        if ( version.dwMajorVersion <= 4 )
        {
            versionStrs.push_back(ST::format("version {}.{} {} (Build {})\n",
                version.dwMajorVersion,
                version.dwMinorVersion,
                version.szCSDVersion,
                version.dwBuildNumber & 0xFFFF));
        }
        else
        {
            versionStrs.push_back(ST::format("{} (Build {})\n",
                version.szCSDVersion,
                version.dwBuildNumber & 0xFFFF));
        }
        break;
    }
#elif HS_BUILD_FOR_UNIX
    struct utsname sysinfo;
    uname(&sysinfo);
    versionStrs.push_back(ST::format("{} {} ({})\n", sysinfo.sysname, sysinfo.release, sysinfo.machine));
#endif
    return versionStrs;
}
