/******************************************************************************
 * sample.c
 * Audio sample management.
 *-----------------------------------------------------------------------------
 * Copyright (C) 2015 - 2016 Osman Celimli
 * For conditions of distribution and use, see copyright notice in coretone.c
 ******************************************************************************/
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "../types.h"
#include "coretone.h"

#include "sample.h"

/******************************************************************************
 * !!!!----   ACTIVE SAMPLE PACKAGE and the FREQUENCY RATIO TABLE    ----!!!!
 ******************************************************************************/
const char szCoreSample_Magic[] = CORETONE_SMPPAK_HEAD_MAGICWORD;
const int8_t acCoreSample_Dummy[] = {0,0,0,0};

uint8_t *pCoreSample_PackBase = NULL;
uint32_t *pCoreSample_DirBase = NULL;
uint32_t uiCoreSample_Count = 0;

int32p32_t *aCoreSample_Fr = NULL;

static void* ct_sample_Alloc(size_t size)
{
	return malloc(size);
}

void ct_sample_shutdown(void)
{
	if(aCoreSample_Fr)
	{
		free(aCoreSample_Fr);
		aCoreSample_Fr = NULL;
	}

	pCoreSample_PackBase = NULL;
	pCoreSample_DirBase = NULL;
	uiCoreSample_Count = 0;
}

static int32_t ct_sample_EnsureAllocated(void)
{
	if(!aCoreSample_Fr)
	{
		aCoreSample_Fr = (int32p32_t*)ct_sample_Alloc(
			sizeof(int32p32_t) * CORETONE_SAMPLES_MAXENTRIES);
		if(!aCoreSample_Fr)
			return -1;

		memset(aCoreSample_Fr, 0,
			sizeof(int32p32_t) * CORETONE_SAMPLES_MAXENTRIES);
	}

	return 0;
}

/* void ct_sample_get(uint32_t uiSample, int8_t **ppData, uint32_t *puiLen)
 *  Fetch the base address and length of sample number uiSample, if the sample
 * is outside the range of the current pack a dummy sample will be provided.
 *----------------------------------------------------------------------------*/
void ct_sample_get(uint32_t uiSample, int8_t **ppData, uint32_t *puiLen)
{
	uint32_t *pEntry;

	if(uiSample < uiCoreSample_Count)
	{
		pEntry = pCoreSample_DirBase + (CORETONE_SMPPAK_ENTRY_SIZE * uiSample);
		*ppData = (int8_t*)(pCoreSample_PackBase + pEntry[CORETONE_SMPPAK_ENTRY_OFF]);
		*puiLen = pEntry[CORETONE_SMPPAK_ENTRY_SLEN];
	}
	else
	{
		*ppData = (int8_t*)acCoreSample_Dummy;
		*puiLen = 0;
	}
}

/* int16p16_t ct_sample_calcPhase(uint32_t uiSample, int16p16_t freq)
 *  Calculate the phase adjustment value for the given sample number based
 * upon a desired playback frequency in Hz.
 *----------------------------------------------------------------------------*/
int16p16_t ct_sample_calcPhase(uint32_t uiSample, int16p16_t freq)
{
	int32p32_t phaseAdj, freqBase, freqRatio;
	int16p16_t phaseTrim;

	phaseTrim.uiWhole = 0;

	if(!aCoreSample_Fr)
		return phaseTrim;

	if(uiSample < uiCoreSample_Count)
	{
		freqBase.lWhole = freq.iWhole;
		freqRatio.lWhole = aCoreSample_Fr[uiSample].lWhole;
		phaseAdj.ulWhole = freqBase.ulWhole * freqRatio.ulWhole;

		phaseTrim.uiWhole = phaseAdj.uiPair.uiHi;
	}

	return phaseTrim;
}

/* int32_t ct_sample_setup(uint8_t *pSamplePak)
 *  Verify the integrity of the supplied sample package, setting it up as our
 * active sample pack if it checks out. The frequency ratio table is also
 * calculated during this time.
 *----------------------------------------------------------------------------*/
int32_t ct_sample_setup(uint8_t *pSamplePak)
{
	double dFr, dSf, dRf, dBf;
	double dFr_int, dFr_frac;
	int16p16_t iSf, iBf;
	int32p32_t lFr;

	uint32_t *pEntry;
	uint32_t uiX;

	uint32_t uiY = (uint32_t)*pSamplePak;
	if(0 != (uiY % sizeof(uint32_t)))
		return -1;

	if(ct_sample_EnsureAllocated() != 0)
		return -1;

	for(uiX = 0; uiX < CORETONE_SMPPAK_HEAD_MAGICLEN; uiX++)
	{
		if(szCoreSample_Magic[uiX] != pSamplePak[uiX])
			return -1;
	}

	memcpy(&uiY, (pSamplePak + CORETONE_SMPPAK_HEAD_COUNT), sizeof(uint32_t));
	if(uiY > CORETONE_SAMPLES_MAXENTRIES)
		return -1;

	pCoreSample_PackBase = pSamplePak;
	pCoreSample_DirBase = (uint32_t*)(pSamplePak + CORETONE_SMPPAK_DIR_BASE);
	uiCoreSample_Count = uiY;

	memset(aCoreSample_Fr, 0,
		sizeof(int32p32_t) * CORETONE_SAMPLES_MAXENTRIES);

	pEntry = pCoreSample_DirBase;
	dRf = CORETONE_RENDER_RATE;

	for(uiX = 0; uiX < uiY; uiX++)
	{
		iSf.uiWhole = pEntry[CORETONE_SMPPAK_ENTRY_SFREQ];
		iBf.uiWhole = pEntry[CORETONE_SMPPAK_ENTRY_BFREQ];

		dFr = iSf.usPair.usLo;
		dSf = iSf.usPair.usHi;
		dSf = (dFr / 65536.0) + dSf;

		dFr = iBf.usPair.usLo;
		dBf = iBf.usPair.usHi;
		dBf = (dFr / 65536.0) + dBf;

		dFr = (dSf / (dRf * dBf)) * 2.0;

		dFr_int = floor(dFr);
		dFr_frac = dFr - dFr_int;
		dFr_frac *= 4294967296.0;

		lFr.iPair.iHi = (int32_t)dFr_int;
		lFr.uiPair.uiLo = (uint32_t)dFr_frac;

		aCoreSample_Fr[uiX].lWhole = lFr.lWhole;
		pEntry += CORETONE_SMPPAK_ENTRY_SIZE;
	}

	return 0;
}