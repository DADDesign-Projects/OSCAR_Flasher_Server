//==================================================================================
// File: cServer.h
// Description: PC-side server for flashing QSPI flash memory over a COM port.
//
// Copyright (c) 2024-2026 Dad Design.
//==================================================================================

#pragma once

#include <string>
#include <stdint.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "Flasher.h"

namespace Dad {

//==================================================================================
// cServer
//
// Manages the PC-side of the QSPI flash protocol:
//   - Opens and configures a COM port.
//   - Builds a flash image in RAM (directory + data).
//   - Transmits it block by block to the target.
//==================================================================================
class cServer {
public:
    // Constructor / destructor
    cServer();
    ~cServer();

    //------------------------------------------------------------------------------
    // COM port management
    //------------------------------------------------------------------------------

    // Open and configure the COM port.
    // QSPi_Size must not exceed QSPI_SIZE.
    bool Init(uint8_t  NumPort,
              uint32_t QSPi_Size = QSPI_PAGE_SIZE * 2,
              DWORD    BaudRate  = CBR_9600,
              BYTE     ByteSize  = 8,
              BYTE     Parity    = NOPARITY,
              BYTE     StopBits  = ONESTOPBIT);

    // Wait for the target's "BLOC<nn>" synchronisation frame (5 s timeout).
    // Returns the requested block number, or -1 on error / timeout.
    int16_t Synchronize();

    // Transmit block NumBloc to the target.
    // Set EndTrans != 0 on the last block.
    bool TransBloc(uint16_t NumBloc, uint8_t EndTrans = 0);

    //------------------------------------------------------------------------------
    // Buffer loading – dispatch by file type
    //------------------------------------------------------------------------------

    // Auto-detect the file type (image / ELF / binary) and add it to the buffer.
    bool addFile(const std::string& filePath, const std::string& fileName);

    //------------------------------------------------------------------------------
    // Buffer queries
    //------------------------------------------------------------------------------

    // Number of payload bytes written so far.
    uint32_t getDataSize()  const { return static_cast<uint32_t>(m_pFirstFreeBuff - m_pBuff); }

    // Number of TRANS_BLOCK_SIZE blocks required (always at least 1).
    uint16_t getNbBlocs()   const {
        return static_cast<uint16_t>(getDataSize() / TRANS_BLOCK_SIZE) + 1;
    }
	uint8_t* getBuffer() const { return m_pBuff; }
    void setDataSize(uint32_t size) { m_QSPI_Size = size; }

protected:
    //------------------------------------------------------------------------------
    // COM port helpers
    //------------------------------------------------------------------------------
    void ResetClass();
    bool Config(DWORD BaudRate, BYTE ByteSize, BYTE Parity, BYTE StopBits);

    //------------------------------------------------------------------------------
    // File-type detection
    //------------------------------------------------------------------------------
    static bool hasExtension(const std::string& fileName,
                             std::initializer_list<const char*> exts);
    static bool isImageFile(const std::string& fileName);
    static bool isElfFile  (const std::string& fileName);
    static bool isOFSFFile(const std::string& fileName);

    //------------------------------------------------------------------------------
    // Buffer writers
    //------------------------------------------------------------------------------

    // Append a uint32_t in little-endian order.
    void writeUint32(uint32_t value);

    // Write the trailing 16-byte "IMAG" magic block (magic + NbFrame + W + H).
    void writeImageMagicBlock(uint32_t nbFrame, int width, int height);

    // Create/finalise a directory entry and advance m_pFile.
    // Must be called after the payload bytes have been written to the buffer.
    bool commitFileEntry(const std::string& fileName,
                         uint32_t           payloadSize,
                         eFileType          type);

    //------------------------------------------------------------------------------
    // Typed loaders
    //------------------------------------------------------------------------------
    bool addCommonFile (const std::string& filePath, const std::string& fileName);
    bool addImageFile  (const std::string& filePath, const std::string& fileName);
    bool addElfFile    (const std::string& filePath, const std::string& fileName);
	bool addOFSFFile   (const std::string& filePath, const std::string& fileName);

    // Image sub-loaders (called with an already-open GDI+ Image).
    bool addSingleImageFile  (Gdiplus::Image* pImage, const std::string& fileName);
    bool addAnimatedImageFile(Gdiplus::Image* pImage, const std::string& fileName);

    // Rasterise pImage (width × height) into the buffer as 32-bit RGBA.
    bool copyGdiPlusImageToBuffer(Gdiplus::Image* pImage, int width, int height);

    //------------------------------------------------------------------------------
    // State
    //------------------------------------------------------------------------------

    // QSPI image buffer
    uint32_t  m_QSPI_Size       = 0;
    uint8_t*  m_pBuff           = nullptr;  // Start of buffer
    uint8_t*  m_pEndBuff        = nullptr;  // One-past-end of buffer
    uint8_t*  m_pFirstFreeBuff  = nullptr;  // Next free byte

    stFile*   m_pFile           = nullptr;  // Current directory entry
    uint8_t   m_IndexFile       = 0;        // Number of files added so far

    // COM port
    HANDLE    m_hCom   = INVALID_HANDLE_VALUE;
    DCB       m_Config = {};
    Dad::Bloc m_Bloc   = {};
};

} // namespace Dad

//***End of file**************************************************************
