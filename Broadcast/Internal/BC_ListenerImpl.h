#pragma once

#include "../BC_AudioFramesHandler.h"
#include "../BC_DispatchQueue.h"
#include "../BC_ErrorHandler.h"
#include "../BC_SuccessHandler.h"

#include <liveMedia.hh>

#include <iostream>
#include <thread>
#include <mutex>

namespace Broadcast
{

class ListenerImpl
{
public:
  ListenerImpl(const std::string& ip,
               std::uint16_t port,
               const std::string& authCode,
               AudioFramesHandlerPtr handler,
               DispatchQueuePtr dispatchQueue,
               SuccessHandlerPtr successHandler,
               ErrorHandlerPtr errorHandler);

  ~ListenerImpl();

  // The main streaming routine (for each "rtsp://" URL):
  void openURL(UsageEnvironment* env);
  void finallizeSession();

  bool isExpired() const { return expired_; }
  void setExpired(bool expired) { expired_ = expired; }

private:
 void reportErrorWithMessage(int code, const std::string& message);
 void reportErrorWithMessage(int code,
                             const char* msgPart1,
                             const char* msgPart2,
                             const char* msgPart3 = nullptr,
                             const char* msgPart4 = nullptr,
                             const char* msgPart5 = nullptr);

 // RTSP 'response handlers':
 static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
 static void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
 static void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

 // Other event handler functions:

 // called when a stream's subsession (e.g., audio or video substream) ends
 static void subsessionAfterPlaying(void* clientData);

 // called when a RTCP "BYE" is received for a subsession
 static void subsessionByeHandler(void* clientData);

 // called at the end of a stream's expected duration (if the stream has not
 // already signaled its end using a RTCP "BYE")
 static void streamTimerHandler(void* clientData);

 // Used to iterate through each stream's 'subsessions', setting up each one:
 static void setupNextSubsession(RTSPClient* rtspClient);

 // Used to shut down and close a stream (including its "RTSPClient" object):
 static void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

private:
    bool expired_ = false;
    std::string ip_;
    std::uint16_t port_ = 0;
    AudioFramesHandlerPtr framesHandler_;
    ErrorHandlerPtr errorHandler_;
    SuccessHandlerPtr successHandler_;

private:
    class StandaloneRTSPClient;
    StandaloneRTSPClient* client_ = nullptr;
};

} // namespace Broadcast
