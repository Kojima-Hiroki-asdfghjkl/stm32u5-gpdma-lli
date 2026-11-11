#include <cstdint>

struct SPI_REGISTER_t {
    uint32_t CR1;       // 0x00
    uint32_t CR2;       // 0x04
    uint32_t CFG1;      // 0x08
    uint32_t CFG2;      // 0x0C
    uint32_t IER;       // 0x10
    uint32_t SR;        // 0x14
    uint32_t IFCR;      // 0x18
    uint32_t AUTOCR;    // 0x1C

    uint32_t TXDR;      // 0x20
    uint32_t Res1[3];   // 0x24-0x2C

    uint32_t RXDR;      // 0x30
    uint32_t Res2[3];   // 0x34-0x3C

    uint32_t CRCPOLY;   // 0x40
    uint32_t TXCRC;     // 0x44
    uint32_t RXCRC;     // 0x48
    uint32_t UDRDR;     // 0x4C
};
static_assert(sizeof(SPI_REGISTER_t) == 0x50);

struct SPI_INIT_REGISTER_t {
    uint32_t CR1;       // 0x00
    uint32_t CR2;       // 0x04
    uint32_t CFG1;      // 0x08
    uint32_t CFG2;      // 0x0C
    uint32_t IER;       // 0x10
    uint32_t SR;        // 0x14
    uint32_t IFCR;      // 0x18
};
static_assert(sizeof(SPI_INIT_REGISTER_t) == 0x1C);

struct SPI_INIT_REGISTER_ACR_t {
    uint32_t CR1;       // 0x00
    uint32_t CR2;       // 0x04
    uint32_t CFG1;      // 0x08
    uint32_t CFG2;      // 0x0C
    uint32_t IER;       // 0x10
    uint32_t SR;        // 0x14
    uint32_t IFCR;      // 0x18
    uint32_t AUTOCR;    // 0x1C
};
static_assert(sizeof(SPI_INIT_REGISTER_ACR_t) == 0x20);