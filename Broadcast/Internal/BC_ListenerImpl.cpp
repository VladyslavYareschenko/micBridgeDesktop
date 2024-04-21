#include "BC_ListenerImpl.h"

#include "BC_BufferedMediaSink.h"

#include <BasicUsageEnvironment.hh>

#include <vector>
#include <sstream>

namespace Broadcast
{

// A function that outputs a string that identifies each stream (for debugging
// output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient)
{
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for
// debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession)
{
  return env << subsession.mediumName() << "/" << subsession.codecName();
}
class StreamClientState {
 public:
  StreamClientState()
      : iter(NULL)
      , session(NULL)
      , subsession(NULL)
      , streamTimerTask(NULL)
      , duration(0.0)
  {
  }

  ~StreamClientState()
  {
    delete iter;
    if (session != NULL)
    {
      // We also need to delete "session", and unschedule "streamTimerTask"
      // (if set)
      UsageEnvironment& env = session->envir(); // alias

      env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
      Medium::close(session);
    }
  }

public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};

Authenticator* getClientAuthentificator()
{
    static Authenticator authCreds = []()
    {
        Authenticator authTor("velvetSweatshop", "wxB2yecE");
        return authTor;
    }();

    return &authCreds;
}

namespace
{
    std::string makeURLFromIP(const std::string& IP, std::uint32_t port)
    {
        std::stringstream urlStream;
        urlStream << "rtsp://" << IP << ":" << port << "/";

        return urlStream.str();
    }
} // namespace

class ListenerImpl::StandaloneRTSPClient : public RTSPClient
{
public:
  static StandaloneRTSPClient* createNew(UsageEnvironment& env,
                                         ListenerImpl& listener,
                                         const std::string& ip,
                                         std::uint16_t port,
                                         int verbosityLevel = 0,
                                         char const* applicationName = NULL,
                                         portNumBits tunnelOverHTTPPortNum = 0)
  {
    return new StandaloneRTSPClient(
        env, listener, ip, port, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
  }

protected:
  StandaloneRTSPClient(UsageEnvironment& env,
                       ListenerImpl& listener,
                       const std::string& ip,
                       std::uint16_t port,
                       int verbosityLevel,
                       char const* applicationName,
                       portNumBits tunnelOverHTTPPortNum)
        : RTSPClient(env, makeURLFromIP(ip, port).c_str(), verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1)
        , destIp(ip)
        , listenerInstance(listener)
  {
  }

  // called only by createNew();
  virtual ~StandaloneRTSPClient() = default;

public:
  StreamClientState scs;

  std::string destIp;
  ListenerImpl& listenerInstance;
};

class DispatchToClientProxy : public ErrorHandler,
                              public SuccessHandler,
                              public AudioFramesHandler
{
public:
  DispatchToClientProxy(DispatchQueuePtr dispatchQueue,
                        ErrorHandlerPtr clientErrorHandler,
                        SuccessHandlerPtr clientSuccessHandler,
                        AudioFramesHandlerPtr clientFramesHandler)
     : dispatchQueue_(std::move(dispatchQueue))
     , clientErrorHandler_(std::move(clientErrorHandler))
     , clientSuccessHandler_(std::move(clientSuccessHandler))
     , clientFramesHandler_(std::move(clientFramesHandler))
   {
   }

  void onFrame(const std::uint8_t* data, std::size_t len) override
  {
//    std::vector<std::uint8_t> bufferedData(len);
//    std::memcpy(bufferedData.data(), data, len);

//    dispatchQueue_->dispatchEvent([handler = clientFramesHandler_, bufferedData = std::move(bufferedData)]
//                                  {
                                    clientFramesHandler_->onFrame(data, len);
//                                  });
  }

  void onErrorOccured(int code, const std::string &errorMsg) override
  {
    dispatchQueue_->dispatchEvent([code, handler = clientErrorHandler_, errorMsg]
                                  {
                                    handler->onErrorOccured(code, errorMsg);
                                  });
  }

  void onConnectSuccess(const std::string& ip) override
  {
      dispatchQueue_->dispatchEvent([handler = clientSuccessHandler_, ip]
                                    {
                                        handler->onConnectSuccess(ip);
                                    });
  }

private:
 DispatchQueuePtr dispatchQueue_;
 ErrorHandlerPtr clientErrorHandler_;
 SuccessHandlerPtr clientSuccessHandler_;
 AudioFramesHandlerPtr clientFramesHandler_;
};

ListenerImpl::ListenerImpl(const std::string& ip,
                           std::uint16_t port,
                           const std::string& authCode,
                           AudioFramesHandlerPtr framesHandler,
                           DispatchQueuePtr dispatchQueue,
                           SuccessHandlerPtr successHandler,
                           ErrorHandlerPtr errorHandler)
    : ip_(ip)
    , port_(port)
{
    getClientAuthentificator()->setUsernameAndPassword("velvetSweatshop", authCode.c_str());

  auto toClientDispatcher = std::make_shared<DispatchToClientProxy>(std::move(dispatchQueue),
                                                                    std::move(errorHandler),
                                                                    std::move(successHandler),
                                                                    std::move(framesHandler));

  framesHandler_ = toClientDispatcher;
  errorHandler_ = toClientDispatcher;
  successHandler_ = toClientDispatcher;
}

void ListenerImpl::finallizeSession()
{
  //const std::lock_guard<std::mutex> lock(finallizeGuard_);

    if (client_)
    {
        shutdownStream(client_);
        client_ = nullptr;
    }
}

ListenerImpl::~ListenerImpl()
{
  finallizeSession();
}

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

void ListenerImpl::openURL(UsageEnvironment* env)
{
  // Begin by creating a "RTSPClient" object.  Note that there is a separate
  // "RTSPClient" object for each stream that we wish to receive (even if
  // more than stream uses the same "rtsp://" URL).
  auto* rtspClient =
      StandaloneRTSPClient::createNew(*env, *this, ip_, port_, RTSP_CLIENT_VERBOSITY_LEVEL, "MicBridge");

  if (rtspClient == NULL)
  {
      reportErrorWithMessage(500, "Failed to create a RTSP client for IP \"", rtspClient->url() , "\": ");
      return;
  }

  client_ = rtspClient;

  // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the
  // stream. Note that this command - like all RTSP commands - is sent
  // asynchronously; we do not block, waiting for a response. Instead, the
  // following function call returns immediately, and we handle the RTSP
  // response later, from within the event loop:
  rtspClient->sendDescribeCommand(continueAfterDESCRIBE, getClientAuthentificator());
}

void ListenerImpl::reportErrorWithMessage(const int code, const std::string& message)
{
  finallizeSession();

  if (client_)
  {
    client_->envir() << message.c_str() << client_->envir().getResultMsg() << "\n";
    errorHandler_->onErrorOccured(code, message +  client_->envir().getResultMsg());
  }
  else
  {
    errorHandler_->onErrorOccured(code, message);
  }
}

void ListenerImpl::reportErrorWithMessage(int code,
                                          const char *msgPart1,
                                          const char *msgPart2,
                                          const char *msgPart3,
                                          const char *msgPart4,
                                          const char *msgPart5)
{
  std::stringstream sstream;

  sstream << msgPart1 << msgPart2;

  if (msgPart3)
    sstream << msgPart3;

  if (msgPart4)
    sstream << msgPart4;

  if (msgPart5)
    sstream << msgPart5;

  reportErrorWithMessage(code, sstream.str());
}

// Implementation of the RTSP 'response handlers':

void ListenerImpl::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
  do {
    auto* client = static_cast<StandaloneRTSPClient*>(rtspClient);

    UsageEnvironment& env = client->envir(); // alias
    StreamClientState& scs = client->scs; // alias
    ListenerImpl& listener = client->listenerInstance; // alias

    if (resultCode != 0)
    {
        listener.reportErrorWithMessage(resultCode, "Failed to get a SDP description: ", resultString);
        delete[] resultString;
        break;
    }

    char* const sdpDescription = resultString;
    env << *rtspClient << "Got a SDP description:\n"
        << sdpDescription << "\n";

    // Create a media session object from this SDP description:
    scs.session = MediaSession::createNew(env, sdpDescription);
    delete[] sdpDescription; // because we don't need it anymore

    if (scs.session == NULL)
    {
      listener.reportErrorWithMessage(resultCode, "Failed to create a MediaSession object from the SDP description: ");
      break;
    }
    else if (!scs.session->hasSubsessions())
    {
     listener.reportErrorWithMessage(resultCode, "This session has no media subsessions (i.e., no \"m=\" lines)");
      break;
    }

    // Then, create and set up our data source objects for the session. We
    // do this by iterating over the session's 'subsessions', calling
    // "MediaSubsession::initiate()", and then sending a RTSP "SETUP"
    // command, on each one. (Each 'subsession' will have its own data
    // source.)
    scs.iter = new MediaSubsessionIterator(*scs.session);
    setupNextSubsession(rtspClient);
    return;
  } while (0);
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP,
// change the following to True:
#define REQUEST_STREAMING_OVER_TCP False

void ListenerImpl::setupNextSubsession(RTSPClient* rtspClient)
{
  auto* client = static_cast<StandaloneRTSPClient*>(rtspClient);

  UsageEnvironment& env = client->envir(); // alias
  StreamClientState& scs = client->scs; // alias
  ListenerImpl& listener = client->listenerInstance; // alias

  scs.subsession = scs.iter->next();
  if (scs.subsession != NULL)
  {
    if (!scs.subsession->initiate())
    {
      listener.reportErrorWithMessage(500,
                                      "Failed to initiate the \"",
                                      scs.subsession->mediumName(),
                                      "/",
                                      scs.subsession->codecName(),
                                      "\" subsession: ");

      setupNextSubsession(rtspClient); // give up on this subsession;
          // go to the next one
    }
    else
    {
      env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
      if (scs.subsession->rtcpIsMuxed())
      {
        env << "client port " << scs.subsession->clientPortNum();
      }
      else
      {
        env << "client ports " << scs.subsession->clientPortNum() << "-"
            << scs.subsession->clientPortNum() + 1;
      }
      env << ")\n";

      // Continue setting up this subsession, by sending a RTSP "SETUP"
      // command:
      rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
    }
    return;
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP
  // "PLAY" command to start the streaming:
  if (scs.session->absStartTime() != NULL)
  {
    // Special case: The stream is indexed by 'absolute' time, so send an
    // appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(),
                                scs.session->absEndTime(), 1.0f, getClientAuthentificator());
  } else
  {
    scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, 0.0f, -1.0f, 1.0f, getClientAuthentificator());
  }
}

void ListenerImpl::continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString)
{
  do {
    auto* client = static_cast<StandaloneRTSPClient*>(rtspClient);

    UsageEnvironment& env = client->envir(); // alias
    StreamClientState& scs = client->scs; // alias
    ListenerImpl& listener = client->listenerInstance; // alias

    if (resultCode != 0)
    {
      listener.reportErrorWithMessage(resultCode,
                                      "Failed to set up the \"",
                                      scs.subsession->mediumName(),
                                      "/",
                                      scs.subsession->codecName(),
                                      "\" subsession: ");
      break;
    }

    env << *rtspClient << "Set up the \"" << *scs.subsession << "\" subsession (";
    if (scs.subsession->rtcpIsMuxed())
    {
      env << "client port " << scs.subsession->clientPortNum();
    }
    else
    {
      env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum() + 1;
    }
    env << ")\n";

    // Having successfully setup the subsession, create a data sink for
    // it, and call "startPlaying()" on it. (This will prepare the data
    // sink to receive data; the actual flow of data from the client won't
    // start happening until later, after we've sent a RTSP "PLAY"
    // command.)

    auto* sink = BufferedMediaSink::createNew(env, *scs.subsession, rtspClient->url());
    if (!sink)
    {
      listener.reportErrorWithMessage(resultCode,
                                      "Failed to create a data sink for the \"",
                                      scs.subsession->mediumName(),
                                      "/",
                                      scs.subsession->codecName(),
                                      "\" subsession: ");
      break;
    }

    sink->setFramesHandler(listener.framesHandler_);
    scs.subsession->sink = sink;

    env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
    scs.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the
        // "RTSPClient" from the subsession
    scs.subsession->sink->startPlaying(*(scs.subsession->readSource()), subsessionAfterPlaying, scs.subsession);
    listener.successHandler_->onConnectSuccess(client->destIp);
    // Also set a handler to be called if a RTCP "BYE" arrives for this
    // subsession:
    if (scs.subsession->rtcpInstance() != NULL)
    {
      scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
      scs.subsession->rtcpInstance()->setRRHandler(subsessionByeHandler, scs.subsession);
      // scs.subsession->rtcpInstance()->setSRHandler(subsessionByeHandler, scs.subsession);
    }
  } while (0);
  delete[] resultString;

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void ListenerImpl::continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString)
{
  Boolean success = False;

  do {
    auto* client = static_cast<StandaloneRTSPClient*>(rtspClient);

    UsageEnvironment& env = client->envir(); // alias
    StreamClientState& scs = client->scs; // alias
    ListenerImpl& listener = client->listenerInstance; // alias

    if (resultCode != 0)
    {
      listener.reportErrorWithMessage(resultCode, "Failed to start playing session: ", resultString);
      break;
    }

    // Set a timer to be handled at the end of the stream's expected
    // duration (if the stream does not already signal its end using a
    // RTCP "BYE").  This is optional.  If, instead, you want to keep the
    // stream active - e.g., so you can later 'seek' back within it and do
    // another RTSP "PLAY" - then you can omit this code. (Alternatively,
    // if you don't want to receive the entire stream, you could set this
    // timer for some shorter value.)
    if (scs.duration > 0) {
      unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's
          // expected duration.  (This is optional.)
      scs.duration += delaySlop;
      unsigned uSecsToDelay = (unsigned)(scs.duration * 1000000);
      scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
    }

    env << *rtspClient << "Started playing session";
    if (scs.duration > 0) {
      env << " (for up to " << scs.duration << " seconds)";
    }
    env << "...\n";

    success = True;
  } while (0);
  delete[] resultString;

  if (!success) {
    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
  }
}

// Implementation of the other event handlers:

void ListenerImpl::subsessionAfterPlaying(void* clientData)
{
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL)
      return; // this subsession is still active
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  shutdownStream(rtspClient);
}

void ListenerImpl::subsessionByeHandler(void* clientData)
{
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
  UsageEnvironment& env = rtspClient->envir(); // alias

  env << *rtspClient << "Received RTCP \"BYE\"";
  //    if (reason != NULL) {
  //        env << " (reason:\"" << reason << "\")";
  //        delete[](char*) reason;
  //    }
  env << " on \"" << *subsession << "\" subsession\n";

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void ListenerImpl::streamTimerHandler(void* clientData)
{
  auto* client = static_cast<StandaloneRTSPClient*>(clientData);
  StreamClientState& scs = client->scs; // alias

  scs.streamTimerTask = NULL;

  // Shut down the stream:
  shutdownStream(client);
}

void ListenerImpl::shutdownStream(RTSPClient* rtspClient, int)
{
  auto* client = static_cast<StandaloneRTSPClient*>(rtspClient);
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = client->scs; // alias

  // First, check whether any subsessions have still to be closed:
  if (scs.session != NULL) {
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*scs.session);
    MediaSubsession* subsession;

    while ((subsession = iter.next()) != NULL) {
      if (subsession->sink != NULL) {
        static_cast<BufferedMediaSink*>(subsession->sink)->setExpired();
        Medium::close(subsession->sink);
        subsession->sink = NULL;

        if (subsession->rtcpInstance() != NULL) {
          subsession->rtcpInstance()->setByeHandler(NULL,
                                                    NULL); // in case the server sends a RTCP "BYE"
          // while handling "TEARDOWN"
        }

        someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive) {
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown
      // the stream. Don't bother handling the response to the
      // "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.session, NULL, getClientAuthentificator());
    }
  }

  env << *rtspClient << "Closing the stream.\n";
  Medium::close(rtspClient);
}

} // namespace Broadcast
