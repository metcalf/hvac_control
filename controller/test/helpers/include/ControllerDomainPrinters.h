#pragma once

#include <iomanip>
#include <limits>
#include <ostream>

#include "ControllerDomain.h"

// gtest printers for domain types, found via ADL, so failed expectations show readable values
namespace ControllerDomain {

inline void PrintTo(const Config::Schedule &schedule, std::ostream *os) {
    // Print temps with full precision so values that differ only by float error don't look
    // identical, plus °F since that's what the UI and tests usually reason in.
    std::ios_base::fmtflags flags = os->flags();
    std::streamsize precision = os->precision();
    *os << std::setprecision(std::numeric_limits<double>::max_digits10) << "{heatC "
        << schedule.heatC << " (" << std::setprecision(4) << ABS_C_TO_F(schedule.heatC)
        << "°F), coolC " << std::setprecision(std::numeric_limits<double>::max_digits10)
        << schedule.coolC << " (" << std::setprecision(4) << ABS_C_TO_F(schedule.coolC)
        << "°F), start " << std::setfill('0') << std::setw(2) << (int)schedule.startHr << ":"
        << std::setw(2) << (int)schedule.startMin << "}";
    os->flags(flags);
    os->precision(precision);
    os->fill(' ');
}

} // namespace ControllerDomain
