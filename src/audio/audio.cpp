/*
** audio.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "audio.h"

#include "audiostream.h"
#include "soundemitter.h"
#include "sharedstate.h"
#include "sharedmidistate.h"
#include "eventthread.h"
#include "exception.h"

#include <string>
#include <vector>

#ifndef MKXPZ_RETRO
#  include "sdl-util.h"
#  include <SDL_thread.h>
#  include <SDL_timer.h>
#endif // MKXPZ_RETRO

struct AudioPrivate
{
    
    std::vector<AudioStream*> bgmTracks;
	AudioStream bgs;
	AudioStream me;

#ifndef MKXPZ_RETRO
	SoundEmitter se;

	SyncPoint &syncPoint;
#endif // MKXPZ_RETRO
    
    float volumeRatio;

	/* The 'MeWatch' is responsible for detecting
	 * a playing ME, quickly fading out the BGM and
	 * keeping it paused/stopped while the ME plays,
	 * and unpausing/fading the BGM back in again
	 * afterwards */
	enum MeWatchState
	{
		MeNotPlaying,
		BgmFadingOut,
		MePlaying,
		BgmFadingIn
	};

#ifndef MKXPZ_RETRO
	struct
	{
		SDL_Thread *thread;
		AtomicFlag termReq;
		MeWatchState state;
	} meWatch;
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
	AudioPrivate()
#else
	AudioPrivate(RGSSThreadData &rtData)
#endif // MKXPZ_RETRO
	    : bgs(ALStream::Looped, "bgs"),
	      me(ALStream::NotLooped, "me"),
#ifndef MKXPZ_RETRO
	      se(rtData.config),
	      syncPoint(rtData.syncPoint),
#endif // MKXPZ_RETRO
          volumeRatio(1)
	{
#ifdef MKXPZ_RETRO
        for (int i = 0; i < 16; i++) { // TODO: read BGM track count from config
#else
        for (int i = 0; i < rtData.config.BGM.trackCount; i++) {
#endif // MKXPZ_RETRO
            std::string id = std::string("bgm" + std::to_string(i));
            bgmTracks.push_back(new AudioStream(ALStream::Looped, id.c_str()));
        }
        
#ifndef MKXPZ_RETRO
		meWatch.state = MeNotPlaying;
		meWatch.thread = createSDLThread
			<AudioPrivate, &AudioPrivate::meWatchFun>(this, "audio_mewatch");
#endif // MKXPZ_RETRO
	}

	~AudioPrivate()
	{
#ifndef MKXPZ_RETRO
		meWatch.termReq.set();
		SDL_WaitThread(meWatch.thread, 0);
#endif // MKXPZ_RETRO
        for (auto track : bgmTracks)
            delete track;
	}
    
    AudioStream *getTrackByIndex(int index) {
        if (index < 0) index = 0;
        if (index > (int)(bgmTracks.size()) - 1) {
            throw Exception(Exception::MKXPError, "requested BGM track %d out of range (max: %d)", index, bgmTracks.size() - 1);
        }
        return bgmTracks[index];
    }

#ifndef MKXPZ_RETRO
	void meWatchFun()
	{
		const float fadeOutStep = 1.f / (200  / AUDIO_SLEEP);
		const float fadeInStep  = 1.f / (1000 / AUDIO_SLEEP);

		while (true)
		{
			syncPoint.passSecondarySync();

			if (meWatch.termReq)
				return;

			switch (meWatch.state)
			{
			case MeNotPlaying:
			{
				me.lockStream();

				if (me.stream.queryState() == ALStream::Playing)
				{
					/* ME playing detected. -> FadeOutBGM */
                    for (auto track : bgmTracks)
                        track->extPaused = true;
                    
					meWatch.state = BgmFadingOut;
				}

				me.unlockStream();

				break;
			}

			case BgmFadingOut :
			{
				me.lockStream();

				if (me.stream.queryState() != ALStream::Playing)
				{
					/* ME has ended while fading OUT BGM. -> FadeInBGM */
					me.unlockStream();
					meWatch.state = BgmFadingIn;

					break;
				}
                
                bool shouldBreak = false;
                
                for (int i = 0; i < (int)(bgmTracks.size()); i++) {
                    AudioStream *track = bgmTracks[i];
                    
                    track->lockStream();
                    
                    float vol = track->getVolume(AudioStream::External);
                    vol -= fadeOutStep;
                    
                    if (vol < 0 || track->stream.queryState() != ALStream::Playing) {
                        /* Either BGM has fully faded out, or stopped midway. -> MePlaying */
                        track->setVolume(AudioStream::External, 0);
                        track->stream.pause();
                        track->unlockStream();
                        
                        // check to see if there are any tracks still playing,
                        // and if the last one was ended this round, this branch should exit
                        std::vector<AudioStream*> playingTracks;
                        for (auto t : bgmTracks)
                            if (t->stream.queryState() == ALStream::Playing)
                                playingTracks.push_back(t);
                        
                        
                        if (playingTracks.size() <= 0 && !shouldBreak) shouldBreak = true;
                        continue;
                    }
                    
                    track->setVolume(AudioStream::External, vol);
                    track->unlockStream();
                    
                }
                if (shouldBreak) {
                    meWatch.state = MePlaying;
                    me.unlockStream();
                    break;
                }
                
				me.unlockStream();

				break;
			}

			case MePlaying :
			{
				me.lockStream();

				if (me.stream.queryState() != ALStream::Playing)
                {
                    /* ME has ended */
                    for (auto track : bgmTracks) {
                        track->lockStream();
                        track->extPaused = false;
                        
                        ALStream::State sState = track->stream.queryState();
                        
                        if (sState == ALStream::Paused) {
                            /* BGM is paused. -> FadeInBGM */
                            track->stream.play();
                            meWatch.state = BgmFadingIn;
                        }
                        else {
                            /* BGM is stopped. -> MeNotPlaying */
                            track->setVolume(AudioStream::External, 1.0f);
                            
                            if (!track->noResumeStop)
                                track->stream.play();
                            
                            meWatch.state = MeNotPlaying;
                        }
                        
                        track->unlockStream();
                    }
				}

                me.unlockStream();

				break;
			}

			case BgmFadingIn :
			{
                for (auto track : bgmTracks)
                    track->lockStream();

				if (bgmTracks[0]->stream.queryState() == ALStream::Stopped)
				{
					/* BGM stopped midway fade in. -> MeNotPlaying */
                    for (auto track : bgmTracks)
                        track->setVolume(AudioStream::External, 1.0f);
					meWatch.state = MeNotPlaying;
                    for (auto track : bgmTracks)
                        track->unlockStream();

					break;
				}

				me.lockStream();

				if (me.stream.queryState() == ALStream::Playing)
				{
					/* ME started playing midway BGM fade in. -> FadeOutBGM */
                    for (auto track : bgmTracks)
                        track->extPaused = true;
					meWatch.state = BgmFadingOut;
					me.unlockStream();
                    for (auto track : bgmTracks)
                        track->unlockStream();

					break;
				}

				float vol = bgmTracks[0]->getVolume(AudioStream::External);
				vol += fadeInStep;

				if (vol >= 1)
				{
					/* BGM fully faded in. -> MeNotPlaying */
					vol = 1.0f;
					meWatch.state = MeNotPlaying;
				}

                for (auto track : bgmTracks)
                    track->setVolume(AudioStream::External, vol);

				me.unlockStream();
                for (auto track : bgmTracks)
                    track->unlockStream();

				break;
			}
			}

			SDL_Delay(AUDIO_SLEEP);
		}
	}
#endif // MKXPZ_RETRO
};

#ifdef MKXPZ_RETRO
Audio::Audio()
	: p(new AudioPrivate())
{}
#else
Audio::Audio(RGSSThreadData &rtData)
	: p(new AudioPrivate(rtData))
{}
#endif // MKXPZ_RETRO


void Audio::bgmPlay(const char *filename,
                    int volume,
                    int pitch,
                    double pos,
                    int track)
{
    if (track == -127) {
        for (int i = 0; i < (int)p->bgmTracks.size(); i++) {
            if (i == 0) {
                continue;
            }
            p->bgmTracks[i]->stop();
        }
        
        track = 0;
    }
	p->getTrackByIndex(track)->play(filename, volume, pitch, pos);
}

void Audio::bgmStop(int track)
{
    if (track == -127) {
        for (auto track : p->bgmTracks)
            track->stop();
        
        return;
    }
    
    p->getTrackByIndex(track)->stop();
}

void Audio::bgmFade(int time, int track)
{
    if (track == -127) {
        for (auto track : p->bgmTracks)
            track->fadeOut(time);
        
        return;
    }
    
    p->getTrackByIndex(track)->fadeOut(time);
}

int Audio::bgmGetVolume(int track)
{
    if (track == -127)
        return p->bgmTracks[0]->getVolume(AudioStream::BaseRatio) * 100;
    
    return p->getTrackByIndex(track)->getVolume(AudioStream::Base) * 100;
}

void Audio::bgmSetVolume(int volume, int track)
{
    float vol = volume / 100.0;
    if (track == -127) {
        for (auto track : p->bgmTracks)
            track->setVolume(AudioStream::BaseRatio, vol);
        
        return;
    }
    p->getTrackByIndex(track)->setVolume(AudioStream::Base, vol);
}


void Audio::bgsPlay(const char *filename,
                    int volume,
                    int pitch,
                    double pos)
{
	p->bgs.play(filename, volume, pitch, pos);
}

void Audio::bgsStop()
{
	p->bgs.stop();
}

void Audio::bgsFade(int time)
{
	p->bgs.fadeOut(time);
}


void Audio::mePlay(const char *filename,
                   int volume,
                   int pitch)
{
	p->me.play(filename, volume, pitch);
}

void Audio::meStop()
{
	p->me.stop();
}

void Audio::meFade(int time)
{
	p->me.fadeOut(time);
}


void Audio::sePlay(const char *filename,
                   int volume,
                   int pitch)
{
#ifndef MKXPZ_RETRO
	p->se.play(filename, volume, pitch);
#endif // MKXPZ_RETRO
}

void Audio::seStop()
{
#ifndef MKXPZ_RETRO
	p->se.stop();
#endif // MKXPZ_RETRO
}

void Audio::setupMidi()
{
#ifndef MKXPZ_RETRO
	shState->midiState().initIfNeeded(shState->config());
#endif // MKXPZ_RETRO
}

double Audio::bgmPos(int track)
{
	return p->getTrackByIndex(track)->playingOffset();
}

double Audio::bgsPos()
{
	return p->bgs.playingOffset();
}

void Audio::reset()
{
    for (auto track : p->bgmTracks) {
    	track->stop();
    }

	p->bgs.stop();
	p->me.stop();
#ifndef MKXPZ_RETRO
	p->se.stop();
#endif // MKXPZ_RETRO
}

Audio::~Audio() { delete p; }
