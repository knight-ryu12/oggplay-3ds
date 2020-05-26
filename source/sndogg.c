#include <3ds.h>

#include "sndogg.h"

#define CODECREG 0x10145000

static const size_t buffSize = 96000;
static bool monoEn = false;
volatile uint32_t nsmp = 0;
volatile int cur_wvbuf = 0;
volatile uint32_t rem_smp = 0;

void setHighFreqMode() {
	const uint16_t original = *(vu16*)0x90145000;
	// Disable DSP
 	*(vu16*)0x90145000 = original & ~(1<<15);
	// Enable 47kHz mode
	*(vu16*)0x90145000 = original | 1<<13;
	// Enable DSP
	*(vu16*)0x90145000 |= 1<<15;
	printf("Reg: %08X\n",*(vu16*)0x90145000);
}

void clearHighFreqMode() {
	const uint16_t original = *(vu16*)0x90145000;
	*(vu16*)0x90145000 = original & ~(1<<15); // Disable DSP
	// Disable 47kHz Mode
	*(vu16*)0x90145000 = original & ~(1<<13);
	*(vu16*)0x90145000 |= 1<<15;
}

Result initSound(u32 sample_rate, bool mono)
{
	Result res;
	res = ndspInit();
	if (R_FAILED(res)) return res;

	ndspChnReset(0);
	ndspChnWaveBufClear(0);
	ndspSetOutputMode(mono?NDSP_OUTPUT_MONO:NDSP_OUTPUT_STEREO);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, sample_rate * 11/16.0F);
	printf("Float Rate: %lf\n",sample_rate * 11 / 16.0F);
	ndspChnSetFormat(0, mono?NDSP_FORMAT_MONO_PCM16:NDSP_FORMAT_STEREO_PCM16);
	monoEn = mono;
	return 0;
}

int initVorbis(const char *path, OggVorbis_File *vorbisFile, vorbis_info **vi)
{
	int ov_res;
	FILE *f = fopen(path, "rb");
	if (f == NULL) return 1;

	ov_res = ov_open(f, vorbisFile, NULL, 0);
	if (ov_res < 0) {
		fclose(f);
		return ov_res;
	}

	*vi = ov_info(vorbisFile, -1);
	if (*vi == NULL) return 3;

	return 0;
}

size_t fillVorbisBuffer(int16_t *buf, size_t samples, OggVorbis_File *vorbisFile)
{
	size_t samplesRead = 0;
	int samplesToRead = samples;

	while(samplesToRead > 0)
	{
		int current_section;
		int samplesJustRead =
			ov_read(vorbisFile, (char*)buf,
					samplesToRead > 4096 ? 4096	: samplesToRead,
					&current_section);

		if(samplesJustRead < 0)
			return samplesJustRead;
		else if(samplesJustRead == 0)
			break;
			/* End of file reached. */

		samplesRead += samplesJustRead;
		samplesToRead -= samplesJustRead;
		buf += samplesJustRead / 2;
	}

	return samplesRead / sizeof(int16_t);
}

void soundThread(void *arg) {
	
	//printf("Hello from sound thread!\n");

	OggVorbis_File *vorbisFile = (OggVorbis_File*)arg;
	ndspWaveBuf waveBuf[2];
	int16_t *samplebuf;
	
	samplebuf = linearAlloc(buffSize * sizeof(int16_t) * 2);
	memset(waveBuf, 0, sizeof(waveBuf));

	waveBuf[0].data_vaddr = samplebuf;
	waveBuf[1].data_vaddr = samplebuf + buffSize;

	waveBuf[0].status = NDSP_WBUF_DONE;
	waveBuf[1].status = NDSP_WBUF_DONE;
	
	svcSleepThread(100*1000); // hack

	while(runSound) {
		while(runSound && !playSound)
			svcSleepThread(10e9 / 60);

		int16_t *cursamplebuf = (int16_t*)waveBuf[cur_wvbuf].data_vaddr;

		nsmp = fillVorbisBuffer(cursamplebuf, buffSize, vorbisFile);
		
		waveBuf[cur_wvbuf].nsamples = monoEn?nsmp:nsmp/2;
		rem_smp = waveBuf[cur_wvbuf].nsamples;

		if (waveBuf[cur_wvbuf].nsamples == 0) break;

		DSP_FlushDataCache(cursamplebuf, buffSize * sizeof(int16_t));
		ndspChnWaveBufAdd(0, &waveBuf[cur_wvbuf]);

		cur_wvbuf ^= 1;
		while(waveBuf[cur_wvbuf].status != NDSP_WBUF_DONE && runSound) {
			rem_smp = waveBuf[cur_wvbuf].nsamples;	
			svcSleepThread(10e9 / (buffSize*2));
		}
	}

	// cleanup
	ndspChnWaveBufClear(0);
	ndspExit();
	linearFree(samplebuf);
	//printf("Bye!\n");
	runSound = 0;
	threadExit(0);
}

