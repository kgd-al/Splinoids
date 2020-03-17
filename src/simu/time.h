#ifndef SIMU_TIME_H
#define SIMU_TIME_H

#include "kgd/external/json.hpp"

namespace simu {

struct Time {
  Time (void);

  const auto& year (void) const { return _year; }
  const auto& day (void) const {  return _day;  }
  const auto& second (void) const { return _second; }
  const auto& msecond (void) const { return _msecond; }
  const auto& timestamp (void) const { return _timestamp; }

  float secondFraction (void) const;
  float dayFraction (void) const;
  float yearFraction (void) const;

  void set (uint y, uint d, uint s, uint ms);
  void set (uint t);

  Time& next (void);
  std::string pretty (void) const;

//  bool isStartOfYear (void) const {
//    return _day == 0 && _hour == 0;
//  }

  static Time fromTimestamp (uint t);

  friend bool operator== (const Time &lhs, const Time &rhs) {
    return lhs._year == rhs._year
        && lhs._day == rhs._day
        && lhs._second == rhs._second
        && lhs._msecond == rhs._msecond;
  }

  friend bool operator< (const Time &lhs, const Time &rhs) {
    if (lhs._year != rhs._year) return lhs._year < rhs._year;
    if (lhs._day != rhs._day) return lhs._day < rhs._day;
    if (lhs._second != rhs._second) return lhs._second < rhs._second;
    return lhs._msecond < rhs._msecond;
  }

  friend bool operator<= (const Time &lhs, const Time &rhs) {
    return lhs < rhs || lhs == rhs;
  }

  friend Time operator- (const Time &lhs, const Time &rhs) {
    return fromTimestamp(lhs.timestamp() - rhs.timestamp());
  }

  friend Time operator+ (const Time &lhs, uint years) {
    return Time(lhs._year+years, lhs._day, lhs._second, lhs._msecond);
  }

  friend std::ostream& operator<< (std::ostream &os, const Time &t) {
    return os << "y" << t._year << "d" << t._day
              << "@" << t._second << "." << t._msecond;
  }

  friend std::istream& operator>> (std::istream &is, Time &t) {
    char c;
    is >> c >> t._year >> c >> t._day >> c >> t._second >> c >> t._msecond;
    return is;
  }

  friend void to_json (nlohmann::json &j, const Time &t);
  friend void from_json (const nlohmann::json &j, Time &t);
  friend void assertEqual (const Time &lhs, const Time &rhs, bool deepcopy);

private:
  uint _year, _day, _second, _msecond;
  uint _timestamp;

  Time (uint y, uint d, uint s, uint ms);
  Time (uint t);

  uint toTimestamp (void) const;
};

} // end of namespace simu
#endif // SIMU_TIME_H
