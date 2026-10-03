#include "hw/rfid_rc522.h"

#include <SPI.h>

#include <cstring>

namespace hw {

using core::kBlockSize;
using core::Uid;
using core::WriteStatus;

namespace {

constexpr uint8_t kTrailerBlock = 7;  // sector 1: blocks 4..7

}  // namespace

void RfidRc522::begin() {
    SPI.begin();
    for (uint8_t i = 0; i < 6; ++i) key_.keyByte[i] = 0xFF;  // factory default key A
    reinit();
}

void RfidRc522::reinit() { mfrc_.PCD_Init(); }

bool RfidRc522::healthy(uint8_t& version) {
    version = mfrc_.PCD_ReadRegister(MFRC522::VersionReg);
    return version != 0x00 && version != 0xFF;
}

bool RfidRc522::detect(Uid& uid, uint8_t& sak) {
    // WUPA also wakes cards we put into HALT last cycle, so a card that lies
    // still on the reader answers every cycle (REQA would ignore it).
    byte atqa[2];
    byte size = sizeof(atqa);
    const MFRC522::StatusCode st = mfrc_.PICC_WakeupA(atqa, &size);
    if (st != MFRC522::STATUS_OK && st != MFRC522::STATUS_COLLISION) return false;
    if (mfrc_.PICC_Select(&mfrc_.uid) != MFRC522::STATUS_OK) return false;
    uid = Uid(mfrc_.uid.uidByte, mfrc_.uid.size);
    sak = mfrc_.uid.sak;
    mfrc_.PICC_HaltA();
    return true;
}

bool RfidRc522::select(const Uid& uid) {
    byte atqa[2];
    byte size = sizeof(atqa);
    const MFRC522::StatusCode st = mfrc_.PICC_WakeupA(atqa, &size);
    if (st != MFRC522::STATUS_OK && st != MFRC522::STATUS_COLLISION) return false;
    if (mfrc_.PICC_Select(&mfrc_.uid) != MFRC522::STATUS_OK) return false;
    return Uid(mfrc_.uid.uidByte, mfrc_.uid.size) == uid;
}

bool RfidRc522::authenticate() {
    return mfrc_.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, kTrailerBlock, &key_, &mfrc_.uid) ==
           MFRC522::STATUS_OK;
}

bool RfidRc522::readBlock(uint8_t block, uint8_t out[kBlockSize]) {
    byte buffer[18];  // 16 data bytes + CRC_A
    byte size = sizeof(buffer);
    if (mfrc_.MIFARE_Read(block, buffer, &size) != MFRC522::STATUS_OK) return false;
    std::memcpy(out, buffer, kBlockSize);
    return true;
}

void RfidRc522::release() {
    mfrc_.PICC_HaltA();
    mfrc_.PCD_StopCrypto1();
}

bool RfidRc522::readBlocks(const Uid& uid, uint8_t block4[kBlockSize], uint8_t block5[kBlockSize]) {
    bool ok = select(uid) && authenticate() && readBlock(core::kNameBlock, block4) &&
              readBlock(core::kMarkerBlock, block5);
    release();
    return ok;
}

WriteStatus RfidRc522::writeBlocks(const Uid& uid, const uint8_t block4[kBlockSize],
                                   const uint8_t block5[kBlockSize]) {
    if (!select(uid)) {
        release();
        return WriteStatus::NoCard;
    }
    if (!authenticate()) {
        release();
        return WriteStatus::Auth;
    }

    uint8_t zeros[kBlockSize] = {};
    // Name first, marker (with the CRC of the new name) second: an interrupted
    // write leaves a CRC mismatch, never a valid half-written card.
    struct Item {
        uint8_t block;
        const uint8_t* data;
    } items[] = {{core::kNameBlock, block4}, {core::kMarkerBlock, block5}, {core::kReservedBlock, zeros}};
    for (const Item& item : items) {
        byte buf[kBlockSize];
        std::memcpy(buf, item.data, kBlockSize);
        if (mfrc_.MIFARE_Write(item.block, buf, kBlockSize) != MFRC522::STATUS_OK) {
            release();
            return WriteStatus::Write;
        }
    }

    uint8_t check4[kBlockSize];
    uint8_t check5[kBlockSize];
    const bool readBack = readBlock(core::kNameBlock, check4) && readBlock(core::kMarkerBlock, check5);
    release();
    if (!readBack || std::memcmp(check4, block4, kBlockSize) != 0 || std::memcmp(check5, block5, kBlockSize) != 0) {
        return WriteStatus::Verify;
    }
    return WriteStatus::Ok;
}

}  // namespace hw
