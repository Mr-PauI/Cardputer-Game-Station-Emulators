/******************************************************************************
 * music.c
 * Music script decoding and playback control.
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
#include "channel.h"
#include "music.h"

/******************************************************************************
 * !!!!----   ACTIVE INSTRUMENT PACK and the NOTE FREQUENCY TABLE    ----!!!!
 ******************************************************************************/
extern CoreTrack_t *aCoreTracks;
extern int32_t iMusMood;

const char szCoreInstr_Magic[] = CORETONE_INSPAK_HEAD_MAGICWORD;
const char szCoreMusic_Magic[] = CORETONE_MUSPAK_HEAD_MAGICWORD;

uint8_t *pCoreInstr_PackBase = NULL;
uint32_t *pCoreInstr_DirBase = NULL;
uint32_t uiCoreInstr_Count = 0;

uint8_t *pCoreMusic_PackBase = NULL;
uint8_t *pCoreMusic_DirBase = NULL;

/* The NOTE FREQUENCY TABLE is generated during the setup of an instrument
 * package and contains the 16.16 frequencies (in Hz) assigned to each note byte.
 *
 * It should be noted (horrible pun not intended) that the values contained
 * in this table are equivalent to the usual 128 frequencies used for General
 * MIDI notes with A440 tuning.
 */
int16p16_t *ausNoteFreqs = NULL;

static void* ct_music_Alloc(size_t size)
{
	return malloc(size);
}

void ct_music_shutdown(void)
{
	if(ausNoteFreqs)
	{
		free(ausNoteFreqs);
		ausNoteFreqs = NULL;
	}

	pCoreInstr_PackBase = NULL;
	pCoreInstr_DirBase = NULL;
	uiCoreInstr_Count = 0;

	pCoreMusic_PackBase = NULL;
	pCoreMusic_DirBase = NULL;
}

static int32_t ct_music_EnsureAllocated(void)
{
	if(!ausNoteFreqs)
	{
		ausNoteFreqs = (int16p16_t*)ct_music_Alloc(sizeof(int16p16_t) * 128);
		if(!ausNoteFreqs)
			return -1;

		memset(ausNoteFreqs, 0, sizeof(int16p16_t) * 128);
	}

	return 0;
}

/******************************************************************************
 * !!!!----                  INSTRUMENT MANAGEMENT                   ----!!!!
 ******************************************************************************/
int32_t ct_instr_setup(uint8_t *pInstrPak)
{
	double dFr, dEx;
	double dFr_int, dFr_frac;

	int16p16_t iFr;
	uint32_t uiX;

	uint32_t uiY = (uint32_t)*pInstrPak;
	if(0 != (uiY % sizeof(uint32_t)))
		return -1;

	if(ct_music_EnsureAllocated() != 0)
		return -1;

	for(uiX = 0; uiX < CORETONE_INSPAK_HEAD_MAGICLEN; uiX++)
	{
		if(szCoreInstr_Magic[uiX] != pInstrPak[uiX])
			return -1;
	}

	memcpy(&uiCoreInstr_Count, (pInstrPak + CORETONE_INSPAK_HEAD_COUNT), sizeof(uint32_t));
	pCoreInstr_PackBase = pInstrPak;
	pCoreInstr_DirBase = (uint32_t*)(pInstrPak + CORETONE_INSPAK_DIR_BASE);

	for(uiX = 0; uiX < 128; uiX++)
	{
		dEx = uiX;
		dFr = pow(2.0, ((dEx - 69.0) / 12.0)) * 440.0;

		dFr_int = floor(dFr);
		dFr_frac = dFr - dFr_int;
		dFr_frac *= 65536.0;
		iFr.sPair.sHi = (int16_t)dFr_int;
		iFr.usPair.usLo = (uint16_t)dFr_frac;

		ausNoteFreqs[uiX].iWhole = iFr.iWhole;
	}

	return 0;
}

/******************************************************************************
 * !!!!----                  MUSIC SCRIPT COMMANDS                   ----!!!!
 ******************************************************************************/
void ct_musicCom_setPriority(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	CoreChannel_t *pChannel;

	pTrack->iPriority =  pTrack->pScript[pTrack->uiOffset++];
	if(0 == pTrack->iPriority)
	{
		pTrack->ucNote = CORETONE_MUSIC_NOTE_INVALID;

		if(pPatch->iInstrument)
		{
			pChannel = pTrack->pChannel;
			pChannel->eMode = eCHANNEL_MODE_OFF;

			pPatch->iPriority = 0;
			pPatch->iInstrument = 0;
		}
	}
}

void ct_musicCom_setPanning(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	pTrack->cPanLeft = pTrack->pScript[pTrack->uiOffset++];
	pTrack->cPanRight = pTrack->pScript[pTrack->uiOffset++];
	ct_music_recalcVol(pTrack);
}

void ct_musicCom_setInstrument(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	pTrack->uiInstSel = pTrack->pScript[pTrack->uiOffset++];
}

void ct_musicCom_noteOn(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	CoreChannel_t *pChannel;

	uint8_t ucNote;
	uint32_t uiOffset, uiSample, uiLen;

	if((pPatch->iPriority < pTrack->iPriority) || pPatch->iInstrument)
	{
		ucNote = pTrack->pScript[pTrack->uiOffset++];
		pTrack->ucNote = ucNote;

		pChannel = pTrack->pChannel;
		pPatch->iPriority = pTrack->iPriority;
		pPatch->iInstrument = -1;

		uiOffset = pTrack->uiInstSel * CORETONE_INSPAK_ENTRY_LEN;

		uiSample = pCoreInstr_DirBase[uiOffset + CORETONE_INSPAK_ENTRY_SAMPLE];
		pPatch->pScript = pCoreInstr_PackBase
			+ pCoreInstr_DirBase[uiOffset + CORETONE_INSPAK_ENTRY_SCRIPT];
		pPatch->uiNoteOff =
			pCoreInstr_DirBase[uiOffset + CORETONE_INSPAK_ENTRY_NOTE_OFF];

		ct_sample_get(uiSample, &(pChannel->pSample), &uiLen);
		pPatch->freqBase = ct_sample_calcPhase(uiSample, ausNoteFreqs[ucNote]);
		pChannel->usSampleLen = uiLen;
		ct_patch_keyOn(pPatch);

		pChannel->cVolLeft = pTrack->cVolLeft;
		pChannel->cVolRight = pTrack->cVolRight;
	}
}

void ct_musicCom_noteOff(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	if(pPatch->iInstrument)
	{
		pTrack->ucNote = CORETONE_MUSIC_NOTE_INVALID;
		ct_patch_keyOff(pPatch);
	}
}

void ct_musicCom_pitch(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	int8p8_t fetchFreq;

	fetchFreq.ucPair.ucLo = pTrack->pScript[pTrack->uiOffset++];
	fetchFreq.ucPair.ucHi = pTrack->pScript[pTrack->uiOffset++];
	pPatch->freqPitch.usPair.usLo = fetchFreq.usWhole;
	fetchFreq.ucPair.ucLo = pTrack->pScript[pTrack->uiOffset++];
	fetchFreq.ucPair.ucHi = pTrack->pScript[pTrack->uiOffset++];
	pPatch->freqPitch.usPair.usHi = fetchFreq.usWhole;

	fetchFreq.ucPair.ucLo = pTrack->pScript[pTrack->uiOffset++];
	fetchFreq.ucPair.ucHi = pTrack->pScript[pTrack->uiOffset++];
	pPatch->pitchAdj.usPair.usLo = fetchFreq.usWhole;
	fetchFreq.ucPair.ucLo = pTrack->pScript[pTrack->uiOffset++];
	fetchFreq.ucPair.ucHi = pTrack->pScript[pTrack->uiOffset++];
	pPatch->pitchAdj.usPair.usHi = fetchFreq.usWhole;
}

void ct_musicCom_loopStart(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	int8_t cCount;

	if(pTrack->uiStackPos < CORETONE_MUSIC_STACKDEPTH)
	{
		cCount = pTrack->pScript[pTrack->uiOffset++];
		pTrack->aiLoopStack[pTrack->uiStackPos] = cCount;
		pTrack->auiAddrStack[pTrack->uiStackPos] = pTrack->uiOffset;

		pTrack->uiStackPos++;
	}
}

void ct_musicCom_loopEnd(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	uint32_t uiX;

	if(pTrack->uiStackPos > 0)
	{
		uiX = pTrack->uiStackPos - 1;

		if((pTrack->aiLoopStack[uiX] >= 0) && (pTrack->aiLoopStack[uiX] < 2))
		{
			pTrack->uiStackPos = uiX;
		}
		else if(pTrack->aiLoopStack[uiX] < 0)
		{
			pTrack->uiOffset = pTrack->auiAddrStack[uiX];
		}
		else
		{
			pTrack->uiOffset = pTrack->auiAddrStack[uiX];
			pTrack->aiLoopStack[uiX]--;
		}
	}
}

void ct_musicCom_call(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	int16p16_t fullOff;
	int8p8_t fetchOff;

	if(pTrack->uiStackPos < CORETONE_MUSIC_STACKDEPTH)
	{
		pTrack->aiLoopStack[pTrack->uiStackPos] =
			CORETONE_MUSIC_CALL_TAG;
		pTrack->auiAddrStack[pTrack->uiStackPos] =
			pTrack->uiOffset + sizeof(uint32_t);

		fetchOff.ucPair.ucLo = pTrack->pScript[pTrack->uiOffset++];
		fetchOff.ucPair.ucHi = pTrack->pScript[pTrack->uiOffset++];
		fullOff.usPair.usLo = fetchOff.usWhole;
		fetchOff.ucPair.ucLo = pTrack->pScript[pTrack->uiOffset++];
		fetchOff.ucPair.ucHi = pTrack->pScript[pTrack->uiOffset++];
		fullOff.usPair.usHi = fetchOff.usWhole;

		pTrack->uiOffset += fullOff.iWhole;
		pTrack->uiStackPos++;
	}
}

void ct_musicCom_return(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	uint32_t uiX;

	if(pTrack->uiStackPos > 0)
	{
		uiX = pTrack->uiStackPos - 1;

		if(CORETONE_MUSIC_CALL_TAG == pTrack->aiLoopStack[uiX])
		{
			pTrack->uiOffset = pTrack->auiAddrStack[uiX];
			pTrack->uiStackPos = uiX;
		}
	}
}

void ct_musicCom_break(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	uint32_t uiX, uiY;

	for(uiX = 0; uiX < CORETONE_CHANNELS; uiX++)
	{
		for(uiY = 0; uiY < aCoreTracks[uiX].uiStackPos; uiY++)
		{
			if(CORETONE_MUSIC_CALL_TAG == aCoreTracks[uiX].aiLoopStack[uiY])
			{
				aCoreTracks[uiX].uiOffset = aCoreTracks[uiX].auiAddrStack[uiY];
				aCoreTracks[uiX].uiStackPos = uiY;
				aCoreTracks[uiX].uiDel = 0;
			}
		}
	}
}

void ct_musicCom_nop(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
}

void ct_musicCom_setMood(CoreTrack_t *pTrack, CorePatch_t *pPatch)
{
	int16p16_t fullMood;
	int8p8_t fetchMood;

	fetchMood.ucPair.ucLo = pTrack->pScript[pTrack->uiOffset++];
	fetchMood.ucPair.ucHi = pTrack->pScript[pTrack->uiOffset++];
	fullMood.usPair.usLo = fetchMood.usWhole;
	fetchMood.ucPair.ucLo = pTrack->pScript[pTrack->uiOffset++];
	fetchMood.ucPair.ucHi = pTrack->pScript[pTrack->uiOffset++];
	fullMood.usPair.usHi = fetchMood.usWhole;

	iMusMood = fullMood.iWhole;
}

/******************************************************************************
 * !!!!----                   MUSIC COMMAND TABLE                    ----!!!!
 *----------------------------------------------------------------------------*/
typedef void (*ct_musicCom_t)(CoreTrack_t *pTrack, CorePatch_t *pPatch);
const ct_musicCom_t aMusicComs[] =
{
	ct_musicCom_setPriority, ct_musicCom_setPanning,
	ct_musicCom_setInstrument,
	ct_musicCom_noteOn, ct_musicCom_noteOff,
	ct_musicCom_pitch,

	ct_musicCom_loopStart, ct_musicCom_loopEnd,
	ct_musicCom_call, ct_musicCom_return, ct_musicCom_break,
	ct_musicCom_nop, ct_musicCom_setMood
};

/******************************************************************************
 * !!!!----                  MUSIC SCRIPT DECODING                   ----!!!!
 ******************************************************************************/
void ct_music_recalcVol(CoreTrack_t *pTrack)
{
	CorePatch_t *pPatch = pTrack->pPatch;
	CoreChannel_t *pChannel = pTrack->pChannel;
	int8p8_t scale_L, scale_R;

	scale_L.sWhole = pTrack->cVolMain * pTrack->cPanLeft;
	scale_L.sWhole = scale_L.sWhole << 1;
	pTrack->cVolLeft = scale_L.cPair.cHi;
	scale_R.sWhole = pTrack->cVolMain * pTrack->cPanRight;
	scale_R.sWhole = scale_R.sWhole << 1;
	pTrack->cVolRight = scale_R.cPair.cHi;

	pTrack->iRecalcVol = 0;
	if(pPatch->iInstrument)
	{
		pChannel->cVolLeft = pTrack->cVolLeft;
		pChannel->cVolRight = pTrack->cVolRight;
	}
}

void ct_music_decode(CoreTrack_t *pTrack)
{
	CorePatch_t *pPatch = pTrack->pPatch;
	CoreChannel_t *pChannel = pTrack->pChannel;
	uint8_t *pScript;
	uint8_t ucByte;

	uint32_t uiX, uiY;

	if(pTrack->iRecalcVol)
	{
		ct_music_recalcVol(pTrack);
	}

	pScript = pTrack->pScript;
	while((0 != pTrack->iPriority) && (0 == pTrack->uiDel))
	{
		ucByte = pScript[pTrack->uiOffset];

		if(ucByte & CORETONE_MUSIC_WAIT)
		{
			for((uiX = 0, uiY = 0);
				((ucByte & CORETONE_MUSIC_WAIT) && (uiY < sizeof(uint32_t)));
				(uiX += 7, uiY++))
			{
				pTrack->uiDel |= ((ucByte & CORETONE_MUSIC_WAIT_MASK) << uiX);

				pTrack->uiOffset++;
				ucByte = pScript[pTrack->uiOffset];
			}
		}
		else
		{
			pTrack->uiOffset++;
			if(ucByte < CORETONE_MUSIC_FOOTER)
			{
				(*(aMusicComs[ucByte]))(pTrack, pPatch);
			}
			else
			{
				pTrack->iPriority = 0;
				if(pPatch->iInstrument)
				{
					pChannel->eMode = eCHANNEL_MODE_OFF;

					pPatch->iPriority = 0;
					pPatch->iInstrument = 0;
				}
			}
		}
	}

	if(0 != pTrack->iPriority)
	{
		pTrack->uiDel--;
	}
}

int32_t ct_music_setup(uint8_t *pMusic)
{
	CorePatch_t *pPatch;
	uint8_t *pCurEntry;
	int8_t cPri;

	uint32_t uiX, uiY, uiZ;

	for(uiX = 0; uiX < CORETONE_MUSPAK_HEAD_MAGICLEN; uiX++)
	{
		if(szCoreMusic_Magic[uiX] != pMusic[uiX])
		{
			return -1;
		}
	}

	pCoreMusic_PackBase = pMusic;
	pCoreMusic_DirBase = pMusic + CORETONE_MUSPAK_DIR_BASE;
	memcpy(&uiZ, (pMusic + CORETONE_MUSPAK_HEAD_TRACKS), sizeof(uint32_t));

	pCurEntry = pCoreMusic_DirBase;
	for(uiX = 0; ((uiX < uiZ) && (uiX < CORETONE_CHANNELS)); uiX++)
	{
		memcpy(&cPri, (pCurEntry + CORETONE_MUSPAK_ENTRY_PRIORITY), sizeof(int8_t));
		memcpy(&uiY, (pCurEntry + CORETONE_MUSPAK_ENTRY_OFFSET), sizeof(uint32_t));

		aCoreTracks[uiX].iPriority = cPri;
		aCoreTracks[uiX].iRecalcVol = -1;

		aCoreTracks[uiX].uiInstSel = 0;
		aCoreTracks[uiX].ucNote = CORETONE_MUSIC_NOTE_INVALID;

		aCoreTracks[uiX].pScript = pCoreMusic_PackBase + uiY;
		aCoreTracks[uiX].uiOffset = 0;
		aCoreTracks[uiX].uiDel = 0;

		aCoreTracks[uiX].cPanLeft = CORETONE_DEFAULT_VOLUME;
		aCoreTracks[uiX].cPanRight = CORETONE_DEFAULT_VOLUME;

		aCoreTracks[uiX].uiStackPos = 0;

		pPatch = aCoreTracks[uiX].pPatch;
		pPatch->freqPitch.iWhole = 0;
		pPatch->pitchAdj.iWhole = 0;
		pCurEntry += CORETONE_MUSPAK_ENTRY_LEN;
	}

	return 0;
}