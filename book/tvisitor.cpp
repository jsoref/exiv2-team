#ifdef  _MSC_VER
bool bVisualStudio = true;
#define _CRT_SECURE_NO_WARNINGS
#define  FSEEK_LONG  long
#pragma  warning(disable : 4996)
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#define  fileno      _fileno
#define  vsnprint    _vsnprintf
#else
bool bVisualStudio = true;
#endif

#include <iostream>
#include <set>
#include <vector>
#include <memory>
#include <cstring>
#include <string>
#include <sstream>
#include <map>
#include <cstdlib>
#include <cassert>
#include <algorithm>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>

#ifndef  _MSC_VER
#include <unistd.h>
#define  FSEEK_LONG uint64_t
#endif

typedef std::set<uint64_t> Visits;

// types of data in Exif Specification
enum type_e
{    kttMin             = 0
,    kttUByte           = 1 //!< Exif BYTE type, 8-bit unsigned integer.
,    kttAscii           = 2 //!< Exif ASCII type, 8-bit byte.
,    kttUShort          = 3 //!< Exif SHORT type, 16-bit (2-byte) unsigned integer.
,    kttULong           = 4 //!< Exif LONG type, 32-bit (4-byte) unsigned integer.
,    kttURational       = 5 //!< Exif RATIONAL type, two LONGs: numerator and denumerator of a fraction.
,    kttByte            = 6 //!< Exif SBYTE type, an 8-bit signed (twos-complement) integer.
,    kttUndefined       = 7 //!< Exif UNDEFINED type, an 8-bit byte that may contain anything.
,    kttShort           = 8 //!< Exif SSHORT type, a 16-bit (2-byte) signed (twos-complement) integer.
,    kttLong            = 9 //!< Exif SLONG type, a 32-bit (4-byte) signed (twos-complement) integer.
,    kttSRational       =10 //!< Exif SRATIONAL type, two SLONGs: numerator and denumerator of a fraction.
,    kttFloat           =11 //!< TIFF FLOAT type, single precision (4-byte) IEEE format.
,    kttDouble          =12 //!< TIFF DOUBLE type, double precision (8-byte) IEEE format.
,    kttIfd             =13 //!< TIFF IFD type, 32-bit (4-byte) unsigned integer.
,    kttNot1            =14
,    kttNot2            =15
,    kttULong8          =16 //!< Exif LONG LONG type, 64-bit (8-byte) unsigned integer.
,    kttLong8           =17 //!< Exif LONG LONG type, 64-bit (8-byte) signed integer.
,    kttIfd8            =18 //!< TIFF IFD type, 64-bit (8-byte) unsigned integer.
,    kttMax             =19
};
const char* typeName(type_e tag)
{
    //! List of TIFF image tags
    const char* result = NULL;
    switch (tag ) {
        case kttUByte      : result = "UBYTE"     ; break;
        case kttAscii      : result = "ASCII"     ; break;
        case kttUShort     : result = "SHORT"     ; break;
        case kttULong      : result = "LONG"      ; break;
        case kttURational  : result = "RATIONAL"  ; break;
        case kttByte       : result = "BYTE"      ; break;
        case kttUndefined  : result = "UNDEFINED" ; break;
        case kttShort      : result = "SSHORT"    ; break;
        case kttLong       : result = "SLONG"     ; break;
        case kttSRational  : result = "SRATIONAL" ; break;
        case kttFloat      : result = "FLOAT"     ; break;
        case kttDouble     : result = "DOUBLE"    ; break;
        case kttIfd        : result = "IFD"       ; break;
        case kttULong8     : result = "LONG8"     ; break;
        case kttLong8      : result = "LONG8"     ; break;
        case kttIfd8       : result = "IFD8"      ; break;
        default            : result = "unknown"   ; break;
    }
    return result;
}

enum endian_e
{   keLittle
,   keBig
,   keImage // used by a field  to say "use image's endian"
};

// Error support
enum error_e
{   kerCorruptedMetadata
,   kerTiffDirectoryTooLarge
,   kerInvalidTypeValue
,   kerInvalidMalloc
,   kerInvalidFileFormat
,   kerFailedToReadImageData
,   kerDataSourceOpenFailed
,   kerNoImageInInputData
,   kerFileDidNotOpen
,   kerUnknownFormat
,   kerAlreadyVisited
,   kerInvalidMemoryAccess
};

void Error (error_e error, std::string msg,std::string m2="")
{
    switch ( error ) {
        case   kerCorruptedMetadata      : std::cerr << "corrupted metadata"       ; break;
        case   kerTiffDirectoryTooLarge  : std::cerr << "tiff directory too large" ; break;
        case   kerInvalidTypeValue       : std::cerr << "invalid type"             ; break;
        case   kerInvalidMalloc          : std::cerr << "invalid malloc"           ; break;
        case   kerInvalidFileFormat      : std::cerr << "invalid file format"      ; break;
        case   kerFailedToReadImageData  : std::cerr << "failed to read image data"; break;
        case   kerDataSourceOpenFailed   : std::cerr << "data source open failed"  ; break;
        case   kerNoImageInInputData     : std::cerr << "not image in input data"  ; break;
        case   kerFileDidNotOpen         : std::cerr << "file did not open"        ; break;
        case   kerUnknownFormat          : std::cerr << "unknown format"           ; break;
        case   kerAlreadyVisited         : std::cerr << "already visited"          ; break;
        case   kerInvalidMemoryAccess    : std::cerr << "invalid memory access"    ; break;
        default                          : std::cerr << "unknown error"            ; break;
    }
    if ( msg.size() ) std::cerr << " " << msg ;
    std::cerr << std::endl;
    _exit(1); // pull the plug!
}

void Error (error_e error, size_t n)
{
    std::ostringstream os ;
    os << n ;
    Error(error,os.str());
}

void Error (error_e error)
{
    Error(error,"");
}
// forward and prototypes
class Io;
class DataBuf ; // forward
typedef unsigned char byte ;
uint8_t  getByte (const DataBuf& buf,size_t offset) ;
uint16_t getShort(const DataBuf& buf,size_t offset,endian_e endian);
uint32_t getLong (const DataBuf& buf,size_t offset,endian_e endian);

class DataBuf
{
public:
    byte*     pData_;
    uint64_t  size_ ;
    DataBuf(uint64_t size=0,uint64_t size_max=0)
    : pData_(NULL)
    , size_(size)
    {
        if ( size ) {
            if ( size_max && size > size_max ) {
                Error(kerInvalidMalloc);
            }
            pData_ = (byte*) std::calloc(size_,1);
        }
    }
    virtual ~DataBuf()
    {
        empty(true);
        size_ = 0 ;
    }
    bool empty(bool bForce=false) {
        bool result = size_ == 0;
        if ( bForce && pData_ && size_ ) {
            std::free(pData_) ;
            pData_ = NULL ;
        }
        if ( bForce ) size_ = 0;
        return result;
    }

    void read (Io& io,uint64_t offset,uint64_t size) ;
    int  strcmp   (const char* str) { return ::strcmp((const char*)pData_,str);}
    bool strequals(const char* str) { return strcmp(str)==0                   ;}
    bool begins       (const char* str,uint64_t offset=0) {
        uint64_t l      = ::strlen(str);
        bool     result = l <= size_-offset;
        size_t   i = 0 ;
        while ( result && i < l ) {
            result = str[i]==pData_[i+offset];
            i++;
        }
        return result;
    }
    void  zero() { for ( uint64_t i = 0 ; i < size_ ; i++ ) pData_[i]=0; }
    char* getChars(uint64_t offset,uint64_t count,char* chars) {
        chars[0] = 0;
        if ( offset + count <= size_ ) {
            for ( uint64_t i = 0 ; i < count ; i++ )
                chars[i] = (char) pData_[offset+i];
            chars[count] = 0;
        }
        return chars;
    }
    uint8_t  getByte (uint64_t offset                ) { return ::getByte (*this,offset)       ; }
    uint16_t getShort(uint64_t offset,endian_e endian) { return ::getShort(*this,offset,endian); }
    uint32_t getLong (uint64_t offset,endian_e endian) { return ::getLong (*this,offset,endian); }

    uint32_t search(uint32_t start,const char* s)
    {
        uint64_t l = ::strlen(s);
        if ( size_ < l ) return 0 ;

        uint32_t found = start;
        uint32_t max   = (uint32_t) ( size_ - l );
        while ( ! begins(s,found) && found < max ) found++;
        return found;
    }

    void copy(void* src,uint64_t size,uint64_t offset=0)
    {
        memcpy(pData_+offset,src,size);
    }
    void copy(uint32_t v,uint64_t offset=0) { copy(&v,4,offset); }
    void copy(DataBuf& src,uint64_t size) { if (size <= src.size_ && size <= size_) copy(src.pData_,size);}
    std::string path() { return path_; }

    std::string toString(type_e type,uint64_t count,endian_e endian,uint64_t offset=0);
    std::string binaryToString(uint64_t start,uint64_t size);
    std::string toUuidString() ;

private:
    std::string path_;
};

// endian and byte swappers
bool isPlatformBigEndian()
{
    union {
        uint32_t i;
        char c[4];
    } e = { 0x01000000 };

    return e.c[0]?true:false;
}
bool   isPlatformLittleEndian() { return !isPlatformBigEndian(); }
endian_e platformEndian() { return isPlatformBigEndian() ? keBig : keLittle; }

uint64_t byteSwap(byte* p,bool bSwap,uint16_t n)
{
    uint64_t value  = 0;
    uint64_t result = 0;
    byte* source_value      = reinterpret_cast<byte *>(&value );
    byte* destination_value = reinterpret_cast<byte *>(&result);

    for (int i = 0; i < n; i++) {
        source_value     [i] = p[i] ;
        destination_value[i] = p[n - i - 1];
    }

    return bSwap ? result : value;
}

uint8_t  getByte(const DataBuf& buf,size_t offset)
{
    if ( offset > buf.size_ ) Error(kerInvalidMemoryAccess);
    return (uint8_t) buf.pData_[offset];
}

uint16_t getShort(byte b[],size_t offset,endian_e endian)
{
    bool bSwap = endian != ::platformEndian();
    return (uint16_t) byteSwap(b+offset,bSwap,2);
}
uint32_t getLong(byte b[],size_t offset,endian_e endian)
{
    bool bSwap = endian != ::platformEndian();
    return (uint32_t) byteSwap(b+offset,bSwap,2);
}

DataBuf getPascalString(DataBuf& buff,uint32_t offset)
{
    uint8_t  L = buff.pData_[offset];  // #abc
    uint16_t l = L==0  ? 2
               : L % 2 ? L + 1
               : L     ;
    DataBuf result (l);
    for ( uint16_t i = 0 ; i < L ; i++ ) {
        result.pData_[i] = buff.pData_[offset+i+1];
    }
    return result;
}

uint16_t getShort(const DataBuf& buf,size_t offset,endian_e endian)
{
    if ( offset+1 > buf.size_ ) Error(kerInvalidMemoryAccess);

    uint16_t v;
    byte*    p = (byte*) &v;
    p[0] = buf.pData_[offset+0];
    p[1] = buf.pData_[offset+1];
    bool bSwap = endian != ::platformEndian();
    return (uint16_t) byteSwap(p,bSwap,2);

}
uint32_t getLong(const DataBuf& buf,size_t offset,endian_e endian)
{
    if ( offset+3 > buf.size_ ) Error(kerInvalidMemoryAccess);

    uint32_t v;
    byte*    p = (byte*) &v;
    p[0] = buf.pData_[offset+0];
    p[1] = buf.pData_[offset+1];
    p[2] = buf.pData_[offset+2];
    p[3] = buf.pData_[offset+3];
    bool bSwap = endian != ::platformEndian();
    return (uint32_t)byteSwap(p,bSwap,4);
}
uint64_t getLong8(const DataBuf& buf,size_t offset,endian_e endian)
{
    if ( offset+7 > buf.size_ ) Error(kerInvalidMemoryAccess);

    uint64_t v;
    byte*    p = reinterpret_cast<byte *>(&v);
    p[0] = buf.pData_[offset+0];
    p[1] = buf.pData_[offset+1];
    p[2] = buf.pData_[offset+2];
    p[3] = buf.pData_[offset+3];
    p[4] = buf.pData_[offset+4];
    p[5] = buf.pData_[offset+5];
    p[6] = buf.pData_[offset+6];
    p[7] = buf.pData_[offset+7];
    bool bSwap = endian != ::platformEndian();
    return byteSwap (p,bSwap,8);
}

// Tiff Data Functions
bool isTypeShort(type_e type) {
     return type == kttUShort
         || type == kttShort
         ;
}
bool isTypeLong(type_e type) {
     return type == kttULong
         || type == kttLong
         || type == kttIfd
         || type == kttFloat
         ;
}
bool isTypeLong8(type_e type) {
    return type == kttULong8
        || type == kttLong8
        || type == kttIfd8
        || type == kttDouble
        ;
}
bool isTypeRational(type_e type) {
     return type == kttURational
         || type == kttSRational
         ;
}
bool isTypeIFD(type_e type)
{
    return type == kttIfd || type == kttIfd8;
}
bool isType1Byte(type_e type)
{
    return type == kttAscii
        || type == kttUByte
        || type == kttByte
        || type == kttUndefined
        ;
}
bool isType2Byte(type_e type)
{
    return isTypeShort(type);
}
bool isType4Byte(type_e type)
{
    return isTypeLong(type)
        || type == kttFloat
        ;
}
bool isType8Byte(type_e type)
{
    return  isTypeRational(type)
         || isTypeLong8(type)
         || type == kttIfd8
         || type == kttDouble
         ;
}
uint16_t typeSize(type_e type)
{
    return isType1Byte(type) ? 1
        :  isType2Byte(type) ? 2
        :  isType4Byte(type) ? 4
        :  isType8Byte(type) ? 8
        :  1 ;
}
type_e getType(const DataBuf& buf,size_t offset,endian_e endian)
{
    return (type_e) getShort(buf,offset,endian);
}
bool typeValid(type_e type,bool bigtiff)
{
    return  bigtiff ? type > kttMin && type < kttMax && type != kttNot1 && type != kttNot2
                    : type >= 1 && type <= 13
    ;
}

// string formatting functions
std::string indent(size_t s)
{
    std::string result ;
    while ( s-- > 1) result += "  ";
    return result ;
}

// chop("a very long string",10) -> "a ver +++"
std::string chop(const std::string& a,size_t max=0)
{
    std::string result = a;
    if ( result.size() > max  && max > 4 ) {
        result = result.substr(0,max-4) + " +++";
    }
    return result;
}

// snip("a very long string",10) -> "a very lo"
std::string snip(const std::string& a,size_t max=0)
{
    std::string result = a;
    if ( result.size() > max  && max) {
        result = result.substr(0,max);
    }
    return result;
}

// join("Exif.Nikon","PictureControl",22) -> "Exif.Nikon.PictureC.."
std::string join(const std::string& a,const std::string& b,size_t max=0)
{
    std::string c = a + "." +  b ;
    if ( max > 2 && c.size() > max ) {
        c = c.substr(0,max-2)+"..";
    }
    return c;
}

std::string stringFormat(const char* format, ...)
{
    std::string result;
    std::vector<char> buffer;
    size_t need = ::strlen(format)*8;  // initial guess
    int rc = -1;

    // vsnprintf writes at most size (2nd parameter) bytes (including \0)
    //           returns the number of bytes required for the formatted string excluding \0
    // the following loop goes through:
    // one iteration (if 'need' was large enough for the for formatted string)
    // or two iterations (after the first call to vsnprintf we know the required length)
    do {
        buffer.resize(need + 1);
        va_list args;            // variable arg list
        va_start(args, format);  // args start after format
        rc = vsnprintf(&buffer[0], buffer.size(), format, args);
        va_end(args);     // free the args
        assert(rc >= 0);  // rc < 0 => we have made an error in the format string
        if (rc > 0)
            need = static_cast<size_t>(rc);
    } while (buffer.size() <= need);

    if (rc > 0)
        result = std::string(&buffer[0], need);
    return result;
}

std::string binaryToString(const byte* b,uint64_t start,uint64_t size)
{
    std::string result;
    size_t i    = start;
    while (i < start+size ) {
        result += (32 <= b[i] && b[i] <= 127) ? b[i]
                : ( 0 == b[i]               ) ? '_'
                : '.'
                ;
        i++ ;
    }
    return result;
}

std::string DataBuf::binaryToString(uint64_t start=0,uint64_t size=0)
{
    return ::binaryToString(pData_,start,size?size:size_);
}

std::string DataBuf::toString(type_e type,uint64_t count=0,endian_e endian=keLittle,uint64_t offset/*=0*/)
{
    std::ostringstream os;
    std::string        sp;
    uint16_t           size = typeSize(type);
    if ( !count )     count = size_/typeSize(type);
    if ( isTypeShort(type) ){
        for ( uint64_t k = 0 ; k < count ; k++ ) {
            os << sp << ::getShort(*this,offset+k*size,endian);
            sp = " ";
        }
    } else if ( isTypeLong(type) ){
        for ( uint64_t k = 0 ; k < count ; k++ ) {
            os << sp << ::getLong(*this,offset+k*size,endian);
            sp = " ";
        }
    } else if ( isTypeRational(type) ){
        for ( uint64_t k = 0 ; k < count ; k++ ) {
            uint32_t a = ::getLong(*this,offset+k*size+0,endian);
            uint32_t b = ::getLong(*this,offset+k*size+4,endian);
            os << sp << a << "/" << b;
            sp = " ";
        }
    } else if ( isType8Byte(type) ) {
        for ( uint64_t k = 0 ; k < count ; k++ ) {
            os << sp << ::getLong8(*this,offset+k*size,endian);
            sp = " ";
        }
    } else if ( type == kttUByte ) {
        for ( size_t k = 0 ; k < count ; k++ )
            os << stringFormat("%s%d",k?" ":"",pData_[offset+k]);
    } else if ( type == kttAscii ) {
        bool bNoNull = true ;
        for ( size_t k = 0 ; bNoNull && k < count ; k++ )
            bNoNull = pData_[offset+k];
        if ( bNoNull )
            os << binaryToString(offset, (size_t)count);
        else
            os << (char*) pData_+offset ;
    } else {
        os << sp << binaryToString(offset, (size_t)count);
    }

    return os.str();
} // DataBuf::toString

std::string DataBuf::toUuidString()
{
    // 123e4567-e89b-12d3-a456-426614174000
    std::string result ;
    if ( size_ >= 16 ) {
        result = ::stringFormat("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"
                               ,pData_[ 0],pData_[ 1],pData_[ 2],pData_[ 3]
                               ,pData_[ 4],pData_[ 5]
                               ,pData_[ 6],pData_[ 7]
                               ,pData_[ 8],pData_[ 9]
                               ,pData_[10],pData_[11],pData_[12],pData_[13],pData_[14],pData_[15]
                               );
    }
    return result ;
} // DataBuf::toUuidString

// Camera manufacturers
enum maker_e
{   kUnknown
,   kCanon
,   kNikon
,   kSony
,   kAgfa
,   kApple
,   kPano
,   kMino
,   kOlym
};

// Canon magic
enum kCanonHeap
{   kStg_InHeapSpace    = 0
,   kStg_InRecordEntry  = 0x4000
};
// Canon tag masks
#define kcAscii        0x0800
#define kcWord         0x1000
#define kcDword        0x1000
#define kcHTP1         0x2800
#define kcHTP2         0x3000
#define kcIDCodeMask   0x07ff
#define kcDataTypeMask 0x3800
enum kCanonType
{   kDT_BYTE            = 0x0000
,   kDT_ASCII           = kcAscii
,   kDT_WORD            = kcWord
,   kDT_DWORD           = kcDword
,   kDT_BYTE2           = 0x2000
,   kDT_HeapTypeProp1   = kcHTP1
,   kDT_HeapTypeProp2   = kcHTP1
,   kTC_WildCard        = 0xffff
,   kTC_Null            = 0x0000
,   kTC_Free            = 0x0001
,   kTC_ExUsed          = 0x0002
,   kTC_Description     = 0x0005|kcAscii
,   kTC_ModelName       = 0x000a|kcAscii
,   kTC_FirmwareVersion = 0x000b|kcAscii
,   kTC_ComponentVersion= 0x000c|kcAscii
,   kTC_ROMOperationMode= 0x000d|kcAscii
,   kTC_OwnerName       = 0x0010|kcAscii
,   kTC_ImageFileName   = 0x0016|kcAscii
,   kTC_ThumbnailFileName=0x001c|kcAscii
,   kTC_TargetImageType = 0x000a|kcWord
,   kTC_SR_ReleaseMethod= 0x0010|kcWord
,   kTC_SR_ReleaseTiming= 0x0011|kcWord
,   kTC_ReleaseSetting  = 0x0016|kcWord
,   kTC_BodySensitivity = 0x001c|kcWord
,   kTC_ImageFormat     = 0x0003|kcDword
,   kTC_RecordID        = 0x0004|kcDword
,   kTC_SelfTimerTime   = 0x0006|kcDword
,   kTC_SR_TargetDistanceSetting = 0x0007|kcDword
,   kTC_BodyID          = 0x000b|kcDword
,   kTC_CapturedTime    = 0x000e|kcDword
,   kTC_ImageSpec       = 0x0010|kcDword
,   kTC_SR_EF           = 0x0013|kcDword
,   kTC_MI_EV           = 0x0014|kcDword
,   kTC_SerialNumber    = 0x0017|kcDword
,   kTC_SR_Exposure     = 0x0018|kcDword
,   kTC_CameraObject    = 0x0007|kcHTP1
,   kTC_ShootingRecord  = 0x0002|kcHTP2
,   kTC_MeasuredInfo    = 0x0003|kcHTP2
,   kTC_CameraSpecificaiton= 0x0004|kcHTP2
};

// TagDict is used to map tag (uint16_t) to string
typedef std::map<uint16_t,std::string> TagDict;
TagDict emptyDict ;
TagDict tiffDict  ;
TagDict exifDict  ;
TagDict canonDict ;
TagDict nikonDict ;
TagDict sonyDict  ;
TagDict agfaDict  ;
TagDict appleDict ;
TagDict panoDict  ;
TagDict minoDict  ;
TagDict olymDict  ;
TagDict gpsDict   ;
TagDict crwDict   ;
TagDict psdDict   ;
TagDict olymCSDict;
TagDict olymEQDict;
TagDict olymRDDict;
TagDict olymR2Dict;
TagDict olymIPDict;
TagDict olymFIDict;
TagDict olymRoDict;

enum ktSpecial
{   ktMakerNote = 0x927c
,   ktGps       = 0x8825
,   ktExif      = 0x8769
,   ktSubIFD    = 0x014a
,   ktMake      = 0x010f
,   ktXMLPacket = 0x02bc
,   ktIPTCNAA   = 0x83bb
,   ktICCProfile= 0x8773
,   ktGroup     = 0xffff
};

std::map<uint16_t,TagDict> iptcDicts;
TagDict iptc0 ;
TagDict iptcEnvelope;
TagDict iptcApplication;

TagDict& ifdDict(maker_e maker,uint16_t tag,TagDict& makerDict)
{
    TagDict& result = makerDict ;
    if ( maker == kOlym ) switch ( tag ) {
        case 0x2010 : result = olymEQDict ; break;
        case 0x2020 : result = olymCSDict ; break;
        case 0x2030 : result = olymRDDict ; break;
        case 0x2031 : result = olymR2Dict ; break;
        case 0x2040 : result = olymIPDict ; break;
        case 0x2050 : result = olymFIDict ; break;
        case 0x3000 : result = olymRoDict ; break;
        default     : /* do nothing */    ; break;
    }
    return result;
}

bool tagKnown(uint16_t tag,const TagDict& tagDict)
{
    return tagDict.find(tag) != tagDict.end();
}

std::string groupName(const TagDict& tagDict,std::string family="Exif" )
{
    std::string group = tagKnown(ktGroup,tagDict)
                      ? tagDict.find(ktGroup)->second
                      : "Unknown"
                      ;
    return family+ "." + group ;
}

std::string tagName(uint16_t tag,const TagDict& tagDict,const size_t max=0,std::string family="Exif")
{
    std::string name = tagKnown(tag,tagDict)
                     ? tagDict.find(tag)->second
                     : stringFormat("%#x",tag)
                     ;

    name =  groupName(tagDict,family) + "." + name;
    if ( max && name.size() > max ){
        name = name.substr(0,max-2)+"..";
    }
    return name;
}

// Binary Records
class Field
{
public:
    Field
    ( std::string name
    , type_e      type
    , uint16_t    start
    , uint16_t    count
    , endian_e    endian = keImage
    )
    : name_  (name)
    , type_  (type)
    , start_ (start)
    , count_ (count)
    , endian_(endian)
    {};
    virtual ~Field() {}
    std::string name  () { return name_   ; }
    type_e      type  () { return type_   ; }
    uint16_t    start () { return start_  ; }
    uint16_t    count () { return count_  ; }
    endian_e    endian() { return endian_ ; }
private:
    std::string name_   ;
    type_e      type_   ;
    uint16_t    start_  ;
    uint16_t    count_  ;
    endian_e    endian_ ;
};
typedef std::vector<Field>   Fields;
typedef std::map<std::string,Fields>  MakerTags;

// global variable
MakerTags makerTags;

// https://github.com/openSUSE/libsolv/blob/master/win32/fmemopen.c
#ifdef _MSC_VER
FILE* fmemopen(void* buf, size_t size, const char* mode)
{
    char temppath[MAX_PATH + 1];
    char tempnam[MAX_PATH + 1];
    DWORD l;
    HANDLE fh;
    FILE* fp;

    if (strcmp(mode, "r") != 0 && strcmp(mode, "r+") != 0)
        return 0;
    l = GetTempPath(MAX_PATH, temppath);
    if (!l || l >= MAX_PATH)
        return 0;
    if (!GetTempFileName(temppath, "solvtmp", 0, tempnam))
        return 0;
    fh = CreateFile(tempnam, DELETE | GENERIC_READ | GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
        NULL);
    if (fh == INVALID_HANDLE_VALUE)
        return 0;
    fp = _fdopen(_open_osfhandle((intptr_t)fh, 0), "w+b");
    if (!fp)
    {
        CloseHandle(fh);
        return 0;
    }
    if (buf && size && fwrite(buf, size, 1, fp) != 1)
    {
        fclose(fp);
        return 0;
    }
    rewind(fp);
    return fp;
}
#endif

// IO support
enum seek_e
{   ksStart   = SEEK_SET
,   ksCurrent = SEEK_CUR
,   ksEnd     = SEEK_END
};
class Io
{
public:
    Io(std::string path,std::string open) // Io object from path
    : path_   (path)
    , start_  (0)
    , size_   (0)
    , restore_(0)
    , f_      (NULL)
    { f_ = ::fopen(path.c_str(),open.c_str()); if ( !f_ ) Error(kerFileDidNotOpen,path); }

    Io(Io& io,size_t start,size_t size=0) // Io object is a substream
    : path_   (io.path())
    , start_  (start+io.start_)
    , size_   (size?size:io.size()-start)
    , restore_(ftell(f_))
    , f_      (io.f_)
    {
        std::ostringstream os;
        os << path_ << ":" << start << "->" << size_;
        path_=os.str();
        seek(0);
    };
    Io(DataBuf& buf)
    : path_   (buf.path())
    , start_  (0)
    , size_   (buf.size_)
    , restore_(0)
    , f_      (NULL)
    {   f_ = fmemopen(buf.pData_,buf.size_, "r");
        if ( !f_ ) Error(kerFileDidNotOpen,path_);
    }

    virtual ~Io() { close(); }
    std::string path() { return path_; }
    uint64_t read(void* buff,uint64_t size)              { return fread(buff,1,size,f_);}
    uint64_t read(DataBuf& buff)                         { return read(buff.pData_,buff.size_); }
    byte     getb()                                      { byte b; if (read(&b,1)==1) return b ; else return -1; }
    int      eof()                                       { return feof(f_) ; }
    uint64_t tell()                                      { return ftell(f_)-start_ ; }
    void     seek(int64_t offset,seek_e whence=ksStart)  { fseek(f_,(FSEEK_LONG)(offset+start_),whence) ; }
    uint64_t size()                                      { if ( size_ ) return size_ ; struct stat st ; fstat(::fileno(f_),&st) ; return st.st_size-start_ ; }
    bool     good()                                      { return f_ ? true : false ; }
    void     close()
    {
        if ( !f_ ) return ;
        if ( start_ == 0 && size_ == 0 && restore_ == 0 ) {
            fclose(f_) ;
        } else {
            fseek(f_,(FSEEK_LONG)restore_,ksStart);
        }
        f_ = NULL  ;
    }
    uint64_t start() { return start_ ; }

    uint32_t getLong(endian_e endian)
    {
        DataBuf buf(4);
        read   (buf);
        return ::getLong(buf,0,endian);
    }
    float getFloat(endian_e endian)
    {
        return (float) getLong(endian);
    }
    uint16_t getShort(endian_e endian)
    {
        byte b[2];
        read(b,2);
        return ::getShort(b,0,endian);
    }
    uint8_t getByte()
    {
        byte   b[1];
        read  (b,1);
        return b[0];
    }
private:
    FILE*       f_;
    std::string path_;
    uint64_t    start_;
    uint64_t    size_;
    uint64_t    restore_;
};

class IoSave // restore Io when function ends
{
public:
    IoSave(Io& io,uint64_t address)
    : io_     (io)
    , restore_(io.tell())
    { io_.seek(address); }
    virtual ~IoSave() {io_.seek(restore_);}
private:
    Io&      io_;
    uint64_t restore_;
};

void DataBuf::read(Io& io,uint64_t offset,uint64_t size)
{
    if ( size ) {
        IoSave restore(io,offset);
        pData_ = (byte*) (pData_ ? std::realloc(pData_,size_+size) : std::malloc(size));
        if ( !path_.size() ) path_=io.path();
        std::ostringstream os ;
        os <<"+"<<io.tell()<<"->"<<size;
        path_ += os.str();
        io.read (pData_+size_,size);
        size_ += size ;
    }
}

// Options for ReportVisitor
typedef uint16_t PSOption;
#define kpsBasic     1
#define kpsRecursive 2
#define kpsXMP       4
#define kpsUnknown   8
#define kpsIcc      16
#define kpsIptc     32


// 1.  declare types
class   Image; // forward
class   TiffImage;
class   CrwImage ;
class   ICC;
class   IFD;
class   CIFF;
class   C8BIM;

// 2. Create abstract "visitor" base class with virtual methods
class Visitor
{
public:
    Visitor(std::ostream& out,PSOption option)
    : out_   (out)
    , option_(option)
    , indent_(0)
    {};
    virtual ~Visitor() {};

    // abstract methods which visitors must define
    virtual void visitBegin   (Image& image,std::string msg="")      = 0 ;
    virtual void visitEnd     (Image& image)                         = 0 ;
    virtual void visitDirBegin(Image& image,uint64_t nEntries)       = 0 ;
    virtual void visitDirEnd  (Image& image,uint64_t start)          = 0 ;
    virtual void visitTag     (Io& io,Image& image
                        ,uint64_t address, uint16_t tag, type_e type
                        ,uint64_t count,   uint64_t offset
                        ,DataBuf& buf,     const TagDict& tagDict  ) = 0 ;
    virtual void visitCiff    (Io& io,Image& image,uint64_t address) = 0 ;
    virtual void visitSegment (Io& io,Image& image,uint64_t address
             ,uint8_t marker,uint16_t length,std::string& signature) = 0 ;
    virtual void visitExif    (Io& io)                               = 0 ;
    virtual void visitChunk   (Io& io,Image& image,uint64_t address,char* chunk,uint32_t length,uint32_t checksum) = 0;
    virtual void visitBox     (Io& io,Image& image,uint64_t address,uint32_t box,uint32_t length) = 0 ;

    // optional methods
    virtual void visitXMP     (DataBuf& /* xmp */ )                                  { return ; }
    virtual void visitICC     (DataBuf& /* icc */ )                                  { return ; }
    virtual void visitICCTag  (const byte* sig,uint32_t offset,uint32_t length)      { return ; }
    virtual void visitIPTC    (Io& io,Image& image
                              ,uint16_t record,uint16_t dataset,uint32_t len
                              ,DataBuf& buff,uint32_t offset)                        { return ; }

    virtual void visitResource(Io& io,Image& image,uint64_t address)                 { return ; }
    virtual void visit8BIM    (Io& io,Image& image,uint32_t offset
                              ,uint16_t kind,DataBuf& name,uint32_t len
                              ,uint32_t data,uint32_t pad ,DataBuf& b)               { return ; }
    virtual void visitRiff    (uint64_t address,std::string chunk
                              ,uint32_t length,DataBuf& data)                        { return ; }
    virtual void visitMrw     (Io& io,Image& image
                              ,uint64_t address,std::string chunk
                              ,uint32_t length,DataBuf& data)                        { return ; }

    PSOption      option() { return option_ ; }
    std::ostream& out()    { return out_    ; }
    std::string   indent() { return ::indent(indent_);}
protected:
    uint32_t      indent_ ;
    PSOption      option_;
    std::ostream& out_   ;
};

// 3. Image has an accept(Visitor&) method
class Image
{
public:
    Image(std::string path)
    : io_(path,"rb")
    , start_    (0)
    , maker_    (kUnknown)
    , makerDict_(emptyDict)
    , bigtiff_  (false)
    , endian_   (keLittle)
    , depth_    (0)
    , valid_    (false)
    {};
    Image(Io io)
    : io_       (io)
    , start_    (0)
    , maker_    (kUnknown)
    , makerDict_(emptyDict)
    , bigtiff_  (false)
    , endian_   (keLittle)
    , depth_    (0)
    , valid_    (false)
    {};
    virtual    ~Image()        { io_.close()      ; }
    bool        valid()        { return false     ; }
    std::string path()         { return io_.path(); }
    endian_e    endian()       { return endian_   ; }
    Io&         io()           { return io_       ; }
    std::string format()       { return format_   ; }
    Visits&     visits()       { return visits_   ; }
    size_t      depth()        { return depth_    ; }
    bool        bigtiff()      { return bigtiff_  ; }
    virtual std::string boxName (uint32_t  box) { return ""; }
    virtual std::string uuidName(DataBuf& uuid) { return ""; }

    void visit(uint64_t address) { // never visit an address twice!
        if ( visits_.find(address) != visits_.end() ) {
            Error(kerAlreadyVisited,address);
        }
        visits_.insert(address);
    }

    virtual void accept(class Visitor& v)=0;

    maker_e     maker_;
    TagDict&    makerDict_;
    void setMaker(maker_e maker) {
        maker_ = maker;
        switch ( maker_ ) {
            case kCanon : makerDict_ = canonDict ; break;
            case kNikon : makerDict_ = nikonDict ; break;
            case kSony  : makerDict_ = sonyDict  ; break;
            case kAgfa  : makerDict_ = agfaDict  ; break;
            case kApple : makerDict_ = appleDict ; break;
            case kPano  : makerDict_ = panoDict  ; break;
            case kMino  : makerDict_ = minoDict  ; break;
            case kOlym  : makerDict_ = olymDict  ; break;
            default : /* do nothing */           ; break;
        }
    }
    void setMaker(DataBuf& buf)
    {
        maker_ = buf.strequals("Canon"            ) ? kCanon
               : buf.strequals("NIKON CORPORATION") ? kNikon
               : buf.strequals("NIKON"            ) ? kNikon
               : buf.strequals("SONY")              ? kSony
               : buf.strequals("AGFAPHOTO")         ? kAgfa
               : buf.strequals("Apple")             ? kApple
               : buf.strequals("Panasonic")         ? kPano
               : buf.strequals("Minolta Co., Ltd."      ) ? kMino
               : buf.strequals("OLYMPUS IMAGING CORP.  ") ? kOlym
               : maker_
               ;
        setMaker(maker_);
    } // setMaker

    bool superBox(uint32_t box)
    {
        std::string name = boxName(box);
        return      name == kJp2Box_jp2h
                ||  name == kJp2Box_moov
                ||  name == kJp2Box_dinf
                ||  name == kJp2Box_iprp
                ||  name == kJp2Box_ipco
                ||  name == kJp2Box_meta
                ||  name == kJp2Box_iinf
                ||  name == kJp2Box_iloc
        ;
    }
    bool fullBox(uint32_t box)
    {
        std::string name = boxName(box);
        return      name == kJp2Box_meta
                ||  name == kJp2Box_iinf
                ||  name == kJp2Box_iloc
        ;
    }

    friend class ReportVisitor;
    friend class IFD      ;
    friend class CIFF     ;

protected:
    bool        valid_ ;
    Visits      visits_;
    uint64_t    start_;
    Io          io_;
    bool        good_;
    uint16_t    magic_;
    endian_e    endian_;
    bool        bigtiff_;
    size_t      depth_;
    std::string format_; // "TIFF", "JPEG" etc...
    std::string header_; // title for report

    bool isPrintXMP(uint16_t type, PSOption option)
    {
        return type == ktXMLPacket && option & kpsXMP;
    }
    friend class ImageEndianSaver;

    const char*  kJp2Box_jP    = "jP  ";
    const char*  kJp2Box_jp2h  = "jp2h";
    const char*  kJp2Box_jp2c  = "jp2c";
    const char*  kJp2Box_mdat  = "mdat";
    const char*  kJp2Box_ftyp  = "ftyp";
    const char*  kJp2Box_moov  = "moov";
    const char*  kJp2Box_dinf  = "dinf";
    const char*  kJp2Box_iprp  = "iprp";
    const char*  kJp2Box_ipco  = "ipco";
    const char*  kJp2Box_meta  = "meta";
    const char*  kJp2Box_hdlr  = "hdlr";
    const char*  kJp2Box_iinf  = "iinf";
    const char*  kJp2Box_iloc  = "iloc";
};

class ImageEndianSaver
{
public:
    ImageEndianSaver(Image& image,endian_e endian)
    : image_ (image)
    , endian_(endian)
    {}
    virtual ~ImageEndianSaver() { image_.endian_ = endian_ ; }
private:
    Image&   image_;
    endian_e endian_;
};

// CIFF and IFD are magic we find inside images
class CIFF
{
public:
    CIFF(Image& image,Io& parent)
    : image_ (image)
    , parent_(parent)
    , vector_(image.io())
    {
        setParent(parent);
    };
    virtual ~CIFF() {};
    void     accept(Visitor& visitor);
    Image&   image () { return image_ ;}
    Io&      parent() { return parent_;}
    Io&      vector() { return vector_;}
    void     setParent(Io& parent)
    {
        IoSave    save(parent,parent.size()-4);
        parent_ = parent;
        size_t    start = parent.getLong(image().endian());
        vector_ = Io(parent,start,parent.size()-start);
    }

    void dumpImageSpec(Visitor& visitor,uint16_t tag,size_t start,size_t count,uint64_t depth)
    {
        image().depth_++;

        depth += image().depth_ ;
        endian_e endian = image_.endian();
        IoSave   restore(parent(),start);

        if ( tag == 0x300a ) { // ImageSpec
            uint32_t  imageWidth         = parent().getLong (endian);
            uint32_t  imageHeight        = parent().getLong (endian);
            uint32_t  pixelAspectRatio   = parent().getLong (endian);
            float     rotationAngle      = parent().getFloat(endian);
            uint32_t  componentBitDepth  = parent().getLong (endian);
            uint32_t  colorBitDepth      = parent().getLong (endian);
            uint32_t  colorBW            = parent().getLong (endian);
            std::cout << ::indent(depth) << stringFormat("width              %d"  , imageWidth)        << std::endl;
            std::cout << ::indent(depth) << stringFormat("height             %d"  , imageHeight)       << std::endl;
            std::cout << ::indent(depth) << stringFormat("pixelAspectRatio   %#x" , pixelAspectRatio)  << std::endl;
            std::cout << ::indent(depth) << stringFormat("rotationAngle      %f"  , rotationAngle)     << std::endl;
            std::cout << ::indent(depth) << stringFormat("componentBitDepth  %d"  , componentBitDepth) << std::endl;
            std::cout << ::indent(depth) << stringFormat("colorBitDepth      %d"  , colorBitDepth)     << std::endl;
            std::cout << ::indent(depth) << stringFormat("colorBW            %d"  , colorBW)           << std::endl;
        }
        Io   ciffParent(parent(),start,count);
        CIFF ciff(image(),ciffParent);
        ciff.accept(visitor);
        image().depth_--;
    }

private:
    Image&   image_  ;
    Io       vector_ ; // the CIFF directory vector_.seek(0); length=victor_.getShort(image.endian()); size = 2 + length*10;
    Io       parent_ ; // the parent io object

    friend class CrwImage ;
};

class IFD
{
public:
    IFD(Image& image,size_t start,bool next=true)
    : image_  (image)
    , start_  (start)
    , io_     (image.io())
    , next_   (next)
    {};

    void     accept        (Visitor& visitor,const TagDict& tagDict=tiffDict);
    void     visitMakerNote(Visitor& visitor,DataBuf& buf,uint64_t count,uint64_t offset);
    void     setIo         (Io& io) { io_ = io; }
    void     setStart      (uint64_t start) { start_ = start; }

    Visits&  visits()    { return image_.visits()  ; }
    maker_e  maker()     { return image_.maker_    ; }
    TagDict& makerDict() { return image_.makerDict_; }
    endian_e endian()    { return image_.endian()  ; }
    void     next(bool next)     { next_ = next ; }

    uint64_t get4or8(DataBuf& dir,uint64_t jump,uint64_t offset,endian_e endian)
    {
        bool     bigtiff = image_.bigtiff_ ;
        offset          *= bigtiff ? 8 : 4 ;
        return   bigtiff ? getLong8(dir,jump+offset,endian) : getLong (dir,jump+offset,endian);
    }

private:
    bool     next_   ;
    Image&   image_  ;
    size_t   start_  ;
    Io&      io_     ;
};

// Concrete Images with an accept() method which calles the Vistor virtual functions (visitBegin/End, visitTag etc.)
class TiffImage : public Image
{
public:
    TiffImage(std::string path)
    : Image(path)
    , next_(true)
    {}
    TiffImage(Io& io,maker_e maker=kUnknown)
    : Image(io)
    , next_(false)
    { setMaker(maker);}

    bool valid()
    {
        if ( !valid_ ) {
            IoSave restore(io(),0);

            // read header
            DataBuf  header(16);
            io_.read(header);

            char c   = (char) header.pData_[0] ;
            char C   = (char) header.pData_[1] ;
            endian_  = c == 'M' ? keBig : keLittle;
            magic_   = getShort(header,2,endian_);
            bigtiff_ = magic_ == 43;
            start_   = bigtiff_ ? getLong8(header,8,endian_) : getLong (header,4,endian_);
            format_  = bigtiff_ ? "BIGTIFF"                  : "TIFF"                    ;
            header_  = " address |    tag                              |      type |    count |    offset | value" ;

            uint16_t bytesize = bigtiff_ ? getShort(header,4,endian_) : 8;
            uint16_t version  = bigtiff_ ? getShort(header,6,endian_) : 0;
                                             // Pano       Olym
            valid_ =  (magic_==42||magic_==43||magic_==85||magic_==0x4f52) && (c == C) && (c=='I'||c=='M') && bytesize == 8 && version == 0;
            // Panosonic have augmented tiffDict with their keys
            if ( magic_ == 85 ) {
                setMaker(kPano);
                for ( TagDict::iterator it = panoDict.begin() ; it != panoDict.end() ; it++ ) {
                    if ( it->first != ktGroup ) {
                        tiffDict[it->first] = it->second;
                    }
                }
            }
        }
        return valid_ ;
    }
    void accept(class Visitor& visitor);
    void accept(Visitor& visitor,TagDict& tagDict);

    bool bigtiff()  { return bigtiff_ ; }
private:
    bool next_;
};

class CrwImage : public Image
{
public:
    CrwImage(std::string path)
    : Image (path)
    , heap_ (*this,io_)
    {
        start_ = 0;
        setMaker(kCanon);
    }
    CrwImage(Io& io)
    : Image (io)
    , heap_ (*this,io_)
    {
        start_ = 0;
        setMaker(kCanon) ;
    }
    bool valid() {
        if ( valid_ ) return valid_;
        header_ = "    tag | mask | code | name                           |  kount | offset | value ";

        IoSave  restore(io(),0);
        bool    result = false;
        DataBuf buf(2+4+8); //xxlongHEAPCCRD xx = II or MM
        if ( io().good() ) {
            io().read(buf);

            char I = 'I';
            char M = 'M';
            char c = buf.pData_[0];
            char C = buf.pData_[1];
            result = ::memcmp(buf.pData_+6,"HEAPCCDR",8) == 0
                    && c==C && (c == I || c == M)
            ;
            if ( result ) {
                endian_ = c == I ? keLittle : keBig ;
                start_  = getLong(buf,2,endian_);
                format_ = "CRW";
                Io parent(io_,start_);
                heap_.setParent (parent); // the parent stream
            }
        }
        valid_= result;
        return result ;
    }
    CIFF heap_;

    virtual void accept(class Visitor& visitor)
    {
        if ( valid() ) {
            IoSave save(io(),start_);
            heap_.accept(visitor);
        } else {
            std::ostringstream os ; os << "expected " << format_ ;
            Error(kerInvalidFileFormat,io().path(), os.str());
        }
    }
};

class JpegImage : public Image
{
public:
    JpegImage(std::string path)
    : Image  (path)
    { init(); }
    JpegImage(Io& io,size_t start,size_t count)
    : Image(Io(io,start,count))
    { init(); }

    bool valid() {
        if ( !valid_ ) {
            IoSave   restore(io(),0);
            byte     h[2];
            io_.read(h,2);
            if ( h[0] == 0xff && h[1] == 0xd8 ) { // .JPEG
                start_  = 0;
                format_ = "JPEG";
                valid_  = true;
            } else if  ( h[0] == 0xff && h[1]==0x01 ) { // .EXV
                DataBuf buf(5);
                io_.read(buf);
                if ( buf.begins("Exiv2") ) {
                    start_  = 7;
                    format_ = "EXV";
                    valid_  = true;
                }
            }
            header_ = " address | marker       |  length | signature";
        }
        return valid_ ;
    }
    virtual void accept(class Visitor& v);

private:
    int advanceToMarker()
    {   // Search for 0xff
        while ( !io_.eof() && io_.getb() != 0xff) {}
        // Search for next byte which isn't 0xff
        int c = -1;
        while ( !io_.eof() && (c=io_.getb()) == 0xff) {}
        return io_.eof() ? -1 : c;
    };
    const byte     dht_      = 0xc4;
    const byte     dqt_      = 0xdb;
    const byte     dri_      = 0xdd;
    const byte     sos_      = 0xda;
    const byte     eoi_      = 0xd9;
    const byte     app0_     = 0xe0;
    const byte     com_      = 0xfe;

    // Start of Frame markers
    const byte     sof0_     = 0xc0;        // start of frame 0, baseline DCT
    const byte     sof15_    = 0xcf;        // start of frame 15, differential lossless, arithmetic coding

    // which markers have a length field?
    bool bHasLength_[256];

    void init()
    {
        endian_ = keLittle;
        start_  = 0       ;
        for (int i = 0; i < 256; i++) {
            bHasLength_[i] = (i >= sof0_ && i <= sof15_) || (i >= app0_ && i <= (app0_ | 0x0F))
            ||               (i == dht_  || i == dqt_    ||  i == dri_  || i == com_  || i == sos_)
            ;
        }
    }
};

class PngImage : public Image
{
public:
    PngImage(std::string path)
    : Image(path)
    {}
    PngImage(Io& io,size_t start,size_t count)
    : Image(Io(io,start,count))
    {}
    bool valid()
    {
        if ( !valid_ ) {
            IoSave   restore(io(),0);
            valid_  = true ;
            const byte pngHeader[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
            for ( size_t i = 0 ; valid_ && i < sizeof (pngHeader ); i ++) {
                valid_ = io().getb() == pngHeader[i];
            }
            start_  = 8       ;
            endian_ = keBig   ;
            format_ = "PNG"   ;
            header_  = "  address | chunk |  length |   checksum | data " ;
        }
        return valid_ ;
    }
    virtual void accept(class Visitor& v);
};

class IPTC : public Image
{
public:
    IPTC(std::string path)
    : Image(path)
    { }
    IPTC(Io& io)
    : Image(io)
    {}

    void accept(Visitor& visitor);

    bool valid()
    {
        if ( !valid_ ) {
            IoSave  restore(io(),0);
            valid_  = io().getb() == 0x1c; // must start with FS (field separator)
            endian_ = keBig ;
            header_ = "    Record | DataSet | Name                           | Length | Data" ;
            format_ = "IPTC";
            start_  = 0;
        }
        return valid_;
    }
};

void IPTC::accept(Visitor& visitor)
{
    if ( valid() ) {
        visitor.visitBegin(*this);

        IoSave restore(io(),start_);
        uint64_t length = io().size();
        DataBuf buff(length);
        io().read(buff);
//      out() << indent() << buff.toString(kttUByte,length);
        uint32_t i  =    0 ; // index
        uint32_t bs =    5 ; // blocksize
        uint8_t  fs = 0x1c ; // field seperator
        while (i+bs < length && ::getByte (buff,i) != fs ) i++; // find fs
        while (i+bs < length && ::getByte (buff,i) == fs ) {    // FS-RE-DS-short = byte, byte, byte, short ... data ....
            uint16_t record   = ::getByte (buff,i+1);
            uint16_t dataset  = ::getByte (buff,i+2);
            uint16_t len      = ::getShort(buff,i+3,keBig);
            visitor.visitIPTC(io(),*this,record,dataset,len,buff,i);
            i += bs + len;
        }
        visitor.visitEnd(*this);
    }
}

class C8BIM : public Image
{
public:
    C8BIM(std::string path)
    : Image(path)
    { }
    C8BIM(Io& io)
    : Image(io)
    {}

    void accept(Visitor& visitor);

    bool valid()
    {
        if ( !valid_ ) {
            endian_ = keBig ;
            IoSave  restore(io(),0);
            DataBuf bim(4);
            memcpy(bim.pData_,"8BIM",4);
            valid_  = io().getLong(endian_) == ::getLong(bim,0,endian_);
            header_ = "     offset |   kind | tagName                      | name |      len | data | " ;
            format_ = "8BIM";
            start_  = 0;
        }
        return valid_;
    }
};

void C8BIM::accept(Visitor& visitor)
{
    if (!valid_) valid();
    if (valid_ && !bVisualStudio) { // TODO: this code hangs in VisualStudio
        visitor.visitBegin((*this)); // tell the visitor

        IoSave  restore(io(),start_);
        DataBuf buff   (io().size()) ;
        io().read(buff) ;
    //  out() << indent() << buff.toString(kttUByte) << std::endl;
        uint32_t offset = 0  ;
        while ( offset+4 < buff.size_ ) {
            uint16_t kind = getShort       (buff,offset+4           ,endian_);
            DataBuf  name = getPascalString(buff,offset+6);
            uint32_t len  = getLong        (buff,offset+6+name.size_,endian_);
            uint32_t pad  = len%2?1:0;
            uint32_t data = (uint32_t)(4+2+4+name.size_); // "8BIM" + short (kind) + name + long (len)
            DataBuf  b(len);
            memcpy(b.pData_,buff.pData_+offset+data,len);
            visitor.visit8BIM(io(),*this,offset,kind,name,len,data,pad,b);
            if ( visitor.option() & kpsRecursive && kind == 0x0404) {
                Io   stream(io(),offset+data,len);
                IPTC iptc(stream);
                iptc.accept(visitor);
            }
            offset += len + pad + data ;
        }
        visitor.visitEnd((*this)); // tell the visitor
    }
}


class PsdImage : public Image
{
public:
    PsdImage(std::string path)
    : Image  (path)
    { }
    PsdImage(Io& io,size_t start,size_t count)
    : Image(Io(io,start,count))
    { }

    bool valid()
    {
        if ( !valid_ ) {
            endian_ = keBig ;
            IoSave   restore(io(),0);
            DataBuf  h(4); io_.read(h);
            uint16_t version = io_.getShort(endian_);

            if ( h.begins("8BPS") && version == 1 && io_.getLong(endian_) == 0 && io_.getShort(endian_) == 0  ) {
                start_  = 26;
                format_ = "PSD";
                valid_  = true;
            }
            ch_     = io_.getShort(endian_);
            width_  = io_.getLong (endian_);
            height_ = io_.getLong (endian_);
            bits_   = io_.getShort(endian_);
            col_    = io_.getLong (endian_);
            header_ = " address | length | data";
        }
        return valid_ ;
    }
    virtual void accept(class Visitor& v);

private:
    uint16_t ch_     ;
    uint32_t width_  ;
    uint32_t height_ ;
    uint16_t bits_   ;
    uint16_t col_    ;
};

void PsdImage::accept(Visitor& visitor)
{
    // Ensure that this is the correct image type
    if (!valid()) {
        std::ostringstream os ; os << "expected " << format_ ;
        Error(kerInvalidFileFormat,io().path(),os.str());
    }
    std::string msg = stringFormat("#ch = %d, width x height = %d x %d, bits/col = %d/%d",ch_,width_,height_,bits_,col_);
    visitor.visitBegin(*this,msg); // tell the visitor

    IoSave restore(io_,0) ;
    DataBuf  lBuff(4);
    uint64_t address = start_ ;
    for ( uint16_t i = 0 ; i <=2 ; i++ ) {
        io().seek(address);
        uint32_t length = io_.getLong(endian());
        visitor.visitResource(io_,*this,address);
        Io    bim(io_,address+4,length);
        C8BIM c8bim(bim);
        c8bim.accept(visitor);
        address += length + 4 ;
    }

    visitor.visitEnd  (*this); // tell the visitor
}

class PgfImage : public Image
{
public:
    PgfImage(std::string path)
    : Image  (path)
    { }
    PgfImage(Io& io,size_t start,size_t count)
    : Image(Io(io,start,count))
    { }

    bool valid()
    {
        if ( !valid_ ) {
            endian_ = keLittle ;
            IoSave   restore(io(),0);
            start_  = 8+16    ;
            DataBuf  h(start_);
            io_.read(h);

            if ( h.begins("PGF")   ) {
                start_  = 8+16;
                format_ = "PGF";
                valid_  = true;
            }
            headersize_ = getLong(h,  4,endian_);
            width_      = getLong(h,8 +0,endian_);
            height_     = getLong(h,8 +4,endian_);
            levels_     = getByte(h,8 +8);
            comp_       = getByte(h,8 +9);
            bpp_        = getByte(h,8+10);
            colors_     = getByte(h,8+11);
            mode_       = getByte(h,8+12);
            bpc_        = getByte(h,8+13);
            if ( mode_ == 2 ) start_ += 4*256; // start following color table
        }
        return valid_ ;
    }
    virtual void accept(class Visitor& v);

private:
    uint32_t headersize_ ;

    uint32_t width_  ;
    uint32_t height_ ;
    uint8_t  levels_ ;
    uint8_t  comp_   ;
    uint8_t  bpp_    ;
    uint8_t  colors_ ;
    uint8_t  mode_   ;
    uint8_t  bpc_    ;
};

void PgfImage::accept(Visitor& visitor)
{
    // Ensure that this is the correct image type
    if (!valid()) {
        std::ostringstream os ; os << "expected " << format_ ;
        Error(kerInvalidFileFormat,io().path(),os.str());
    }
    std::string msg = stringFormat("headersize = %d, width x height = %d x %d levels,comp = %d,%d bpp,colors = %d,%d mode,bpc = %d,%d start = %d",headersize_,width_,height_,levels_,comp_,bpp_,colors_,mode_,bpc_,start_);
    visitor.visitBegin(*this,msg); // tell the visitor

    PngImage(io(),start_,this->headersize_).accept(visitor);

    visitor.visitEnd  (*this); // tell the visitor
}

class Jp2Image : public Image
{
public:
    Jp2Image(std::string path)
    : Image(path)
    { init() ; }
    Jp2Image(Io& io,size_t start,size_t count)
    : Image(Io(io,start,count))
    { init(); }

    bool valid()
    {
        if ( !valid_ ) {
            start_ = 0;
            IoSave     restore (io(),start_);
            uint32_t   box    ;
            io().getLong(endian_); // length
            io().read   (&box,4) ; // box

            valid_ = boxName(box) == kJp2Box_jP ;
            if ( boxName(box) == kJp2Box_ftyp ) {
                valid_  = true ;
                io().read(&box,4);
                brand_ = boxName(box);
                format_ = "JP2 (" + brand_ + ")";
            }
            header_ = " address |   length |  box | uuid | data";
        }
        return valid_ ;
    }
    virtual void accept(class Visitor& v);

    std::string boxName(uint32_t box)
    {
        char           name[5];
        std::memcpy   (name,&box,4);
        name[4] = 0   ;
        return std::string(name) ;
    }
    std::string uuidName(DataBuf& data)
    {
        std::string uuid = data.toUuidString();
        return uuids_.find(uuid) != uuids_.end() ? uuids_[uuid]  : "";
    }
    std::string brand_      ;
    uint16_t    exifID_     ;
    uint32_t    exifOffset_ ;
    uint32_t    exifLength_ ;

    void init()
    {
        endian_     = keBig  ;
        format_     = "JP2"  ;
        brand_      = "jp2"  ;
        exifID_     = 0      ;
        exifLength_ = 0      ;
        exifOffset_ = 0      ;

        const char*  kJp2UuidExif  = "4a706754-6966-6645-7869-662d3e4a5032" ; // "JpgTiffExif->JP2";
        const char*  kJp2UuidIptc  = "33c7a4d2-b81d-4723-a0ba-f1a3e097ad38" ;
        const char*  kJp2UuidXmp   = "be7acfcb-97a9-42e8-9c71-999491e3afac" ;
        const char*  kJp2UuidCano  = "85c0b687-820f-11e0-8111-f4ce462b6a48" ;

        uuids_[kJp2UuidExif] = "exif" ;
        uuids_[kJp2UuidIptc] = "iptc" ;
        uuids_[kJp2UuidXmp ] = "xmp"  ;
        uuids_[kJp2UuidCano] = "cano" ;
    }

private:
    std::map<std::string,std::string> uuids_;

};

class ICC : public Image
{
public:
    ICC(std::string path)
    : Image(path)
    { init(); }
    ICC(Io& io,maker_e maker=kUnknown)
    : Image(io)
    { init(); }

    void init()
    {   depth_=0;
        valid_=false;
        endian_=keBig;
        format_ = "ICC";
        start_ =128;
    }

    void accept(class Visitor& visitor);

    bool valid()
    {
        if ( !valid_ ) {
            IoSave  restore(io(),0);
            valid_  = io().getLong(endian_) == io().size();
            header_ = "   sig |   offset |   length" ;
        }
        return valid_;
    }
};

void ICC::accept(class Visitor& visitor)
{
    if ( !valid_ ) valid();
    if (  valid_ ) {
        visitor.visitBegin((*this)); // tell the visitor

        IoSave restore(io(),start_);
        uint32_t nEntries = io().getLong(endian_);
        while  ( nEntries-- ) {
            DataBuf sig(5);
            io().read(sig.pData_,4);
            uint32_t offset = io().getLong(endian_);
            uint32_t length = io().getLong(endian_);
            visitor.visitICCTag(sig.pData_,offset,length);
        }
        visitor.visitEnd((*this)); // tell the visitor
    }
}

class RiffImage : public Image
{
public:
    RiffImage(std::string path)
    : Image(path)
    { init() ; }
    RiffImage(Io& io,maker_e maker=kUnknown)
    : Image(io)
    { init() ; }

    void init()
    {   depth_  = 0       ;
        valid_  = false   ;
        endian_ = keLittle;
        format_ = "RIFF"  ;
        start_  =   12    ;
    }

    void accept(class Visitor& visitor);

    bool valid()
    {
        if ( !valid_ ) {
            IoSave  restore(io(),0);
            DataBuf header(12);
            io().read(header);
            fileLength_   = ::getLong(header,4,endian_);
            valid_        =  header.begins("RIFF") && fileLength_ <= io().size();
            char             signature[5];
            format_       =  header.getChars(8,4,signature);
            header_       = " address | chunk |   length | data " ;
        }
        return valid_;
    }
private:
    uint32_t fileLength_ ;
};

void RiffImage::accept(class Visitor& visitor)
{
    if ( !valid_ ) valid();
    if (  valid_ ) {
        visitor.visitBegin((*this)); // tell the visitor

        IoSave   restore(io(),start_);
        uint64_t address = start_;
        DataBuf  riff(8);
        DataBuf  data(40);  // buffer to pass data to visitRiff()
        while (  address+8 < fileLength_+12 ) {
            visit(address);
            io().seek(address);
            io().read(riff);

            char        signature[5];
            std::string chunk   = riff.getChars(0,4,signature);
            uint32_t    length  = ::getLong(riff,4,endian_) ;
            uint64_t    pad     = length % 2 ? 1 : 0        ; // pad if length is odd
            uint64_t    next    = io().tell() + length +pad ;

            data.zero();
            io().read(data.pData_,length < data.size_?length:data.size_);
            visitor.visitRiff(address,chunk,length,data);

            if ( chunk == "XMP " || chunk == "ICCP" ) {
                DataBuf    Data(length);
                io().seek(address+8);
                io().read(Data);
                if ( chunk == "XMP "    ) visitor.visitXMP(Data);
                if ( chunk == "ICCP"    ) visitor.visitICC(Data);
            }
            if ( chunk == "EXIF" ) {
                Io tiff(io(),address+8,length);
                visitor.visitExif(tiff);
            }
            address = next ;
        }
        visitor.visitEnd((*this)); // tell the visitor
    }
}

class MrwImage : public Image
{
public:
    MrwImage(std::string path)
    : Image(path)
    {}
    MrwImage(Io& io,maker_e maker=kMino)
    : Image(io)
    {}


    void accept(class Visitor& visitor);

    bool valid()
    {
        if ( !valid_ ) {
            endian_ = keBig ;
            format_ = "MRW" ;
            start_  = 8     ;
            IoSave  restore(io(),0);
            DataBuf header(8);
            io().read(header);
            image_        = header.getLong(4,endian_);
            valid_        = header.getByte(0) == 0 && header.begins("MRM",1) && image_ <= io().size() ;
            header_       = " address | kind |   length | data ";
        }
        return valid_;
    }
private:
    uint32_t image_  ; // first byte of image
};

void MrwImage::accept(class Visitor& visitor)
{
    if ( !valid_ ) valid();
    if (  valid_ ) {
        visitor.visitBegin((*this)); // tell the visitor
        DataBuf image(40);
        IoSave restore(io(),image_+8);
        io().read(image);
        visitor.visitMrw(io(),*this,0,"MRM",image_,image);

        io().seek(start_);
        uint64_t    address = io().tell() ;
        while ( io().tell() < image_ ) {
            DataBuf     block(8);
            io().read  (block);
            
            uint32_t    length  = block.getLong(4,endian_);
            char        signature[4];
            std::string chunk = block.getChars(1,3,signature);
            
            DataBuf     data(length < 40 ? length : 40 );
            io().read(data);
            visitor.visitMrw(io(),*this,address,chunk,length,data);
            address += 8+length;
            io().seek(address);
        }
        
        visitor.visitEnd((*this)); // tell the visitor
    }
}

void JpegImage::accept(Visitor& visitor)
{
    // Ensure that this is the correct image type
    if (!valid()) {
        std::ostringstream os ; os << "expected " << format_ ;
        Error(kerInvalidFileFormat,io().path(),os.str());
    }
    IoSave save(io(),0);
    visitor.visitBegin((*this)); // tell the visitor

    enum                             // kes = Exif State
    { kesNone = 0                    // not reading exif
    , kesAdobe                       // in a chain of APP1/Exif__ segments
    , kesAgfa                        // in AGFA segments of 65535
    }          exifState = kesNone ;
    DataBuf    exif                ; // buffer to suck up exif data
    uint64_t   nExif     = 0       ; // Count the segments in Exif
    uint64_t   aExif     = 0       ; // Remember address of block0

    DataBuf    ICC                 ; // buffer to suck up ICC
    DataBuf    XMP                 ; // buffer to suck up XMP
    bool       bExtXMP   = false   ;

    // Step along linked list of segments
    bool     done = false;
    while ( !done ) {
        // step to next marker
        int  marker = advanceToMarker();
        if ( marker < 0 ) {
            Error(kerInvalidFileFormat,io().path());
        }

        size_t      address       = io_.tell()-2;
        DataBuf     buf(48);

        // Read size and signature
        uint64_t    bufRead       = io_.read(buf);
        uint16_t    length        = bHasLength_[marker] ? getShort(buf,0,keBig):0;
        bool        bAppn         = marker >= app0_ && marker <= (app0_ +15) ;
        bool        bApp2         = marker == app0_ +2 ;
        bool        bHasSignature = marker == com_ || bAppn ;
        std::string signature     = bHasSignature ? buf.binaryToString(2, buf.size_ - 2): "";

        bool        bExif         = bAppn && signature.size() >  6 && signature.find("Exif"       ) == 0;
        bool        bICC          = bAppn && signature.size() > 10 && signature.find("ICC_PROFILE") == 0;
        exifState                 = bExif       ? kesAdobe
                                  : (exifState == kesAdobe && length == 65535) ? kesAgfa
                                  : kesNone ;

        if ( exifState ) { // suck up the Exif data
            size_t chop = bExif ? 6 : 0 ;
            exif.read(io_,(address+2)+2+chop,length-2-chop); // read into memory
            if ( !nExif ++ ) aExif = (address+2)+2+chop ;
            if ( exifState == kesAgfa && length != 65535 && !bExif ) exifState = kesNone;
        }

        // deal with deferred Exif metadata
        if ( !exif.empty() && !exifState )
        {
            IoSave save(io_,aExif);
            Io     file(io_,aExif,exif.size_); // stream on the file
            Io     memory(exif);               // stream on memory buffer
            visitor.visitExif(nExif == 1 ? file :memory ); // tell the visitor
            exif.empty(true)  ; // empty the exif buffer
            nExif     = 0     ; // reset the block counter
        }
        // deal with deferred XMP and ICC
        if ( !XMP.empty() && !bAppn ) {
            visitor.visitXMP(XMP); // tell the visitor
            bExtXMP = false ;
            XMP.empty(true) ; // empty the exif buffer
        }
        if ( !ICC.empty() && !bApp2 ) {
            visitor.visitICC(ICC); // tell the visitor
            ICC.empty(true) ; // empty the exif buffer
        }
        visitor.visitSegment(io_,*this,address,marker,length,signature); // tell the visitor

        if ( bAppn ) {
            // http://www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMPSpecificationPart3.pdf p75
            // $ exiv2 -pS test/data/exiv2-bug922.jpg
            // STRUCTURE OF JPEG FILE: test/data/exiv2-bug922.jpg
            // address | marker     | length  | data
            //       0 | 0xd8 SOI   |       0
            //       2 | 0xe1 APP1  |     911 | Exif..MM.*.......%.........#....
            //     915 | 0xe1 APP1  |     870 | http://ns.adobe.com/xap/1.0/.<x:
            //    1787 | 0xe1 APP1  |   65460 | http://ns.adobe.com/xmp/extensio
            if ( signature.find("http://ns.adobe.com/x") == 0 ) {
                // extract XMP
                if (length > 0) {
                    io_.seek(io_.tell() - bufRead);
                    std::vector<byte> xmp(length + 1);
                    io_.read(&xmp[0], length);
                    int start = 0;

                    // http://wwwimages.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMPSpecificationPart3.pdf
                    // if we find HasExtendedXMP, set the flag and ignore this block
                    // the first extended block is a copy of the Standard block.
                    // a robust implementation allows extended blocks to be out of sequence
                    // we could implement out of sequence with a dictionary of sequence/offset
                    // and dumping the XMP in a post read operation similar to kpsIptcErase
                    // for the moment, dumping 'on the fly' is working fine
                    if (!bExtXMP) {
                        while (xmp.at(start)) {
                            start++;
                        }
                        start++;
                        const std::string xmp_from_start = binaryToString(
                            reinterpret_cast<byte*>(&xmp.at(0)),start, length - start);
                        if (xmp_from_start.find("HasExtendedXMP", start) != xmp_from_start.npos) {
                            start = length;  // ignore this segment, we'll get extended packet in following segments
                            bExtXMP = true;
                        }
                    } else {
                        start = 2 + 35 + 32 + 4 + 4;  // Adobe Spec, p19
                    }
                    XMP.read(io_,address+2+start,length - start); // read the XMP from the stream
                }
            }
            if ( bICC ) {
                uint16_t start = 2+14 ; // "\mark\length\ICC_PROFILE\0\C\Z" = \C: 1 byte chunk. \Z: 1 byte chunks.
                ICC.read(io_,address+2+start,length - start); // read the XMP from the stream
            }
            if ( signature.find("Photoshop 3.0") == 0 && visitor.option() & kpsRecursive) {
                uint16_t start = 2+2+14 ; // "\mark\length\Photoshop 3.0\08BIM..."
                Io    bim(io_,address+start,length-start);
                C8BIM c8bim(bim);
                c8bim.accept(visitor);
            }
        }

        // Jump past the segment
        io_.seek(address+2+length); // address is previous marker
        done = marker == eoi_ || marker == sos_ || io().eof();
    } // while !done

    visitor.visitEnd((*this)); // tell the visitor
}  // JpegImage::visit

// 4. Create concrete "visitors"
class ReportVisitor: public Visitor
{
public:
    ReportVisitor(std::ostream& out, PSOption option)
    : Visitor(out,option)
    {
        nm_[0xd8] = "SOI";
        nm_[0xd9] = "EOI";
        nm_[0xda] = "SOS";
        nm_[0xdb] = "DQT";
        nm_[0xdd] = "DRI";
        nm_[0xfe] = "COM";
        nm_[0xc4] = "DHT";

        // 0xe0 .. 0xef are APPn
        // 0xc0 .. 0xcf are SOFn (except 4)
        for (int i = 0; i <= 15; i++) {
            nm_[0xe0 + i] = stringFormat("APP%d",i);
            if (i != 4) {
                nm_[0xc0 + i] = stringFormat("SOF%d", i);
            }
        }

        for (int i = 0; i < 256; i++) {
            hasLength_[i] = (i >= sof0_ && i <= sof15_) || (i >= app0_ && i <= (app0_ | 0x0F)) ||
                            (i == dht_  || i == dqt_    || i == dri_   || i == com_  ||i == sos_);
        }
    }
    void visitChunk   (Io& io,Image& image,uint64_t address,char* chunk,uint32_t length,uint32_t checksum);
    void visitBox     (Io& io,Image& image,uint64_t address,uint32_t box,uint32_t length);

    void visitSegment (Io& io,Image& image,uint64_t address
             ,uint8_t marker,uint16_t length,std::string& signature);

    void visitBegin(Image& image,std::string msg="");
    void visitDirBegin(Image& image,uint64_t nEntries)
    {
        //size_t depth = image.depth();
        //out() << indent(depth) << stringFormat("+%d",nEntries) << std::endl;
    }
    void visitDirEnd(Image& image,uint64_t start)
    {
        // if ( start ) out() << std::endl;
    }
    void visitCiff
    ( Io&                   io
    , Image&                image
    , uint64_t              address
    ) {
        IoSave  restore(io,address);
    }
    void visitXMP (DataBuf& xmp);
    void visitICC (DataBuf& xmp);
    void visitExif(Io&      io );

    bool printTag(std::string& name)
    {
        return name.find(".0x") == std::string::npos
               ||      option()  & kpsUnknown
               ;
    }

    void visitTag     ( Io&  io,Image& image, uint64_t  address
                      , uint16_t         tag, type_e       type
                      , uint64_t       count, uint64_t offset
                      , DataBuf&         buf, const TagDict& tagDict);
    void visitICCTag  (const byte* tag,uint32_t offset,uint32_t length);
    void visitIPTC    (Io& io,Image& image
                      ,uint16_t record,uint16_t dataset,uint32_t len
                      ,DataBuf& buff,uint32_t offset);
    void visitResource(Io& io,Image& image,uint64_t address);
    void visit8BIM    (Io& io,Image& image,uint32_t offset
                      ,uint16_t kind,DataBuf& name,uint32_t len
                      ,uint32_t data,uint32_t pad,DataBuf& b);
    void visitRiff    (uint64_t address,std::string chunk
                      ,uint32_t length,DataBuf& data);
    void visitMrw     (Io& io,Image& image
                      ,uint64_t address,std::string chunk
                      ,uint32_t length,DataBuf& data);

    void visitEnd(Image& image)
    {
        if ( option() & kpsBasic || option() & kpsRecursive ) {
            out() << indent() << "END: " << image.path() << std::endl;
        }
        indent_--;
        image.depth_--;
    } // visitEnd

private:
    std::string    nm_       [256];
    bool           hasLength_[256];

    const byte     dht_      = 0xc4;
    const byte     dqt_      = 0xdb;
    const byte     dri_      = 0xdd;
    const byte     sos_      = 0xda;
    const byte     eoi_      = 0xd9;
    const byte     app0_     = 0xe0;
    const byte     com_      = 0xfe;

    // Start of Frame markers
    const byte     sof0_     = 0xc0;        // start of frame 0, baseline DCT
    const byte     sof15_    = 0xcf;        // start of frame 15, differential lossless, arithmetic coding

};

void CIFF::accept(Visitor& visitor)
{
    IoSave restore(vector_,0);
    uint16_t length = vector_.getShort(image().endian());

    size_t   depth = image().depth();
    visitor.visitBegin(image());

    DataBuf buf(10);
    for ( int i = 0 ; i < length ; i++ ) {
        vector_.read(buf);
        uint16_t    tag    = getShort(buf,0,image_.endian());
        uint32_t    count  = getLong (buf,2,image_.endian());
        uint32_t    offset = getLong (buf,6,image_.endian());
        std::string mask   = tag & kStg_InRecordEntry ? "I" : "H";
        uint16_t    data   = tag & kcDataTypeMask;
        mask   +=   data == kcAscii ? 'A'
                :   data == kcWord  ? 'W'
                :   data == kcDword ? 'D'
                :   data == kcHTP1  ? '1'
                :   data == kcHTP2  ? '2'
                :   ' '
                ;

        uint32_t    kount = tag & kStg_InRecordEntry ? 8 : count ;
        uint32_t    code  = tag & kcIDCodeMask                 ;
        uint32_t    Offset= tag&kStg_InRecordEntry && kount <= 8 ? 12345678 : offset;
        std::string offst = kount > 8 ? stringFormat("%6d",offset) : stringFormat("%6s","");
        bool        bLF   = true ; // line ending needed
        std::cout << ::indent(depth)<< stringFormat(" %6#x | %-4s | %4d | %-30s | %6d | %s | ",tag,mask.c_str(),code,tagName(tag,crwDict,28).c_str(),kount,offst.c_str()) ;

        if ( tag == 0x2008 )        {  // ThumbnailImage
            std::cout << std::endl;
            bLF = false ;
            JpegImage jpeg(parent_,offset,count);
            jpeg.accept(visitor);
        } else if ( Offset == 12345678 || tag&kDT_ASCII ) { // little binary record or ascii
            DataBuf buf(kount);
            if ( count > 8 ) {
                IoSave  save(parent_,offset);
                parent_.read(buf);
            } else {
                buf.copy(count);
                buf.copy(offset,4);
            }
            std::cout << chop(tag&kDT_ASCII ? buf.toString(kttAscii, count,image().endian()) : buf.binaryToString(),40);
        } else if ( tag == 0x300a || tag == 0x300b ) { // ImageSpec || ExifInformation
            std::cout << std::endl;
            bLF = false ;
            dumpImageSpec(visitor,tag,offset,count,depth+5);
        } else if ( count < 500 ){ // Some stuff
            DataBuf buf(count);
            IoSave  save(parent_,offset);
            parent_.read(buf);
            std::cout << chop(buf.binaryToString(),40);
        }
        if ( bLF ) std::cout << std::endl ;
    }
    visitor.visitEnd(image());
}

void IFD::visitMakerNote(Visitor& visitor,DataBuf& buf,uint64_t count,uint64_t offset)
{
    if ( image_.maker_ == kNikon ) {
        // Nikon MakerNote is embeded tiff `II*_....` 10 bytes into the data!
        size_t punt = buf.strequals("Nikon") ? 10 : 0 ;
        Io     io(io_,offset+punt,count-punt);
        TiffImage makerNote(io,image_.maker_);
        makerNote.accept(visitor,makerDict());
    } else if ( image_.maker_ == kAgfa && buf.strequals("ABC") ) {
        // Agfa  MakerNote is an IFD `ABC_IIdL...`  6 bytes into the data!
        ImageEndianSaver save(image_,keLittle);
        IFD makerNote(image_,offset+6,false);
        makerNote.accept(visitor,makerDict());
    } else if ( image_.maker_ == kApple && buf.strequals("Apple iOS")) {
        // Apple  MakerNote is an IFD `Apple iOS__.MM_._._.___.___._._.__..`  26 bytes into the data!
        ImageEndianSaver save(image_,keBig);
        IFD makerNote(image_,offset+26,false);
        makerNote.accept(visitor,makerDict());
    } else if ( image_.maker_ == kMino ) {
        ImageEndianSaver save(image_,keBig);  // Always bigEndian
        IFD makerNote(image_,offset,false);
        makerNote.accept(visitor,makerDict());
    } else if ( image_.maker_ == kOlym ) {
        Io     io(io_,offset,count);
        TiffImage makerNote(io,image_.maker_);
        makerNote.start_ = 12  ; // "OLYMPUS\0II\0x3\0x0"E#
        makerNote.valid_ = true; // Valid without magic=42
        makerNote.accept(visitor,makerDict());
    } else {
        bool   bNext = maker()  != kSony;                                        // Sony no trailing next
        size_t punt  = maker()  == kSony && buf.strequals("SONY DSC ") ? 12 : 0; // Sony 12 byte punt
        IFD makerNote(image_,offset+punt,bNext);
        makerNote.accept(visitor,makerDict());
    }
} // visitMakerNote

void IFD::accept(Visitor& visitor,const TagDict& tagDict/*=tiffDict*/)
{
    IoSave   save(io_,start_);
    bool     bigtiff = image_.bigtiff();
    endian_e endian  = image_.endian();

    if ( !image_.depth_ ) image_.visits().clear();
    visitor.visitBegin(image_);
    if ( image_.depth_ > 100 ) Error(kerCorruptedMetadata) ; // weird file

    // buffer
    DataBuf  entry(bigtiff ? 20 : 12);
    uint64_t start=start_;
    while  ( start ) {
        // Read top of directory
        io_.seek(start);
        io_.read(entry.pData_, bigtiff ? 8 : 2);
        uint64_t nEntries = bigtiff ? getLong8(entry,0,endian) : getShort(entry,0,endian);

        if ( nEntries > 500 ) Error(kerTiffDirectoryTooLarge,nEntries);
        visitor.visitDirBegin(image_,nEntries);
        uint64_t a0 = start + (bigtiff?8:2) + nEntries * entry.size_; // addresss to read next

        // Run along the directory
        for ( uint64_t i = 0 ; i < nEntries ; i ++ ) {
            const uint64_t address = start + (bigtiff?8:2) + i* entry.size_ ;
            image_.visit(address); // never visit the same place twice!
            io_.seek(address);

            io_.read(entry);
            uint16_t tag    = getShort(entry,  0,endian);
            type_e   type   = getType (entry,  2,endian);
            uint64_t count  = get4or8 (entry,4,0,endian);
            uint64_t offset = get4or8 (entry,4,1,endian);

            if ( !typeValid(type,bigtiff) ) {
                Error(kerInvalidTypeValue,type);
            }

            uint64_t size   = typeSize(type) ;
            size_t   alloc  = size*count     ;
            DataBuf  buff(alloc);
            if ( alloc <= (bigtiff?8:4) ) {
                buff.copy(&offset,size*count);
            } else {
                IoSave save(io_,offset);
                io_.read(buff);
            }
            if ( tagDict == tiffDict && tag == ktMake ) image_.setMaker(buff);
            visitor.visitTag(io_,image_,address,tag,type,count,offset,buff,tagDict);  // Tell the visitor

            // recursion anybody?
            if ( tagDict == tiffDict ) {
                Io       io(io_,offset,count);
                switch ( tag ) {
                    case ktXMLPacket  : visitor.visitXMP(buff)   ; break;
                    case ktICCProfile : ICC (io).accept(visitor) ; break;
                    case ktIPTCNAA    : IPTC(io).accept(visitor) ; break;
                }
            }

            if ( type == kttIfd ) {
                for ( uint64_t i = 0 ; i < count ; i++ ) {
                    offset = get4or8 (buff,0,i,endian);
                    IFD(image_,offset,false).accept(visitor,ifdDict(image_.maker_,tag,makerDict()));
                }
            } else switch ( tag ) {
                case ktGps       : IFD(image_,offset,false).accept(visitor,gpsDict );break;
                case ktExif      : IFD(image_,offset,false).accept(visitor,exifDict);break;
                case ktMakerNote :         visitMakerNote(visitor,buff,count,offset);break;
                default          : /* do nothing                                  */;break;
            }
        } // for i < nEntries

        start = 0; // !stop
        if ( next_ ) {
            io_.seek(a0);
            io_.read(entry.pData_, bigtiff?8:4);
            start = bigtiff?getLong8(entry,0,endian):getLong(entry,0,endian);
        }
        visitor.visitDirEnd(image_,start);
    } // while start != 0

    visitor.visitEnd(image_);
} // IFD::accept

void TiffImage::accept(class Visitor& visitor)
{
    accept(visitor,tiffDict);
}

void TiffImage::accept(Visitor& visitor,TagDict& tagDict)
{
    if ( valid() ) {
        IFD ifd(*this,start_,next_);
        ifd.accept(visitor,tagDict);
    } else {
        std::ostringstream os ; os << "expected " << format_ ;
        Error(kerInvalidFileFormat,io().path(), os.str());
    }
} // TiffImage::accept

void PngImage::accept(class Visitor& v)
{
    if ( valid() ) {
        v.visitBegin(*this);
        IoSave restore(io(),start_);
        uint64_t address = start_ ;
        while (  address < io().size() ) {
            io().seek(address );
            uint32_t  length  = io().getLong(endian_);
            uint64_t  next    = address + length + 12;
            char      chunk  [5] ;
            io().read(chunk  ,4) ;
            chunk[4]        = 0  ; // nul byte

            io().seek(next-4);                            // jump over data to checksum
            uint32_t  chksum  = io().getLong(endian_);
            v.visitChunk(io(),*this,address,chunk,length,chksum); // tell the visitor
            address = next ;
        }
        v.visitEnd(*this);
    }
}

void Jp2Image::accept(class Visitor& v)
{
    if ( !valid() ) return ;

    if ( !depth_ ) visits_.clear() ;
    v.visitBegin(*this);
    IoSave restore(io(),start_);
    uint64_t address = start_ ;
    while (  address < io().size() ) {
        io().seek(address );
        uint32_t  length  = io().getLong(endian_);
        uint32_t  box     ;
        io().read(&box,4) ;

        if ( length > io().size() ) {
            // Error(kerCorruptedMetadata);
        }
        DataBuf  data(length-8);
        uint32_t skip      = 0 ;
        if ( length ) {
            IoSave restore(io(),io().tell());
            io().read(data);
        }

        v.visitBox(io(),*this,address,box,length); // tell the visitor

        // 8.11.3.1
        if ( boxName(box) == "iloc" ) { // 0 1 0 0 0 0 0 1 0 0 42 179 0 0 46 167 a,b= 10931,11943
            uint8_t  version = 0 ;
            uint32_t flags   = 0 ;
            if ( fullBox(box) ) {
                skip += 4 ;
                flags = io().getLong(keBig); // version/flags
                version = (int8_t ) flags >> 24 ;
                version &= 0x00ffffff ;
            }
            uint16_t u          = getByte(data,skip) ; skip++;
            uint16_t offsetSize = u >> 4  ;
            uint16_t lengthSize = u & 0xF ;
            
            uint16_t indexSize  = 0       ;
                     u          = getByte(data,skip) ; skip++ ;
            if ( version == 1 || version == 2 ) {
                indexSize = u & 0xF ;
            }
            // uint16_t baseOffsetSize = u >> 4;
            
            uint32_t itemCount  = version < 2 ? getShort(data,skip,keBig) : getLong(data,skip,keBig);
            skip               += version < 2 ?               2           :         4               ;
            if ( offsetSize == 4 && lengthSize == 4 && ((length-16) % itemCount) == 0 ) {
                uint32_t step = (length-16)/itemCount                  ; // length of data per item.
                uint32_t base = skip;
                for ( uint32_t i = 0 ; i < itemCount ; i++ ) {
                    skip=base+i*step ; // move in 16 or 14 byte steps
                    uint32_t ID     = version > 2 ? getLong(data,skip,keBig) : getShort(data,skip,keBig);
                    uint32_t offset = getLong(data,skip+step-8,keBig);
                    uint32_t ldata  = getLong(data,skip+step-4,keBig);
                    v.out() << v.indent() << stringFormat("%8d | %8d |  ext | %4d | %6d,%6d",address+skip,step,ID,offset,ldata) << std::endl;
                    if ( ID == exifID_ ) {
                        exifLength_ = ldata ;
                        exifOffset_ = offset ;
                    }
                }
            }
        }
        // 8.11.6.2
        if ( boxName(box) == "infe" ) { // .__._.__hvc1_ 2 0 0 1 0 1 0 0 104 118 99 49 0
                             getLong (data,skip,keBig) ; skip+=4;
            uint16_t   ID =  getShort(data,skip,keBig) ; skip+=2;
                             getShort(data,skip,keBig) ; skip+=2; // protection
            std::string name((const char*)data.pData_+skip);
            if ( name == "Exif" ) {
                exifID_ = ID ;
            }
        }

        // recursion if superbox
        if ( superBox(box) ) {
            uint64_t skip    = 0 ;
            uint8_t  version = 0 ;
            uint32_t flags   = 0 ;
            uint32_t nEntries= 0 ;
            bool     bRecurse= true;
            if ( fullBox(box) ) {
                skip += 4 ;
                flags = io().getLong(keBig); // version/flags
                version = (int8_t ) flags >> 24 ;
                version &= 0x00ffffff ;
            }

            // 8.11.6.1.
            if ( boxName(box) == "iinf" ) {
                nEntries = version ? io().getLong(keBig) : io().getShort(keBig);
                skip    += version ?         4           :         2           ;
            } else if ( boxName(box) == "iloc" ) {
                bRecurse = false; // we've already searched this box.
            }
            if ( bRecurse ) {
                Jp2Image jp2(io(),io().tell(),length-8-skip);
                jp2.valid_=true;
                jp2.exifID_ = exifID_;
                jp2.accept(v);
                if ( jp2.exifID_ ) {
                    exifID_     = jp2.exifID_;
                }
                if ( jp2.exifLength_ || jp2.exifOffset_ ) {
                    exifLength_ = jp2.exifLength_;
                    exifOffset_ = jp2.exifOffset_;
                }
            }
        }

        // recursion?
        if ( boxName(box) == "uuid" ) {
            DataBuf uuid(16);
            io().read(uuid);
            if ( uuidName(uuid) == "cano" ) {
                Jp2Image jp2(io(),io().tell(),length-16);
                jp2.valid_=true;
                jp2.accept(v);
            }
        } else if ( boxName(box) == "mdat" && (v.option() & kpsRecursive) && exifID_ ) {
            // TODO:  We're not storing exifOffset_/exifLength_ correctly
            Io t1(io(),exifOffset_+10,exifLength_-10);
            Io t2(io(),exifOffset_+ 4,exifLength_ -4);
            TiffImage tiff1(t1);
            TiffImage tiff2(t2);
            if ( tiff1.valid() ) tiff1.accept(v);
            if ( tiff2.valid() ) tiff2.accept(v);
        }
        address = (boxName(box) == kJp2Box_jp2c
               ||  boxName(box) == kJp2Box_mdat) ? io().size()
                : address + length;
    }
    v.visitEnd(*this);
} // Jp2Image::accept()

void ReportVisitor::visitIPTC(Io& io,Image& image
                          ,uint16_t record,uint16_t dataset,uint32_t len
                          ,DataBuf& buff,uint32_t offset)
{
    if ( option() & (kpsBasic|kpsRecursive) ) {
        TagDict& iptcDict = iptcDicts.find(record) != iptcDicts.end() ? iptcDicts[record] : iptc0;
        std::string tag  = tagName(dataset,iptcDict,30,"Iptc");
        if ( printTag(tag) ) {
            out() << indent() << stringFormat("    %6d | %7d | %-30s | %6d | ", record, dataset,tag.c_str(),len)
                  << chop(::binaryToString(buff.pData_,offset+5,len),60) << std::endl;
        }
    }
}

void ReportVisitor::visitSegment(Io& io,Image& image,uint64_t address
         ,uint8_t marker,uint16_t length,std::string& signature)
{
    if ( option() & (kpsBasic|kpsRecursive) ) {
        DataBuf buf( length < 40 ? length : 40 );
        IoSave  save(io,address+4);
        io.read(buf);
        std::string value = buf.toString(kttUndefined,buf.size_,image.endian());
        if ( option() & (kpsBasic | kpsRecursive) ) {
            out() <<           stringFormat("%8ld | 0xff%02x %-5s", address,marker,nm_[marker].c_str())
                  << (length ? stringFormat(" | %7d | %s", length,value.c_str()) : "")
                  << std::endl;
        }
    }
}

void ReportVisitor::visitICCTag(const byte* tag,uint32_t offset,uint32_t length)
{
    if ( option() & (kpsBasic | kpsRecursive) ) {
        out() << indent() << stringFormat("%6s | %8d | %8d ",tag,offset,length) << std::endl;
    }
}

void ReportVisitor::visitBegin(Image& image,std::string msg)
{
    image.depth_++;
    indent_++;
    if ( option() & (kpsBasic|kpsRecursive) ) {
        char c = image.endian() == keBig ? 'M' : 'I';
        out() << indent() << stringFormat("STRUCTURE OF %s FILE (%c%c): ",image.format().c_str(),c,c) <<  image.io().path() << std::endl;
        if ( msg.size() ) out() << indent() << msg << std::endl;
        if ( image.header_.size() ) {
            out() << indent() << image.header_ << std::endl;
        }
    }
}

void ReportVisitor::visitTag
( Io&            io
, Image&         image
, uint64_t       address
, uint16_t       tag
, type_e         type
, uint64_t       count
, uint64_t       offset
, DataBuf&       buf
, const TagDict& tagDict
) {
    if ( option() & (kpsBasic|kpsRecursive) ) {
        std::string offsetS ;
        if ( typeSize(type)*count > (image.bigtiff_?8:4) ) {
            std::ostringstream os ;
            os  <<  offset;
            offsetS         = os.str();
        }

        std::string    name = tagName(tag,tagDict,28);
        std::string   value = buf.toString(type,count,image.endian_);

        if ( printTag(name) ) {
            out() << indent()
                  << stringFormat("%8u | %#06x %-28s |%10s |%9u |%10s | "
                        ,address,tag,name.c_str(),::typeName(type),count,offsetS.c_str())
                  << chop(value,40)
                  << std::endl
            ;
            if ( makerTags.find(name) != makerTags.end() ) {
                for (Field field : makerTags[name] ) {
                    std::string n      = join(groupName(tagDict),field.name(),28);
                    endian_e    endian = field.endian() == keImage ? image.endian() : field.endian();
                    out() << indent()
                          << stringFormat("%8u | %#06x %-28s |%10s |%9u |%10s | "
                                         ,offset+field.start(),tag,n.c_str(),typeName(field.type()),field.count(),"")
                          << chop(buf.toString(field.type(),field.count(),endian,field.start()),40)
                          << std::endl
                    ;
                }
            }
        }
    }
} // visitTag

void ReportVisitor::visitXMP(DataBuf& xmp)
{
    if ( option() & kpsXMP ) out() << xmp.pData_;
}

void ReportVisitor::visitICC(DataBuf& icc)
{
    Io  profile(icc);
    switch ( option() ) {
        case kpsIcc        : out().write((const char*)icc.pData_,icc.size_); break;
        case kpsRecursive  : ICC(profile).accept(*this)                    ; break;
        default            : /* do nothing */                              ; break;
    }
}

void ReportVisitor::visitExif(Io& io)
{
    if ( option() & kpsRecursive ) {
        // Beautiful.  io is a tiff file, call TiffImage::accept(visitor)
        TiffImage(io).accept(*this);
    }
}

void ReportVisitor::visitChunk(Io& io,Image& image,uint64_t address
                        ,char* chunk,uint32_t length,uint32_t chksum)
{
    IoSave save(io,address+8);
    if ( option() & (kpsBasic | kpsRecursive) ) {
        DataBuf   data(length > 40 ? 40 : length ); // read enougth data for reporting purposes
        io.read(data);
        out() << indent() << stringFormat(" %8d |  %s | %7d | %#10x | ",address,chunk,length,chksum);
        out() << data.toString(kttUndefined,data.size_,image.endian()) << std::endl;
    }

    if ( option() & kpsRecursive && std::strcmp(chunk,"eXIf") == 0 ) {
        DataBuf   data(length);  // read the whole chunk
        io.read(data);
        Io        tiff(io,address+8,length);
        TiffImage(tiff).accept(*this);
    }

    if ( option() & kpsXMP && std::strcmp(chunk,"iTXt")==0 ) {
        DataBuf   data(length); // read the whole chunk
        io.read(data);
        if ( data.strcmp("XML:com.adobe.xmp")==0 ) {
            out() << data.pData_+22 ;
        }
    }
}

void ReportVisitor::visitResource(Io& io,Image& image,uint64_t address)
{
    IoSave   restore(io,address);
    uint32_t length = io.getLong(image.endian());
    DataBuf  buff(length > 40 ? 40 : length);
    io.read(buff);
    out() << indent() << stringFormat("%8d | %6d | ",address,length) << buff.binaryToString() << std::endl;
}

void ReportVisitor::visit8BIM(Io& io,Image& image,uint32_t offset
                ,uint16_t kind,DataBuf& name,uint32_t len,uint32_t data,uint32_t pad,DataBuf& b)
{
    std::string tag = ::tagName(kind,psdDict,40,"PSD");
    if ( printTag(tag) ) {
        out() << indent() << stringFormat("   %8d | %06#x | %-28s | %4s | %8d | %2d+%1d | "
                            ,offset,kind,tag.c_str(),(char*)name.pData_,len,data,pad)
        <<        b.binaryToString(0,len>40?40:len+data+pad)
        << std::endl;
    }
}

void ReportVisitor::visitRiff(uint64_t address,std::string chunk
                ,uint32_t length,DataBuf& data)
{
    if ( option() & (kpsBasic | kpsRecursive) ) {
        out() << indent() << stringFormat("%8d | %4s  | %8d | ",address,chunk.c_str(),length)
              << snip(data.binaryToString(),length) << std::endl;
    }
}

void ReportVisitor::visitMrw(Io& io,Image& image,uint64_t address,std::string chunk
                ,uint32_t length,DataBuf& data)
{
    if ( option() & (kpsBasic | kpsRecursive) ) {
        out() << indent() << stringFormat("%8d | %4s | %8d | ",address,chunk.c_str(),length)
              << snip(data.binaryToString(),length) << std::endl;
    }
    
    if ( kpsRecursive && chunk == "TTW" ) {
        Io tiff(io,address+8,length);
        visitExif(tiff);
    }

}

void ReportVisitor::visitBox(Io& io,Image& image,uint64_t address
                            ,uint32_t box,uint32_t length)
{
    IoSave save(io,address+8);
    length -= 8              ;
    DataBuf data(length);
    io.read(data);

    std::string name = image.boxName (box);
    std::string uuid = name == "uuid" ? image.uuidName(data) : "";
    if ( name == "uuid" && !uuid.size() ) {
        std::cout << "unrecognised uuid = " << data.toUuidString() << std::endl;
    }

    if ( option() & (kpsBasic | kpsRecursive) ) {
        out() << indent() << stringFormat("%8d | %8d | %4s | %4s | ",address,length,name.c_str(),uuid.c_str() );
        uint64_t start    = uuid.size() ? 16 : 0;
        if ( length > 40+start ) length = 40;
        out() << data.toString(kttUndefined,length,image.endian(),start)
              << " " << data.toString(kttUByte,length,image.endian(),start)
              << std::endl;
    }
    if ( option() & kpsRecursive ){
        if ( uuid == "exif" ) {
            Io        tiff(io,address+8+16,data.size_-16); // uuid is 16 bytes (128 bits)
            TiffImage(tiff).accept(*this);
        } else {
            std::map<std::string,TagDict> dicts ;
            dicts["CMT1"] = tiffDict;
            dicts["CMT2"] = exifDict;
            dicts["CMT3"] = canonDict;
            dicts["CMT4"] = gpsDict;
            if ( dicts.find(name) != dicts.end() ) {
                Io        tiff(io,address+8,data.size_);
                TiffImage(tiff).accept(*this,dicts[name]);
            }
        }
    }

    if ( option() & kpsXMP && uuid == "xmp " ) {
        out() << data.pData_+17 ;
    }
}

std::unique_ptr<Image> ImageFactory(std::string path)
{
    TiffImage tiff(path); if ( tiff.valid() ) return std::unique_ptr<Image> (new TiffImage(path));
    JpegImage jpeg(path); if ( jpeg.valid() ) return std::unique_ptr<Image> (new JpegImage(path));
    CrwImage  crw (path); if (  crw.valid() ) return std::unique_ptr<Image> (new  CrwImage(path));
    PngImage  png (path); if (  png.valid() ) return std::unique_ptr<Image> (new  PngImage(path));
    Jp2Image  jp2 (path); if (  jp2.valid() ) return std::unique_ptr<Image> (new  Jp2Image(path));
    ICC       icc (path); if (  icc.valid() ) return std::unique_ptr<Image> (new  ICC     (path));
    PsdImage  psd (path); if (  psd.valid() ) return std::unique_ptr<Image> (new  PsdImage(path));
    PgfImage  pgf (path); if (  pgf.valid() ) return std::unique_ptr<Image> (new  PgfImage(path));
    MrwImage  mrw (path); if (  mrw.valid() ) return std::unique_ptr<Image> (new  MrwImage(path));
    RiffImage riff(path); if ( riff.valid() ) return std::unique_ptr<Image> (new RiffImage(path));
    return NULL;
}

void init(); // prototype

int main(int argc,const char* argv[])
{
    init();

    int rc = 0;
    if ( argc == 2 || argc == 3 ) {
        // Parse the visitor options
        PSOption option = kpsBasic;
        if ( argc == 3 ) {
            std::string arg(argv[1]);
            option  = arg.find("R") != std::string::npos ? kpsRecursive
                    : arg.find("X") != std::string::npos ? kpsXMP
                    : arg.find("S") != std::string::npos ? kpsBasic
                    : arg.find("C") != std::string::npos ? kpsIcc
                    : option
                    ;
            if ( arg.find("U") != std::string::npos ) option |= kpsUnknown;
            if ( arg.find("I") != std::string::npos ) option |= kpsIptc   ;
        }

        ReportVisitor visitor(std::cout,option);

        // Open the image
        std::string             path  = argv[argc-1];
        std::unique_ptr<Image> pImage = ImageFactory(path);
        if (   pImage ) pImage->accept(visitor);
        else            Error(kerUnknownFormat,path);
    } else {
        std::cout << "usage: " << argv[0] << " [ { U | S | R | X | C | I } ] path" << std::endl;
        rc = 1;
    }
    return rc;
}

// simply data.  Nothing interesting.
void init()
{
    if ( tiffDict.size() ) return; // don't do this twice!

    tiffDict  [ktGroup ] = "Image";
    tiffDict  [ 0x83bb ] = "IPTCNAA";
    tiffDict  [ 0x02bc ] = "XMLPacket";
    tiffDict  [ 0x8773 ] = "InterColorProfile";
    tiffDict  [ 0x8769 ] = "ExifTag";
    tiffDict  [ 0x014a ] = "SubIFD";
    tiffDict  [ 0x00fe ] = "NewSubfileType";
    tiffDict  [ 0x0100 ] = "ImageWidth";
    tiffDict  [ 0x0101 ] = "ImageLength";
    tiffDict  [ 0x0102 ] = "BitsPerSample";
    tiffDict  [ 0x0103 ] = "Compression";
    tiffDict  [ 0x0106 ] = "PhotometricInterpretation";
    tiffDict  [ 0x010e ] = "ImageDescription";
    tiffDict  [ 0x010f ] = "Make";
    tiffDict  [ 0x0110 ] = "Model";
    tiffDict  [ 0x0111 ] = "StripOffsets";
    tiffDict  [ 0x0112 ] = "Orientation";
    tiffDict  [ 0x0115 ] = "SamplesPerPixel";
    tiffDict  [ 0x0116 ] = "RowsPerStrip";
    tiffDict  [ 0x0117 ] = "StripByteCounts";
    tiffDict  [ 0x011a ] = "XResolution";
    tiffDict  [ 0x011b ] = "YResolution";
    tiffDict  [ 0x011c ] = "PlanarConfiguration";
    tiffDict  [ 0x0128 ] = "ResolutionUnit";
    tiffDict  [ 0x0131 ] = "Software";
    tiffDict  [ 0x0132 ] = "DateTime";
    tiffDict  [ 0x013b ] = "Artist";
    tiffDict  [ 0x0213 ] = "YCbCrPositioning";

    exifDict  [ktGroup ] = "Photo";
    exifDict  [ 0x9286 ] = "UserComment";
    exifDict  [ 0x927c ] = "MakerNote";
    exifDict  [ 0x829a ] = "ExposureTime";
    exifDict  [ 0x829d ] = "FNumber";
    exifDict  [ 0x8822 ] = "ExposureProgram";
    exifDict  [ 0x8827 ] = "ISOSpeedRatings";
    exifDict  [ 0x8830 ] = "SensitivityType";
    exifDict  [ 0x9000 ] = "ExifVersion";
    exifDict  [ 0x9003 ] = "DateTimeOriginal";
    exifDict  [ 0x9004 ] = "DateTimeDigitized";
    exifDict  [ 0x9101 ] = "ComponentsConfiguration";
    exifDict  [ 0x9102 ] = "CompressedBitsPerPixel";
    exifDict  [ 0x9204 ] = "ExposureBiasValue";
    exifDict  [ 0x9205 ] = "MaxApertureValue";
    exifDict  [ 0x9207 ] = "MeteringMode";
    exifDict  [ 0x9208 ] = "LightSource";
    exifDict  [ 0x9209 ] = "Flash";
    exifDict  [ 0x920a ] = "FocalLength";
    exifDict  [ 0xa000 ] = "FlashpixVersion";
    exifDict  [ 0xa001 ] = "ColorSpace";
    exifDict  [ 0xa002 ] = "PixelXDimension";
    exifDict  [ 0xa003 ] = "PixelYDimension";
    exifDict  [ 0xa005 ] = "InteropTag";
    exifDict  [ 0x0001 ] = "InteropIndex";
    exifDict  [ 0x0002 ] = "InteropVersion";
    exifDict  [ 0xa300 ] = "FileSource";
    exifDict  [ 0xa301 ] = "SceneType";
    exifDict  [ 0xa401 ] = "CustomRendered";
    exifDict  [ 0xa402 ] = "ExposureMode";
    exifDict  [ 0xa403 ] = "WhiteBalance";
    exifDict  [ 0xa406 ] = "SceneCaptureType";
    exifDict  [ 0xa408 ] = "Contrast";
    exifDict  [ 0xa409 ] = "Saturation";
    exifDict  [ 0xa40a ] = "Sharpness";
    exifDict  [ 0xc4a5 ] = "PrintImageMatching";

    nikonDict [ktGroup ] = "Nikon";
    nikonDict [ 0x0001 ] = "Version";
    nikonDict [ 0x0002 ] = "ISOSpeed";
    nikonDict [ 0x0004 ] = "Quality";
    nikonDict [ 0x0005 ] = "WhiteBalance";
    nikonDict [ 0x0007 ] = "Focus";
    nikonDict [ 0x0008 ] = "FlashSetting";
    nikonDict [ 0x0009 ] = "FlashDevice";
    nikonDict [ 0x000b ] = "WhiteBalanceBias";
    nikonDict [ 0x000c ] = "WB_RBLevels";
    nikonDict [ 0x000d ] = "ProgramShift";
    nikonDict [ 0x000e ] = "ExposureDiff";
    nikonDict [ 0x0012 ] = "FlashComp";
    nikonDict [ 0x0013 ] = "ISOSettings";
    nikonDict [ 0x0016 ] = "ImageBoundary";
    nikonDict [ 0x0017 ] = "FlashExposureComp";
    nikonDict [ 0x0018 ] = "FlashBracketComp";
    nikonDict [ 0x0019 ] = "ExposureBracketComp";
    nikonDict [ 0x001b ] = "CropHiSpeed";
    nikonDict [ 0x001c ] = "ExposureTuning";
    nikonDict [ 0x001d ] = "SerialNumber";
    nikonDict [ 0x001e ] = "ColorSpace";
    nikonDict [ 0x0023 ] = "PictureControl";

    canonDict [ktGroup ] = "Canon";
    canonDict [ 0x0001 ] = "Macro";
    canonDict [ 0x0002 ] = "Selftimer";
    canonDict [ 0x0003 ] = "Quality";
    canonDict [ 0x0004 ] = "FlashMode";
    canonDict [ 0x0005 ] = "DriveMode";
    canonDict [ 0x0007 ] = "FocusMode";
    canonDict [ 0x000a ] = "ImageSize";
    canonDict [ 0x000b ] = "EasyMode";
    canonDict [ 0x000c ] = "DigitalZoom";
    canonDict [ 0x000d ] = "Contrast";
    canonDict [ 0x000e ] = "Saturation";
    canonDict [ 0x000f ] = "Sharpness";
    canonDict [ 0x0010 ] = "ISOSpeed";
    canonDict [ 0x0011 ] = "MeteringMode";
    canonDict [ 0x0012 ] = "FocusType";

    gpsDict   [ktGroup ] = "GPSInfo";
    gpsDict   [ 0x0000 ] = "GPSVersionID";
    gpsDict   [ 0x0001 ] = "GPSLatitudeRef";
    gpsDict   [ 0x0002 ] = "GPSLatitude";
    gpsDict   [ 0x0003 ] = "GPSLongitudeRef";
    gpsDict   [ 0x0004 ] = "GPSLongitude";
    gpsDict   [ 0x0005 ] = "GPSAltitudeRef";
    gpsDict   [ 0x0006 ] = "GPSAltitude";
    gpsDict   [ 0x0007 ] = "GPSTimeStamp";
    gpsDict   [ 0x0008 ] = "GPSSatellites";
    gpsDict   [ 0x0012 ] = "GPSMapDatum";
    gpsDict   [ 0x001d ] = "GPSDateStamp";

    sonyDict  [ktGroup ] = "Sony";
    sonyDict  [ 0x0001 ] = "Offset";
    sonyDict  [ 0x0002 ] = "ByteOrder";
    sonyDict  [ 0xb020 ] = "ColorReproduction";
    sonyDict  [ 0xb040 ] = "Macro";
    sonyDict  [ 0xb041 ] = "ExposureMode";
    sonyDict  [ 0xb042 ] = "FocusMode";
    sonyDict  [ 0xb043 ] = "AFMode";
    sonyDict  [ 0xb044 ] = "AFIlluminator";
    sonyDict  [ 0xb047 ] = "JPEGQuality";
    sonyDict  [ 0xb048 ] = "FlashLevel";
    sonyDict  [ 0xb049 ] = "ReleaseMode";
    sonyDict  [ 0xb04a ] = "SequenceNumber";
    sonyDict  [ 0xb04b ] = "AntiBlur";
    sonyDict  [ 0xb04e ] = "LongExposureNoiseReduction";

    agfaDict  [ktGroup ] = "Agfa";
    agfaDict  [ 0x0001 ] = "One";
    agfaDict  [ 0x0002 ] = "Size";
    agfaDict  [ 0x0003 ] = "Three";
    agfaDict  [ 0x0004 ] = "Four";
    agfaDict  [ 0x0005 ] = "Thumbnail";

    appleDict [ktGroup ] = "Apple";
    appleDict [ 0x0001 ] = "One";
    appleDict [ 0x0002 ] = "Two";
    appleDict [ 0x0003 ] = "Three";
    appleDict [ 0x0004 ] = "Four";
    appleDict [ 0x0005 ] = "Five";
    appleDict [ 0x0006 ] = "Six";
    appleDict [ 0x0007 ] = "Seven";
    appleDict [ 0x0008 ] = "Eight";
    appleDict [ 0x0009 ] = "Nine";
    appleDict [ 0x000a ] = "Ten";
    appleDict [ 0x000b ] = "Eleven";
    appleDict [ 0x000c ] = "Twelve";
    appleDict [ 0x000d ] = "Thirteen";

    panoDict  [ktGroup ] = "Panosonic";
    panoDict  [ 0x0001 ] = "Version";
    panoDict  [ 0x0002 ] = "SensorWidth";
    panoDict  [ 0x0003 ] = "SensorHeight";
    panoDict  [ 0x0004 ] = "SensorTopBorder";
    panoDict  [ 0x0005 ] = "SensorLeftBorder";
    panoDict  [ 0x0006 ] = "ImageHeight";
    panoDict  [ 0x0007 ] = "ImageWidth";
    panoDict  [ 0x0011 ] = "RedBalance";
    panoDict  [ 0x0012 ] = "BlueBalance";
    panoDict  [ 0x0017 ] = "ISOSpeed";
    panoDict  [ 0x0024 ] = "WBRedLevel";
    panoDict  [ 0x0025 ] = "WBGreenLevel";
    panoDict  [ 0x0026 ] = "WBBlueLevel";
    panoDict  [ 0x002e ] = "PreviewImage";

    minoDict  [ktGroup ] = "Minolta";
    minoDict  [ 0x0000 ] = "BlockDescriptor";
    minoDict  [ 0x0001 ] = "CameraSettings";
    minoDict  [ 0x0010 ] = "AutoFocus";
    minoDict  [ 0x0040 ] = "ImageSize";
    minoDict  [ 0x0081 ] = "Thumbnail";
    
    olymDict  [ktGroup ] = "Olympus";
    olymDict  [ 0x0100 ] = "ThumbnailImage";
    olymDict  [ 0x0104 ] = "BodyFirmwareVersion";
    olymDict  [ 0x0200 ] = "SpecialMode";
    olymDict  [ 0x0201 ] = "Quality";
    olymDict  [ 0x0202 ] = "Macro";
    olymDict  [ 0x0203 ] = "BWMode";
    olymDict  [ 0x0204 ] = "DigitalZoom";
    // Olympus IFDs
    olymDict  [ 0x2010 ] = "Equipment";  // see ifdDict()
    olymDict  [ 0x2020 ] = "CameraSettings";
    olymDict  [ 0x2030 ] = "RawDevelopment";
    olymDict  [ 0x2031 ] = "RawDevelopment2";
    olymDict  [ 0x2040 ] = "ImageProcessing";
    olymDict  [ 0x2050 ] = "FocusInfo";
    olymDict  [ 0x3000 ] = "RawInfo";
    
    olymCSDict[ktGroup ] = "OlympusCS";
    olymCSDict[ 0x0000 ] = "Version";
    olymCSDict[ 0x0100 ] = "PreviewImageValid";
    olymCSDict[ 0x0101 ] = "PreviewImageStart";
    olymCSDict[ 0x0102 ] = "PreviewImageLength";
    olymCSDict[ 0x0200 ] = "ExposureMode";
    
    olymEQDict[ktGroup ] = "OlympusEQ";
    olymEQDict[ 0x0000 ] = "Version";
    olymEQDict[ 0x0100 ] = "CameraType";
    olymEQDict[ 0x0101 ] = "SerialNumber";
    olymEQDict[ 0x0102 ] = "InternalSerialNumber";
    
    olymRDDict[ktGroup ] = "OlymRawDev";
    olymRDDict[ 0x0000 ] = "Version";
    olymRDDict[ 0x0100 ] = "ExposureBiasValue";
    olymRDDict[ 0x0101 ] = "WhiteBalanceValue";
    olymRDDict[ 0x0102 ] = "WBFineAdjustment";
    olymRDDict[ 0x0103 ] = "GrayPoint";
    
    olymR2Dict[ktGroup ] = "OlymRawDev2";
    olymR2Dict[ 0x0000 ] = "Version";
    olymR2Dict[ 0x0100 ] = "ExposureBiasValue";
    olymR2Dict[ 0x0101 ] = "WhiteBalance";
    olymR2Dict[ 0x0102 ] = "WhiteBalanceValue";
    olymR2Dict[ 0x0103 ] = "WBFineAdjustment";
    olymR2Dict[ 0x0104 ] = "GrayPoint";
    
    olymIPDict[ktGroup ] = "OlymImgProc";
    olymIPDict[ 0x0000 ] = "Version";
    olymIPDict[ 0x0100 ] = "WB_RBLevels";
    
    olymFIDict[ktGroup ] = "OlymFocusInfo";
    olymFIDict[ 0x0000 ] = "Version";
    olymFIDict[ 0x0209 ] = "AutoFocus";
    olymFIDict[ 0x0210 ] = "SceneDetect";
    olymFIDict[ 0x0211 ] = "SceneArea";
    
    olymRoDict[ktGroup ] = "OlymRawInfo";
    olymRoDict[ 0x0000 ] = "Version";
    olymRoDict[ 0x0100 ] = "WB_RBLevelsUsed";
    olymRoDict[ 0x0110 ] = "WB_RBLevelsAuto";
    
    crwDict   [ktGroup ] = "CRW";
    crwDict   [ 0x0032 ] = "CanonColorInfo1";
    crwDict   [ 0x0805 ] = "CanonFileDescription";
    crwDict   [ 0x080a ] = "CanonRawMakeModel";
    crwDict   [ 0x080b ] = "CanonFirmwareVersion";
    crwDict   [ 0x080c ] = "ComponentVersion";
    crwDict   [ 0x080d ] = "ROMOperationMode";
    crwDict   [ 0x0810 ] = "OwnerName";
    crwDict   [ 0x0815 ] = "CanonImageType";
    crwDict   [ 0x0816 ] = "OriginalFileName";
    crwDict   [ 0x0817 ] = "ThumbnailFileName";
    crwDict   [ 0x100a ] = "TargetImageType";
    crwDict   [ 0x1010 ] = "ShutterReleaseMethod";
    crwDict   [ 0x1011 ] = "ShutterReleaseTiming";
    crwDict   [ 0x1016 ] = "ReleaseSetting";
    crwDict   [ 0x101c ] = "BaseISO";
    crwDict   [ 0x1028 ] = "CanonFlashInfo";
    crwDict   [ 0x1029 ] = "CanonFocalLength";
    crwDict   [ 0x102a ] = "CanonShotInfo";
    crwDict   [ 0x102c ] = "CanonColorInfo2";
    crwDict   [ 0x102d ] = "CanonCameraSettings";
    crwDict   [ 0x1030 ] = "WhiteSample";
    crwDict   [ 0x1031 ] = "SensorInfo";
    crwDict   [ 0x1033 ] = "CustomFunctions10D";
    crwDict   [ 0x1038 ] = "CanonAFInfo";
    crwDict   [ 0x1093 ] = "CanonFileInfo";
    crwDict   [ 0x10a9 ] = "ColorBalance";
    crwDict   [ 0x10b5 ] = "RawJpgInfo";
    crwDict   [ 0x10ae ] = "ColorTemperature";
    crwDict   [ 0x10b4 ] = "ColorSpace";
    crwDict   [ 0x1803 ] = "ImageFormat";
    crwDict   [ 0x1804 ] = "RecordID";
    crwDict   [ 0x1806 ] = "SelfTimerTime";
    crwDict   [ 0x1807 ] = "TargetDistanceSetting";
    crwDict   [ 0x180b ] = "SerialNumber";
    crwDict   [ 0x180e ] = "TimeStamp";
    crwDict   [ 0x1810 ] = "ImageInfo";
    crwDict   [ 0x1813 ] = "FlashInfo";
    crwDict   [ 0x1814 ] = "MeasuredEV";
    crwDict   [ 0x1817 ] = "FileNumber";
    crwDict   [ 0x1818 ] = "ExposureInfo";
    crwDict   [ 0x1834 ] = "CanonModelID";
    crwDict   [ 0x1835 ] = "DecoderTable";
    crwDict   [ 0x183b ] = "SerialNumberFormat";
    crwDict   [ 0x2005 ] = "RawData";
    crwDict   [ 0x2007 ] = "JpgFromRaw";
    crwDict   [ 0x2008 ] = "ThumbnailImage";
    crwDict   [ 0x2804 ] = "ImageDescription";
    crwDict   [ 0x2807 ] = "CameraObject";
    crwDict   [ 0x3002 ] = "ShootingRecord";
    crwDict   [ 0x3003 ] = "MeasuredInfo";
    crwDict   [ 0x3004 ] = "CameraSpecification";
    crwDict   [ 0x300a ] = "ImageProps";
    crwDict   [ 0x300b ] = "ExifInformation";

    // Fields
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcVersion"         ,kttAscii , 0, 4));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcName"            ,kttAscii , 4,20));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcBase"            ,kttAscii ,24,20));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcAdjust"          ,kttUByte, 48, 1));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcQuickAdjust"     ,kttUByte, 49, 1));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcSharpness"       ,kttUByte, 50, 1));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcContrast"        ,kttUByte, 51, 1));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcBrightness"      ,kttUByte, 52, 1));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcSaturation"      ,kttUByte, 53, 1));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcHueAdjustment"   ,kttUByte, 54, 1));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcFilterEffect"    ,kttUByte, 55, 1));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcFilterEffect"    ,kttUByte, 56, 1));
    makerTags["Exif.Nikon.PictureControl"].push_back(Field("PcToningSaturation",kttUByte, 57, 1));

    // Iptc dicts
    iptcEnvelope   [ktGroup] = "Envelope"      ;
    iptcEnvelope   [      0] = "ModelVersion"  ;
    iptcEnvelope   [      5] = "Destination"   ;
    iptcEnvelope   [     90] = "CharacterSet"  ;

    iptcApplication[ktGroup] = "Application"   ;
    iptcApplication[      0] = "RecordVersion" ;
    iptcApplication[     12] = "Subject"       ;
    iptcApplication[    120] = "Caption"       ;
    iptcDicts[1]             = iptcEnvelope;
    iptcDicts[2]             = iptcApplication ;

    psdDict        [ktGroup] = "8BIM"          ;
    psdDict        [ 0x0404] = "IPTCNAA"       ;
    psdDict        [ 0x040C] = "Thumbnail"     ;
    psdDict        [ 0x040F] = "ICCProfile"    ;
    psdDict        [ 0x0421] = "Version"       ;
    psdDict        [ 0x0422] = "Exif"          ;
    psdDict        [ 0x0423] = "Exif"          ;
    psdDict        [ 0x0424] = "XMP"           ;
}

// That's all Folks!
////
