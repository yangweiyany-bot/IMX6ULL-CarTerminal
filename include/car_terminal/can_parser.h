#pragma once

#include "car_terminal/can_types.h"
#include "car_terminal/vehicle_data.h"

#include <array>
#include <optional>
#include <stdexcept>
#include <vector>

namespace car_terminal {

class CanParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void validateCanFilter(const CanFilterRule& filter);
void validateCanSignalMapping(const CanSignalMapping& mapping);
double decodeCanSignal(const RawCanFrame& frame,
                       const CanSignalMapping& mapping);

class CanVehicleDataParser {
public:
    explicit CanVehicleDataParser(std::vector<CanSignalMapping> mappings);

    std::optional<VehicleData> consume(const RawCanFrame& frame);

private:
    std::vector<CanSignalMapping> mappings_;
    std::array<std::optional<double>, 3> values_;
    std::array<bool, 3> fresh_{};
    std::size_t sampleNumber_{};
};

}  // namespace car_terminal
