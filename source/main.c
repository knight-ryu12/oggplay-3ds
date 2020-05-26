#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include "sndogg.h"

volatile u32 runSound, playSound;

extern volatile int cur_wvbuf;
extern volatile uint32_t nsmp;
extern volatile uint32_t rem_smp;

void do_new_speedup(void)
{
	Result res;
	u8 model;

	res = ptmSysmInit();
	if (R_FAILED(res)){ 
		#ifdef DEBUG
			printf("ptmSysmInit(): %08lx\n",res);
		#endif
		return;
	}
	res = cfguInit();
	if (R_FAILED(res)) return;
	CFGU_GetSystemModel(&model);
	if (model == 2 || model >= 4) {
		PTMSYSM_ConfigureNew3DSCPU(3);
		printf("Using 804MHz mode.\n");
	}
	ptmSysmExit();
	cfguExit();
	return;
}

int main(int argc, char* argv[])
{
	Result res;
	int32_t main_prio;
	Thread snd_thr;
	vorbis_info *vi;
	vorbis_comment *vc;
	OggVorbis_File vf;
	
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	do_new_speedup();

	res = romfsInit();
	printf("%08lX\n",res);
	res = initVorbis("romfs:/dragonatlas.ogg",&vf,&vi);
	printf("%08lX\n",res);

	vc = ov_comment(&vf,-1);

	// begin dump vorbis info
	printf("V %d\n",vi->version);
	printf("C %d\n",vi->channels);
	printf("R %ld\n",vi->rate);

	printf("v %s\n",vc->vendor);
	
	for(int i=0; i<vc->comments; i++)
		printf("%d %s\n",i,vc->user_comments[i]);

	svcGetThreadPriority(&main_prio, CUR_THREAD_HANDLE);

	setHighFreqMode();

	initSound(vi->rate, vi->channels==1?true:false);

	runSound = playSound = 1;
	snd_thr = threadCreate(soundThread, &vf, 32768, main_prio + 1, 0, true);

	// Main loop
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		//printf("%ld->%ld:%d\r",nsmp,rem_smp,cur_wvbuf);
		if(runSound==0) break;
		gfxSwapBuffers();
		hidScanInput();

		// Your code goes here
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu
	}

	runSound = playSound = 0;
	threadJoin(snd_thr, U64_MAX);

	fclose((FILE*)(vf.datasource));
	clearHighFreqMode();
	romfsExit();
	gfxExit();
	return 0;
}
