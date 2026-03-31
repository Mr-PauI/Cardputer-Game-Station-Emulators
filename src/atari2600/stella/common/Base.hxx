#pragma GCC optimize("Os")

//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2014 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: Base.hxx 2838 2014-01-17 23:34:03Z stephena $
//============================================================================

#ifndef BASE_HXX
#define BASE_HXX

#include "bspf.hxx"

namespace Common {

/**
  This class implements several functions for converting integer data
  into strings in multiple bases, with different formats (# of characters,
  upper/lower-case, etc).

  @author  Stephen Anthony
*/
class Base
{
  public:
    enum Format {
      F_16,
      F_16_1,
      F_16_2,
      F_16_4,
      F_16_8,
      F_10,
      F_2,
      F_2_8,
      F_2_16,
      F_DEFAULT
    };

  public:
    static void setFormat(Base::Format base) { myDefaultBase = base; }
    static Base::Format format()             { return myDefaultBase; }

    static void setHexUppercase(bool enable);
    static bool hexUppercase() { return myHexUppercase; }

    /** Écriture HEX sur largeur fixe sans iomanip */
    static std::string HEX2(uInt32 value);
    static std::string HEX4(uInt32 value);
    static std::string HEX8(uInt32 value);

    /** Convert integer to a string in the given base format */
    static string toString(int value,
      Common::Base::Format outputBase = Common::Base::F_DEFAULT);

  private:
    Base() { }

    static string toHexString(uInt32 value, uInt8 width);
    static string toBinString(uInt32 value, uInt8 width);

  private:
    static Format myDefaultBase;
    static bool myHexUppercase;

    static const char* myLowerFmt[4];
    static const char* myUpperFmt[4];
    static const char** myFmt;
};

} // Namespace Common

#endif