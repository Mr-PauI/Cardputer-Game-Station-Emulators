/*---------------------------------------------------------------------------
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version. See also the license.txt file for
 *	additional informations.
 *---------------------------------------------------------------------------
 */


#ifndef _MEMORYH_
#define _MEMORYH_

#ifndef INLINE
#define INLINE inline
#endif

// Additions by MrPaul for 16 native bus width reads(when possible) and optional boundary safety fallback
// #define NGP_OPTIMIZATION_16BIT_READ
//#define NGP_OPTIMIZATION_16BIT_WRITE
//#define NGP_Z80_16BIT_READ
//#define NGP_32BIT_READ
#define TLCSMEMREAD_BOUNDARY_SAFETY 0
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1) 
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#include "types.h"
#include "neopopsound.h"
#include "sound.h"
#include "input.h"
#include "graphics.h"
#include "flash.h"

#ifdef DRZ80
#include "DrZ80_support.h"
#else
#ifdef CZ80
#include "cz80_support.h"
#else
#include "z80.h"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum ROM size is 4 megabytes */
#define MAINROM_SIZE_MAX (0)

extern unsigned char* mainram;      // ex 64K statique
extern unsigned char* cpurom;       // ex 0x10000 statique
extern const unsigned char* mainrom;
extern unsigned char* cpuram; 
extern unsigned char  ldcRegs[64]; 

bool ngp_mem_alloc_init(size_t mainram_sz, size_t cpurom_sz);
void ngp_mem_free(void);
void ngp_mem_init(void);
bool ngp_mem_set_rom(const void *rom_base, size_t rom_len);

/* Z80 memory functions */

extern unsigned char (*z80MemReadB)(unsigned short addr);
extern unsigned short (*z80MemReadW)(unsigned short addr);
extern void (*z80MemWriteB)(unsigned short addr, unsigned char data);
extern void (*z80MemWriteW)(unsigned short addr, unsigned short data);
extern void (*z80PortWriteB)(unsigned char port, unsigned char data);
extern unsigned char (*z80PortReadB)(unsigned char port);

#if defined(DRZ80) || defined(CZ80)
unsigned char z80ngpMemReadB(unsigned short addr);
unsigned short z80ngpMemReadW(unsigned short addr);
void DrZ80ngpMemWriteB(unsigned char data, unsigned short addr);
void DrZ80ngpMemWriteW(unsigned short data, unsigned short addr);
void DrZ80ngpPortWriteB(unsigned short port, unsigned char data);
unsigned char DrZ80ngpPortReadB(unsigned short port);
#endif

extern unsigned char realBIOSloaded;

static inline const unsigned char* get_address(unsigned int addr)
{
   addr&= 0x00FFFFFF;
   if (addr<0x00200000)
   {
      if (addr<0x000008a0)
         return &cpuram[addr];
      if (addr>0x00003fff && addr<0x00018000)
      {
         switch (addr)  /* Thanks Koyote */
         {
            case 0x6F80:
               mainram[addr-0x00004000] = 0xFF;
               break;
            case 0x6F80+1:
               mainram[addr-0x00004000] = 0x03;
               break;
            case 0x6F85:
               mainram[addr-0x00004000] = 0x00;
               break;
            case 0x6F82:
               mainram[addr-0x00004000] = ngpInputState;
               break;
            case 0x6DA2:
               mainram[addr-0x00004000] = 0x80;
               break;
         }
         return &mainram[addr-0x00004000];
      }
   }
   else
   {
      if (addr<0x00400000)
         return &mainrom[(addr-0x00200000) /*&cartAddrMask*/];
      if(addr<0x00800000) /* Flavor added */
         return 0;
      if (addr<0x00A00000)
         return &mainrom[(addr-(0x00800000-0x00200000))/*&cartAddrMask*/];
      if(addr<0x00FF0000) /* Flavor added */
         return 0;

      return &cpurom[addr-0x00ff0000];
   }
   return 0;  /* Flavor ERROR */
}

/* read a byte from a memory address (addr) */
static INLINE unsigned char tlcsMemReadB(unsigned int addr)
{
	addr&= 0x00FFFFFF;

	if(currentCommand == COMMAND_INFO_READ)
        return flashReadInfo(addr);

	if (addr < 0x00200000)
   {
      if (addr < 0x000008A0)
      {
         if(addr == 0xBC)
            ngpSoundExecute();
         return cpuram[addr];
      }
      else if (addr > 0x00003FFF && addr < 0x00018000)
      {
         switch (addr)  /* Thanks Koyote */
         {
            case 0x6DA2:
               return 0x80;
            case 0x6F80:
               return 0xFF;
            case 0x6F80+1:
               return 0x03;
            case 0x6F85:
               return 0x00;
            case 0x6F82:
               return ngpInputState;
            default:
               break;
         }
         return mainram[addr-0x00004000];
      }
   }
	else
	{
		if (addr<0x00400000)
            return mainrom[(addr-0x00200000)/*&cartAddrMask*/];
		if (addr<0x00800000)
            return 0xFF;
		if (addr<0x00a00000)
            return mainrom[(addr-(0x00800000-0x00200000))/*&cartAddrMask*/];
		if (addr<0x00ff0000)
            return 0xFF;
		return cpurom[addr-0x00ff0000];
	}
	return 0xFF;
}

#if defined NGP_OPTIMIZATION_16BIT_READ
static INLINE unsigned short tlcsMemReadW(unsigned int addr)
{
    const unsigned char *gA = get_address(addr);
    if (!gA) return 0;

   return (gA[0] | (gA[1] << 8));
}
#else
/* read a word from a memory address (addr) */
static INLINE unsigned short tlcsMemReadW(unsigned int addr)
{
#ifdef TARGET_GP2X
   register unsigned short i asm("r0");
   register unsigned char *gA asm("r1");

   gA = get_address(addr);

   if(gA == 0)
      return 0;

   asm volatile(
         "ldrb	%0, [%1]\n\t"
         "ldrb	r2, [%1, #1]\n\t"
         "orr	%0, %0, r2, asl #8"
         : "=r" (i)
         : "r" (gA)
         : "r2");

   return i;
#else
   return tlcsMemReadB(addr) | (tlcsMemReadB(addr+1) << 8);
#endif
}
#endif


#ifdef NGP_32BIT_READ
static inline unsigned int tlcsMemReadL(unsigned int addr)
{
    addr &= 0x00FFFFFF;

    /* RAM: cpuram */
    if (addr < 0x00200000)
    {
        if (addr < 0x000008A0)
        {
            /* direct 32-bit read from cpuram (ESP32-S3 RAM safe) */
            return *(uint32_t *)&cpuram[addr];
        }

        /* mainram range with special registers */
        if (addr > 0x00003FFF && addr < 0x00018000)
        {
            unsigned int off = addr - 0x00004000;

            switch (addr)  /* Thanks Koyote */
            {
                case 0x6F80:
                    mainram[off] = 0xFF;
                    break;
                case 0x6F80 + 1:
                    mainram[off] = 0x03;
                    break;
                case 0x6F85:
                    mainram[off] = 0x00;
                    break;
                case 0x6F82:
                    mainram[off] = ngpInputState;
                    break;
                case 0x6DA2:
                    mainram[off] = 0x80;
                    break;
                default:
                    break;
            }

            return *(uint32_t *)&mainram[off];
        }
    }
    else
    {
        unsigned int i;

        /* ROM region 0x00200000 - 0x003FFFFF */
        if (addr < 0x00400000)
        {
            const uint8_t *p = &mainrom[addr - 0x00200000];
            i  = *(p++);
            i |= (*(p++)) << 8;
            i |= (*(p++)) << 16;
            i |= (unsigned int)(*p) << 24;
            return i;
        }

        if (addr < 0x00800000) /* Flavor added: unmapped region */
            return 0;

        /* ROM region 0x00800000 - 0x009FFFFF (mirrored) */
        if (addr < 0x00A00000)
        {
            const uint8_t *p = &mainrom[addr - (0x00800000 - 0x00200000)];
            i  = *(p++);
            i |= (*(p++)) << 8;
            i |= (*(p++)) << 16;
            i |= (unsigned int)(*p) << 24;
            return i;
        }

        if (addr < 0x00FF0000) /* Flavor added: unmapped region */
            return 0;

        /* cpurom region */
        {
            const uint8_t *p = &cpurom[addr - 0x00FF0000];
            i  = *(p++);
            i |= (*(p++)) << 8;
            i |= (*(p++)) << 16;
            i |= (unsigned int)(*p) << 24;
            return i;
        }
    }

    /* Shouldn't reach here — return 0 as error fallback */
    return 0;
}
#else
/* read a long word from a memory address (addr) */
static INLINE unsigned int tlcsMemReadL(unsigned int addr)
{
#ifdef TARGET_GP2X
   register unsigned int i asm("r0");
   register unsigned char *gA asm("r4");

   gA = get_address(addr);

   if(gA == 0)
      return 0;

   asm volatile(
         "bic	r1,%1,#3	\n"
         "ldmia	r1,{r0,r3}	\n"
         "ands	r1,%1,#3	\n"
         "movne	r2,r1,lsl #3	\n"
         "movne	r0,r0,lsr r2	\n"
         "rsbne	r1,r2,#32	\n"
         "orrne	r0,r0,r3,lsl r1"
         : "=r"(i)
         : "r"(gA)
         : "r1","r2","r3");

   return i;
#else
   unsigned int i;
   const unsigned char *gA = get_address(addr);

   if(gA == 0)
      return 0;

   i = *(gA++);
   i |= (*(gA++)) << 8;
   i |= (*(gA++)) << 16;
   i |= (unsigned int)(*gA) << 24;

   return i;
#endif
}
#endif

/* write a byte (data) to a memory address (addr) */
static INLINE void tlcsMemWriteB(unsigned int addr, unsigned char data)
{
   addr&= 0x00FFFFFF;
   if (addr<0x000008a0)
   {
      switch(addr)
      {
         case 0xA0:	/* L CH Sound Source Control Register */
            if (cpuram[0xB8] == 0x55 && cpuram[0xB9] == 0xAA)
               Write_SoundChipNoise(data);/*Flavor SN76496Write(0, data); */
            break;
         case 0xA1:	/* R CH Sound Source Control Register */
            if (cpuram[0xB8] == 0x55 && cpuram[0xB9] == 0xAA)
               Write_SoundChipTone(data); /*Flavor SN76496Write(0, data); */
            break;
         case 0xA2:	/* L CH DAC Control Register */
            ngpSoundExecute();
            if (cpuram[0xB8] == 0xAA)
               dac_writeL(data); /*Flavor DAC_data_w(0,data); */
            break;
         case 0xB8:	/* Z80 Reset */
         case 0xB9:	/* Sourd Source Reset Control Register */
            switch(data)
            {
               case 0x55:
                  ngpSoundStart();
                  break;
               case 0xAA:
                  ngpSoundExecute();
                  ngpSoundOff();
                  break;
            }
            break;
         case 0xBA:
            ngpSoundExecute();
#if defined(DRZ80) || defined(CZ80)
            Z80_Cause_Interrupt(Z80_NMI_INT);
#else
            z80Interrupt(Z80NMI);
#endif
            break;
      }
      cpuram[addr] = data;
      return;
   }
   else if (addr>0x00003fff && addr<0x00018000)
   {
      if (addr == 0x87E2 && mainram[0x47F0] != 0xAA)
         return;		/* disallow writes to GEMODE */

      mainram[addr-0x00004000] = data;
      return;
   }
   else if (addr>=0x00200000 && addr<0x00400000)
      flashChipWrite(addr, data);
   else if (addr>=0x00800000 && addr<0x00A00000)
      flashChipWrite(addr, data);
}

#ifdef NGP_16BIT_WRITE
/* write a word (data) to a memory address (addr) */
static INLINE void tlcsMemWriteW16(unsigned int addr, unsigned short data)
{
    addr &= 0x00FFFFFF;

    if (addr < 0x000008a0)
    {
        unsigned char low = (unsigned char)(data); // auto-truncate to 8 bits
        unsigned char high = (unsigned char)((data >> 8)); // auto-truncate to 8 bits
        /* segmented I/O region: use byte writes to trigger side effects */
        tlcsMemWriteB(addr, low);
        tlcsMemWriteB(addr + 1, high);
        return;
    }
    else if (addr > 0x00003fff && addr < 0x00018000)
    {
        /* main RAM: direct 16-bit write for speed */
        if (addr == 0x87E2 && mainram[0x47F0] != 0xAA)
            return;  /* disallow writes to GEMODE */
         uint8_t* p= &mainram[addr - 0x00004000];
         if (((uintptr_t)p)&0x1)
         {
             *(p++) = (unsigned char)(data);
             *(p)   = (unsigned char)(data >> 8);
         }
         else     
            *(uint16_t *)(mainram + (addr - 0x00004000)) = data;
        return;
    }
    else if ((addr >= 0x00200000 && addr < 0x00400000) ||
             (addr >= 0x00800000 && addr < 0x00A00000))
    {
        /* flash regions: still need to write two bytes */
        unsigned char low = (unsigned char)(data); // auto-truncate to 8 bits
        unsigned char high = (unsigned char)((data >> 8)); // auto-truncate to 8 bits
        flashChipWrite(addr, low);
        flashChipWrite(addr + 1, high);
        return;
    }
}
#endif

#ifdef __cplusplus
}
#endif

#endif  /* _MEMORYH_ */
