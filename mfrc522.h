#ifndef MFRC522_H
#define MFRC522_H

#include <avr/io.h>

// --- Status Codes ---
#define MI_OK                   0
#define MI_NOTAGERR             1
#define MI_ERR                  2

// --- MFRC522 Commands ---
#define PCD_IDLE                0x00
#define PCD_TRANSCEIVE          0x0C
#define PCD_RESETPHASE          0x0F

// --- PICC Commands ---
#define PICC_REQIDL             0x26
#define PICC_ANTICOLL           0x93
#define PICC_SElECTTAG          0x93

// --- MFRC522 Registers ---
#define CommandReg              0x01
#define ComIrqReg               0x04
#define DivIrqReg               0x05
#define ErrorReg                0x06
#define FIFODataReg             0x09
#define FIFOLevelReg            0x0A
#define ControlReg              0x0C
#define BitFramingReg           0x0D
#define CollReg                 0x0E
#define ModeReg                 0x11
#define TxControlReg            0x14
#define TModeReg                0x2A
#define TPrescalerReg           0x2B
#define TReloadRegL             0x2C
#define TReloadRegH             0x2D
#define VersionReg              0x37

// --- Function Prototypes ---
void MFRC522_Init();
uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t MFRC522_Anticoll(uint8_t *serNum);
void MFRC522_WriteRegister(uint8_t addr, uint8_t val);
uint8_t MFRC522_ReadRegister(uint8_t addr);
void MFRC522_SetBitMask(uint8_t reg, uint8_t mask);
void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask);
void MFRC522_AntennaOn();
uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint8_t *backLen);

// --- User-facing Functions ---
// These are the two functions your main.c file uses
uint8_t MFRC522_IsCardPresent();
uint8_t MFRC522_ReadCardSerial(uint8_t *str);

#endif