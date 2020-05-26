#pragma once


#include <3ds.h>

#include <string.h>

#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>

extern volatile u32 runSound, playSound;

void setHighFreqMode();
void clearHighFreqMode();

Result initSound(u32 sample_rate,bool mono);

int initVorbis(const char *path, OggVorbis_File *vorbisFile, vorbis_info **vi);
void soundThread(void *arg);
