//------------------------------------------------------------------------
// Copyright(c) 2024 Dad Design.
// 
// Utility for flashing QSPIFlash memory from a PC
// Memory structure and transmission blocks
//-----------------------------------------------------------------------
#pragma once
#include <stdint.h>

namespace Dad {

#define QSPI_SIZE           16777216                        // Size of the QSPI flash memory 16M = 16 * 1024 * 1024
#define QSPI_PAGE_SIZE      4096                            // 4K per page
#define QSPI_PAGE_COUNT     2048                            // Number of QSPI pages for the flasher 2024 * 4096 4MB
#define QSPI_ADRESSE        0x90000000

//#define TRANS_BLOCK_COUNT   4                              // Number of Blocks to transmit per QSPI page (1,2,4,8,16,32)
#define TRANS_BLOCK_SIZE    1024                             // Size of a transmission block

#define MAX_ENTRY_NAME      40                              // Number of pages reserved at the beginning of QSPI Flash
#define DIR_FILE_COUNT      80                              // Number of entries in the Directory

    // Directory structure
    typedef struct stFile {
        char     Name[MAX_ENTRY_NAME];
        uint32_t Size;
        uint32_t DataAddress;
		uint32_t FileType;
    } Directory[DIR_FILE_COUNT];

    // Structure of the memory area in QSPI used by the flasher
    typedef uint8_t Page[QSPI_PAGE_SIZE];
    struct stQSPI {
        Page Data[QSPI_PAGE_COUNT];               // Pages used by file data
    };
    static_assert(sizeof(stQSPI) <= QSPI_SIZE, "Memory used > QSPI Flash memory size");

    // Structure of a transmission block on the server side
    struct Bloc {
        char        StartMarker[4];             // Block start delimiter "BLOC"
        uint16_t    NumBloc;                    // Block number
        uint8_t     _CRC;                       // Checksum
        uint8_t     _EndTrans;                  // End of transmission identifier
        uint8_t     Data[TRANS_BLOCK_SIZE];     // Data
        char        EndMarker[3];               // Block end delimiter "END"
    };

    // Structure of a transmission block on the client side
    struct MsgClient {
        char        StartMarker[4];             // Block start delimiter "BLOC", "STOP"
        uint16_t    NumBloc;                    // Expected block number
    };

    enum eFileType {
        FILE_TYPE_BIN = 0x2851,
        FILE_TYPE_IMG = 0x2852,
        FILE_TYPE_ELF = 0x2853
	};

} //Dad