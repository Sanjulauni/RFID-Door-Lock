#include "mfrc522.h"
#include <avr/io.h>
#include <util/delay.h>

#define MFRC522_CS_DDR      DDRB
#define MFRC522_CS_PORT     PORTB
#define MFRC522_CS_PIN      PB2
#define MFRC522_RST_DDR     DDRB
#define MFRC522_RST_PORT    PORTB
#define MFRC522_RST_PIN     PB1

uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)));
    return SPDR;
}

void MFRC522_WriteRegister(uint8_t addr, uint8_t val) {
    MFRC522_CS_PORT &= ~(1 << MFRC522_CS_PIN);
    spi_transfer((addr << 1) & 0x7E);
    spi_transfer(val);
    MFRC522_CS_PORT |= (1 << MFRC522_CS_PIN);
}

uint8_t MFRC522_ReadRegister(uint8_t addr) {
    MFRC522_CS_PORT &= ~(1 << MFRC522_CS_PIN);
    spi_transfer(((addr << 1) & 0x7E) | 0x80);
    uint8_t val = spi_transfer(0x00);
    MFRC522_CS_PORT |= (1 << MFRC522_CS_PIN);
    return val;
}

void MFRC522_SetBitMask(uint8_t reg, uint8_t mask) {
    MFRC522_WriteRegister(reg, MFRC522_ReadRegister(reg) | mask);
}

void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask) {
    MFRC522_WriteRegister(reg, MFRC522_ReadRegister(reg) & (~mask));
}

void MFRC522_AntennaOn() {
    uint8_t temp = MFRC522_ReadRegister(TxControlReg);
    if (!(temp & 0x03)) {
        MFRC522_SetBitMask(TxControlReg, 0x03);
    }
}

void MFRC522_Init() {
    MFRC522_RST_DDR |= (1 << MFRC522_RST_PIN);
    MFRC522_RST_PORT |= (1 << MFRC522_RST_PIN);
    
    MFRC522_WriteRegister(CommandReg, PCD_RESETPHASE);
    _delay_ms(10);
    MFRC522_WriteRegister(TModeReg, 0x8D);
    MFRC522_WriteRegister(TPrescalerReg, 0x3E);
    MFRC522_WriteRegister(TReloadRegL, 30);
    MFRC522_WriteRegister(TReloadRegH, 0);
    MFRC522_WriteRegister(0x15, 0x40); // TxAskReg
    MFRC522_WriteRegister(ModeReg, 0x3D);
    MFRC522_AntennaOn();
}

uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint8_t *backLen) {
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    switch (command) {
        case PCD_TRANSCEIVE:
            irqEn = 0x77;
            waitIRq = 0x30;
            break;
        default:
            break;
    }

    MFRC522_WriteRegister(ComIrqReg, irqEn | 0x80);
    MFRC522_ClearBitMask(ComIrqReg, 0x80);
    MFRC522_SetBitMask(FIFOLevelReg, 0x80);
    MFRC522_WriteRegister(CommandReg, PCD_IDLE);

    for (i = 0; i < sendLen; i++) {
        MFRC522_WriteRegister(FIFODataReg, sendData[i]);
    }

    MFRC522_WriteRegister(CommandReg, command);
    if (command == PCD_TRANSCEIVE) {
        MFRC522_SetBitMask(BitFramingReg, 0x80);
    }

    i = 2000;
    while (1) {
        n = MFRC522_ReadRegister(ComIrqReg);
        i--;
        if (~(i != 0 && !(n & 0x01) && !(n & waitIRq))) {
            break;
        }
    }

    MFRC522_ClearBitMask(BitFramingReg, 0x80);

    if (i != 0) {
        if (!(MFRC522_ReadRegister(ErrorReg) & 0x1B)) {
            status = MI_OK;
            if (n & irqEn & 0x01) {
                status = MI_NOTAGERR;
            }

            if (command == PCD_TRANSCEIVE) {
                n = MFRC522_ReadRegister(FIFOLevelReg);
                lastBits = MFRC522_ReadRegister(ControlReg) & 0x07;
                if (lastBits) {
                    *backLen = (n - 1) * 8 + lastBits;
                } else {
                    *backLen = n * 8;
                }

                if (n == 0) {
                    n = 1;
                }
                if (n > 16) {
                    n = 16;
                }

                for (i = 0; i < n; i++) {
                    backData[i] = MFRC522_ReadRegister(FIFODataReg);
                }
            }
        } else {
            status = MI_ERR;
        }
    }
    return status;
}

uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType) {
    uint8_t status;
    uint8_t backBits;
    MFRC522_WriteRegister(BitFramingReg, 0x07);
    TagType[0] = reqMode;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);
    if ((status != MI_OK) || (backBits != 0x10)) {
        status = MI_ERR;
    }
    return status;
}

uint8_t MFRC522_Anticoll(uint8_t *serNum) {
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint8_t unLen;

    MFRC522_WriteRegister(BitFramingReg, 0x00);
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

    if (status == MI_OK) {
        for (i = 0; i < 4; i++) {
            serNumCheck ^= serNum[i];
        }
        if (serNumCheck != serNum[i]) {
            status = MI_ERR;
        }
    }
    return status;
}

// --- User-facing API ---
uint8_t MFRC522_IsCardPresent() {
    uint8_t status;
    uint8_t buffer[2];
    status = MFRC522_Request(PICC_REQIDL, buffer);
    return (status == MI_OK);
}

uint8_t MFRC522_ReadCardSerial(uint8_t *str) {
    uint8_t status;
    status = MFRC522_Anticoll(str);
    return (status == MI_OK);
}