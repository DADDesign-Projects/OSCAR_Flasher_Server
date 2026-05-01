//==================================================================================
// File: cServer.cpp
// Description: PC-side server for flashing QSPI flash memory over a COM port.
//
// Copyright (c) 2024-2026 Dad Design.
// Credit: https://github.com/serge1/elfio  (ELF parser)
//==================================================================================

#define NOMINMAX
#include <algorithm>
#include <windows.h>
#include <fstream>
#include <vector>

#include "cServer.h"
#include "elfio/elfio.hpp"

namespace Dad {

//==================================================================================
// Construction / destruction
//==================================================================================

cServer::cServer()
{
    m_hCom = INVALID_HANDLE_VALUE;
    memcpy_s(m_Bloc.StartMarker, sizeof(m_Bloc.StartMarker), "BLOC", 4);
    memcpy_s(m_Bloc.EndMarker,   sizeof(m_Bloc.EndMarker),   "END",  3);
}

cServer::~cServer()
{
    ResetClass();
}

// Close the COM port and free the QSPI buffer.
void cServer::ResetClass()
{
    if (m_hCom != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hCom);
        m_hCom = INVALID_HANDLE_VALUE;
    }
    if (m_pBuff != nullptr) {
        delete[] m_pBuff;
        m_pBuff          = nullptr;
        m_pEndBuff       = nullptr;
        m_pFirstFreeBuff = nullptr;
        m_pFile          = nullptr;
    }
    m_IndexFile = 0;
}

//==================================================================================
// COM port management
//==================================================================================

// Apply BaudRate / framing to the already-open handle.
bool cServer::Config(DWORD BaudRate, BYTE ByteSize, BYTE Parity, BYTE StopBits)
{
    if (!GetCommState(m_hCom, &m_Config)) return false;

    m_Config.BaudRate = BaudRate;
    m_Config.ByteSize = ByteSize;
    m_Config.Parity   = Parity;
    m_Config.StopBits = StopBits;

    if (!SetCommState(m_hCom, &m_Config)) return false;
    if (!GetCommState(m_hCom, &m_Config)) return false;  // Confirm

    PurgeComm(m_hCom, PURGE_RXCLEAR);
    return true;
}

// Open COM port, allocate buffer, configure timeouts.
bool cServer::Init(uint8_t  NumPort,
                   uint32_t QSPi_Size,
                   DWORD    BaudRate,
                   BYTE     ByteSize,
                   BYTE     Parity,
                   BYTE     StopBits)
{
    ResetClass();

    // Validate and allocate the QSPI image buffer
    if (QSPi_Size > QSPI_SIZE) return false;

    m_pBuff = new (std::nothrow) uint8_t[QSPi_Size];
    if (m_pBuff == nullptr) return false;

    m_QSPI_Size      = QSPi_Size;
    memset(m_pBuff, 0, QSPi_Size);
    m_pFile          = reinterpret_cast<stFile*>(m_pBuff);
    m_pEndBuff       = m_pBuff + QSPi_Size;
    m_pFirstFreeBuff = m_pBuff + sizeof(Directory);   // Skip the directory table

    // Build the COM port path ("\\.\\COMn")
    wchar_t portName[16];
    swprintf_s(portName, L"\\\\.\\COM%u", static_cast<unsigned>(NumPort));

    m_hCom = CreateFileW(portName,
                         GENERIC_READ | GENERIC_WRITE,
                         0, NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hCom == INVALID_HANDLE_VALUE) return false;

    // Initialise DCB and configure
    SecureZeroMemory(&m_Config, sizeof(DCB));
    m_Config.DCBlength = sizeof(DCB);
    if (!Config(BaudRate, ByteSize, Parity, StopBits)) {
        CloseHandle(m_hCom);
        m_hCom = INVALID_HANDLE_VALUE;
        return false;
    }

    // Non-blocking reads (return immediately with whatever bytes are available)
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout         = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    if (!SetCommTimeouts(m_hCom, &timeouts)) {
        CloseHandle(m_hCom);
        m_hCom = INVALID_HANDLE_VALUE;
        return false;
    }
    return true;
}

// Synchronise: wait for the "BLOC<nn>" frame and return the block number.
int16_t cServer::Synchronize()
{
    enum class eStep {
        Marker_B, Marker_L, Marker_O, Marker_C,
        NumBloc_Low, NumBloc_High
    } step = eStep::Marker_B;

    int16_t  numBloc  = 0;
    DWORD    nRead    = 0;
    uint8_t  byte     = 0;
    uint64_t deadline = GetTickCount64() + 5000;    // 5 s timeout

    while (GetTickCount64() < deadline) {
        if (!ReadFile(m_hCom, &byte, 1, &nRead, NULL)) return -1;
        if (nRead == 0) continue;

        switch (step) {
        case eStep::Marker_B:
            if (byte == 'B') step = eStep::Marker_L;
            break;
        case eStep::Marker_L:
            step = (byte == 'L') ? eStep::Marker_O : eStep::Marker_B;
            break;
        case eStep::Marker_O:
            step = (byte == 'O') ? eStep::Marker_C : eStep::Marker_B;
            break;
        case eStep::Marker_C:
            step = (byte == 'C') ? eStep::NumBloc_Low : eStep::Marker_B;
            break;
        case eStep::NumBloc_Low:
            numBloc = static_cast<int16_t>(byte);
            step    = eStep::NumBloc_High;
            break;
        case eStep::NumBloc_High:
            numBloc |= static_cast<int16_t>(byte << 8);
            return numBloc;
        }
    }
    return -1;  // Timeout
}

// Transmit one block to the target.
bool cServer::TransBloc(uint16_t NumBloc, uint8_t EndTrans)
{
    PurgeComm(m_hCom, PURGE_RXCLEAR);

    const uint8_t* pSrc     = m_pBuff + (NumBloc * TRANS_BLOCK_SIZE);
    uint8_t*       pDst     = m_Bloc.Data;
    uint8_t        calcCRC  = 0;

    for (uint16_t i = 0; i < TRANS_BLOCK_SIZE; ++i) {
        calcCRC += *pSrc;
        *pDst++ = *pSrc++;
    }

    m_Bloc.NumBloc    = NumBloc;
    m_Bloc._EndTrans  = EndTrans;
    m_Bloc._CRC       = calcCRC;

    DWORD nWritten = 0;
    return WriteFile(m_hCom, &m_Bloc, sizeof(Dad::Bloc), &nWritten, NULL)
        && (nWritten == sizeof(Dad::Bloc));
}

//==================================================================================
// File-type detection
//==================================================================================

// Returns true when fileName's extension (case-insensitive) is in exts.
bool cServer::hasExtension(const std::string&                   fileName,
                           std::initializer_list<const char*>   exts)
{
    size_t dot = fileName.find_last_of('.');
    if (dot == std::string::npos) return false;

    std::string ext = fileName.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (const char* e : exts)
        if (ext == e) return true;

    return false;
}

bool cServer::isImageFile(const std::string& fileName)
{
    return hasExtension(fileName, { ".png", ".jpg", ".jpeg", ".bmp",
                                    ".tif", ".tiff", ".gif" });
}

bool cServer::isElfFile(const std::string& fileName)
{
    return hasExtension(fileName, { ".elf" });
}

//==================================================================================
// Low-level buffer helpers
//==================================================================================

// Append value as 4 little-endian bytes.
void cServer::writeUint32(uint32_t value)
{
    *m_pFirstFreeBuff++ = static_cast<uint8_t>( value        & 0xFF);
    *m_pFirstFreeBuff++ = static_cast<uint8_t>((value >>  8) & 0xFF);
    *m_pFirstFreeBuff++ = static_cast<uint8_t>((value >> 16) & 0xFF);
    *m_pFirstFreeBuff++ = static_cast<uint8_t>((value >> 24) & 0xFF);
}

// Write the 16-byte "IMAG" trailing block shared by all image types.
//   Bytes [0..3]  : 'I','M','A','G'
//   Bytes [4..7]  : nbFrame  (uint32_t LE)
//   Bytes [8..11] : width    (uint32_t LE)
//   Bytes [12..15]: height   (uint32_t LE)
void cServer::writeImageMagicBlock(uint32_t nbFrame, int width, int height)
{
    *m_pFirstFreeBuff++ = 'I';
    *m_pFirstFreeBuff++ = 'M';
    *m_pFirstFreeBuff++ = 'A';
    *m_pFirstFreeBuff++ = 'G';
    writeUint32(nbFrame);
    writeUint32(static_cast<uint32_t>(width));
    writeUint32(static_cast<uint32_t>(height));
}

// Finalise a directory entry after the payload has been written.
// payloadSize is the total byte count that was appended (including any magic block).
bool cServer::commitFileEntry(const std::string& fileName,
                              uint32_t           payloadSize,
                              eFileType          type)
{
    if (m_IndexFile >= DIR_FILE_COUNT) return false;

    strncpy_s(m_pFile->Name, fileName.c_str(), MAX_ENTRY_NAME - 1);
    m_pFile->Name[MAX_ENTRY_NAME - 1] = '\0';
    m_pFile->Size        = payloadSize;
    m_pFile->DataAddress = QSPI_ADRESSE + static_cast<uint32_t>(
                               m_pFirstFreeBuff - payloadSize - m_pBuff);
    m_pFile->FileType    = type;

    ++m_pFile;
    ++m_IndexFile;
    return true;
}

//==================================================================================
// Public dispatcher
//==================================================================================

bool cServer::addFile(const std::string& filePath, const std::string& fileName)
{
    if (isImageFile(fileName)) return addImageFile(filePath, fileName);
    if (isElfFile(fileName))   return addElfFile  (filePath, fileName);
    return addCommonFile(filePath, fileName);
}

//==================================================================================
// Common (binary) file loader
//==================================================================================

bool cServer::addCommonFile(const std::string& filePath, const std::string& fileName)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto fileSize = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    if ((m_pFirstFreeBuff + fileSize) > m_pEndBuff) return false;
    if (!file.read(reinterpret_cast<char*>(m_pFirstFreeBuff), fileSize)) return false;

    // Record where the data starts before advancing the pointer
    uint8_t* pDataStart    = m_pFirstFreeBuff;
    m_pFirstFreeBuff      += fileSize;

    // 4-byte alignment
    m_pFirstFreeBuff = reinterpret_cast<uint8_t*>(
        (reinterpret_cast<uintptr_t>(m_pFirstFreeBuff) + 3) & ~uintptr_t(3));

    // Commit directory entry (Size = raw file bytes, no magic block)
    strncpy_s(m_pFile->Name, fileName.c_str(), MAX_ENTRY_NAME - 1);
    m_pFile->Name[MAX_ENTRY_NAME - 1] = '\0';
    m_pFile->Size        = fileSize;
    m_pFile->DataAddress = QSPI_ADRESSE + static_cast<uint32_t>(pDataStart - m_pBuff);
    m_pFile->FileType    = FILE_TYPE_BIN;
    ++m_pFile;
    ++m_IndexFile;

    return true;
}

//==================================================================================
// Image file loaders
//==================================================================================

// Rasterise pImage (width × height) as 32-bit BGRA into the buffer.
// GDI+ BitmapData is already BGRA; we store it as-is.
bool cServer::copyGdiPlusImageToBuffer(Gdiplus::Image* pImage, int width, int height)
{
    Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
    if (bitmap.GetLastStatus() != Gdiplus::Ok) return false;

    Gdiplus::Graphics g(&bitmap);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetSmoothingMode    (Gdiplus::SmoothingModeHighQuality);
    g.SetPixelOffsetMode  (Gdiplus::PixelOffsetModeHighQuality);
    if (g.DrawImage(pImage, 0, 0, width, height) != Gdiplus::Ok) return false;

    Gdiplus::BitmapData bmpData;
    Gdiplus::Rect       rect(0, 0, width, height);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead,
                        PixelFormat32bppARGB, &bmpData) != Gdiplus::Ok)
        return false;

    const BYTE* pSrc = static_cast<BYTE*>(bmpData.Scan0);
    for (int y = 0; y < height; ++y) {
        const BYTE* row = pSrc + (y * bmpData.Stride);
        for (int x = 0; x < width; ++x, row += 4) {
            *m_pFirstFreeBuff++ = row[0]; // B
            *m_pFirstFreeBuff++ = row[1]; // G
            *m_pFirstFreeBuff++ = row[2]; // R
            *m_pFirstFreeBuff++ = row[3]; // A
        }
    }

    bitmap.UnlockBits(&bmpData);
    return true;
}

// Top-level image loader: initialises GDI+, dispatches to single/animated.
bool cServer::addImageFile(const std::string& filePath, const std::string& fileName)
{
    Gdiplus::GdiplusStartupInput startupInput;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &startupInput, NULL) != Gdiplus::Ok)
        return false;

    bool ok = false;
    {
        std::wstring wPath(filePath.begin(), filePath.end());
        Gdiplus::Image image(wPath.c_str());

        if (image.GetLastStatus() == Gdiplus::Ok) {
            UINT frameCount = image.GetFrameCount(&Gdiplus::FrameDimensionTime);
            ok = (frameCount > 1)
               ? addAnimatedImageFile(&image, fileName)
               : addSingleImageFile (&image, fileName);
        }
    }

    Gdiplus::GdiplusShutdown(token);
    return ok;
}

bool cServer::addSingleImageFile(Gdiplus::Image* pImage, const std::string& fileName)
{
    const int      width     = static_cast<int>(pImage->GetWidth());
    const int      height    = static_cast<int>(pImage->GetHeight());
    const uint32_t totalSize = 16 + static_cast<uint32_t>(width * height * 4);

    if ((m_pFirstFreeBuff + totalSize) > m_pEndBuff) return false;

    uint8_t* pDataStart = m_pFirstFreeBuff;

    if (!copyGdiPlusImageToBuffer(pImage, width, height)) return false;
    writeImageMagicBlock(1, width, height);

    // Commit directory entry
    strncpy_s(m_pFile->Name, fileName.c_str(), MAX_ENTRY_NAME - 1);
    m_pFile->Name[MAX_ENTRY_NAME - 1] = '\0';
    m_pFile->Size        = totalSize;
    m_pFile->DataAddress = QSPI_ADRESSE + static_cast<uint32_t>(pDataStart - m_pBuff);
    m_pFile->FileType    = FILE_TYPE_IMG;
    ++m_pFile;
    ++m_IndexFile;
    return true;
}

bool cServer::addAnimatedImageFile(Gdiplus::Image* pImage, const std::string& fileName)
{
    const UINT     frameCount = pImage->GetFrameCount(&Gdiplus::FrameDimensionTime);
    const int      width      = static_cast<int>(pImage->GetWidth());
    const int      height     = static_cast<int>(pImage->GetHeight());
    const uint32_t totalSize  = 16 + static_cast<uint32_t>(width * height * 4 * frameCount);

    if ((m_pFirstFreeBuff + totalSize) > m_pEndBuff) return false;

    uint8_t* pDataStart = m_pFirstFreeBuff;

    for (UINT frame = 0; frame < frameCount; ++frame) {
        pImage->SelectActiveFrame(&Gdiplus::FrameDimensionTime, frame);
        if (!copyGdiPlusImageToBuffer(pImage, width, height)) return false;
    }
    writeImageMagicBlock(frameCount, width, height);

    // Commit directory entry
    strncpy_s(m_pFile->Name, fileName.c_str(), MAX_ENTRY_NAME - 1);
    m_pFile->Name[MAX_ENTRY_NAME - 1] = '\0';
    m_pFile->Size        = totalSize;
    m_pFile->DataAddress = QSPI_ADRESSE + static_cast<uint32_t>(pDataStart - m_pBuff);
    m_pFile->FileType    = FILE_TYPE_IMG;
    ++m_pFile;
    ++m_IndexFile;
    return true;
}

//==================================================================================
// ELF file loader
//==================================================================================

// ELF trailing block layout (immediately after the raw file bytes):
//   [0..3]  'E','L','F','0'
//   [4..7]  nbRegions  (uint32_t LE)
//   per region (20 bytes each):
//     file_offset, dest_addr, source_addr, copy_size, zero_size  (uint32_t LE each)

bool cServer::addElfFile(const std::string& filePath, const std::string& fileName)
{
    // -- Parse ELF segments --------------------------------------------------
    ELFIO::elfio reader;
    if (!reader.load(filePath))                    return false;
    if (reader.get_machine() != ELFIO::EM_ARM)     return false;

    struct CopyRegion {
        uint32_t file_offset;
        uint32_t dest_addr;
        uint32_t source_addr;
        uint32_t copy_size;
        uint32_t zero_size;
    };

    std::vector<CopyRegion> regions;
    for (const auto& seg : reader.segments) {
        if (seg->get_type() != ELFIO::PT_LOAD) continue;
        regions.push_back({
            static_cast<uint32_t>(seg->get_offset()),
            static_cast<uint32_t>(seg->get_virtual_address()),
            static_cast<uint32_t>(seg->get_physical_address()),
            static_cast<uint32_t>(seg->get_file_size()),
            static_cast<uint32_t>(seg->get_memory_size() - seg->get_file_size())
        });
    }

    // -- Load raw ELF bytes --------------------------------------------------
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    const uint32_t fileSize  = static_cast<uint32_t>(file.tellg());
    const uint32_t magicSize = 8 + static_cast<uint32_t>(regions.size()) * 20;
    const uint32_t totalSize = fileSize + magicSize;

    file.seekg(0, std::ios::beg);

    if ((m_pFirstFreeBuff + totalSize) > m_pEndBuff) return false;
    if (!file.read(reinterpret_cast<char*>(m_pFirstFreeBuff), fileSize)) return false;

    uint8_t* pDataStart    = m_pFirstFreeBuff;
    m_pFirstFreeBuff      += fileSize;

    // -- Write ELF magic block -----------------------------------------------
    *m_pFirstFreeBuff++ = 'E';
    *m_pFirstFreeBuff++ = 'L';
    *m_pFirstFreeBuff++ = 'F';
    *m_pFirstFreeBuff++ = '0';
    writeUint32(static_cast<uint32_t>(regions.size()));

    for (const auto& r : regions) {
        writeUint32(r.file_offset);
        writeUint32(r.dest_addr);
        writeUint32(r.source_addr);
        writeUint32(r.copy_size);
        writeUint32(r.zero_size);
    }

    // 4-byte alignment
    m_pFirstFreeBuff = reinterpret_cast<uint8_t*>(
        (reinterpret_cast<uintptr_t>(m_pFirstFreeBuff) + 3) & ~uintptr_t(3));

    // -- Commit directory entry ----------------------------------------------
    strncpy_s(m_pFile->Name, fileName.c_str(), MAX_ENTRY_NAME - 1);
    m_pFile->Name[MAX_ENTRY_NAME - 1] = '\0';
    m_pFile->Size        = fileSize;     // Size = raw ELF only (magic appended separately)
    m_pFile->DataAddress = QSPI_ADRESSE + static_cast<uint32_t>(pDataStart - m_pBuff);
    m_pFile->FileType    = FILE_TYPE_ELF;
    ++m_pFile;
    ++m_IndexFile;
    return true;
}

} // namespace Dad

//***End of file**************************************************************
