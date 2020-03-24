#include "time.h"

#include "config.h"

namespace simu {

const auto& TPS (void) {  return config::Simulation::ticksPerSecond();  }
const auto& SPD (void) {  return config::Simulation::secondsPerDay();   }
const auto& DPY (void) {  return config::Simulation::daysPerYear();     }

Time::Time (void) : Time(0,0,0,0) {}
Time::Time (uint y, uint d, uint s, uint ms) {
  set(y, d, s, ms);
}

Time::Time (uint t) {
  set(t);
}

void Time::set (uint y, uint d, uint s, uint ms) {
  _year = y;  _day = d; _second = s; _msecond = ms;
  _timestamp = toTimestamp();
}

void Time::set (uint t) {
  _timestamp = t;
  _msecond = t % TPS(); t /= TPS();
  _second = t % SPD(); t /= SPD();
  _day = t % DPY();  t/= DPY();
  _year = t;
}

float Time::secondFraction (void) const {  return float(_msecond) / TPS(); }
float Time::dayFraction (void) const {  return float(_second) / SPD(); }
float Time::yearFraction (void) const {  return float(_day) / DPY(); }

Time& Time::next (void) {
  _timestamp++;
  _msecond++;
  if (_msecond == TPS())  _msecond = 0, _second++;
  if (_second == SPD())   _second = 0,  _day++;
  if (_day == DPY())      _day = 0,     _year++;
  return *this;
}

std::string Time::pretty (void) const {
  static const int M_digits = std::ceil(log10(TPS()));
  static const int S_digits = std::ceil(log10(SPD()));
  static const int D_digits = std::ceil(log10(DPY()));

  std::ostringstream oss;
  oss << std::setfill('0')
      << "y" << _year
      << "d" << std::setw(D_digits) << _day
      << "s" << std::setw(S_digits) << _second
      << "." << std::setw(M_digits) << _msecond;
  return oss.str();
}

Time Time::fromTimestamp(uint timestamp) {
  Time t;
  t.set(timestamp);
  return t;
}

uint Time::toTimestamp(void) const {
//  return _hour + H() * (_day + _year * D());
    return _msecond + TPS() *  (_second + SPD() * (_day + _year * DPY()));
}

void to_json (nlohmann::json &j, const Time &t) {
  std::ostringstream oss;
  oss << t;
  j = oss.str();
}

void from_json (const nlohmann::json &j, Time &t) {
  std::istringstream iss (j.get<std::string>());
  iss >> t;
}

void assertEqual (const Time &lhs, const Time &rhs, bool deepcopy) {
  using utils::assertEqual;
  assertEqual(lhs.msecond(), rhs.msecond(), deepcopy);
  assertEqual(lhs.second(), rhs.second(), deepcopy);
  assertEqual(lhs.day(), rhs.day(), deepcopy);
  assertEqual(lhs.year(), rhs.year(), deepcopy);
}

} // end of namespace simu
