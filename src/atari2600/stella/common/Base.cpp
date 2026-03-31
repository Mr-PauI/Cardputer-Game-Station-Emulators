
#include "Base.hxx"
#include <cstdio>

// const std::string EmptyString("");

namespace Common {

Base::Format Base::myDefaultBase = Base::F_16;
bool Base::myHexUppercase = false;

const char* Base::myLowerFmt[4] = { "%02x", "%04x", "%08x", "%016llx" };
const char* Base::myUpperFmt[4] = { "%02X", "%04X", "%08X", "%016llX" };
const char** Base::myFmt = Base::myLowerFmt;

void Base::setHexUppercase(bool enable)
{
  myHexUppercase = enable;
  myFmt = enable ? myUpperFmt : myLowerFmt;
}

string Base::toHexString(uInt32 value, uInt8 width)
{
  static const char* digitsLower = "0123456789abcdef";
  static const char* digitsUpper = "0123456789ABCDEF";
  const char* digits = myHexUppercase ? digitsUpper : digitsLower;

  string result(width, '0');

  for(int i = width - 1; i >= 0; --i)
  {
    result[i] = digits[value & 0x0F];
    value >>= 4;
  }

  return result;
}

string Base::toBinString(uInt32 value, uInt8 width)
{
  string result(width, '0');

  for(int i = width - 1; i >= 0; --i)
  {
    result[i] = (value & 1) ? '1' : '0';
    value >>= 1;
  }

  return result;
}

// std::ostream& Base::HEX2(std::ostream& os, uInt32 value)
// {
//   os << toHexString(value & 0xFF, 2);
//   return os;
// }

// std::ostream& Base::HEX4(std::ostream& os, uInt32 value)
// {
//   os << toHexString(value & 0xFFFF, 4);
//   return os;
// }

// std::ostream& Base::HEX8(std::ostream& os, uInt32 value)
// {
//   os << toHexString(value, 8);
//   return os;
// }

string Base::HEX2(uInt32 value)
{
  return toHexString(value & 0xFF, 2);
}

string Base::HEX4(uInt32 value)
{
  return toHexString(value & 0xFFFF, 4);
}

string Base::HEX8(uInt32 value)
{
  return toHexString(value, 8);
}

string Base::toString(int value, Common::Base::Format outputBase)
{
  if(outputBase == Common::Base::F_DEFAULT)
    outputBase = myDefaultBase;

  char buf[64];

  switch(outputBase)
  {
    case F_16:
      if((value & ~0xFF) == 0)
        return toHexString((uInt32)value, 2);
      else if((value & ~0xFFFF) == 0)
        return toHexString((uInt32)value, 4);
      else
        return toHexString((uInt32)value, 8);

    case F_16_1:
      return toHexString((uInt32)value, 2);

    case F_16_2:
      return toHexString((uInt32)value, 4);

    case F_16_4:
      return toHexString((uInt32)value, 8);

    case F_16_8:
    {
      unsigned long long v = (unsigned long long)(uInt32)value;
      std::snprintf(buf, sizeof(buf),
        myHexUppercase ? "%016llX" : "%016llx", v);
      return buf;
    }

    case F_10:
      std::snprintf(buf, sizeof(buf), "%d", value);
      return buf;

    case F_2:
      if((value & ~0xFF) == 0)
        return toBinString((uInt32)value, 8);
      else
        return toBinString((uInt32)value, 16);

    case F_2_8:
      return toBinString((uInt32)value, 8);

    case F_2_16:
      return toBinString((uInt32)value, 16);

    default:
      return "";
  }
}

} // namespace Common