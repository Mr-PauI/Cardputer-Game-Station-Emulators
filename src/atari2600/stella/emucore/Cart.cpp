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
// $Id: Cart.cxx 2839 2014-01-19 19:41:27Z stephena $
//============================================================================

// // #include <cassert>
#include <cstring>

#include "bspf.hxx"
#include "Cart.hxx"
#include "Cart0840.hxx"
#include "Cart2K.hxx"
#include "Cart3E.hxx"
#include "Cart3F.hxx"
#include "Cart4A50.hxx"
#include "Cart4K.hxx"
#include "Cart4KSC.hxx"
#include "CartAR.hxx"
#include "CartCM.hxx"
#include "CartCTY.hxx"
#include "CartCV.hxx"
#include "CartDPC.hxx"
#include "CartDPCPlus.hxx"
#include "CartE0.hxx"
#include "CartE7.hxx"
#include "CartEF.hxx"
#include "CartEFSC.hxx"
#include "CartBF.hxx"
#include "CartBFSC.hxx"
#include "CartDF.hxx"
#include "CartDFSC.hxx"
#include "CartF0.hxx"
#include "CartF4.hxx"
#include "CartF4SC.hxx"
#include "CartF6.hxx"
#include "CartF6SC.hxx"
#include "CartF8.hxx"
#include "CartF8SC.hxx"
#include "CartFA.hxx"
#include "CartFA2.hxx"
#include "CartFE.hxx"
#include "CartMC.hxx"
#include "CartSB.hxx"
#include "CartUA.hxx"
#include "CartX07.hxx"
#include "MD5.hxx"
#include "Props.hxx"
#include "Settings.hxx"
#ifdef DEBUGGER_SUPPORT
  #include "Debugger.hxx"
  #include "CartDebug.hxx"
#endif

static string makeAboutString(const string& type, const string& autodetect,
                              const string& id, uInt32 size)
{
  char tmp[96];
  if(size < 1024)
    std::snprintf(tmp, sizeof(tmp), "%s%s%s (%uB) ",
      type.c_str(), autodetect.c_str(), id.c_str(), (unsigned)size);
  else
    std::snprintf(tmp, sizeof(tmp), "%s%s%s (%uK) ",
      type.c_str(), autodetect.c_str(), id.c_str(), (unsigned)(size / 1024));

  return string(tmp);
}

static string makeMulticartId(uInt32 index)
{
  char tmp[16];
  std::snprintf(tmp, sizeof(tmp), " [G%u]", (unsigned)(index + 1));
  return string(tmp);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Cartridge* Cartridge::create(const uInt8* image, uInt32 size, string& md5,
     string& dtype, string& id, const OSystem& osystem, Settings& settings)
{
  Cartridge* cartridge = 0;
  string type = dtype;
  string autodetect = "";

  // See if we should try to auto-detect the cartridge type
  // If we ask for extended info, always do an autodetect
  if(type == "AUTO" || settings.getBool("rominfo"))
  {
    const string& detected = autodetectType(image, size);
    autodetect = "*";
    // if(type != "AUTO" && type != detected)
      // cerr << "Auto-detection not consistent: " << type << ", " << detected << endl;

    type = detected;
  }

  // Check for multicart first; if found, get the correct part of the image
  if(type == "2IN1")
  {
    if(size == 2*2048 || size == 2*4096 || size == 2*8192 || size == 2*16384)
    {
      dtype = type;
      type = createFromMultiCart(image, size, 2, md5, id, settings);
    }
    else
      dtype = "WRONG_SIZE";
  }
  else if(type == "4IN1")
  {
    if(size == 4*2048 || size == 4*4096 || size == 4*8192)
    {
      dtype = type;
      type = createFromMultiCart(image, size, 4, md5, id, settings);
    }
    else
      dtype = "WRONG_SIZE";
  }
  else if(type == "8IN1")
  {
    if(size == 8*2048 || size == 8*4096 || size == 8*8192)
    {
      dtype = type;
      type = createFromMultiCart(image, size, 8, md5, id, settings);
    }
    else
      dtype = "WRONG_SIZE";
  }
  else if(type == "16IN1")
  {
    if(size == 16*2048 || size == 16*4096 || size == 16*8192)
    {
      dtype = type;
      type = createFromMultiCart(image, size, 16, md5, id, settings);
    }
    else
      dtype = "WRONG_SIZE";
  }
  else if(type == "32IN1")
  {
    if(size == 32*2048 || size == 32*4096)
    {
      dtype = type;
      type = createFromMultiCart(image, size, 32, md5, id, settings);
    }
    else
      dtype = "WRONG_SIZE";
  }
  else if(type == "64IN1")
  {
    if(size == 64*2048 || size == 64*4096)
    {
      dtype = type;
      type = createFromMultiCart(image, size, 64, md5, id, settings);
    }
    else
      dtype = "WRONG_SIZE";
  }
  else if(type == "128IN1")
  {
    if(size == 128*2048 || size == 128*4096)
    {
      dtype = type;
      type = createFromMultiCart(image, size, 128, md5, id, settings);
    }
    else
      dtype = "WRONG_SIZE";
  }

  // We should know the cart's type by now so let's create it
  if(type == "0840")
    cartridge = new Cartridge0840(image, size, settings);
  else if(type == "2K")
    cartridge = new Cartridge2K(image, size, settings);
  else if(type == "3E")
    cartridge = new Cartridge3E(image, size, settings);
  else if(type == "3F")
    cartridge = new Cartridge3F(image, size, settings);
  // else if(type == "4A50")
  //   cartridge = new Cartridge4A50(image, size, settings);
  else if(type == "4K")
    cartridge = new Cartridge4K(image, size, settings);
  else if(type == "4KSC")
    cartridge = new Cartridge4KSC(image, size, settings);
  else if(type == "AR")
    cartridge = new CartridgeAR(image, size, settings);
  else if(type == "CM")
    cartridge = new CartridgeCM(image, size, settings);
  // else if(type == "CTY")
  //   cartridge = new CartridgeCTY(image, size, osystem);
  else if(type == "CV")
    cartridge = new CartridgeCV(image, size, settings);
  else if(type == "DPC")
    cartridge = new CartridgeDPC(image, size, settings);
  else if(type == "DPC+")
    cartridge = new CartridgeDPCPlus(image, size, settings);
  else if(type == "E0")
    cartridge = new CartridgeE0(image, size, settings);
  else if(type == "E7")
    cartridge = new CartridgeE7(image, size, settings);
  else if(type == "EF")
    cartridge = new CartridgeEF(image, size, settings);
  else if(type == "EFSC")
    cartridge = new CartridgeEFSC(image, size, settings);
  // else if(type == "BF")
  //   cartridge = new CartridgeBF(image, size, settings);
  // else if(type == "BFSC")
  //   cartridge = new CartridgeBFSC(image, size, settings);
  // else if(type == "DF")
  //   cartridge = new CartridgeDF(image, size, settings);
  // else if(type == "DFSC")
  //   cartridge = new CartridgeDFSC(image, size, settings);
  else if(type == "F0" || type == "MB")
    cartridge = new CartridgeF0(image, size, settings);
  else if(type == "F4")
    cartridge = new CartridgeF4(image, size, settings);
  else if(type == "F4SC")
    cartridge = new CartridgeF4SC(image, size, settings);
  else if(type == "F6")
    cartridge = new CartridgeF6(image, size, settings);
  else if(type == "F6SC")
    cartridge = new CartridgeF6SC(image, size, settings);
  else if(type == "F8")
    cartridge = new CartridgeF8(image, size, md5, settings);
  else if(type == "F8SC")
    cartridge = new CartridgeF8SC(image, size, settings);
  else if(type == "FA" || type == "FASC")
    cartridge = new CartridgeFA(image, size, settings);
  else if(type == "FA2")
    cartridge = new CartridgeFA2(image, size, osystem);
  else if(type == "FE")
    cartridge = new CartridgeFE(image, size, settings);
  // else if(type == "MC")
  //   cartridge = new CartridgeMC(image, size, settings);
  else if(type == "UA")
    cartridge = new CartridgeUA(image, size, settings);
  else if(type == "SB")
    cartridge = new CartridgeSB(image, size, settings);
  else if(type == "X07")
    cartridge = new CartridgeX07(image, size, settings);
  else if(dtype == "WRONG_SIZE")
  {
    return NULL;
  }
  else
  {
    return NULL;
  }

  aboutStorage() = makeAboutString(type, autodetect, id, size);

  return cartridge;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
string Cartridge::createFromMultiCart(const uInt8*& image, uInt32& size,
    uInt32 numroms, string& md5, string& id, Settings& settings)
{
  uInt32 i = settings.getInt("romloadcount");
  size /= numroms;
  image += i * size;

  md5 = MD5(image, size);
  id = makeMulticartId(i);

  settings.setValue("romloadcount", (i + 1) % numroms);

  if(size <= 2048)       return "2K";
  else if(size == 4096)  return "4K";
  else if(size == 8192)  return "F8";
  else                   return "4K";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Cartridge::Cartridge(const Settings& settings)
  : mySettings(settings),
    myStartBank(0),
    myBankChanged(true),
    myCodeAccessBase(NULL),
    myBankLocked(false)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Cartridge::~Cartridge()
{
  if(myCodeAccessBase)
    delete[] myCodeAccessBase;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge::save()
{
  // cerr << "Cartridge::save disabled on this build" << endl;
  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt16 Cartridge::startBank()
{
  return myStartBank;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge::bankChanged()
{
  bool changed = myBankChanged;
  myBankChanged = false;
  return changed;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge::registerRamArea(uInt16 start, uInt16 size,
                                uInt16 roffset, uInt16 woffset)
{
#ifdef DEBUGGER_SUPPORT
  RamArea area;
  area.start   = start;
  area.size    = size;
  area.roffset = roffset;
  area.woffset = woffset;
  myRamAreaList.push_back(area);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge::triggerReadFromWritePort(uInt16 address)
{
#ifdef DEBUGGER_SUPPORT
  if(!mySystem->autodetectMode())
    Debugger::debugger().cartDebug().triggerReadFromWritePort(address);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge::createCodeAccessBase(uInt32 size)
{
#ifdef DEBUGGER_SUPPORT
  myCodeAccessBase = new uInt8[size];
  memset(myCodeAccessBase, CartDebug::ROW, size);
#else
  myCodeAccessBase = NULL;
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
string Cartridge::autodetectType(const uInt8* image, uInt32 size)
{
  const char* type = 0;

  if((size % 8448) == 0 || size == 6144)
  {
    type = "AR";
  }
  else if(size < 2048)
  {
    type = "2K";
  }
  else if((size == 2048) ||
          (size == 4096 && memcmp(image, image + 2048, 2048) == 0))
  {
    type = isProbablyCV(image, size) ? "CV" : "2K";
  }
  else if(size == 4096)
  {
    if(isProbablyCV(image,size))
      type = "CV";
    else if(isProbably4KSC(image,size))
      type = "4KSC";
    else
      type = "4K";
  }
  else if(size == 8*1024)
  {
    uInt8 signature[] = { 0x8D, 0xF9, 0x1F };
    bool f8 = searchForBytes(image, size, signature, 3, 2);

    if(isProbablySC(image, size))
      type = "F8SC";
    else if(memcmp(image, image + 4096, 4096) == 0)
      type = "4K";
    else if(isProbablyE0(image, size))
      type = "E0";
    else if(isProbably3E(image, size))
      type = "3E";
    else if(isProbably3F(image, size))
      type = "3F";
    else if(isProbablyUA(image, size))
      type = "UA";
    else if(isProbablyFE(image, size) && !f8)
      type = "FE";
    else if(isProbably0840(image, size))
      type = "0840";
    else
      type = "F8";
  }
  else if(size >= 10240 && size <= 10496)
  {
    type = "DPC";
  }
  else if(size == 12*1024)
  {
    type = "FA";
  }
  else if(size == 16*1024)
  {
    if(isProbablySC(image, size))
      type = "F6SC";
    else if(isProbablyE7(image, size))
      type = "E7";
    else if(isProbably3E(image, size))
      type = "3E";
    else
      type = "F6";
  }
  else if(size == 24*1024 || size == 28*1024)
  {
    type = "FA2";
  }
  else if(size == 29*1024)
  {
    if(isProbablyARM(image, size))
      type = "FA2";
    else
      type = "DPC+";
  }
  else if(size == 32*1024)
  {
    if(isProbablySC(image, size))
      type = "F4SC";
    else if(isProbably3E(image, size))
      type = "3E";
    else if(isProbably3F(image, size))
      type = "3F";
    else if(isProbablyDPCplus(image, size))
      type = "DPC+";
    else if(isProbablyCTY(image, size))
      type = "CTY";
    else if(isProbablyFA2(image, size))
      type = "FA2";
    else
      type = "F4";
  }
  else if(size == 64*1024)
  {
    if(isProbably3E(image, size))
      type = "3E";
    else if(isProbably3F(image, size))
      type = "3F";
    else if(isProbably4A50(image, size))
      type = "4A50";
    else if(isProbablyEF(image, size, type))
      ;
    else if(isProbablyX07(image, size))
      type = "X07";
    else
      type = "F0";
  }
  else if(size == 128*1024)
  {
    if(isProbably3E(image, size))
      type = "3E";
    else if(isProbablyDF(image, size, type))
      ;
    else if(isProbably3F(image, size))
      type = "3F";
    else if(isProbably4A50(image, size))
      type = "4A50";
    else if(isProbablySB(image, size))
      type = "SB";
    else
      type = "MC";
  }
  else if(size == 256*1024)
  {
    if(isProbably3E(image, size))
      type = "3E";
    else if(isProbablyBF(image, size, type))
      ;
    else if(isProbably3F(image, size))
      type = "3F";
    else
      type = "SB";
  }
  else
  {
    if(isProbably3E(image, size))
      type = "3E";
    else if(isProbably3F(image, size))
      type = "3F";
    else
      type = "4K";
  }

  return type;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge::searchForBytes(const uInt8* image, uInt32 imagesize,
                               const uInt8* signature, uInt32 sigsize,
                               uInt32 minhits)
{
  uInt32 count = 0;
  for(uInt32 i = 0; i < imagesize - sigsize; ++i)
  {
    uInt32 matches = 0;
    for(uInt32 j = 0; j < sigsize; ++j)
    {
      if(image[i+j] == signature[j])
        ++matches;
      else
        break;
    }
    if(matches == sigsize)
    {
      ++count;
      i += sigsize;
    }
    if(count >= minhits)
      break;
  }

  return (count >= minhits);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge::isProbablySC(const uInt8* image, uInt32 size)
{
  uInt32 banks = size / 4096;
  for(uInt32 i = 0; i < banks; ++i)
  {
    uInt8 first = image[i*4096];
    for(uInt32 j = 0; j < 256; ++j)
    {
      if(image[i*4096+j] != first)
        return false;
    }
  }
  return true;
}

bool Cartridge::isProbably4KSC(const uInt8* image, uInt32 size)
{
  uInt8 first = image[0];
  for(uInt32 i = 1; i < 256; ++i)
    if(image[i] != first)
      return false;

  if((image[size-6] == 'S') && (image[size-5] == 'C'))
    return true;

  return false;
}

bool Cartridge::isProbablyARM(const uInt8* image, uInt32 size)
{
  uInt8 signature[2][4] = {
    { 0xA0, 0xC1, 0x1F, 0xE0 },
    { 0x00, 0x80, 0x02, 0xE0 }
  };
  if(searchForBytes(image, 1024, signature[0], 4, 1))
    return true;
  else
    return searchForBytes(image, 1024, signature[1], 4, 1);
}

bool Cartridge::isProbably0840(const uInt8* image, uInt32 size)
{
  uInt8 signature1[3][3] = {
    { 0xAD, 0x00, 0x08 },
    { 0xAD, 0x40, 0x08 },
    { 0x2C, 0x00, 0x08 }
  };
  for(uInt32 i = 0; i < 3; ++i)
    if(searchForBytes(image, size, signature1[i], 3, 2))
      return true;

  uInt8 signature2[2][4] = {
    { 0x0C, 0x00, 0x08, 0x4C },
    { 0x0C, 0xFF, 0x0F, 0x4C }
  };
  for(uInt32 i = 0; i < 2; ++i)
    if(searchForBytes(image, size, signature2[i], 4, 2))
      return true;

  return false;
}

bool Cartridge::isProbably3E(const uInt8* image, uInt32 size)
{
  uInt8 signature[] = { 0x85, 0x3E, 0xA9, 0x00 };
  return searchForBytes(image, size, signature, 4, 1);
}

bool Cartridge::isProbably3F(const uInt8* image, uInt32 size)
{
  uInt8 signature[] = { 0x85, 0x3F };
  return searchForBytes(image, size, signature, 2, 2);
}

bool Cartridge::isProbably4A50(const uInt8* image, uInt32 size)
{
  if(image[size-6] == 0x50 && image[size-5] == 0x4A)
    return true;

  if(((image[0xfffd] & 0x1f) == 0x1f) &&
     (image[image[0xfffd] * 256 + image[0xfffc]] == 0x0c) &&
     ((image[image[0xfffd] * 256 + image[0xfffc] + 2] & 0xfe) == 0x6e))
    return true;

  return false;
}

bool Cartridge::isProbablyCTY(const uInt8* image, uInt32 size)
{
  (void)image;
  (void)size;
  return false;
}

bool Cartridge::isProbablyCV(const uInt8* image, uInt32 size)
{
  uInt8 signature[2][3] = {
    { 0x9D, 0xFF, 0xF3 },
    { 0x99, 0x00, 0xF4 }
  };
  if(searchForBytes(image, size, signature[0], 3, 1))
    return true;
  else
    return searchForBytes(image, size, signature[1], 3, 1);
}

bool Cartridge::isProbablyDPCplus(const uInt8* image, uInt32 size)
{
  uInt8 signature[] = { 'D', 'P', 'C', '+' };
  return searchForBytes(image, size, signature, 4, 2);
}

bool Cartridge::isProbablyE0(const uInt8* image, uInt32 size)
{
  uInt8 signature[8][3] = {
    { 0x8D, 0xE0, 0x1F },
    { 0x8D, 0xE0, 0x5F },
    { 0x8D, 0xE9, 0xFF },
    { 0x0C, 0xE0, 0x1F },
    { 0xAD, 0xE0, 0x1F },
    { 0xAD, 0xE9, 0xFF },
    { 0xAD, 0xED, 0xFF },
    { 0xAD, 0xF3, 0xBF }
  };
  for(uInt32 i = 0; i < 8; ++i)
    if(searchForBytes(image, size, signature[i], 3, 1))
      return true;

  return false;
}

bool Cartridge::isProbablyE7(const uInt8* image, uInt32 size)
{
  uInt8 signature[7][3] = {
    { 0xAD, 0xE2, 0xFF },
    { 0xAD, 0xE5, 0xFF },
    { 0xAD, 0xE5, 0x1F },
    { 0xAD, 0xE7, 0x1F },
    { 0x0C, 0xE7, 0x1F },
    { 0x8D, 0xE7, 0xFF },
    { 0x8D, 0xE7, 0x1F }
  };
  for(uInt32 i = 0; i < 7; ++i)
    if(searchForBytes(image, size, signature[i], 3, 1))
      return true;

  return false;
}

bool Cartridge::isProbablyEF(const uInt8* image, uInt32 size, const char*& type)
{
  uInt8 efef[] = { 'E', 'F', 'E', 'F' };
  uInt8 efsc[] = { 'E', 'F', 'S', 'C' };
  if(searchForBytes(image + size - 8, 8, efef, 4, 1))
  {
    type = "EF";
    return true;
  }
  else if(searchForBytes(image + size - 8, 8, efsc, 4, 1))
  {
    type = "EFSC";
    return true;
  }

  bool isEF = false;
  uInt8 signature[4][3] = {
    { 0x0C, 0xE0, 0xFF },
    { 0xAD, 0xE0, 0xFF },
    { 0x0C, 0xE0, 0x1F },
    { 0xAD, 0xE0, 0x1F }
  };
  for(uInt32 i = 0; i < 4; ++i)
  {
    if(searchForBytes(image, size, signature[i], 3, 1))
    {
      isEF = true;
      break;
    }
  }

  if(isEF)
  {
    type = isProbablySC(image, size) ? "EFSC" : "EF";
    return true;
  }

  return false;
}

bool Cartridge::isProbablyBF(const uInt8* image, uInt32 size, const char*& type)
{
  uInt8 bf[] = { 'B', 'F', 'B', 'F' };
  uInt8 bfsc[] = { 'B', 'F', 'S', 'C' };
  if(searchForBytes(image + size - 8, 8, bf, 4, 1))
  {
    type = "BF";
    return true;
  }
  else if(searchForBytes(image + size - 8, 8, bfsc, 4, 1))
  {
    type = "BFSC";
    return true;
  }

  return false;
}

bool Cartridge::isProbablyDF(const uInt8* image, uInt32 size, const char*& type)
{
  uInt8 df[] = { 'D', 'F', 'D', 'F' };
  uInt8 dfsc[] = { 'D', 'F', 'S', 'C' };
  if(searchForBytes(image + size - 8, 8, df, 4, 1))
  {
    type = "DF";
    return true;
  }
  else if(searchForBytes(image + size - 8, 8, dfsc, 4, 1))
  {
    type = "DFSC";
    return true;
  }

  return false;
}

bool Cartridge::isProbablyFA2(const uInt8* image, uInt32 size)
{
  for(uInt32 i = 29*1024; i < 32*1024; ++i)
    if(image[i] != 0)
      return false;

  return true;
}

bool Cartridge::isProbablyFE(const uInt8* image, uInt32 size)
{
  uInt8 signature[4][5] = {
    { 0x20, 0x00, 0xD0, 0xC6, 0xC5 },
    { 0x20, 0xC3, 0xF8, 0xA5, 0x82 },
    { 0xD0, 0xFB, 0x20, 0x73, 0xFE },
    { 0x20, 0x00, 0xF0, 0x84, 0xD6 }
  };
  for(uInt32 i = 0; i < 4; ++i)
    if(searchForBytes(image, size, signature[i], 5, 1))
      return true;

  return false;
}

bool Cartridge::isProbablySB(const uInt8* image, uInt32 size)
{
  uInt8 signature[2][3] = {
    { 0xBD, 0x00, 0x08 },
    { 0xAD, 0x00, 0x08 }
  };
  if(searchForBytes(image, size, signature[0], 3, 1))
    return true;
  else
    return searchForBytes(image, size, signature[1], 3, 1);
}

bool Cartridge::isProbablyUA(const uInt8* image, uInt32 size)
{
  uInt8 signature[3][3] = {
    { 0x8D, 0x40, 0x02 },
    { 0xAD, 0x40, 0x02 },
    { 0xBD, 0x1F, 0x02 }
  };
  for(uInt32 i = 0; i < 3; ++i)
    if(searchForBytes(image, size, signature[i], 3, 1))
      return true;

  return false;
}

bool Cartridge::isProbablyX07(const uInt8* image, uInt32 size)
{
  uInt8 signature[6][3] = {
    { 0xAD, 0x0D, 0x08 },
    { 0xAD, 0x1D, 0x08 },
    { 0xAD, 0x2D, 0x08 },
    { 0x0C, 0x0D, 0x08 },
    { 0x0C, 0x1D, 0x08 },
    { 0x0C, 0x2D, 0x08 }
  };
  for(uInt32 i = 0; i < 6; ++i)
    if(searchForBytes(image, size, signature[i], 3, 1))
      return true;

  return false;
}

Cartridge::Cartridge(const Cartridge& cart)
  : mySettings(cart.mySettings)
{
  assert(false);
}

Cartridge& Cartridge::operator = (const Cartridge&)
{
  assert(false);
  return *this;
}

// string Cartridge::myAboutString = "";