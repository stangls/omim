#include "indexer/feature_impl.hpp"

#include "base/string_utils.hpp"
#include "base/logging.hpp"
#include "base/math.hpp"


namespace feature
{

bool IsDigit(int c)
{
  return (int('0') <= c && c <= int('9'));
}

bool IsNumber(strings::UniString const & s)
{
  for (size_t i = 0; i < s.size(); ++i)
  {
    // android production ndk-r8d has bug. "еда" detected as a number.
    if (!IsDigit(s[i]))
      return false;
  }
  return true;
}

bool IsStreetNumber(strings::UniString const & s)
{
  size_t count = s.size();
  if (count >= 2)
  {
    /// add different localities in future, if it's a problem.
    string streetEndings [] = {"st", "nd", "rd", "th"};
    for (size_t i = 0; i < ARRAY_SIZE(streetEndings); ++i)
    {
      size_t start = count - streetEndings[i].size();
      bool flag = false;
      for (size_t j = 0; j < streetEndings[i].size(); ++j)
      {
        if (streetEndings[i][j] != s[start + j])
        {
          flag = true;
          break;
        }
      }
      if (flag)
        return false;
    }
    return true;
  }
  return false;
}

bool IsHouseNumberDeepCheck(strings::UniString const & s)
{
  size_t const count = s.size();
  if (count == 0)
    return false;
  if (!IsDigit(s[0]))
    return false;
  if (IsStreetNumber(s))
    return false;
  return (count < 8);
}

bool IsHouseNumber(string const & s)
{
  return (!s.empty() && IsDigit(s[0]));
}

bool IsHouseNumber(strings::UniString const & s)
{
  return (!s.empty() && IsDigit(s[0]));
}

uint8_t PopulationToRank(uint64_t p)
{
  return static_cast<uint8_t>(min(0xFF, my::rounds(log(double(p)) / log(1.1))));
}

uint64_t RankToPopulation(uint8_t r)
{
  return static_cast<uint64_t>(pow(1.1, r));
}

} // namespace feature
