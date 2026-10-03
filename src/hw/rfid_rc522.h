// RC522 access (MFRC522 library): presence cycle with WUPA/HLTA, sector 1
// read/write with the default key, health check.
#pragma once

#include <MFRC522.h>

#include "core/app.h"

namespace hw {

class RfidRc522 : public core::IRfid {
public:
    RfidRc522(uint8_t ssPin, uint8_t rstPin) : mfrc_(ssPin, rstPin) {}

    void begin();

    bool detect(core::Uid& uid, uint8_t& sak) override;
    bool readBlocks(const core::Uid& uid, uint8_t block4[core::kBlockSize],
                    uint8_t block5[core::kBlockSize]) override;
    core::WriteStatus writeBlocks(const core::Uid& uid, const uint8_t block4[core::kBlockSize],
                                  const uint8_t block5[core::kBlockSize]) override;
    bool healthy(uint8_t& version) override;
    void reinit() override;

private:
    // Wake and select the card `uid`; false if it does not answer.
    bool select(const core::Uid& uid);
    bool authenticate();
    bool readBlock(uint8_t block, uint8_t out[core::kBlockSize]);
    void release();

    MFRC522 mfrc_;
    MFRC522::MIFARE_Key key_{};
};

}  // namespace hw
