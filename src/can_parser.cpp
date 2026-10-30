#include "car_terminal/can_parser.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace car_terminal {
namespace {

constexpr std::uint32_t kStandardIdentifierMaximum = 0x7FFU;
constexpr std::uint32_t kExtendedIdentifierMaximum = 0x1FFFFFFFU;

std::uint32_t identifierMaximum(CanFrameFormat format) {
    return format == CanFrameFormat::Standard
               ? kStandardIdentifierMaximum
               : kExtendedIdentifierMaximum;
}

std::size_t signalIndex(VehicleSignal signal) {
    switch (signal) {
    case VehicleSignal::SpeedKmh:
        return 0;
    case VehicleSignal::TemperatureC:
        return 1;
    case VehicleSignal::VoltageV:
        return 2;
    }
    throw CanParseError("Unknown vehicle signal");
}

const char* signalName(VehicleSignal signal) {
    switch (signal) {
    case VehicleSignal::SpeedKmh:
        return "speed";
    case VehicleSignal::TemperatureC:
        return "temperature";
    case VehicleSignal::VoltageV:
        return "voltage";
    }
    return "unknown";
}

bool frameMatches(const RawCanFrame& frame,
                  const CanSignalMapping& mapping) {
    return frame.identifier == mapping.identifier &&
           frame.format == mapping.format;
}

}  // namespace

void validateCanFilter(const CanFilterRule& filter) {
    const std::uint32_t maximum = identifierMaximum(filter.format);
    if (filter.identifier > maximum) {
        throw CanParseError("CAN filter identifier is outside its frame format");
    }
    if (filter.mask == 0 || filter.mask > maximum) {
        throw CanParseError(
            "CAN filter mask must be non-zero and fit its frame format");
    }
}

void validateCanSignalMapping(const CanSignalMapping& mapping) {
    if (mapping.identifier > identifierMaximum(mapping.format)) {
        throw CanParseError(
            std::string("CAN ") + signalName(mapping.signal) +
            " mapping identifier is outside its frame format");
    }
    if (mapping.byteLength == 0 || mapping.byteLength > 8) {
        throw CanParseError(
            std::string("CAN ") + signalName(mapping.signal) +
            " mapping byte length must be in [1, 8]");
    }
    if (mapping.byteOffset >= 8 ||
        mapping.byteLength > 8 - mapping.byteOffset) {
        throw CanParseError(
            std::string("CAN ") + signalName(mapping.signal) +
            " mapping exceeds the classic CAN payload");
    }
    if (!std::isfinite(mapping.scale) || mapping.scale == 0.0) {
        throw CanParseError(
            std::string("CAN ") + signalName(mapping.signal) +
            " mapping scale must be finite and non-zero");
    }
    if (!std::isfinite(mapping.valueOffset)) {
        throw CanParseError(
            std::string("CAN ") + signalName(mapping.signal) +
            " mapping offset must be finite");
    }
}

double decodeCanSignal(const RawCanFrame& frame,
                       const CanSignalMapping& mapping) {
    validateCanSignalMapping(mapping);
    if (frame.errorFrame) {
        throw CanParseError("CAN error frames cannot be decoded as vehicle data");
    }
    if (frame.remoteTransmissionRequest) {
        throw CanParseError("CAN remote frames do not contain signal data");
    }
    if (frame.dataLength > frame.data.size()) {
        throw CanParseError("Classic CAN data length exceeds 8 bytes");
    }
    if (!frameMatches(frame, mapping)) {
        throw CanParseError("CAN frame does not match the signal mapping");
    }
    if (mapping.byteOffset > frame.dataLength ||
        mapping.byteLength > frame.dataLength - mapping.byteOffset) {
        throw CanParseError("CAN frame data is shorter than the signal mapping");
    }

    std::uint64_t rawValue = 0;
    if (mapping.byteOrder == CanByteOrder::LittleEndian) {
        for (std::size_t index = 0; index < mapping.byteLength; ++index) {
            rawValue |= static_cast<std::uint64_t>(
                            frame.data[mapping.byteOffset + index])
                        << (index * 8U);
        }
    } else {
        for (std::size_t index = 0; index < mapping.byteLength; ++index) {
            rawValue =
                (rawValue << 8U) |
                frame.data[mapping.byteOffset + index];
        }
    }

    const std::size_t bitCount = mapping.byteLength * 8U;
    long double numericValue = static_cast<long double>(rawValue);
    if (mapping.valueType == CanValueType::Signed) {
        const std::uint64_t signMask = std::uint64_t{1}
                                       << (bitCount - 1U);
        if ((rawValue & signMask) != 0U) {
            numericValue -= std::ldexp(1.0L, static_cast<int>(bitCount));
        }
    }

    const long double scaled =
        numericValue * static_cast<long double>(mapping.scale) +
        static_cast<long double>(mapping.valueOffset);
    if (!std::isfinite(scaled) ||
        scaled > std::numeric_limits<double>::max() ||
        scaled < -std::numeric_limits<double>::max()) {
        throw CanParseError("Decoded CAN signal is not a finite double");
    }
    return static_cast<double>(scaled);
}

CanVehicleDataParser::CanVehicleDataParser(
    std::vector<CanSignalMapping> mappings)
    : mappings_(std::move(mappings)) {
    std::array<bool, 3> present{};
    for (const auto& mapping : mappings_) {
        validateCanSignalMapping(mapping);
        const std::size_t index = signalIndex(mapping.signal);
        if (present[index]) {
            throw CanParseError(std::string("Duplicate CAN mapping for ") +
                                signalName(mapping.signal));
        }
        present[index] = true;
    }
    if (!std::all_of(present.begin(), present.end(), [](bool value) {
            return value;
        })) {
        throw CanParseError(
            "CAN mappings must define speed, temperature, and voltage");
    }
}

std::optional<VehicleData> CanVehicleDataParser::consume(
    const RawCanFrame& frame) {
    if (frame.errorFrame) {
        throw CanParseError("Received a SocketCAN error frame");
    }
    if (frame.remoteTransmissionRequest) {
        throw CanParseError("Received an unsupported CAN remote frame");
    }
    if (frame.dataLength > frame.data.size()) {
        throw CanParseError("Classic CAN data length exceeds 8 bytes");
    }

    bool matched = false;
    for (const auto& mapping : mappings_) {
        if (!frameMatches(frame, mapping)) {
            continue;
        }
        matched = true;
        const std::size_t index = signalIndex(mapping.signal);
        values_[index] = decodeCanSignal(frame, mapping);
        fresh_[index] = true;
    }

    if (!matched ||
        !std::all_of(fresh_.begin(), fresh_.end(), [](bool value) {
            return value;
        })) {
        return std::nullopt;
    }

    fresh_.fill(false);
    ++sampleNumber_;
    return VehicleData{
        sampleNumber_,
        std::chrono::system_clock::now(),
        *values_[signalIndex(VehicleSignal::SpeedKmh)],
        *values_[signalIndex(VehicleSignal::TemperatureC)],
        *values_[signalIndex(VehicleSignal::VoltageV)],
    };
}

}  // namespace car_terminal
