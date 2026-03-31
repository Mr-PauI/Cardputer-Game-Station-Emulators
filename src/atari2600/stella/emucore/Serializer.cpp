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
//============================================================================

#ifndef SERIALIZER_HXX
#define SERIALIZER_HXX

#include <vector>
#include <string>
#include <cstddef>
#include <cstring>

#include "bspf.hxx"

class Serializer
{
  public:
    Serializer(const string& filename, bool readonly = false)
      : myPos(0),
        myValid(false),
        myReadonly(readonly)
    {
      (void)filename;
      // File mode disabled in this embedded build
      myValid = false;
    }

    Serializer(void)
      : myPos(0),
        myValid(true),
        myReadonly(false)
    {
    }

    virtual ~Serializer(void) {}

  public:
    bool isValid(void)
    {
      return myValid;
    }

    void reset(void)
    {
      myPos = 0;
    }

    uInt8 getByte(void)
    {
      if(myPos >= myData.size())
        return 0;

      return myData[myPos++];
    }

    void getByteArray(uInt8* array, uInt32 size)
    {
      if(array == 0 || size == 0)
        return;

      for(uInt32 i = 0; i < size; ++i)
        array[i] = getByte();
    }

    uInt16 getShort(void)
    {
      uInt16 value = 0;
      value |= (uInt16)getByte() << 8;
      value |= (uInt16)getByte();
      return value;
    }

    void getShortArray(uInt16* array, uInt32 size)
    {
      if(array == 0 || size == 0)
        return;

      for(uInt32 i = 0; i < size; ++i)
        array[i] = getShort();
    }

    uInt32 getInt(void)
    {
      uInt32 value = 0;
      value |= (uInt32)getByte() << 24;
      value |= (uInt32)getByte() << 16;
      value |= (uInt32)getByte() << 8;
      value |= (uInt32)getByte();
      return value;
    }

    void getIntArray(uInt32* array, uInt32 size)
    {
      if(array == 0 || size == 0)
        return;

      for(uInt32 i = 0; i < size; ++i)
        array[i] = getInt();
    }

    string getString(void)
    {
      const uInt32 len = getInt();
      if(len == 0)
        return string();

      string s;
      s.resize(len);

      for(uInt32 i = 0; i < len; ++i)
        s[i] = (char)getByte();

      return s;
    }

    bool getBool(void)
    {
      const uInt8 v = getByte();
      return v == TruePattern;
    }

    void putByte(uInt8 value)
    {
      if(myReadonly)
        return;

      if(myPos == myData.size())
        myData.push_back(value);
      else
        myData[myPos] = value;

      ++myPos;
      myValid = true;
    }

    void putByteArray(const uInt8* array, uInt32 size)
    {
      if(array == 0 || size == 0 || myReadonly)
        return;

      for(uInt32 i = 0; i < size; ++i)
        putByte(array[i]);
    }

    void putShort(uInt16 value)
    {
      putByte((uInt8)((value >> 8) & 0xFF));
      putByte((uInt8)(value & 0xFF));
    }

    void putShortArray(const uInt16* array, uInt32 size)
    {
      if(array == 0 || size == 0 || myReadonly)
        return;

      for(uInt32 i = 0; i < size; ++i)
        putShort(array[i]);
    }

    void putInt(uInt32 value)
    {
      putByte((uInt8)((value >> 24) & 0xFF));
      putByte((uInt8)((value >> 16) & 0xFF));
      putByte((uInt8)((value >> 8) & 0xFF));
      putByte((uInt8)(value & 0xFF));
    }

    void putIntArray(const uInt32* array, uInt32 size)
    {
      if(array == 0 || size == 0 || myReadonly)
        return;

      for(uInt32 i = 0; i < size; ++i)
        putInt(array[i]);
    }

    void putString(const string& str)
    {
      putInt((uInt32)str.size());
      for(size_t i = 0; i < str.size(); ++i)
        putByte((uInt8)str[i]);
    }

    void putBool(bool b)
    {
      putByte(b ? TruePattern : FalsePattern);
    }

    std::string get()
    {
      return std::string((const char*)myData.data(), myData.size());
    }

    void set(const std::string& data)
    {
      myData.assign(data.begin(), data.end());
      myPos = 0;
      myValid = true;
    }

  private:
    std::vector<uInt8> myData;
    size_t myPos;
    bool myValid;
    bool myReadonly;

    enum {
      TruePattern  = 0xfe,
      FalsePattern = 0x01
    };
};

#endif