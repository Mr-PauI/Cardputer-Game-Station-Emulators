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
// $Id: Props.cxx 2838 2014-01-17 23:34:03Z stephena $
//============================================================================

#include <cctype>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "bspf.hxx"
#include "Props.hxx"

static string intToStringLocal(int value)
{
  char buf[16];
  char tmp[16];
  int pos = 0;
  bool neg = false;

  if(value == 0)
    return "0";

  if(value < 0)
  {
    neg = true;
    value = -value;
  }

  while(value > 0)
  {
    tmp[pos++] = char('0' + (value % 10));
    value /= 10;
  }

  int out = 0;
  if(neg)
    buf[out++] = '-';

  while(pos > 0)
    buf[out++] = tmp[--pos];

  buf[out] = '\0';
  return string(buf);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Properties::Properties()
{
  setDefaults();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Properties::Properties(const Properties& properties)
{
  copy(properties);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Properties::~Properties()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const string& Properties::get(PropertyType key) const
{
  if(key >= 0 && key < LastPropType)
    return myProperties[key];
  else {
    static const string empty;
    return empty;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Properties::set(PropertyType key, const string& value)
{
  if(key >= 0 && key < LastPropType)
  {
    myProperties[key] = value;

    switch(key)
    {
      case Cartridge_Type:
      case Display_Format:
        if(BSPF_equalsIgnoreCase(myProperties[key], "AUTO-DETECT"))
          myProperties[key] = "AUTO";
      case Cartridge_Sound:
      case Console_LeftDifficulty:
      case Console_RightDifficulty:
      case Console_TelevisionType:
      case Console_SwapPorts:
      case Controller_Left:
      case Controller_Right:
      case Controller_SwapPaddles:
      case Controller_MouseAxis:
      case Display_Phosphor:
      {
        transform(myProperties[key].begin(), myProperties[key].end(),
                  myProperties[key].begin(), (int(*)(int)) toupper);
        break;
      }

      case Display_PPBlend:
      {
        int blend = atoi(myProperties[key].c_str());
        if(blend < 0 || blend > 100) blend = 77;
        myProperties[key] = intToStringLocal(blend);
        break;
      }

      default:
        break;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Properties::load(FILE* in)
{
  setDefaults();

  if(!in)
    return;

  string key, value;
  for(;;)
  {
    key = readQuotedString(in);
    if(ferror(in))
      return;

    if(key == "")
      break;

    value = readQuotedString(in);
    if(ferror(in))
      return;

    PropertyType type = getPropertyType(key);
    set(type, value);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Properties::save(FILE* out) const
{
  if(!out)
    return;

  bool changed = false;
  for(int i = 0; i < LastPropType; ++i)
  {
    if(myProperties[i] != ourDefaultProperties[i])
    {
      writeQuotedString(out, ourPropertyNames[i]);
      fputc(' ', out);
      writeQuotedString(out, myProperties[i]);
      fputc('\n', out);
      changed = true;
    }
  }

  if(changed)
  {
    writeQuotedString(out, "");
    fputc('\n', out);
    fputc('\n', out);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
string Properties::readQuotedString(FILE* in)
{
  if(!in)
    return "";

  int c = 0;

  while((c = fgetc(in)) != EOF)
  {
    if(c == '"')
      break;
  }

  if(c == EOF)
    return "";

  string s;
  while((c = fgetc(in)) != EOF)
  {
    if(c == '\\')
    {
      int next = fgetc(in);
      if(next == '"' || next == '\\')
        s += char(next);
      else if(next == EOF)
        break;
      else
      {
        s += '\\';
        s += char(next);
      }
    }
    else if(c == '"')
      break;
    else if(c == '\r')
      continue;
    else
      s += char(c);
  }

  return s;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Properties::writeQuotedString(FILE* out, const string& s)
{
  if(!out)
    return;

  fputc('"', out);
  for(uInt32 i = 0; i < s.length(); ++i)
  {
    if(s[i] == '\\')
    {
      fputc('\\', out);
      fputc('\\', out);
    }
    else if(s[i] == '"')
    {
      fputc('\\', out);
      fputc('"', out);
    }
    else
      fputc(s[i], out);
  }
  fputc('"', out);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Properties& Properties::operator = (const Properties& properties)
{
  if(this != &properties)
    copy(properties);

  return *this;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Properties::copy(const Properties& properties)
{
  for(int i = 0; i < LastPropType; ++i)
    myProperties[i] = properties.myProperties[i];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Properties::print(FILE* out) const
{
  if(!out)
    return;

  fprintf(out, "%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
    get(Cartridge_MD5).c_str(),
    get(Cartridge_Name).c_str(),
    get(Cartridge_Manufacturer).c_str(),
    get(Cartridge_ModelNo).c_str(),
    get(Cartridge_Note).c_str(),
    get(Cartridge_Rarity).c_str(),
    get(Cartridge_Sound).c_str(),
    get(Cartridge_Type).c_str(),
    get(Console_LeftDifficulty).c_str(),
    get(Console_RightDifficulty).c_str(),
    get(Console_TelevisionType).c_str(),
    get(Console_SwapPorts).c_str(),
    get(Controller_Left).c_str(),
    get(Controller_Right).c_str(),
    get(Controller_SwapPaddles).c_str(),
    get(Controller_MouseAxis).c_str(),
    get(Display_Format).c_str(),
    get(Display_YStart).c_str(),
    get(Display_Height).c_str(),
    get(Display_Phosphor).c_str(),
    get(Display_PPBlend).c_str());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Properties::setDefaults()
{
  for(int i = 0; i < LastPropType; ++i)
    myProperties[i] = ourDefaultProperties[i];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
PropertyType Properties::getPropertyType(const string& name)
{
  for(int i = 0; i < LastPropType; ++i)
    if(ourPropertyNames[i] == name)
      return (PropertyType)i;

  return LastPropType;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Properties::printHeader(FILE* out)
{
  if(!out)
    return;

  fputs("Cartridge_MD5|"
        "Cartridge_Name|"
        "Cartridge_Manufacturer|"
        "Cartridge_ModelNo|"
        "Cartridge_Note|"
        "Cartridge_Rarity|"
        "Cartridge_Sound|"
        "Cartridge_Type|"
        "Console_LeftDifficulty|"
        "Console_RightDifficulty|"
        "Console_TelevisionType|"
        "Console_SwapPorts|"
        "Controller_Left|"
        "Controller_Right|"
        "Controller_SwapPaddles|"
        "Controller_MouseAxis|"
        "Display_Format|"
        "Display_YStart|"
        "Display_Height|"
        "Display_Phosphor|"
        "Display_PPBlend\n", out);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* Properties::ourDefaultProperties[LastPropType] = {
  "",          // Cartridge.MD5
  "",          // Cartridge.Manufacturer
  "",          // Cartridge.ModelNo
  "Untitled",  // Cartridge.Name
  "",          // Cartridge.Note
  "",          // Cartridge.Rarity
  "MONO",      // Cartridge.Sound
  "AUTO",      // Cartridge.Type
  "B",         // Console.LeftDifficulty
  "B",         // Console.RightDifficulty
  "COLOR",     // Console.TelevisionType
  "NO",        // Console.SwapPorts
  "JOYSTICK",  // Controller.Left
  "JOYSTICK",  // Controller.Right
  "NO",        // Controller.SwapPaddles
  "AUTO",      // Controller.MouseAxis
  "AUTO",      // Display.Format
  "34",        // Display.YStart
  "210",       // Display.Height
  "NO",        // Display.Phosphor
  "77"         // Display.PPBlend
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* Properties::ourPropertyNames[LastPropType] = {
  "Cartridge.MD5",
  "Cartridge.Manufacturer",
  "Cartridge.ModelNo",
  "Cartridge.Name",
  "Cartridge.Note",
  "Cartridge.Rarity",
  "Cartridge.Sound",
  "Cartridge.Type",
  "Console.LeftDifficulty",
  "Console.RightDifficulty",
  "Console.TelevisionType",
  "Console.SwapPorts",
  "Controller.Left",
  "Controller.Right",
  "Controller.SwapPaddles",
  "Controller.MouseAxis",
  "Display.Format",
  "Display.YStart",
  "Display.Height",
  "Display.Phosphor",
  "Display.PPBlend"
};