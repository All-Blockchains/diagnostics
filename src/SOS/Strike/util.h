// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#ifndef __util_h__
#define __util_h__

#define CONVERT_FROM_SIGN_EXTENDED(offset) ((ULONG_PTR)(offset))

// So we can use the PAL_TRY_NAKED family of macros without dependencies on utilcode.
inline void RestoreSOToleranceState() {}

#include <cor.h>
#include <corsym.h>
#include <clrdata.h>
#include <palclr.h>
#include <new>
#include <functional>

#if !defined(FEATURE_PAL)
#include <dia2.h>
#endif

#ifdef STRIKE
#if defined(_MSC_VER)
#pragma warning(disable:4200)
#pragma warning(default:4200)
#endif
#include "data.h"
#endif //STRIKE

#include <cordebug.h>
#include <static_assert.h>
#include <string>
#include <extensions.h>
#include <releaseholder.h>
#include "hostimpl.h"
#include "targetimpl.h"
#include "runtimeimpl.h"
#include "symbols.h"
#include "crosscontext.h"

typedef LPCSTR  LPCUTF8;
typedef LPSTR   LPUTF8;

#include "contract.h"
#undef NOTHROW
#ifdef FEATURE_PAL
#define NOTHROW
#else
#define NOTHROW (std::nothrow)
#endif

DECLARE_HANDLE(OBJECTHANDLE);

#if defined(_TARGET_WIN64_)
#define WIN64_8SPACES "        "
#define WIN86_8SPACES ""
#define POINTERSIZE "16"
#define POINTERSIZE_HEX 16
#define POINTERSIZE_BYTES 8
#define POINTERSIZE_TYPE "I64"
#else
#define WIN64_8SPACES ""
#define WIN86_8SPACES "        "
#define POINTERSIZE "8"
#define POINTERSIZE_HEX 8
#define POINTERSIZE_BYTES 4
#define POINTERSIZE_TYPE "I32"
#endif

#ifndef TARGET_POINTER_SIZE
#define TARGET_POINTER_SIZE POINTERSIZE_BYTES
#endif // TARGET_POINTER_SIZE

#undef _ASSERTE
#ifdef _DEBUG
#define _ASSERTE(expr)         \
    do { if (!(expr) ) { ExtErr("_ASSERTE fired:\n\t%s\n", #expr); if (IsDebuggerPresent()) DebugBreak(); } } while (0)
#else
#define _ASSERTE(expr) ((void)0)
#endif

#undef _ASSERT
#define _ASSERT _ASSERTE

// The native symbol reader dll name
#if defined(_AMD64_)
#define NATIVE_SYMBOL_READER_DLL "Microsoft.DiaSymReader.Native.amd64.dll"
#elif defined(_X86_)
#define NATIVE_SYMBOL_READER_DLL "Microsoft.DiaSymReader.Native.x86.dll"
#elif defined(_ARM_)
#define NATIVE_SYMBOL_READER_DLL "Microsoft.DiaSymReader.Native.arm.dll"
#elif defined(_ARM64_)
#define NATIVE_SYMBOL_READER_DLL "Microsoft.DiaSymReader.Native.arm64.dll"
#endif

class MethodTable;

#define MD_NOT_YET_LOADED ((DWORD_PTR)-1)
/*
 * HANDLES
 *
 * The default type of handle is a strong handle.
 *
 */
#define HNDTYPE_DEFAULT                         HNDTYPE_STRONG
#define HNDTYPE_WEAK_DEFAULT                    HNDTYPE_WEAK_LONG
#define HNDTYPE_WEAK_SHORT                      (0)
#define HNDTYPE_WEAK_LONG                       (1)
#define HNDTYPE_STRONG                          (2)
#define HNDTYPE_PINNED                          (3)
#define HNDTYPE_VARIABLE                        (4)
#define HNDTYPE_REFCOUNTED                      (5)
#define HNDTYPE_DEPENDENT                       (6)
#define HNDTYPE_ASYNCPINNED                     (7)
#define HNDTYPE_SIZEDREF                        (8)
#define HNDTYPE_WEAK_WINRT                      (9)
#define HNDTYPE_WEAK_INTERIOR_POINTER           (10)


class BaseObject
{
    MethodTable    *m_pMethTab;
};

const BYTE gElementTypeInfo[] = {
#define TYPEINFO(e,ns,c,s,g,ia,ip,if,im,gv)    s,
#include "cortypeinfo.h"
#undef TYPEINFO
};

typedef struct tagLockEntry
{
    tagLockEntry *pNext;    // next entry
    tagLockEntry *pPrev;    // prev entry
    DWORD dwULockID;
    DWORD dwLLockID;        // owning lock
    WORD wReaderLevel;      // reader nesting level
} LockEntry;

#define MAX_CLASSNAME_LENGTH    1024

enum EEFLAVOR {UNKNOWNEE, MSCOREE, MSCORWKS, MSCOREND};

#include "sospriv.h"
extern IXCLRDataProcess *g_clrData;
extern ISOSDacInterface *g_sos;
extern ISOSDacInterface15 *g_sos15;
extern ISOSDacInterface16 *g_sos16;

#include "dacprivate.h"

// This class is templated for easy modification.  We may need to update the CachedString
// or related classes to use WCHAR instead of char in the future.
template <class T, int count, int size>
class StaticData
{
public:
    StaticData()
    {
        for (int i = 0; i < count; ++i)
            InUse[i] = false;
    }

    // Whether the individual data pointers in the cache are in use.
    bool InUse[count];

    // The actual data itself.
    T Data[count][size];

    // The number of arrays in the cache.
    static const int Count;

    // The size of each individual array.
    static const int Size;
};

class CachedString
{
public:
    CachedString();
    CachedString(const CachedString &str);
    ~CachedString();

    const CachedString &operator=(const CachedString &str);

    // Returns the capacity of this string.
    size_t GetStrLen() const
    {
        return mSize;
    }

    // Returns a mutable character pointer.  Be sure not to write past the
    // length of this string.
    inline operator char *()
    {
        return mPtr;
    }

    // Returns a const char representation of this string.
    inline operator const char *() const
    {
        return GetPtr();
    }

    // To ensure no AV's, any time a constant pointer is requested, we will
    // return an empty string "" if we hit an OOM.  This will only happen
    // if we hit an OOM and do not check for it before using the string.
    // If you request a non-const char pointer out of this class, it may be
    // null (see operator char *).
    inline const char *GetPtr() const
    {
        if (!mPtr || IsOOM())
            return "";

        return mPtr;
    }

    // Returns true if we ran out of memory trying to allocate the string
    // or the refcount.
    bool IsOOM() const
    {
        return mIndex == -2;
    }

    // allocate a string of the specified size.  this will Clear() any
    // previously allocated string.  call IsOOM() to check for failure.
    void Allocate(int size);

private:
    // Copies rhs into this string.
    void Copy(const CachedString &rhs);

    // Clears this string, releasing any underlying memory.
    void Clear();

    // Creates a new string.
    void Create();

    // Sets an out of memory state.
    void SetOOM();

private:
    char *mPtr;

    // The reference count.  This may be null if there is only one copy
    // of this string.
    mutable unsigned int *mRefCount;

    // mIndex contains the index of the cached pointer we are using, or:
    // ~0 - poison value we initialize it to for debugging purposes
    // -1 - mPtr points to a pointer we have new'ed
    // -2 - We hit an oom trying to allocate either mCount or mPtr
    int mIndex;

    // contains the size of current string
    int mSize;

private:
    static StaticData<char, 4, 1024> cache;
};

// Things in this namespace should not be directly accessed/called outside of
// the output-related functions.
namespace Output
{
    extern unsigned int g_bSuppressOutput;
    extern unsigned int g_Indent;
    extern unsigned int g_DMLEnable;
    extern bool g_bDbgOutput;
    extern bool g_bDMLExposed;

    inline bool IsOutputSuppressed()
    { return g_bSuppressOutput > 0; }

    inline void ResetIndent()
    { g_Indent = 0; }

    inline void SetDebugOutputEnabled(bool enabled)
    { g_bDbgOutput = enabled; }

    inline bool IsDebugOutputEnabled()
    { return g_bDbgOutput; }

    inline void SetDMLExposed(bool exposed)
    { g_bDMLExposed = exposed; }

    inline bool IsDMLExposed()
    { return g_bDMLExposed; }

    enum FormatType
    {
        DML_None,
        DML_MethodTable,
        DML_MethodDesc,
        DML_EEClass,
        DML_Module,
        DML_IP,
        DML_Object,
        DML_Domain,
        DML_Assembly,
        DML_ThreadID,
        DML_ValueClass,
        DML_DumpHeapMT,
        DML_ListNearObj,
        DML_ThreadState,
        DML_PrintException,
        DML_RCWrapper,
        DML_CCWrapper,
        DML_ManagedVar,
        DML_Async,
        DML_IL,
        DML_ComWrapperRCW,
        DML_ComWrapperCCW,
        DML_TaggedMemory,

        DML_Last
    };

    /**********************************************************************\
    * This function builds a DML string for a ValueClass.  If DML is       *
    * enabled, this function returns a DML string based on the format      *
    * type.  Otherwise this returns a string containing only the hex value *
    * of addr.                                                             *
    *                                                                      *
    * Params:                                                              *
    *   disp - the display address of the object                           *
    *   mt - the method table of the ValueClass                            *
    *   addr - the address of the ValueClass                               *
    *   type - the format type to use to output this object                *
    *   fill - whether or not to pad the hex value with zeros              *
    *                                                                      *
    \**********************************************************************/
    CachedString BuildVCValue(CLRDATA_ADDRESS disp, CLRDATA_ADDRESS mt, CLRDATA_ADDRESS addr, FormatType type, bool fill = true);

    /**********************************************************************\
    * This function builds a DML string with a display name.  If DML is enabled,  *
    * this function returns a DML string based on the format type.         *
    * Otherwise this returns a string containing only the hex value of     *
    * addr.                                                                *
    *                                                                      *
    * Params:                                                              *
    *   disp - the display address of the object                           *
    *   addr - the address of the object                                   *
    *   type - the format type to use to output this object                *
    *   fill - whether or not to pad the hex value with zeros              *
    *                                                                      *
    \**********************************************************************/
    CachedString BuildHexValue(CLRDATA_ADDRESS disp, CLRDATA_ADDRESS addr, FormatType type, bool fill = true);

    /**********************************************************************\
    * This function builds a DML string for an object.  If DML is enabled, *
    * this function returns a DML string based on the format type.         *
    * Otherwise this returns a string containing only the hex value of     *
    * addr.                                                                *
    *                                                                      *
    * Params:                                                              *
    *   addr - the address of the object                                   *
    *   len  - associated length                                           *
    *   type - the format type to use to output this object                *
    *   fill - whether or not to pad the hex value with zeros              *
    *                                                                      *
    \**********************************************************************/
    CachedString BuildHexValueWithLength(CLRDATA_ADDRESS addr, size_t len, FormatType type, bool fill = true);

    /**********************************************************************\
    * This function builds a DML string for an managed variable name.      *
    * If DML is enabled, this function returns a DML string that will      *
    * enable the expansion of that managed variable using the !ClrStack    *
    * command to display the variable's fields, otherwise it will just     *
    * return the variable's name as a string.
    *                                                                      *
    * Params:                                                              *
    *   expansionName - the current variable expansion string              *
    *   frame - the frame that contains the variable of interest           *
    *   simpleName - simple name of the managed variable                   *
    *                                                                      *
    \**********************************************************************/
    CachedString BuildManagedVarValue(__in_z LPCWSTR expansionName, ULONG frame, __in_z LPCWSTR simpleName, FormatType type);
    CachedString BuildManagedVarValue(__in_z LPCWSTR expansionName, ULONG frame, int indexInArray, FormatType type);    //used for array indices (simpleName = "[<indexInArray>]")
}

class NoOutputHolder
{
public:
    NoOutputHolder(BOOL bSuppress = TRUE);
    ~NoOutputHolder();

private:
    BOOL mSuppress;
};

class EnableDMLHolder
{
public:
    EnableDMLHolder(BOOL enable);
    ~EnableDMLHolder();

private:
    BOOL mEnable;
};

size_t CountHexCharacters(CLRDATA_ADDRESS val);

HRESULT OutputVaList(ULONG mask, PCSTR format, va_list args);

// Normal output.
void DMLOut(PCSTR format, ...);         /* Prints out DML strings. */
void IfDMLOut(PCSTR format, ...);       /* Prints given DML string ONLY if DML is enabled; prints nothing otherwise. */
void ExtOut(PCSTR Format, ...);         /* Prints out to ExtOut (no DML). */
void ExtWarn(PCSTR Format, ...);        /* Prints out to ExtWarn (no DML). */
void ExtErr(PCSTR Format, ...);         /* Prints out to ExtErr (no DML). */
void ExtDbgOut(PCSTR Format, ...);      /* Prints out to ExtOut in a checked build (no DML). */
void WhitespaceOut(int count);          /* Prints out "count" number of spaces in the output. */

// Change indent for ExtOut
inline void IncrementIndent()  { Output::g_Indent++; }
inline void DecrementIndent()  { if (Output::g_Indent > 0) Output::g_Indent--; }
inline void ExtOutIndent()  { WhitespaceOut(Output::g_Indent << 2); }

// DML Generation Methods
#define DMLListNearObj(addr) Output::BuildHexValue(addr, addr, Output::DML_ListNearObj).GetPtr()
#define DMLDumpHeapMT(addr) Output::BuildHexValue(addr, addr, Output::DML_DumpHeapMT).GetPtr()
#define DMLMethodTable(addr) Output::BuildHexValue(addr, addr, Output::DML_MethodTable).GetPtr()
#define DMLMethodDesc(addr) Output::BuildHexValue(addr, addr, Output::DML_MethodDesc).GetPtr()
#define DMLClass(addr) Output::BuildHexValue(addr, addr, Output::DML_EEClass).GetPtr()
#define DMLModule(addr) Output::BuildHexValue(addr, addr, Output::DML_Module).GetPtr()
#define DMLIP(ip) Output::BuildHexValue(ip, ip, Output::DML_IP).GetPtr()
#define DMLObject(addr) Output::BuildHexValue(addr, addr, Output::DML_Object).GetPtr()
#define DMLByRefObject(byref, addr) Output::BuildHexValue(byref, addr, Output::DML_Object).GetPtr()
#define DMLDomain(addr) Output::BuildHexValue(addr, addr, Output::DML_Domain).GetPtr()
#define DMLAssembly(addr) Output::BuildHexValue(addr, addr, Output::DML_Assembly).GetPtr()
#define DMLThreadID(id) Output::BuildHexValue(id, id, Output::DML_ThreadID, false).GetPtr()
#define DMLValueClass(mt, addr) Output::BuildVCValue(addr, mt, addr, Output::DML_ValueClass).GetPtr()
#define DMLByRefValueClass(byref, mt, addr) Output::BuildVCValue(byref, mt, addr, Output::DML_ValueClass).GetPtr()
#define DMLRCWrapper(addr) Output::BuildHexValue(addr, addr, Output::DML_RCWrapper).GetPtr()
#define DMLCCWrapper(addr) Output::BuildHexValue(addr, addr, Output::DML_CCWrapper).GetPtr()
#define DMLManagedVar(expansionName,frame,simpleName) Output::BuildManagedVarValue(expansionName, frame, simpleName, Output::DML_ManagedVar).GetPtr()
#define DMLAsync(addr) Output::BuildHexValue(addr, addr, Output::DML_Async).GetPtr()
#define DMLIL(addr) Output::BuildHexValue(addr, addr, Output::DML_IL).GetPtr()
#define DMLComWrapperRCW(addr) Output::BuildHexValue(addr, addr, Output::DML_ComWrapperRCW).GetPtr()
#define DMLComWrapperCCW(addr) Output::BuildHexValue(addr, addr, Output::DML_ComWrapperCCW).GetPtr()
#define DMLTaggedMemory(addr, len) Output::BuildHexValueWithLength(addr, len, Output::DML_TaggedMemory).GetPtr()

bool IsDMLEnabled();

#ifndef SOS_Assert
#define SOS_Assert _ASSERTE
#endif

void ConvertToLower(__out_ecount(len) char *buffer, size_t len);

extern const char * const DMLFormats[];
int GetHex(CLRDATA_ADDRESS addr, __out_ecount(len) char *out, size_t len, bool fill);

// A simple string class for mutable strings.  We cannot use STL, so this is a stand in replacement
// for std::string (though it doesn't use the same interface).
template <class T, size_t (__cdecl *LEN)(const T *), errno_t (__cdecl *COPY)(T *, size_t, const T * _Src)>
class BaseString
{
public:
    BaseString()
        : mStr(0), mSize(0), mLength(0)
    {
        const size_t size = 64;

        mStr = new T[size];
        mSize = size;
        mStr[0] = 0;
    }

    BaseString(const T *str)
        : mStr(0), mSize(0), mLength(0)
    {
        CopyFrom(str, LEN(str));
    }

    BaseString(const BaseString<T, LEN, COPY> &rhs)
        : mStr(0), mSize(0), mLength(0)
    {
        *this = rhs;
    }

    ~BaseString()
    {
        Clear();
    }

    const BaseString<T, LEN, COPY> &operator=(const BaseString<T, LEN, COPY> &rhs)
    {
        Clear();
        CopyFrom(rhs.mStr, rhs.mLength);
        return *this;
    }

    const BaseString<T, LEN, COPY> &operator=(const T *str)
    {
        Clear();
        CopyFrom(str, LEN(str));
        return *this;
    }

    const BaseString<T, LEN, COPY> &operator +=(const T *str)
    {
        size_t len = LEN(str);
        CopyFrom(str, len);
        return *this;
    }

    const BaseString<T, LEN, COPY> &operator +=(const BaseString<T, LEN, COPY> &str)
    {
        CopyFrom(str.mStr, str.mLength);
        return *this;
    }

    BaseString<T, LEN, COPY> operator+(const T *str) const
    {
        return BaseString<T, LEN, COPY>(mStr, mLength, str, LEN(str));
    }

    BaseString<T, LEN, COPY> operator+(const BaseString<T, LEN, COPY> &str) const
    {
        return BaseString<T, LEN, COPY>(mStr, mLength, str.mStr, str.mLength);
    }

    operator const T *() const
    {
        return mStr;
    }

    const T *c_str() const
    {
        return mStr;
    }

    size_t GetLength() const
    {
        return mLength;
    }

private:
    BaseString(const T * str1, size_t len1, const T * str2, size_t len2)
    : mStr(0), mSize(0), mLength(0)
    {
        const size_t size = len1 + len2 + 1 + ((len1 + len2) >> 1);
        mStr = new T[size];
        mSize = size;

        CopyFrom(str1, len1);
        CopyFrom(str2, len2);
    }

    void Clear()
    {
        mLength = 0;
        mSize = 0;
        if (mStr)
        {
            delete [] mStr;
            mStr = 0;
        }
    }

    void CopyFrom(const T *str, size_t len)
    {
        if (mLength + len + 1 >= mSize)
            Resize(mLength + len + 1);

        COPY(mStr+mLength, mSize-mLength, str);
        mLength += len;
    }

    void Resize(size_t size)
    {
        /* We always resize at least one half bigger than we need.  When CopyFrom requests a resize
         * it asks for the exact size that's needed to concatenate strings.  However in practice
         * it's common to add multiple strings together in a row, e.g.:
         *    String foo = "One " + "Two " + "Three " + "Four " + "\n";
         * Ensuring the size of the string is bigger than we need, and that the minimum size is 64,
         * we will cut down on a lot of needless resizes at the cost of a few bytes wasted in some
         * cases.
         */
        size += size >> 1;
        if (size < 64)
            size = 64;

        T *newStr = new T[size];

        if (mStr)
        {
            COPY(newStr, size, mStr);
            delete [] mStr;
        }
        else
        {
            newStr[0] = 0;
        }

        mStr = newStr;
        mSize = size;
    }
private:
    T *mStr;
    size_t mSize, mLength;
};

typedef BaseString<char, strlen, strcpy_s> String;
typedef BaseString<WCHAR, _wcslen, wcscpy_s> WString;

template<class T>
void Flatten(__out_ecount(len) T *data, unsigned int len)
{
    for (unsigned int i = 0; i < len; ++i)
        if (data[i] < 32 || (data[i] > 126 && data[i] <= 255))
            data[i] = '.';
    data[len] = 0;
}

void Flatten(__out_ecount(len) char *data, unsigned int len);

/* Formats for the Format class.  We support the following formats:
 *      Pointer - Same as %p.
 *      Hex - Same as %x (same as %p, but does not output preceding zeros.
 *      PrefixHex - Same as %x, but prepends 0x.
 *      Decimal - Same as %d.
 * Strings and wide strings don't use this.
 */
class Formats
{
public:
    enum Format
    {
        Default,
        Pointer,
        Hex,
        PrefixHex,
        Decimal,
    };
};

enum Alignment
{
    AlignLeft,
    AlignRight
};

namespace Output
{
    /* Defines how a value will be printed.  This class understands how to format
     * and print values according to the format and DML settings provided.
     * The raw templated class handles the pointer/integer case.  Support for
     * character arrays and wide character arrays are handled by template
     * specializations.
     *
     * Note that this class is not used directly.  Instead use the typedefs and
     * macros which define the type of data you are outputing (for example ObjectPtr,
     * MethodTablePtr, etc).
     */
    template <class T>
    class Format
    {
    public:
        Format(T value)
            : mValue(value), mFormat(Formats::Default), mDml(Output::DML_None)
        {
        }

        Format(T value, Formats::Format format, Output::FormatType dmlType)
            : mValue(value), mFormat(format), mDml(dmlType)
        {
        }

        Format(const Format<T> &rhs)
            : mValue(rhs.mValue), mFormat(rhs.mFormat), mDml(rhs.mDml)
        {
        }

        /* Prints out the value according to the Format and DML settings provided.
         */
        void Output() const
        {
            if (IsDMLEnabled() && mDml != Output::DML_None)
            {
                const int len = GetDMLWidth(mDml);
                char *buffer = (char*)alloca(len);

                BuildDML(buffer, len, (CLRDATA_ADDRESS)mValue, mFormat, mDml);
                DMLOut(buffer);
            }
            else
            {
                if (mFormat == Formats::Default || mFormat == Formats::Pointer)
                {
                    ExtOut("%p", SOS_PTR(mValue));
                }
                else
                {
                    const char *format = NULL;
                    if (mFormat == Formats::PrefixHex)
                    {
                        format = "0x%x";
                    }
                    else if (mFormat == Formats::Hex)
                    {
                        format = "%x";
                    }
                    else if (mFormat == Formats::Decimal)
                    {
                        format = "%d";
                    }

                    ExtOut(format, (__int32)mValue);
                }

            }
        }

        /* Prints out the value based on a specified width and alignment.
         * Params:
         *   align - Whether the output should be left or right justified.
         *   width - The output width to fill.
         * Note:
         *   This function guarantees that exactly width will be printed out (so if width is 24,
         *   exactly 24 characters will be printed), even if the output wouldn't normally fit
         *   in the space provided.  This function makes no guarantees as to what part of the
         *   data will be printed in the case that width isn't wide enough.
         */
        void OutputColumn(Alignment align, int width) const
        {
            bool leftAlign = align == AlignLeft;
            if (IsDMLEnabled() && mDml != Output::DML_None)
            {
                const int len = GetDMLColWidth(mDml, width);
                char *buffer = (char*)alloca(len);

                BuildDMLCol(buffer, len, (CLRDATA_ADDRESS)mValue, mFormat, mDml, leftAlign, width);
                DMLOut(buffer);
            }
            else
            {
                int precision = GetPrecision();
                if (mFormat == Formats::Default || mFormat == Formats::Pointer)
                {
                    if (precision > width)
                        precision = width;

                    ExtOut(leftAlign ? "%-*.*p" : "%*.*p", width, precision, SOS_PTR(mValue));
                }
                else
                {
                    const char *format = NULL;
                    if (mFormat == Formats::PrefixHex)
                    {
                        format = leftAlign ? "0x%-*.*x" : "0x%*.*x";
                        width -= 2;
                    }
                    else if (mFormat == Formats::Hex)
                    {
                        format = leftAlign ? "%-*.*x" : "%*.*x";
                    }
                    else if (mFormat == Formats::Decimal)
                    {
                        format = leftAlign ? "%-*.*d" : "%*.*d";
                    }

                    if (precision > width)
                        precision = width;

                    ExtOut(format, width, precision, (__int32)mValue);
                }
            }
        }

        /* Converts this object into a Wide char string.  This allows you to write the following code:
         *    WString foo = L"bar " + ObjectPtr(obj);
         * Where ObjectPtr is a subclass/typedef of this Format class.
         */
        operator WString() const
        {
            String str = *this;
            const char *cstr = (const char *)str;

            int len = MultiByteToWideChar(CP_ACP, 0, cstr, -1, NULL, 0);
            WCHAR *buffer = (WCHAR *)alloca(len*sizeof(WCHAR));

            MultiByteToWideChar(CP_ACP, 0, cstr, -1, buffer, len);

            return WString(buffer);
        }

        /* Converts this object into a String object.  This allows you to write the following code:
         *    String foo = "bar " + ObjectPtr(obj);
         * Where ObjectPtr is a subclass/typedef of this Format class.
         */
        operator String() const
        {
            if (IsDMLEnabled() && mDml != Output::DML_None)
            {
                const int len = GetDMLColWidth(mDml, 0);
                char *buffer = (char*)alloca(len);

                BuildDMLCol(buffer, len, (CLRDATA_ADDRESS)mValue, mFormat, mDml, false, 0);
                return buffer;
            }
            else
            {
                char buffer[64];
                if (mFormat == Formats::Default || mFormat == Formats::Pointer)
                {
                    sprintf_s(buffer, ARRAY_SIZE(buffer), "%p", (int *)(SIZE_T)mValue);
                    ConvertToLower(buffer, ARRAY_SIZE(buffer));
                }
                else
                {
                    const char *format = NULL;
                    if (mFormat == Formats::PrefixHex)
                        format = "0x%x";
                    else if (mFormat == Formats::Hex)
                        format = "%x";
                    else if (mFormat == Formats::Decimal)
                        format = "%d";

                    sprintf_s(buffer, ARRAY_SIZE(buffer), format, (__int32)mValue);
                    ConvertToLower(buffer, ARRAY_SIZE(buffer));
                }

                return buffer;
            }
        }

    private:
        int GetPrecision() const
        {
            if (mFormat == Formats::Hex || mFormat == Formats::PrefixHex)
            {
                ULONGLONG val = mValue;
                int count = 0;
                while (val)
                {
                    val >>= 4;
                    count++;
                }

                if (count == 0)
                    count = 1;

                return count;
            }

            else if (mFormat == Formats::Decimal)
            {
                T val = mValue;
                int count = (val > 0) ? 0 : 1;
                while (val)
                {
                    val /= 10;
                    count++;
                }

                return count;
            }

            // mFormat == Formats::Pointer
            return sizeof(int*)*2;
        }

        static inline void BuildDML(__out_ecount(len) char *result, int len, CLRDATA_ADDRESS value, Formats::Format format, Output::FormatType dmlType)
        {
            BuildDMLCol(result, len, value, format, dmlType, true, 0);
        }

        static int GetDMLWidth(Output::FormatType dmlType)
        {
            return GetDMLColWidth(dmlType, 0);
        }

        static void BuildDMLCol(__out_ecount(len) char *result, int len, CLRDATA_ADDRESS value, Formats::Format format, Output::FormatType dmlType, bool leftAlign, int width)
        {
            char hex[64];
            int count = GetHex(value, hex, ARRAY_SIZE(hex), format != Formats::Hex);
            int i = 0;

            if (!leftAlign)
            {
                for (; i < width - count; ++i)
                    result[i] = ' ';

                result[i] = 0;
            }

            int written = sprintf_s(result+i, len - i, DMLFormats[dmlType], hex, hex);

            SOS_Assert(written != -1);
            if (written != -1)
            {
                for (i = i + written; i < width; ++i)
                    result[i] = ' ';

                result[i] = 0;
            }
        }

        static int GetDMLColWidth(Output::FormatType dmlType, int width)
        {
            return 1 + 4*sizeof(int*) + (int)strlen(DMLFormats[dmlType]) + width;
        }

    private:
        T mValue;
        Formats::Format mFormat;
        Output::FormatType mDml;
     };

     /* Format class used for strings.
      */
    template <>
    class Format<const char *>
    {
    public:
        Format(const char *value)
            : mValue(value)
        {
        }

        Format(const Format<const char *> &rhs)
            : mValue(rhs.mValue)
        {
        }

        void Output() const
        {
            if (IsDMLEnabled())
                DMLOut("%s", mValue);
            else
                ExtOut("%s", mValue);
        }

        void OutputColumn(Alignment align, int width) const
        {
            int precision = (int)strlen(mValue);

            if (precision > width)
                precision = width;

            const char *format = align == AlignLeft ? "%-*.*s" : "%*.*s";

            if (IsDMLEnabled())
                DMLOut(format, width, precision, mValue);
            else
                ExtOut(format, width, precision, mValue);
        }

    private:
        const char *mValue;
    };

    /* Format class for wide char strings.
     */
    template <>
    class Format<const WCHAR *>
    {
    public:
        Format(const WCHAR *value)
            : mValue(value)
        {
        }

        Format(const Format<const WCHAR *> &rhs)
            : mValue(rhs.mValue)
        {
        }

        void Output() const
        {
            if (IsDMLEnabled())
                DMLOut("%S", mValue);
            else
                ExtOut("%S", mValue);
        }

        void OutputColumn(Alignment align, int width) const
        {
            int precision = (int)_wcslen(mValue);
            if (precision > width)
                precision = width;

            const char *format = align == AlignLeft ? "%-*.*S" : "%*.*S";

            if (IsDMLEnabled())
                DMLOut(format, width, precision, mValue);
            else
                ExtOut(format, width, precision, mValue);
        }

    private:
        const WCHAR *mValue;
    };


    template <class T>
    void InternalPrint(const T &t)
    {
        Format<T>(t).Output();
    }

    template <class T>
    void InternalPrint(const Format<T> &t)
    {
        t.Output();
    }

    inline void InternalPrint(const char t[])
    {
        Format<const char *>(t).Output();
    }
}

#define DefineFormatClass(name, format, dml) \
    template <class T>                       \
    Output::Format<T> name(T value)          \
    { return Output::Format<T>(value, format, dml); }

DefineFormatClass(EEClassPtr, Formats::Pointer, Output::DML_EEClass);
DefineFormatClass(ObjectPtr, Formats::Pointer, Output::DML_Object);
DefineFormatClass(ExceptionPtr, Formats::Pointer, Output::DML_PrintException);
DefineFormatClass(ModulePtr, Formats::Pointer, Output::DML_Module);
DefineFormatClass(MethodDescPtr, Formats::Pointer, Output::DML_MethodDesc);
DefineFormatClass(AppDomainPtr, Formats::Pointer, Output::DML_Domain);
DefineFormatClass(ThreadState, Formats::Hex, Output::DML_ThreadState);
DefineFormatClass(ThreadID, Formats::Hex, Output::DML_ThreadID);
DefineFormatClass(RCWrapper, Formats::Pointer, Output::DML_RCWrapper);
DefineFormatClass(CCWrapper, Formats::Pointer, Output::DML_CCWrapper);
DefineFormatClass(InstructionPtr, Formats::Pointer, Output::DML_IP);
DefineFormatClass(ILPtr, Formats::Pointer, Output::DML_IL);
DefineFormatClass(NativePtr, Formats::Pointer, Output::DML_None);

DefineFormatClass(Decimal, Formats::Decimal, Output::DML_None);
DefineFormatClass(Pointer, Formats::Pointer, Output::DML_None);
DefineFormatClass(PrefixHex, Formats::PrefixHex, Output::DML_None);
DefineFormatClass(Hex, Formats::Hex, Output::DML_None);

#undef DefineFormatClass

template <class T0>
void Print(const T0 &val0)
{
    Output::InternalPrint(val0);
}

template <class T0, class T1>
void Print(const T0 &val0, const T1 &val1)
{
    Output::InternalPrint(val0);
    Output::InternalPrint(val1);
}

template <class T0>
void PrintLn(const T0 &val0)
{
    Output::InternalPrint(val0);
    ExtOut("\n");
}

template <class T0, class T1>
void PrintLn(const T0 &val0, const T1 &val1)
{
    Output::InternalPrint(val0);
    Output::InternalPrint(val1);
    ExtOut("\n");
}

template <class T0, class T1, class T2>
void PrintLn(const T0 &val0, const T1 &val1, const T2 &val2)
{
    Output::InternalPrint(val0);
    Output::InternalPrint(val1);
    Output::InternalPrint(val2);
    ExtOut("\n");
}


/* This class handles the formatting for output which is in a table format.  To use this class you define
 * how the table is formatted by setting the number of columns in the table, the default column width,
 * the default column alignment, the indentation (whitespace) for the table, and the amount of padding
 * (whitespace) between each column. Once this has been setup, you output rows at a time or individual
 * columns to build the output instead of manually tabbing out space.
 * Also note that this class was built to work with the Format class.  When outputing data, use the
 * predefined output types to specify the format (such as ObjectPtr, MethodDescPtr, Decimal, etc).  This
 * tells the TableOutput class how to display the data, and where applicable, it automatically generates
 * the appropriate DML output.  See the DefineFormatClass macro.
 */
class TableOutput
{
public:

    TableOutput()
        : mColumns(0), mDefaultWidth(0), mIndent(0), mPadding(0), mCurrCol(0), mDefaultAlign(AlignLeft),
          mWidths(0), mAlignments(0)
      {
      }
    /* Constructor.
     * Params:
     *   numColumns - the number of columns the table has
     *   defaultColumnWidth - the default width of each column
     *   alignmentDefault - whether columns are by default left aligned or right aligned
     *   indent - the amount of whitespace to prefix at the start of the row (in characters)
     *   padding - the amount of whitespace to place between each column (in characters)
     */
    TableOutput(int numColumns, int defaultColumnWidth, Alignment alignmentDefault = AlignLeft, int indent = 0, int padding = 1)
        : mColumns(numColumns), mDefaultWidth(defaultColumnWidth), mIndent(indent), mPadding(padding), mCurrCol(0), mDefaultAlign(alignmentDefault),
          mWidths(0), mAlignments(0)
    {
    }

    ~TableOutput()
    {
        Clear();
    }

    /* See the documentation for the constructor.
     */
    void ReInit(int numColumns, int defaultColumnWidth, Alignment alignmentDefault = AlignLeft, int indent = 0, int padding = 1);

    /* Sets the amount of whitespace to prefix at the start of the row (in characters).
     */
    void SetIndent(int indent)
    {
        SOS_Assert(indent >= 0);

        mIndent = indent;
    }

    /* Sets the exact widths for the the given columns.
     * Params:
     *   columns - the number of columns you are providing the width for, starting at the first column
     *   ... - an int32 for each column (given by the number of columns in the first parameter).
     * Example:
     *    If you have 5 columns in the table, you can set their widths like so:
     *       tableOutput.SetWidths(5, 2, 3, 5, 7, 13);
     * Note:
     *    It's fine to pass a value for "columns" less than the number of columns in the table.  This
     *    is useful when you set the default column width to be correct for most of the table, and need
     *    to make a minor adjustment to a few.
     */
    void SetWidths(int columns, ...);

    /* Individually sets a column to the given width.
     * Params:
     *   col - the column to set, 0 indexed
     *   width - the width of the column (note this must be non-negative)
     */
    void SetColWidth(int col, int width);

    /* Individually sets the column alignment.
     * Params:
     *   col - the column to set, 0 indexed
     *   align - the new alignment (left or right) for the column
     */
    void SetColAlignment(int col, Alignment align);


    /* The WriteRow family of functions allows you to write an entire row of the table at once.
     * The common use case for the TableOutput class is to individually output each column after
     * calculating what the value should contain.  However, this would be tedious if you already
     * knew the contents of the entire row which usually happenes when you are printing out the
     * header for the table.  To use this, simply pass each column as an individual parameter,
     * for example:
     *    tableOutput.WriteRow("First Column", "Second Column", Decimal(3), PrefixHex(4), "Fifth Column");
     */
    template <class T0, class T1>
    void WriteRow(T0 t0, T1 t1)
    {
        WriteColumn(0, t0);
        WriteColumn(1, t1);
    }

    template <class T0, class T1, class T2>
    void WriteRow(T0 t0, T1 t1, T2 t2)
    {
        WriteColumn(0, t0);
        WriteColumn(1, t1);
        WriteColumn(2, t2);
    }


    template <class T0, class T1, class T2, class T3>
    void WriteRow(T0 t0, T1 t1, T2 t2, T3 t3)
    {
        WriteColumn(0, t0);
        WriteColumn(1, t1);
        WriteColumn(2, t2);
        WriteColumn(3, t3);
    }


    template <class T0, class T1, class T2, class T3, class T4>
    void WriteRow(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4)
    {
        WriteColumn(0, t0);
        WriteColumn(1, t1);
        WriteColumn(2, t2);
        WriteColumn(3, t3);
        WriteColumn(4, t4);
    }

    template <class T0, class T1, class T2, class T3, class T4, class T5>
    void WriteRow(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5)
    {
        WriteColumn(0, t0);
        WriteColumn(1, t1);
        WriteColumn(2, t2);
        WriteColumn(3, t3);
        WriteColumn(4, t4);
        WriteColumn(5, t5);
    }

    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
    void WriteRow(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9)
    {
        WriteColumn(0, t0);
        WriteColumn(1, t1);
        WriteColumn(2, t2);
        WriteColumn(3, t3);
        WriteColumn(4, t4);
        WriteColumn(5, t5);
        WriteColumn(6, t6);
        WriteColumn(7, t7);
        WriteColumn(8, t8);
        WriteColumn(9, t9);
    }

    /* The WriteColumn family of functions is used to output individual columns in the table.
     * The intent is that the bulk of the table will be generated in a loop like so:
     *   while (condition) {
     *      int value1 = CalculateFirstColumn();
     *      table.WriteColumn(0, value1);
     *
     *      String value2 = CalculateSecondColumn();
     *      table.WriteColumn(1, value2);
     *   }
     * Params:
     *   col - the column to write, 0 indexed
     *   t - the value to write
     * Note:
     *   You should generally use the specific instances of the Format class to generate output.
     *   For example, use the "Decimal", "Pointer", "ObjectPtr", etc.  When passing data to this
     *   function.  This tells the Table class how to display the value.
     */
    template <class T>
    void WriteColumn(int col, const Output::Format<T> &t)
    {
        SOS_Assert(col >= 0);
        SOS_Assert(col < mColumns);

        if (col != mCurrCol)
            OutputBlankColumns(col);

        if (col == 0)
            OutputIndent();

        bool lastCol = col == mColumns - 1;

        if (!lastCol)
            t.OutputColumn(GetColAlign(col), GetColumnWidth(col));
        else
            t.Output();

        ExtOut(lastCol ? "\n" : GetWhitespace(mPadding));

        if (lastCol)
            mCurrCol = 0;
        else
            mCurrCol = col+1;
    }

    template <class T>
    void WriteColumn(int col, T t)
    {
        WriteColumn(col, Output::Format<T>(t));
    }

    void WriteColumn(int col, const String &str)
    {
        WriteColumn(col, Output::Format<const char *>(str));
    }

    void WriteColumn(int col, const WString &str)
    {
        WriteColumn(col, Output::Format<const WCHAR *>(str));
    }

    void WriteColumn(int col, __in_z WCHAR *str)
    {
        WriteColumn(col, Output::Format<const WCHAR *>(str));
    }

    void WriteColumn(int col, const WCHAR *str)
    {
        WriteColumn(col, Output::Format<const WCHAR *>(str));
    }

    inline void WriteColumn(int col, __in_z char *str)
    {
        WriteColumn(col, Output::Format<const char *>(str));
    }

    /* Writes a column using a printf style format.  You cannot use the Format class with
     * this function to specify how the output should look, use printf style formatting
     * with the appropriate parameters instead.
     */
    void WriteColumnFormat(int col, const char *fmt, ...)
    {
        SOS_Assert(strstr(fmt, "%S") == NULL);

        char result[128];

        va_list list;
        va_start(list, fmt);
        vsprintf_s(result, ARRAY_SIZE(result), fmt, list);
        va_end(list);

        WriteColumn(col, result);
    }

    void WriteColumnFormat(int col, const WCHAR *fmt, ...)
    {
        WCHAR result[128];

        va_list list;
        va_start(list, fmt);
        vswprintf_s(result, ARRAY_SIZE(result), fmt, list);
        va_end(list);

        WriteColumn(col, result);
    }

    /* This function is a shortcut for writing the next column.  (That is, the one after the
     * one you just wrote.)
     */
    template <class T>
    void WriteColumn(T t)
    {
        WriteColumn(mCurrCol, t);
    }

private:
    void Clear();
    void AllocWidths();
    int GetColumnWidth(int col);
    Alignment GetColAlign(int col);
    const char *GetWhitespace(int amount);
    void OutputBlankColumns(int col);
    void OutputIndent();

private:
    int mColumns, mDefaultWidth, mIndent, mPadding, mCurrCol;
    Alignment mDefaultAlign;
    int *mWidths;
    Alignment *mAlignments;
};

#ifndef FEATURE_PAL
HRESULT GetClrModuleImages(__in IXCLRDataModule* module, __in CLRDataModuleExtentType desiredType, __out PULONG64 pBase, __out PULONG64 pSize);
#endif
HRESULT GetMethodDefinitionsFromName(DWORD_PTR ModulePtr, IXCLRDataModule* mod, const char* name, IXCLRDataMethodDefinition **ppMethodDefinitions, int numMethods, int *numMethodsNeeded);
HRESULT GetMethodDescsFromName(DWORD_PTR ModulePtr, IXCLRDataModule* mod, const char* name, DWORD_PTR **pOut, int *numMethodDescs);

HRESULT FileNameForModule (const DacpModuleData * const pModule, __out_ecount (MAX_LONGPATH) WCHAR *fileName);
HRESULT FileNameForModule (DWORD_PTR pModuleAddr, __out_ecount (MAX_LONGPATH) WCHAR *fileName);
void IP2MethodDesc (DWORD_PTR IP, DWORD_PTR &methodDesc, JITTypes &jitType,
                    DWORD_PTR &gcinfoAddr);
const char *ElementTypeName (unsigned type);
void DisplayFields (CLRDATA_ADDRESS cdaMT, DacpMethodTableData *pMTD, DacpMethodTableFieldData *pMTFD,
                    DWORD_PTR dwStartAddr = 0, BOOL bFirst=TRUE, BOOL bValueClass=FALSE);
int GetObjFieldOffset(CLRDATA_ADDRESS cdaObj, __in_z LPCWSTR wszFieldName, BOOL bFirst=TRUE);
int GetObjFieldOffset(CLRDATA_ADDRESS cdaObj, CLRDATA_ADDRESS cdaMT, __in_z LPCWSTR wszFieldName, BOOL bFirst=TRUE, DacpFieldDescData* pDacpFieldDescData=NULL);
int GetValueFieldOffset(CLRDATA_ADDRESS cdaMT, __in_z LPCWSTR wszFieldName, DacpFieldDescData* pDacpFieldDescData=NULL);

BOOL IsValidToken(DWORD_PTR ModuleAddr, mdTypeDef mb);
void NameForToken_s(DacpModuleData *pModule, mdTypeDef mb, __out_ecount (capacity_mdName) WCHAR *mdName, size_t capacity_mdName,
                  bool bClassName=true);
void NameForToken_s(DWORD_PTR ModuleAddr, mdTypeDef mb, __out_ecount (capacity_mdName) WCHAR *mdName, size_t capacity_mdName,
                  bool bClassName=true);
HRESULT NameForToken_s(mdTypeDef mb, IMetaDataImport *pImport, __out_ecount (capacity_mdName) WCHAR *mdName,  size_t capacity_mdName,
                     bool bClassName);

void vmmap();
void vmstat();

#ifndef FEATURE_PAL
///////////////////////////////////////////////////////////////////////////////////////////////////
// Support for managed stack tracing
//

DWORD_PTR GetDebuggerJitInfo(DWORD_PTR md);

///////////////////////////////////////////////////////////////////////////////////////////////////
#endif // FEATURE_PAL

template <typename SCALAR>
inline
int bitidx(SCALAR bitflag)
{
    for (int idx = 0; idx < static_cast<int>(sizeof(bitflag))*8; ++idx)
    {
        if (bitflag & (1 << idx))
        {
            _ASSERTE((bitflag & (~(1 << idx))) == 0);
            return idx;
        }
    }
    return -1;
}

#ifndef FEATURE_PAL
HRESULT
DllsName(
    ULONG_PTR addrContaining,
    __out_ecount (MAX_LONGPATH) WCHAR *dllName
    );
#endif

inline
BOOL IsElementValueType (CorElementType cet)
{
    return (cet >= ELEMENT_TYPE_BOOLEAN && cet <= ELEMENT_TYPE_R8)
        || cet == ELEMENT_TYPE_VALUETYPE || cet == ELEMENT_TYPE_I || cet == ELEMENT_TYPE_U;
}


#define safemove(dst, src) \
SafeReadMemory (TO_TADDR(src), &(dst), sizeof(dst), NULL)

extern "C" PDEBUG_DATA_SPACES g_ExtData;

#include "arrayholder.h"

// This class acts a smart pointer which calls the Release method on any object
// you place in it when the ToRelease class falls out of scope.  You may use it
// just like you would a standard pointer to a COM object (including if (foo),
// if (!foo), if (foo == 0), etc) except for two caveats:
//     1. This class never calls AddRef and it always calls Release when it
//        goes out of scope.
//     2. You should never use & to try to get a pointer to a pointer unless
//        you call Release first, or you will leak whatever this object contains
//        prior to updating its internal pointer.
template<class T>
class ToRelease
{
public:
    ToRelease()
        : m_ptr(NULL)
    {}

    ToRelease(T* ptr)
        : m_ptr(ptr)
    {}

    ~ToRelease()
    {
        Release();
    }

    void operator=(T *ptr)
    {
        Release();

        m_ptr = ptr;
    }

    T* operator->()
    {
        return m_ptr;
    }

    operator T*()
    {
        return m_ptr;
    }

    T** operator&()
    {
        return &m_ptr;
    }

    T* GetPtr() const
    {
        return m_ptr;
    }

    T* Detach()
    {
        T* pT = m_ptr;
        m_ptr = NULL;
        return pT;
    }

    void Release()
    {
        if (m_ptr != NULL)
        {
            m_ptr->Release();
            m_ptr = NULL;
        }
    }

private:
    T* m_ptr;
};

BOOL InitializeHeapData();
BOOL IsServerBuild ();
UINT GetMaxGeneration();
UINT GetGcHeapCount();
BOOL GetGcStructuresValid();

void DisassembleToken(IMetaDataImport* i, DWORD token);
ULONG GetILSize(DWORD_PTR ilAddr); // REturns 0 if error occurs
HRESULT DecodeILFromAddress(IMetaDataImport *pImport, TADDR ilAddr);
void DecodeIL(IMetaDataImport *pImport, BYTE *buffer, ULONG bufSize);
void DecodeDynamicIL(BYTE *data, ULONG Size, DacpObjectData& tokenArray);
ULONG DisplayILOperation(const UINT indentCount, BYTE* pBuffer, ULONG position, std::function<void(DWORD)>& func);

bool IsRuntimeVersion(DWORD major);
bool IsRuntimeVersion(VS_FIXEDFILEINFO& fileInfo, DWORD major);
bool IsRuntimeVersionAtLeast(DWORD major);
bool IsRuntimeVersionAtLeast(VS_FIXEDFILEINFO& fileInfo, DWORD major);
bool CheckBreakingRuntimeChange(int* pVersion = nullptr);
#ifndef FEATURE_PAL
BOOL IsRetailBuild (size_t base);
BOOL GetSOSVersion(VS_FIXEDFILEINFO *pFileInfo);
#endif

BOOL IsDumpFile ();
const WCHAR GetTargetDirectorySeparatorW();

// IsMiniDumpFile will return true if 1) we are in
// a small format minidump, and g_InMinidumpSafeMode is true.
extern BOOL g_InMinidumpSafeMode;

BOOL IsMiniDumpFile();
void ReportOOM();

BOOL SafeReadMemory (TADDR offset, PVOID lpBuffer, ULONG cb, PULONG lpcbBytesRead);
BOOL NameForMD_s (DWORD_PTR pMD, __out_ecount (capacity_mdName) WCHAR *mdName, size_t capacity_mdName);
BOOL NameForMT_s (DWORD_PTR MTAddr, __out_ecount (capacity_mdName) WCHAR *mdName, size_t capacity_mdName);

WCHAR *CreateMethodTableName(TADDR mt, TADDR cmt = (TADDR)0);

void isRetAddr(DWORD_PTR retAddr, DWORD_PTR* whereCalled);
DWORD_PTR GetValueFromExpression (___in __in_z const char *const str);
void LoadRuntimeSymbols();

void DomainInfo(DacpAppDomainData* pDomain);
void AssemblyInfo(DacpAssemblyData* pAssembly);

size_t GetNumComponents(TADDR obj);

extern DacpUsefulGlobalsData g_special_usefulGlobals;

class HeapStat
{
protected:
    struct Node
    {
        DWORD_PTR data;
        DWORD count;
        size_t totalSize;
        Node* left;
        Node* right;
        Node ()
            : data(0), count(0), totalSize(0), left(NULL), right(NULL)
        {
        }
    };
    BOOL bHasStrings;
    Node *head;
    BOOL fLinear;
public:
    HeapStat ()
        : bHasStrings(FALSE), head(NULL), fLinear(FALSE)
    {}
    ~HeapStat()
    {
        Delete();
    }
    // TODO: Change the aSize argument to size_t when we start supporting
    // TODO: object sizes above 4GB
    void Add (DWORD_PTR aData, DWORD aSize);
    void Sort ();
    void Print (const char* label = NULL);
    void Delete ();
    void HasStrings(BOOL abHasStrings)
        {
            bHasStrings = abHasStrings;
        }
private:
    int CompareData(DWORD_PTR n1, DWORD_PTR n2);
    void SortAdd (Node *&root, Node *entry);
    void LinearAdd (Node *&root, Node *entry);
    void ReverseLeftMost (Node *root);
    void Linearize();
};

class CGCDesc;

// The information MethodTableCache returns.
struct MethodTableInfo
{
    bool IsInitialized()       { return BaseSize != 0; }

    DWORD BaseSize;           // Caching BaseSize and ComponentSize for a MethodTable
    DWORD ComponentSize;      // here has HUGE perf benefits in heap traversals.
    BOOL  bContainsPointers;
    BOOL  bCollectible;
    DWORD_PTR* GCInfoBuffer;  // Start of memory of GC info
    CGCDesc* GCInfo;    // Just past GC info (which is how it is stored)
    bool  ArrayOfVC;
    TADDR LoaderAllocatorObjectHandle;
};

class MethodTableCache
{
protected:

    struct Node
    {
        DWORD_PTR data;            // This is the key (the method table pointer)
        MethodTableInfo info;  // The info associated with this MethodTable
        Node* left;
        Node* right;
        Node (DWORD_PTR aData) : data(aData), left(NULL), right(NULL)
        {
            info.BaseSize = 0;
            info.ComponentSize = 0;
            info.bContainsPointers = false;
            info.bCollectible = false;
            info.GCInfo = NULL;
            info.ArrayOfVC = false;
            info.GCInfoBuffer = NULL;
            info.LoaderAllocatorObjectHandle = (TADDR)0;
        }
    };
    Node *head;
public:
    MethodTableCache ()
        : head(NULL)
    {}
    ~MethodTableCache() { Clear(); }

    // Always succeeds, if it is not present it adds an empty Info struct and returns that
    // Thus you must call 'IsInitialized' on the returned value before using it
    MethodTableInfo* Lookup(DWORD_PTR aData);

    void Clear ();
private:
    int CompareData(DWORD_PTR n1, DWORD_PTR n2);
    void ReverseLeftMost (Node *root);
};

extern MethodTableCache g_special_mtCache;

struct DumpArrayFlags
{
    DWORD_PTR startIndex;
    DWORD_PTR Length;
    BOOL bDetail;
    LPSTR strObject;
    BOOL bNoFieldsForElement;

    DumpArrayFlags ()
        : startIndex(0), Length((DWORD_PTR)-1), bDetail(FALSE), strObject (0), bNoFieldsForElement(FALSE)
    {}
    ~DumpArrayFlags ()
    {
        if (strObject)
            delete [] strObject;
    }
}; //DumpArrayFlags



// -----------------------------------------------------------------------

#define BIT_SBLK_IS_HASH_OR_SYNCBLKINDEX    0x08000000
#define BIT_SBLK_FINALIZER_RUN              0x40000000
#define BIT_SBLK_SPIN_LOCK                  0x10000000
#define SBLK_MASK_LOCK_THREADID             0x000003FF   // special value of 0 + 1023 thread ids
#define SBLK_MASK_LOCK_RECLEVEL             0x0000FC00   // 64 recursion levels
#define SBLK_APPDOMAIN_SHIFT                16           // shift right this much to get appdomain index
#define SBLK_MASK_APPDOMAININDEX            0x000007FF   // 2048 appdomain indices
#define SBLK_RECLEVEL_SHIFT                 10           // shift right this much to get recursion level
#define BIT_SBLK_IS_HASHCODE            0x04000000
#define MASK_HASHCODE                   ((1<<HASHCODE_BITS)-1)
#define SYNCBLOCKINDEX_BITS             26
#define MASK_SYNCBLOCKINDEX             ((1<<SYNCBLOCKINDEX_BITS)-1)

HRESULT GetMTOfObject(TADDR obj, TADDR *mt);

struct GCHandleStatistics
{
    HeapStat hs;

    DWORD strongHandleCount;
    DWORD pinnedHandleCount;
    DWORD asyncPinnedHandleCount;
    DWORD refCntHandleCount;
    DWORD weakLongHandleCount;
    DWORD weakShortHandleCount;
    DWORD variableCount;
    DWORD sizedRefCount;
    DWORD dependentCount;
    DWORD weakWinRTHandleCount;
    DWORD weakInteriorPointerHandleCount;
    DWORD unknownHandleCount;
    GCHandleStatistics()
        : strongHandleCount(0), pinnedHandleCount(0), asyncPinnedHandleCount(0), refCntHandleCount(0),
          weakLongHandleCount(0), weakShortHandleCount(0), variableCount(0), sizedRefCount(0),
          dependentCount(0), weakWinRTHandleCount(0), weakInteriorPointerHandleCount(0), unknownHandleCount(0)
    {}
    ~GCHandleStatistics()
    {
        hs.Delete();
    }
};

BOOL IsSameModuleName (const char *str1, const char *str2);
BOOL IsModule (DWORD_PTR moduleAddr);
BOOL IsMethodDesc (DWORD_PTR value);
BOOL IsMethodTable (DWORD_PTR value);
BOOL IsStringObject (size_t obj);
BOOL IsObjectArray (DWORD_PTR objPointer);
BOOL IsObjectArray (DacpObjectData *pData);
BOOL IsDerivedFrom(CLRDATA_ADDRESS mtObj, __in_z LPCWSTR baseString);
BOOL IsDerivedFrom(CLRDATA_ADDRESS mtObj, DWORD_PTR modulePtr, mdTypeDef typeDef);
BOOL TryGetMethodDescriptorForDelegate(CLRDATA_ADDRESS delegateAddr, CLRDATA_ADDRESS* pMD);

#ifdef FEATURE_PAL
void FlushMetadataRegions();
HRESULT GetMetadataMemory(CLRDATA_ADDRESS address, ULONG32 bufferSize, BYTE* buffer);
#endif

/* Returns a list of all modules in the process.
 * Params:
 *      name - The name of the module you would like.  If mName is NULL the all modules are returned.
 *      numModules - The number of modules in the array returned.
 * Returns:
 *      An array of modules whose length is *numModules, NULL if an error occurred.  Note that if this
 *      function succeeds but finds no modules matching the name given, this function returns a valid
 *      array, but *numModules will equal 0.
 * Note:
 *      You must clean up the return value of this array by calling delete [] on it, or using the
 *      ArrayHolder class.
 */
DWORD_PTR *ModuleFromName(__in_opt LPSTR name, int *numModules);
HRESULT GetModuleFromAddress(___in CLRDATA_ADDRESS peAddress, ___out IXCLRDataModule** ppModule);
void GetInfoFromName(DWORD_PTR ModuleAddr, const char* name, mdTypeDef* retMdTypeDef=NULL);
void GetInfoFromModule (DWORD_PTR ModuleAddr, ULONG token, DWORD_PTR *ret=NULL);

/////////////////////////////////////////////////////////////////////////////////////////////////////////

struct strobjInfo
{
    size_t  methodTable;
    DWORD   m_StringLength;
};

CLRDATA_ADDRESS GetAppDomainForMT(CLRDATA_ADDRESS mtPtr);
CLRDATA_ADDRESS GetAppDomain(CLRDATA_ADDRESS objPtr);

BOOL IsMTForFreeObj(DWORD_PTR pMT);

enum ARGTYPE {COBOOL,COSIZE_T,COHEX,COSTRING};
struct CMDOption
{
    const char* name;
    void *vptr;
    ARGTYPE type;
    BOOL hasValue;
    BOOL hasSeen;
};
struct CMDValue
{
    void *vptr;
    ARGTYPE type;
};
BOOL GetCMDOption(const char *string, CMDOption *option, size_t nOption,
                  CMDValue *arg, size_t maxArg, size_t *nArg);

void DumpMDInfo(DWORD_PTR dwStartAddr, CLRDATA_ADDRESS dwRequestedIP = 0, BOOL fStackTraceFormat = FALSE);
void DumpMDInfoFromMethodDescData(DacpMethodDescData * pMethodDescData, BOOL fStackTraceFormat);
void GetDomainList(DWORD_PTR *&domainList, int &numDomain);
HRESULT GetThreadList(DWORD_PTR **threadList, int *numThread);
CLRDATA_ADDRESS GetCurrentManagedThread(); // returns current managed thread if any

void ReloadSymbolWithLineInfo();

size_t FunctionType (size_t EIP);

size_t Align (size_t nbytes);
// Aligns large objects
size_t AlignLarge (size_t nbytes);

ULONG OSPageSize ();
size_t NextOSPageAddress (size_t addr);

// This version of objectsize reduces the lookup of methodtables in the DAC.
// It uses g_special_mtCache for it's work.
BOOL GetSizeEfficient(DWORD_PTR dwAddrCurrObj,
    DWORD_PTR dwAddrMethTable, BOOL bLarge, size_t& s, BOOL& bContainsPointers);

BOOL GetCollectibleDataEfficient(DWORD_PTR dwAddrMethTable, BOOL& bCollectible, TADDR& loaderAllocatorObjectHandle);

void CharArrayContent(TADDR pos, ULONG num, bool widechar);
void StringObjectContent (size_t obj, BOOL fLiteral=FALSE, const int length=-1);  // length=-1: dump everything in the string object.

UINT FindAllPinnedAndStrong (DWORD_PTR handlearray[],UINT arraySize);

const char *EHTypeName(EHClauseType et);

struct StringHolder
{
    LPSTR data;
    StringHolder() : data(NULL) { }
    ~StringHolder() { if(data) delete [] data; }
};


ULONG DebuggeeType();

inline BOOL IsKernelDebugger ()
{
    return DebuggeeType() == DEBUG_CLASS_KERNEL;
}

void    ResetGlobals(void);
HRESULT LoadClrDebugDll(void);

extern IMetaDataImport* MDImportForModule (DacpModuleData *pModule);
extern IMetaDataImport* MDImportForModule (DWORD_PTR pModule);

//*****************************************************************************
//
// **** CQuickBytes
// This helper class is useful for cases where 90% of the time you allocate 512
// or less bytes for a data structure.  This class contains a 512 byte buffer.
// Alloc() will return a pointer to this buffer if your allocation is small
// enough, otherwise it asks the heap for a larger buffer which is freed for
// you.  No mutex locking is required for the small allocation case, making the
// code run faster, less heap fragmentation, etc...  Each instance will allocate
// 520 bytes, so use accordinly.
//
//*****************************************************************************
template <DWORD SIZE, DWORD INCREMENT>
class CQuickBytesBase
{
public:
    CQuickBytesBase() :
        pbBuff(0),
        iSize(0),
        cbTotal(SIZE)
    { }

    void Destroy()
    {
        if (pbBuff)
        {
            delete[] (BYTE*)pbBuff;
            pbBuff = 0;
        }
    }

    void *Alloc(SIZE_T iItems)
    {
        iSize = iItems;
        if (iItems <= SIZE)
        {
            cbTotal = SIZE;
            return (&rgData[0]);
        }
        else
        {
            if (pbBuff)
                delete[] (BYTE*)pbBuff;
            pbBuff = new BYTE[iItems];
            cbTotal = pbBuff ? iItems : 0;
            return (pbBuff);
        }
    }

    // This is for conformity to the CQuickBytesBase that is defined by the runtime so
    // that we can use it inside of some GC code that SOS seems to include as well.
    //
    // The plain vanilla "Alloc" version on this CQuickBytesBase doesn't throw either,
    // so we'll just forward the call.
    void *AllocNoThrow(SIZE_T iItems)
    {
        return Alloc(iItems);
    }

    HRESULT ReSize(SIZE_T iItems)
    {
        void *pbBuffNew;
        if (iItems <= cbTotal)
        {
            iSize = iItems;
            return NOERROR;
        }

        pbBuffNew = new BYTE[iItems + INCREMENT];
        if (!pbBuffNew)
            return E_OUTOFMEMORY;
        if (pbBuff)
        {
            memcpy(pbBuffNew, pbBuff, cbTotal);
            delete[] (BYTE*)pbBuff;
        }
        else
        {
            _ASSERTE(cbTotal == SIZE);
            memcpy(pbBuffNew, rgData, SIZE);
        }
        cbTotal = iItems + INCREMENT;
        iSize = iItems;
        pbBuff = pbBuffNew;
        return NOERROR;

    }

    operator PVOID()
    { return ((pbBuff) ? pbBuff : &rgData[0]); }

    void *Ptr()
    { return ((pbBuff) ? pbBuff : &rgData[0]); }

    SIZE_T Size()
    { return (iSize); }

    SIZE_T MaxSize()
    { return (cbTotal); }

    void        *pbBuff;
    SIZE_T      iSize;              // number of bytes used
    SIZE_T      cbTotal;            // total bytes allocated in the buffer
    // use UINT64 to enforce the alignment of the memory
    UINT64 rgData[(SIZE+sizeof(UINT64)-1)/sizeof(UINT64)];
};

#define     CQUICKBYTES_BASE_SIZE           512
#define     CQUICKBYTES_INCREMENTAL_SIZE    128

class CQuickBytesNoDtor : public CQuickBytesBase<CQUICKBYTES_BASE_SIZE, CQUICKBYTES_INCREMENTAL_SIZE>
{
};

class CQuickBytes : public CQuickBytesNoDtor
{
public:
    CQuickBytes() { }

    ~CQuickBytes()
    {
        Destroy();
    }
};

template <DWORD CQUICKBYTES_BASE_SPECIFY_SIZE>
class CQuickBytesNoDtorSpecifySize : public CQuickBytesBase<CQUICKBYTES_BASE_SPECIFY_SIZE, CQUICKBYTES_INCREMENTAL_SIZE>
{
};

template <DWORD CQUICKBYTES_BASE_SPECIFY_SIZE>
class CQuickBytesSpecifySize : public CQuickBytesNoDtorSpecifySize<CQUICKBYTES_BASE_SPECIFY_SIZE>
{
public:
    CQuickBytesSpecifySize() { }

    ~CQuickBytesSpecifySize()
    {
        CQuickBytesNoDtorSpecifySize<CQUICKBYTES_BASE_SPECIFY_SIZE>::Destroy();
    }
};


#define STRING_SIZE 10
class CQuickString : public CQuickBytesBase<STRING_SIZE, STRING_SIZE>
{
public:
    CQuickString() { }

    ~CQuickString()
    {
        Destroy();
    }

    void *Alloc(SIZE_T iItems)
    {
        return CQuickBytesBase<STRING_SIZE, STRING_SIZE>::Alloc(iItems*sizeof(WCHAR));
    }

    HRESULT ReSize(SIZE_T iItems)
    {
        return CQuickBytesBase<STRING_SIZE, STRING_SIZE>::ReSize(iItems * sizeof(WCHAR));
    }

    SIZE_T Size()
    {
        return CQuickBytesBase<STRING_SIZE, STRING_SIZE>::Size() / sizeof(WCHAR);
    }

    SIZE_T MaxSize()
    {
        return CQuickBytesBase<STRING_SIZE, STRING_SIZE>::MaxSize() / sizeof(WCHAR);
    }

    WCHAR* String()
    {
        return (WCHAR*) Ptr();
    }

};

enum GetSignatureStringResults
{
    GSS_SUCCESS,
    GSS_ERROR,
    GSS_INSUFFICIENT_DATA,
};

GetSignatureStringResults GetMethodSignatureString (PCCOR_SIGNATURE pbSigBlob, ULONG ulSigBlob, DWORD_PTR dwModuleAddr, CQuickBytes *sigString);
GetSignatureStringResults GetSignatureString (PCCOR_SIGNATURE pbSigBlob, ULONG ulSigBlob, DWORD_PTR dwModuleAddr, CQuickBytes *sigString);
void GetMethodName(mdMethodDef methodDef, IMetaDataImport * pImport, CQuickBytes *fullName);

#ifndef _TARGET_WIN64_
#define     itoa_s_ptr _itoa_s
#define     itow_s_ptr _itow_s
#else
#define     itoa_s_ptr _i64toa_s
#define     itow_s_ptr _i64tow_s
#endif

#ifdef FEATURE_PAL
extern "C"
int  _itoa_s( int inValue, char* outBuffer, size_t inDestBufferSize, int inRadix );
extern "C"
int  _ui64toa_s( unsigned __int64 inValue, char* outBuffer, size_t inDestBufferSize, int inRadix );
#endif // FEATURE_PAL

struct MemRange
{
    MemRange (ULONG64 s = (TADDR)0, size_t l = 0, MemRange * n = NULL)
        : start(s), len (l), next (n)
        {}

    bool InRange (ULONG64 addr)
    {
        return addr >= start && addr < start + len;
    }

    ULONG64 start;
    size_t len;
    MemRange * next;
}; //struct MemRange

#ifndef FEATURE_PAL

class StressLogMem
{
private:
    // use a linked list for now, could be optimazied later
    MemRange * list;

    void AddRange (ULONG64 s, size_t l)
    {
        list = new MemRange (s, l, list);
    }

public:
    StressLogMem () : list (NULL)
        {}
    ~StressLogMem ();
    bool Init (ULONG64 stressLogAddr, IDebugDataSpaces* memCallBack);
    bool IsInStressLog (ULONG64 addr);
}; //class StressLogMem

// An adapter class that DIA consumes so that it can read PE data from the an image
// This implementation gets the backing data from the image loaded in debuggee memory
// that has been layed out identical to the disk format (ie not seperated by section)
class PEOffsetMemoryReader : IDiaReadExeAtOffsetCallback
{
public:
    PEOffsetMemoryReader(TADDR moduleBaseAddress);

    // IUnknown implementation
    HRESULT __stdcall QueryInterface(REFIID riid, VOID** ppInterface);
    ULONG __stdcall AddRef();
    ULONG __stdcall Release();

    // IDiaReadExeAtOffsetCallback implementation
    HRESULT __stdcall ReadExecutableAt(DWORDLONG fileOffset, DWORD cbData, DWORD* pcbData, BYTE data[]);

private:
    TADDR m_moduleBaseAddress;
    volatile ULONG m_refCount;
};

// An adapter class that DIA consumes so that it can read PE data from the an image
// This implementation gets the backing data from the image loaded in debuggee memory
// that has been layed out in LoadLibrary format
class PERvaMemoryReader : IDiaReadExeAtRVACallback
{
public:
    PERvaMemoryReader(TADDR moduleBaseAddress);

    // IUnknown implementation
    HRESULT __stdcall QueryInterface(REFIID riid, VOID** ppInterface);
    ULONG __stdcall AddRef();
    ULONG __stdcall Release();

    // IDiaReadExeAtOffsetCallback implementation
    HRESULT __stdcall ReadExecutableAtRVA(DWORD relativeVirtualAddress, DWORD cbData, DWORD* pcbData, BYTE data[]);

private:
    TADDR m_moduleBaseAddress;
    volatile ULONG m_refCount;
};

#endif // !FEATURE_PAL

WString BuildRegisterOutput(const SOSStackRefData &ref, bool printObj = true);
WString MethodNameFromIP(CLRDATA_ADDRESS methodDesc, BOOL bSuppressLines = FALSE, BOOL bAssemblyName = FALSE, BOOL bDisplacement = FALSE, BOOL bAdjustIPForLineNumber = FALSE);
HRESULT GetGCRefs(ULONG osID, SOSStackRefData **ppRefs, unsigned int *pRefCnt, SOSStackRefError **ppErrors, unsigned int *pErrCount);
WString GetFrameFromAddress(TADDR frameAddr, IXCLRDataStackWalk *pStackwalk = NULL, BOOL bAssemblyName = FALSE);

HRESULT PreferCanonMTOverEEClass(CLRDATA_ADDRESS eeClassPtr, BOOL *preferCanonMT, CLRDATA_ADDRESS *outCanonMT = NULL);

/* This cache is used to read data from the target process if the reads are known
 * to be sequential.
 */
class LinearReadCache
{
public:
    LinearReadCache(ULONG pageSize = 0x10000);
    ~LinearReadCache();

    /* Reads an address out of the target process, caching the page of memory read.
     * Params:
     *   addr - The address to read out of the target process.
     *   t - A pointer to the data to stuff it in.  We will read sizeof(T) data
     *       from the process and write it into the location t points to.  This
     *       parameter must be non-null.
     * Returns:
     *   True if the read succeeded.  False if it did not, usually as a result
     *   of the memory simply not being present in the target process.
     * Note:
     *   The state of *t is undefined if this function returns false.  We may
     *   have written partial data to it if we return false, so you must
     *   absolutely NOT use it if Read returns false.
     */
    template <class T>
    bool Read(TADDR addr, T *t, bool update = true)
    {
        _ASSERTE(t);

        // Unfortunately the ctor can fail the alloc for the byte array.  In this case
        // we'll just fall back to non-cached reads.
        if (mPage == NULL)
            return MisalignedRead(addr, t);

        // Is addr on the current page?  If not read the page of memory addr is on.
        // If this fails, we will fall back to a raw read out of the process (which
        // is what MisalignedRead does).
        if ((addr < mCurrPageStart) || (addr - mCurrPageStart > mCurrPageSize))
            if (!update || !MoveToPage(addr))
                return MisalignedRead(addr, t);

        // If MoveToPage succeeds, we MUST be on the right page.
        _ASSERTE(addr >= mCurrPageStart);

        // However, the amount of data requested may fall off of the page.  In that case,
        // fall back to MisalignedRead.
        TADDR offset = addr - mCurrPageStart;
        if (offset + sizeof(T) > mCurrPageSize)
            return MisalignedRead(addr, t);

        // If we reach here we know we are on the right page of memory in the cache, and
        // that the read won't fall off of the end of the page.
#ifdef _DEBUG
        mReads++;
#endif

        *t = *reinterpret_cast<T*>(mPage+offset);
        return true;
    }

    void EnsureRangeInCache(TADDR start, unsigned int size)
    {
        if (mCurrPageStart == start)
        {
            if (size <= mCurrPageSize)
                return;

            // Total bytes to read, don't overflow buffer.
            unsigned int total = size + mCurrPageSize;
            if (total + mCurrPageSize > mPageSize)
                total = mPageSize-mCurrPageSize;

            // Read into the middle of the buffer, update current page size.
            ULONG read = 0;
            HRESULT hr = g_ExtData->ReadVirtual(mCurrPageStart+mCurrPageSize, mPage+mCurrPageSize, total, &read);
            mCurrPageSize += read;

            if (hr != S_OK)
            {
                mCurrPageStart = 0;
                mCurrPageSize = 0;
            }
        }
        else
        {
            MoveToPage(start, size);
        }
    }

    void ClearStats()
    {
#ifdef _DEBUG
        mMisses = 0;
        mReads = 0;
        mMisaligned = 0;
#endif
    }

    void PrintStats(const char *func)
    {
#ifdef _DEBUG
        char buffer[1024];
        sprintf_s(buffer, ARRAY_SIZE(buffer), "Cache (%s): %d reads (%2.1f%% hits), %d misses (%2.1f%%), %d misaligned (%2.1f%%).\n",
                                             func, mReads, 100*(mReads-mMisses)/(float)(mReads+mMisaligned), mMisses,
                                             100*mMisses/(float)(mReads+mMisaligned), mMisaligned, 100*mMisaligned/(float)(mReads+mMisaligned));
        OutputDebugStringA(buffer);
#endif
    }

private:
    /* Sets the cache to the page specified by addr, or false if we could not move to
     * that page.
     */
    bool MoveToPage(TADDR addr, unsigned int size = 0x18);

    /* Attempts to read from the target process if the data possibly crosses the
     * boundaries of the page.
     */
    template<class T>
    inline bool MisalignedRead(TADDR addr, T *t)
    {
        ULONG fetched = 0;
        HRESULT hr = g_ExtData->ReadVirtual(addr, (BYTE*)t, sizeof(T), &fetched);

        if (FAILED(hr) || fetched != sizeof(T))
            return false;

        mMisaligned++;
        return true;
    }

private:
    TADDR mCurrPageStart;
    ULONG mPageSize, mCurrPageSize;
    BYTE *mPage;

    int mMisses, mReads, mMisaligned;
};

// Helper class used in ClrStackFromPublicInterface() to keep track of explicit EE Frames
// (i.e., "internal frames") on the stack.  Call Init() with the appropriate
// ICorDebugThread3, and this class will initialize itself with the set of internal
// frames.  You can then call PrintPrecedingInternalFrames during your stack walk to
// have this class output any internal frames that "precede" (i.e., that are closer to
// the leaf than) the specified ICorDebugFrame.
class InternalFrameManager
{
private:
    // TODO: Verify constructor AND destructor is called for each array element
    // TODO: Comment about hard-coding 1000
    ToRelease<ICorDebugInternalFrame2> m_rgpInternalFrame2[1000];
    ULONG32 m_cInternalFramesActual;
    ULONG32 m_iInternalFrameCur;

public:
    InternalFrameManager();
    HRESULT Init(ICorDebugThread3 * pThread3);
    HRESULT PrintPrecedingInternalFrames(ICorDebugFrame * pFrame);

private:
    HRESULT PrintCurrentInternalFrame();
};

#undef LIMITED_METHOD_DAC_CONTRACT 
#define LIMITED_METHOD_DAC_CONTRACT ((void)0)
#undef LIMITED_METHOD_CONTRACT 
#define LIMITED_METHOD_CONTRACT ((void)0)
#undef WRAPPER_NO_CONTRACT 
#define WRAPPER_NO_CONTRACT ((void)0)
#undef SUPPORTS_DAC 
#define SUPPORTS_DAC ((void)0)

//////////////////////////////////////////////////////////////////////////////
// enum CorElementTypeZapSig defines some additional internal ELEMENT_TYPE's
// values that are only used by ZapSig signatures.
//////////////////////////////////////////////////////////////////////////////
typedef enum CorElementTypeZapSig
{
    // ZapSig encoding for ELEMENT_TYPE_VAR and ELEMENT_TYPE_MVAR. It is always followed
    // by the RID of a GenericParam token, encoded as a compressed integer.
    ELEMENT_TYPE_VAR_ZAPSIG = 0x3b,

    // UNUSED = 0x3c,

    // ZapSig encoding for native value types in IL stubs. IL stub signatures may contain
    // ELEMENT_TYPE_INTERNAL followed by ParamTypeDesc with ELEMENT_TYPE_VALUETYPE element
    // type. It acts like a modifier to the underlying structure making it look like its
    // unmanaged view (size determined by unmanaged layout, blittable, no GC pointers).
    //
    // ELEMENT_TYPE_NATIVE_VALUETYPE_ZAPSIG is used when encoding such types to NGEN images.
    // The signature looks like this: ET_NATIVE_VALUETYPE_ZAPSIG ET_VALUETYPE <token>.
    // See code:ZapSig.GetSignatureForTypeHandle and code:SigPointer.GetTypeHandleThrowing
    // where the encoding/decoding takes place.
    ELEMENT_TYPE_NATIVE_VALUETYPE_ZAPSIG = 0x3d,

    ELEMENT_TYPE_CANON_ZAPSIG            = 0x3e,     // zapsig encoding for System.__Canon
    ELEMENT_TYPE_MODULE_ZAPSIG           = 0x3f,     // zapsig encoding for external module id#

} CorElementTypeZapSig;

#include "sigparser.h"

#endif // __util_h__
