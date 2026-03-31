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
// $Id: Variant.hxx 2838 2014-01-17 23:34:03Z stephena $
//============================================================================

#ifndef VARIANT_HXX
#define VARIANT_HXX

#include <cstdio>
#include <cstdlib>

#include "Array.hxx"
#include "Rect.hxx"
#include "bspf.hxx"

/**
  This class implements a very simple variant type, which is convertible
  to several other types. It stores the actual data as a string, and
  converts to other types as required.

  @author Stephen Anthony
*/
class Variant
{
  private:
    string data;

    static string fromInt32(Int32 i)
    {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%ld", (long)i);
      return string(buf);
    }

    static string fromULong(unsigned long i)
    {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%lu", i);
      return string(buf);
    }

    static string fromUInt(unsigned int i)
    {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%u", i);
      return string(buf);
    }

    static string fromFloat(float f)
    {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%f", (double)f);
      return string(buf);
    }

    static string fromDouble(double d)
    {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%f", d);
      return string(buf);
    }

    static string fromBool(bool b)
    {
      return b ? "1" : "0";
    }

    static string fromSize(const GUI::Size& s)
    {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%d%c%d", s.w, 'x', s.h);
      return string(buf);
    }

  public:
    Variant() : data("") { }

    Variant(const string& s) : data(s) { }
    Variant(const char* s) : data(s ? s : "") { }

    Variant(Int32 i) : data(fromInt32(i)) { }
    Variant(unsigned long i) : data(fromULong(i)) { }
    Variant(unsigned int i) : data(fromUInt(i)) { }
    Variant(float f) : data(fromFloat(f)) { }
    Variant(double d) : data(fromDouble(d)) { }
    Variant(bool b) : data(fromBool(b)) { }
    Variant(const GUI::Size& s) : data(fromSize(s)) { }

    const string& toString() const { return data; }
    int toInt() const { return std::atoi(data.c_str()); }
    float toFloat() const { return (float)std::atof(data.c_str()); }
    bool toBool() const { return data == "1" || data == "true"; }
    GUI::Size toSize() const { return GUI::Size(data); }

    bool operator==(const Variant& v) const { return data == v.data; }
    bool operator!=(const Variant& v) const { return data != v.data; }

    string asString() const {return data;}
};

// extern const Variant EmptyVariant;

class VariantList : public Common::Array< pair<string, Variant> >
{
  public:
    void push_back(const Variant& name, const Variant& tag)
    {
      ensureCapacity(_size + 1);
      _data[_size++] = make_pair(name.toString(), tag);
    }

    void push_back(const Variant& name)
    {
      static const Variant empty("");
      push_back(name, empty);
    }
};

#endif