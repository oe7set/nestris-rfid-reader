// USB serial: line reader with a length cap, line writer.
#pragma once

#include <Arduino.h>

#include "core/app.h"
#include "core/protocol.h"

namespace hw {

class SerialLink : public core::ILink {
public:
    void begin(uint32_t baud);
    void send(const std::string& line) override;

    // Reads what is available. Returns true and fills `line` for every
    // complete line; sets `tooLong` when an over-long line was dropped.
    bool poll(std::string& line, bool& tooLong);

private:
    std::string buffer_;
    bool overflow_ = false;
};

}  // namespace hw
