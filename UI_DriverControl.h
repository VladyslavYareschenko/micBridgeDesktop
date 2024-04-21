#pragma once

#include "UI_AudioLevelsIODevice.h"

#include "Broadcast/BC_AudioFramesHandler.h"

#include <Windows.h>

class DriverControlFramesSender : public Broadcast::AudioFramesHandler
{
public:
   DriverControlFramesSender();

   AudioInfo* getAudioInfoIODevice();

   void onFrame(const std::uint8_t *, std::size_t len) override;

private:
   void onDecodedData(const std::uint8_t* buffer, std::size_t len);

private:
   HANDLE driverHandle_;
   AudioInfo audioInfo_;
};

