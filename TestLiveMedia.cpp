/*
 * LiveMediaModule.cpp
 *
 *  Created on: 19 nov. 2014
 *      Author: ebeuque
 */

#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include <liveMedia_version.hh>
#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <H264VideoRTPSource.hh>

// Don't include GroupsockHelper.hh due to the conflict on gettimeofday()
// Declaration from "GroupsockHelper.hh" :
unsigned increaseReceiveBufferTo(UsageEnvironment& env, int socket, unsigned requestedSize);

#define TIMEOUT_CHECKALIVE 10000000

/////////////////////////////////
// Utility function declaration
/////////////////////////////////

char* p_strconcat(const char* str1, ...);
void p_log(const char* format, ...);
int64_t p_timeval_diffms(const timeval& tv1, const timeval& tv2);
void timer_text(const char* szFormat, const struct timeval* tv, char* buf, size_t size);

#define timercpy(dst, src) \
		(dst)->tv_sec = (src)->tv_sec; \
		(dst)->tv_usec = (src)->tv_usec;

//////////////////////////////////
// Custom RTSPClient declaration
//////////////////////////////////

class LiveMediaModuleContext;

class CustomRTSPClient : public RTSPClient
{
public:
	static CustomRTSPClient* createNew(LiveMediaModuleContext* pLiveMediaModuleContext, char const* rtspURL, int iVerbosityLevel);

protected:
	CustomRTSPClient(LiveMediaModuleContext* pLiveMediaModuleContext, char const* rtspURL, int iVerbosityLevel);
	virtual ~CustomRTSPClient();

public:
	// CustomRTSPClient implementation
	static void continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString);
	static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
	static void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
	static void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

	static void handlePingWithOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString);
	static void subsessionAfterPlaying(void* clientData);
	static void subsessionByeHandler(void* clientData);
	static void streamCheckAliveHandler(void* clientData);
	static void streamTimerHandler(void* clientData);

private:
	LiveMediaModuleContext* m_pLiveMediaModuleContext;
};

//////////////////////////////////
// Custom MediaSink declaration
//////////////////////////////////

#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 2000000

class DummySink: public MediaSink
{
public:
	static DummySink* createNew(LiveMediaModuleContext* pLiveMediaModuleContext);

private:
	DummySink(LiveMediaModuleContext* pLiveMediaModuleContext);
	virtual ~DummySink();

	static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
			struct timeval presentationTime, unsigned durationInMicroseconds);
	void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned /*durationInMicroseconds*/);

private:
	Boolean continuePlaying();

private:
	LiveMediaModuleContext* m_pLiveMediaModuleContext;
	u_int8_t* m_pReceiveBuffer;
	MediaSubsession& m_mediaSubSession;
};

/////////////////////////////////////////////
// Custom BasicUsageEnvironment declaration
/////////////////////////////////////////////

class CustomBasicUsageEnvironment : public BasicUsageEnvironment
{
public:
	CustomBasicUsageEnvironment(TaskScheduler& taskScheduler);

	static BasicUsageEnvironment* createNew(TaskScheduler& taskScheduler);

	void printOutput();

	void appendLog(const char *szNewLog);

	UsageEnvironment& operator<<(char const* str);
	UsageEnvironment& operator<<(int i);
	UsageEnvironment& operator<<(unsigned u);
	UsageEnvironment& operator<<(double d);
	UsageEnvironment& operator<<(void* p);

private:
	char* m_szCurMsg;
};

/////////////////////////////////////////////
// LiveMediaModuleContext declaration
/////////////////////////////////////////////

class LiveMediaModuleContext
{
public:
	LiveMediaModuleContext(int iVerbosityLevel);
	virtual ~LiveMediaModuleContext();
	void reset();
	void setWithPingOptions(bool bEnable);
	void cleanSesssion();
	void continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString);
	void handlePingWithOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString);
	void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
	void setupNextSubsession(RTSPClient* rtspClient);
	void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
	void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);
	void subsessionAfterPlaying(RTSPClient* rtspClient, MediaSubsession* subsession);
	void subsessionByeHandler(RTSPClient* rtspClient, MediaSubsession* subsession);
	void streamCheckAliveHandler(CustomRTSPClient* rtspClient);
	void streamTimerHandler(CustomRTSPClient* rtspClient);
	void shutdownStream(RTSPClient* rtspClient);
	void closeStream(RTSPClient* rtspClient);
	int start(const char* szMRL, const char* szUser, const char* szPass, bool bTCP);

public:
	TaskScheduler* m_scheduler;
	UsageEnvironment* m_env;

	char m_eventLoopWatchVariable;

	bool m_bTransportUDP;
	RTSPClient* m_pRtspClient;
	MediaSession* m_pMediaSession;
	MediaSubsessionIterator* m_pMediaSubsessionIterator;
	MediaSubsession* m_pMediaSubsession;
	TaskToken m_streamTimerTask;
	TaskToken m_streamCheckAliveTask;
	double m_duration;

	bool m_bError;

	int m_iVerbosityLevel;
	bool m_bVerbose;

	timeval m_tvLastPacket;

	bool m_bWithPingOptions;
};


/////////////////////////////////
// Utility function definition
/////////////////////////////////

char* p_strconcat(const char* str1, ...)
{
	va_list args;
	size_t len;
	size_t bufSize;

	char* szResult;
	char* szCurrent;

	if(!str1){
		return NULL;
	}

	len = strlen(str1) + 1; // count \0

	// Count len of all
	va_start (args, str1);
	szCurrent = va_arg (args, char*);
	while (szCurrent)
	{
		len += strlen (szCurrent);
		szCurrent = va_arg (args, char*);
	}
	va_end (args);

	bufSize = len*sizeof(char);
	szResult = (char*)malloc(bufSize);

	// Copy the first string
	strcpy(szResult, str1);
	len = strlen(str1);

	va_start (args, str1);
	szCurrent = va_arg (args, char*);
	while (szCurrent)
	{
		strcpy (szResult+len, szCurrent);
		len += strlen(szCurrent);
		szCurrent = va_arg (args, char*);
	}
	va_end (args);

	return szResult;
}

void p_log(const char* format, ...)
{
	timeval tvNow;
	gettimeofday(&tvNow, NULL);
	char szBufTime[50];
	timer_text("%Y-%m-%d %H:%M:%S", &tvNow, szBufTime, 50);
	fprintf(stderr, "[%s] ", szBufTime);

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	fflush(stderr);
	va_end(args);

	fprintf(stderr, "\n");
}

int64_t p_timeval_diffms(const timeval& tv1, const timeval& tv2)
{
	int64_t tv1Ms = (tv1.tv_sec * 1000 + tv1.tv_usec/1000);
	int64_t tv2Ms = (tv2.tv_sec * 1000 + tv2.tv_usec/1000);
	return tv1Ms - tv2Ms;
}

void timer_text(const char* szFormat, const struct timeval* tv, char* buf, size_t size)
{
	char tmbuf[64];
	struct tm time_tm;
	time_t tmpTime = tv->tv_sec;
	localtime_r(&tmpTime, &time_tm);
	strftime(tmbuf, sizeof(tmbuf), szFormat, &time_tm);
	snprintf(buf, size, "%s,%06ld", tmbuf, (long)tv->tv_usec);
}

//////////////////////////////////
// Custom RTSPClient declaration
//////////////////////////////////

CustomRTSPClient* CustomRTSPClient::createNew(LiveMediaModuleContext* pLiveMediaModuleContext, char const* rtspURL, int iVerbosityLevel)
{
	return new CustomRTSPClient(pLiveMediaModuleContext, rtspURL, iVerbosityLevel);
}

CustomRTSPClient::CustomRTSPClient(LiveMediaModuleContext* pLiveMediaModuleContext, char const* rtspURL, int iVerbosityLevel)
#if LIVEMEDIA_LIBRARY_VERSION_INT < 1385424000
	: RTSPClient(*pLiveMediaModuleContext->m_env, rtspURL, iVerbosityLevel, NULL, 0)
#else
	: RTSPClient(*pLiveMediaModuleContext->m_env, rtspURL, iVerbosityLevel, NULL, 0, -1)
#endif
{
	m_pLiveMediaModuleContext = pLiveMediaModuleContext;
}

CustomRTSPClient::~CustomRTSPClient()
{
}

void CustomRTSPClient::continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	((CustomRTSPClient*)rtspClient)->m_pLiveMediaModuleContext->continueAfterOPTIONS(rtspClient, resultCode, resultString);
}

void CustomRTSPClient::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	((CustomRTSPClient*)rtspClient)->m_pLiveMediaModuleContext->continueAfterDESCRIBE(rtspClient, resultCode, resultString);
}

void CustomRTSPClient::continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	((CustomRTSPClient*)rtspClient)->m_pLiveMediaModuleContext->continueAfterSETUP(rtspClient, resultCode, resultString);
}

void CustomRTSPClient::continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	((CustomRTSPClient*)rtspClient)->m_pLiveMediaModuleContext->continueAfterPLAY(rtspClient, resultCode, resultString);
}

void CustomRTSPClient::handlePingWithOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	((CustomRTSPClient*)rtspClient)->m_pLiveMediaModuleContext->handlePingWithOPTIONS(rtspClient, resultCode, resultString);
}

void CustomRTSPClient::subsessionAfterPlaying(void* clientData)
{
	MediaSubsession* subsession = (MediaSubsession*)clientData;
	CustomRTSPClient* rtspClient = (CustomRTSPClient*)(subsession->miscPtr);
	rtspClient->m_pLiveMediaModuleContext->subsessionAfterPlaying(rtspClient, subsession);
}

void CustomRTSPClient::subsessionByeHandler(void* clientData)
{
	MediaSubsession* subsession = (MediaSubsession*)clientData;
	CustomRTSPClient* rtspClient = (CustomRTSPClient*)(subsession->miscPtr);
	rtspClient->m_pLiveMediaModuleContext->subsessionByeHandler(rtspClient, subsession);
}

void CustomRTSPClient::streamCheckAliveHandler(void* clientData)
{
	((CustomRTSPClient*)clientData)->m_pLiveMediaModuleContext->streamCheckAliveHandler((CustomRTSPClient*)clientData);
}

void CustomRTSPClient::streamTimerHandler(void* clientData)
{
	((CustomRTSPClient*)clientData)->m_pLiveMediaModuleContext->streamTimerHandler((CustomRTSPClient*)clientData);
}

//////////////////////////////////
// Custom MediaSink definition
//////////////////////////////////

DummySink* DummySink::createNew(LiveMediaModuleContext* pLiveMediaModuleContext)
{
	return new DummySink(pLiveMediaModuleContext);
}

DummySink::DummySink(LiveMediaModuleContext* pLiveMediaModuleContext)
	: MediaSink(*pLiveMediaModuleContext->m_env), m_mediaSubSession(*pLiveMediaModuleContext->m_pMediaSubsession)
{
	m_pLiveMediaModuleContext = pLiveMediaModuleContext;

	m_pReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
}

DummySink::~DummySink()
{
	if(m_pReceiveBuffer){
		delete[] m_pReceiveBuffer;
		m_pReceiveBuffer = NULL;
	}
}

void DummySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
		struct timeval presentationTime, unsigned durationInMicroseconds)
{
	DummySink* sink = (DummySink*)clientData;
	sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void DummySink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned /*durationInMicroseconds*/)
{
	if(m_pLiveMediaModuleContext->m_iVerbosityLevel >= 3){
		envir() << m_mediaSubSession.mediumName() << "/" << m_mediaSubSession.codecName() << ":\tReceived " << frameSize << " bytes";
		if (numTruncatedBytes > 0) envir() << " (with " << numTruncatedBytes << " bytes truncated)";
		char uSecsStr[6+1];
		sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
		envir() << ".\tPresentation time: " << (int)presentationTime.tv_sec << "." << uSecsStr;
		if (m_mediaSubSession.rtpSource() != NULL && !m_mediaSubSession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
			envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
		}
	#ifdef DEBUG_PRINT_NPT
		envir() << "\tNPT: " << m_mediaSubSession.getNormalPlayTime(presentationTime);
	#else
		envir() << "\n";
	#endif
	}
	bool bRes = true;

	timeval tvNow;
	gettimeofday(&tvNow, NULL);

	if(numTruncatedBytes > 0){
		bRes = false;
	}

	if(bRes){
		bool bRTCPSync = true;
		if (m_mediaSubSession.rtpSource() != NULL && !m_mediaSubSession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
			bRTCPSync = false;
		}
		// Keep last packet time
		timercpy(&m_pLiveMediaModuleContext->m_tvLastPacket, &tvNow);
	}

	// Then continue, to request the next frame of data:
	if(bRes){
		continuePlaying();
	}else{
		p_log("[Access::livemedia] Cannot demux data");
		fSource->close(*m_pLiveMediaModuleContext->m_env, m_mediaSubSession.mediumName());
	}
}

Boolean DummySink::continuePlaying()
{
	//p_log("[Access::livemedia] continuePlaying: %d bytes", fSource->maxFrameSize());
	if (fSource){
		// Request the next frame of data from our input source. "afterGettingFrame()" will get called later, when it arrives:
		fSource->getNextFrame(m_pReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
				afterGettingFrame, this,
				onSourceClosure, this);
		return True;
	}else{
		p_log("[Access::livemedia] Cannot continue playing, fSource is null");
		return False; // sanity check (should not happen)
	}
}

/////////////////////////////////////////////
// Custom BasicUsageEnvironment definition
/////////////////////////////////////////////

CustomBasicUsageEnvironment::CustomBasicUsageEnvironment(TaskScheduler& taskScheduler)
	: BasicUsageEnvironment(taskScheduler)
{
	m_szCurMsg = NULL;
}

BasicUsageEnvironment* CustomBasicUsageEnvironment::createNew(TaskScheduler& taskScheduler)
{
	return new CustomBasicUsageEnvironment(taskScheduler);
}

void CustomBasicUsageEnvironment::printOutput()
{
	if(m_szCurMsg){
		size_t iLen = strlen(m_szCurMsg);
		if(m_szCurMsg[iLen-1] == '\n')
		{
			char delim[] = "\n";
			char *szLine = strtok(m_szCurMsg, delim);

			while(szLine != NULL)
			{
				iLen = strlen(m_szCurMsg);
				if(szLine[iLen-1] == '\r'){
					p_log("[LibLiveMedia] %.*s", iLen-1, szLine);
				}else{
					p_log("[LibLiveMedia] %.*s", iLen, szLine);
				}
				szLine = strtok(NULL, delim);
			}

			free(m_szCurMsg);
			m_szCurMsg = NULL;
		}
	}
}

void CustomBasicUsageEnvironment::appendLog(const char *szNewLog)
{
	if(m_szCurMsg){
		char* szTmp;
		szTmp = p_strconcat(m_szCurMsg, szNewLog, NULL);
		free(m_szCurMsg);
		m_szCurMsg = szTmp;
	}else{
		m_szCurMsg = strdup(szNewLog);
	}
}

UsageEnvironment& CustomBasicUsageEnvironment::operator<<(char const* str)
{
	//BasicUsageEnvironment::operator <<(str);
	if (str == NULL){
		appendLog("(NULL)");
	}else{
		appendLog(str);
	}
	printOutput();
	return *this;
}

UsageEnvironment& CustomBasicUsageEnvironment::operator<<(int i)
{
	char buf[20];
	sprintf(buf, "%d", i);
	appendLog(buf);
	printOutput();
	return *this;
}

UsageEnvironment& CustomBasicUsageEnvironment::operator<<(unsigned u)
{
	char buf[20];
	sprintf(buf, "%u", u);
	appendLog(buf);
	printOutput();
	return *this;
}

UsageEnvironment& CustomBasicUsageEnvironment::operator<<(double d) {
	char buf[50];
	sprintf(buf, "%f", d);
	appendLog(buf);
	printOutput();
	return *this;
}

UsageEnvironment& CustomBasicUsageEnvironment::operator<<(void* p)
{
	char buf[20];
	sprintf(buf, "%p", p);
	appendLog(buf);
	printOutput();
	return *this;
}

////////////////////////////
// Private context instance
////////////////////////////


LiveMediaModuleContext::LiveMediaModuleContext(int iVerbosityLevel)
{
	m_iVerbosityLevel = iVerbosityLevel;
	m_scheduler = BasicTaskScheduler::createNew();

	if(m_iVerbosityLevel > 0) {
		m_env = CustomBasicUsageEnvironment::createNew(*m_scheduler);
		m_bVerbose = true;
	}else{
		m_env = BasicUsageEnvironment::createNew(*m_scheduler);
		m_bVerbose = false;
	}
	m_eventLoopWatchVariable = 0;
	m_bTransportUDP = true;
	m_pRtspClient = NULL;
	m_pMediaSession = NULL;
	m_pMediaSubsessionIterator = NULL;
	m_pMediaSubsession = NULL;
	m_streamTimerTask = NULL;
	m_streamCheckAliveTask = NULL;
	m_duration = 0;
	m_bError = false;
	timerclear(&m_tvLastPacket);
	m_bWithPingOptions = true;
}

LiveMediaModuleContext::~LiveMediaModuleContext()
{
	cleanSesssion();
	m_env->reclaim();
	if(m_scheduler){
		delete m_scheduler;
		m_scheduler = NULL;
	}
}

void LiveMediaModuleContext::reset()
{
	p_log("[Access::livemedia] Reseting");
	m_eventLoopWatchVariable = 0;
	m_bError = false;
	m_duration = 0;
	timerclear(&m_tvLastPacket);
	p_log("[Access::livemedia] Reseting done");
}

void LiveMediaModuleContext::setWithPingOptions(bool bEnable)
{
	m_bWithPingOptions = bEnable;
}

void LiveMediaModuleContext::cleanSesssion()
{
	if(m_pMediaSubsessionIterator){
		delete m_pMediaSubsessionIterator;
		m_pMediaSubsessionIterator = NULL;
	}
	if(m_pMediaSession){
		m_env->taskScheduler().unscheduleDelayedTask(m_streamCheckAliveTask);
		m_env->taskScheduler().unscheduleDelayedTask(m_streamTimerTask);
		Medium::close(m_pMediaSession);
		m_pMediaSession = NULL;
	}
	m_streamCheckAliveTask = NULL;
	m_streamTimerTask = NULL;
	m_duration = 0;
	timerclear(&m_tvLastPacket);
}

void LiveMediaModuleContext::continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	do {
		if (resultCode != 0) {
			m_bError = true;
			p_log("[Access::livemedia] Failed to get a OPTIONS description: %s", resultString);
			delete[] resultString;
			break;
		}

		p_log("[Access::livemedia] Got a OPTIONS description: %s", resultString);
		delete[] resultString;
		
		m_pRtspClient->sendDescribeCommand(CustomRTSPClient::continueAfterDESCRIBE);

		return;
	} while (0);
	// An unrecoverable error occurred with this stream.
	shutdownStream(rtspClient);
}

void LiveMediaModuleContext::handlePingWithOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	do {
		if (resultCode != 0) {
			// Disable state error, since sometimes it fails without beiing an error
			//m_bError = true;
			p_log("[Access::livemedia] Failed to get a OPTIONS description: %s", resultString);
			delete[] resultString;
			break;
		}

		delete[] resultString;

		return;
	} while (0);
	// An unrecoverable error occurred with this stream.
	//shutdownStream(rtspClient);
}

void LiveMediaModuleContext::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	do {
		if (resultCode != 0) {
			m_bError = true;
			p_log("[Access::livemedia] Failed to get a SDP description: %s", resultString);
			delete[] resultString;
			break;
		}

		char* const szSdpDescription = resultString;
		if(m_bVerbose){
			p_log("[Access::livemedia] Got a SDP description : %s", szSdpDescription);
		}else{
			p_log("[Access::livemedia] Got a SDP description");
		}
		// Create a media session object from this SDP description:
		m_pMediaSession = MediaSession::createNew(*m_env, szSdpDescription);
		delete[] szSdpDescription; // because we don't need it anymore
		if (m_pMediaSession == NULL) {
			m_bError = true;
			p_log("[Access::livemedia] Failed to create a MediaSession object from the SDP description: %s", m_env->getResultMsg());
			break;
		} else if (!m_pMediaSession->hasSubsessions()) {
			m_bError = true;
			p_log("[Access::livemedia] This session has no media subsessions");
			break;
		}
		p_log("[Access::livemedia] Session name: %s", m_pMediaSession->sessionName());

		// Display transport mode used
		if(m_bTransportUDP){
			p_log("[Access::livemedia] Using transport UDP");
		}else{
			p_log("[Access::livemedia] Using transport TCP");
		}

		// Then, create and set up our data source objects for the session. We do this by iterating over the session's 'subsessions',
		// calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
		// (Each 'subsession' will have its own data source.)
		m_pMediaSubsessionIterator = new MediaSubsessionIterator(*m_pMediaSession);
		setupNextSubsession(rtspClient);
		return;
	} while (0);
	// An unrecoverable error occurred with this stream.
	shutdownStream(rtspClient);
}

void LiveMediaModuleContext::setupNextSubsession(RTSPClient* rtspClient)
{
	m_pMediaSubsession = m_pMediaSubsessionIterator->next();
	if (m_pMediaSubsession != NULL) {
		p_log("[Access::livemedia] Initiate %s/%s subsession", m_pMediaSubsession->mediumName(), m_pMediaSubsession->codecName());
		if (!m_pMediaSubsession->initiate()) {
			p_log("[Access::livemedia] Failed to initiate the %s/%s subsession: %s",
					m_pMediaSubsession->mediumName(), m_pMediaSubsession->codecName(), m_env->getResultMsg());
			setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
		} else {
			if (true /*m_pMediaSubsession->rtcpIsMuxed()*/) {
				p_log("[Access::livemedia] Initiated the %s/%s subsession (client port %d)",
						m_pMediaSubsession->mediumName(), m_pMediaSubsession->codecName(), m_pMediaSubsession->clientPortNum());
			} else {
				p_log("[Access::livemedia] Initiated the %s/%s subsession (client ports %d-%d)",
						m_pMediaSubsession->mediumName(), m_pMediaSubsession->codecName(), m_pMediaSubsession->clientPortNum(), m_pMediaSubsession->clientPortNum()+1);
			}

			size_t iReceiveBuffer = 0;
			if(strcmp(m_pMediaSubsession->mediumName(), "video") == 0){
				iReceiveBuffer = 2000000;  // For video we use 2MB socket buffer
			}else if(strcmp(m_pMediaSubsession->mediumName(), "audio") == 0){
				iReceiveBuffer = 100000; // For audio we use 100KB socket buffer 
			}

			if(m_pMediaSubsession->rtpSource() != NULL) {
				// For some media we may need to adjust the socket buffer
				int fd = m_pMediaSubsession->rtpSource()->RTPgs()->socketNum();
				if(iReceiveBuffer > 0){
					increaseReceiveBufferTo(*m_env, fd, iReceiveBuffer);
				}

				// Increase the RTP reorder timebuffer just a bit 
				m_pMediaSubsession->rtpSource()->setPacketReorderingThresholdTime(200000);
			}

			// Continue setting up this subsession, by sending a RTSP "SETUP" command:
			Boolean bStreamUsingTCP = (m_bTransportUDP ? False : True);
			rtspClient->sendSetupCommand(*m_pMediaSubsession, CustomRTSPClient::continueAfterSETUP, False, bStreamUsingTCP);
		}
		return;
	}

	// We've finished setting up all of the subsessions. Now, send a RTSP "PLAY" command to start the streaming:
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1385424000
	if (m_pMediaSession->absStartTime() != NULL) {
		// Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
		rtspClient->sendPlayCommand(*m_pMediaSession, CustomRTSPClient::continueAfterPLAY, m_pMediaSession->absStartTime(), m_pMediaSession->absEndTime());
	} else {
#endif
		m_duration = m_pMediaSession->playEndTime() - m_pMediaSession->playStartTime();
		rtspClient->sendPlayCommand(*m_pMediaSession, CustomRTSPClient::continueAfterPLAY);
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1385424000
	}
#endif
}

void LiveMediaModuleContext::continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	do {
		if (resultCode != 0) {
			m_bError = true;
			p_log("[Access::livemedia] Failed to set up the %s/%s subsession: %s",
					m_pMediaSubsession->mediumName(), m_pMediaSubsession->codecName(), resultString);
			break;
		}

		if (true /*m_pMediaSubsession->rtcpIsMuxed()*/) {
			p_log("[Access::livemedia] Set up the %s/%s subsession (client port %d)",
					m_pMediaSubsession->mediumName(), m_pMediaSubsession->codecName(), m_pMediaSubsession->clientPortNum());
		} else {
			p_log("[Access::livemedia] Initiated the %s/%s subsession (client ports %d-%d)",
					m_pMediaSubsession->mediumName(), m_pMediaSubsession->codecName(), m_pMediaSubsession->clientPortNum(), m_pMediaSubsession->clientPortNum()+1);
		}

		// Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
		// (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
		// after we've sent a RTSP "PLAY" command.)
		m_pMediaSubsession->sink = DummySink::createNew(this);
		// perhaps use your own custom "MediaSink" subclass instead
		if (m_pMediaSubsession->sink == NULL) {
			m_bError = true;
			p_log("[Access::livemedia] Failed to create a data sink for the %s/%s subsession: %s",
					m_pMediaSubsession->mediumName(), m_pMediaSubsession->codecName(), m_env->getResultMsg());
			break;
		}

		p_log("[Access::livemedia] Created a data sink for the %s/%s subsession: %s",
				m_pMediaSubsession->mediumName(), m_pMediaSubsession->codecName(), m_env->getResultMsg());
		m_pMediaSubsession->miscPtr = rtspClient; // a hack to let subsession handle functions get the "RTSPClient" from the subsession
		m_pMediaSubsession->sink->startPlaying(*(m_pMediaSubsession->readSource()), CustomRTSPClient::subsessionAfterPlaying, m_pMediaSubsession);
		// Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
		if (m_pMediaSubsession->rtcpInstance() != NULL) {
			m_pMediaSubsession->rtcpInstance()->setByeHandler(CustomRTSPClient::subsessionByeHandler, m_pMediaSubsession);
		}

		m_streamCheckAliveTask = m_env->taskScheduler().scheduleDelayedTask(TIMEOUT_CHECKALIVE, (TaskFunc*)CustomRTSPClient::streamCheckAliveHandler, rtspClient);

	} while (0);
	delete[] resultString;
	// Set up the next subsession, if any:
	setupNextSubsession(rtspClient);
}

void LiveMediaModuleContext::continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	Boolean success = False;
	do {
		if (resultCode != 0) {
			m_bError = true;
			p_log("[Access::livemedia] Failed to start playing session: %s", resultString);
			break;
		}
		// Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
		// using a RTCP "BYE"). This is optional. If, instead, you want to keep the stream active - e.g., so you can later
		// 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
		// (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
		if (m_duration > 0) {
			unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration. (This is optional.)
			m_duration += delaySlop;
			unsigned uSecsToDelay = (unsigned)(m_duration*1000000);
			m_streamTimerTask = m_env->taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)CustomRTSPClient::streamTimerHandler, rtspClient);
		}

		if (m_duration > 0) {
			p_log("[Access::livemedia] Started playing session (for up to %f seconds)", m_duration);
		}else{
			p_log("[Access::livemedia] Started playing session");
		}
		success = True;
	} while (0);
	delete[] resultString;
	if (!success) {
		// An unrecoverable error occurred with this stream.
		shutdownStream(rtspClient);
	}
}

void LiveMediaModuleContext::subsessionAfterPlaying(RTSPClient* rtspClient, MediaSubsession* subsession)
{
	// Begin by closing this subsession's stream:
	Medium::close(subsession->sink);
	subsession->sink = NULL;
	// Next, check whether *all* subsessions' streams have now been closed:
	MediaSession& session = subsession->parentSession();
	MediaSubsessionIterator iter(session);
	while ((subsession = iter.next()) != NULL) {
		if (subsession->sink != NULL) return; // this subsession is still active
	}
	// All subsessions' streams have now been closed, so shutdown the client:
	shutdownStream(rtspClient);
}

void LiveMediaModuleContext::subsessionByeHandler(RTSPClient* rtspClient, MediaSubsession* subsession)
{
	p_log("[Access::livemedia] Received RTCP \"BYE\" on %s/%s subsession",
			subsession->mediumName(), subsession->codecName());
	// Now act as if the subsession had closed:
	subsessionAfterPlaying(rtspClient, subsession);
}

void LiveMediaModuleContext::streamCheckAliveHandler(CustomRTSPClient* rtspClient)
{
	timeval tvNow;
	gettimeofday(&tvNow, NULL);

	int64_t iDiffMs = p_timeval_diffms(tvNow, m_tvLastPacket);
	if(iDiffMs > 30000)
	{
		m_bError = true; // Timeout is an error
		p_log("[Access::livemedia] No data received in the last %d ms", 30000);
		m_streamCheckAliveTask = NULL;
		// Shutdown the stream
		shutdownStream(rtspClient);
	}else{
        // Some stream have a session timeout, so we need to send a command to tell we are alive
		// Axis camera with firmware >= 5.60
		if(m_bWithPingOptions){
			m_pRtspClient->sendOptionsCommand(CustomRTSPClient::handlePingWithOPTIONS);
		}

		m_streamCheckAliveTask = m_env->taskScheduler().scheduleDelayedTask(TIMEOUT_CHECKALIVE, (TaskFunc*)CustomRTSPClient::streamCheckAliveHandler, rtspClient);
	}
}

void LiveMediaModuleContext::streamTimerHandler(CustomRTSPClient* rtspClient)
{
	m_streamTimerTask = NULL;
	// Shutdown the stream
	shutdownStream(rtspClient);
}

void LiveMediaModuleContext::shutdownStream(RTSPClient* rtspClient)
{
	p_log("[Access::livemedia] Stream shutdown");
	// First, check whether any subsessions have still to be closed:
	if (m_pMediaSession != NULL) {
		Boolean someSubsessionsWereActive = False;
		MediaSubsessionIterator iter(*m_pMediaSession);
		MediaSubsession* subsession;
		while ((subsession = iter.next()) != NULL) {
			if (subsession->sink != NULL) {
				Medium::close(subsession->sink);
				subsession->sink = NULL;
				if (subsession->rtcpInstance() != NULL) {
					subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
				}
				someSubsessionsWereActive = True;
			}
		}
		if (someSubsessionsWereActive) {
			// Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
			// Don't bother handling the response to the "TEARDOWN".
			rtspClient->sendTeardownCommand(*m_pMediaSession, NULL);
		}
	}
	m_eventLoopWatchVariable = -1;
}

void LiveMediaModuleContext::closeStream(RTSPClient* rtspClient)
{
	p_log("[Access::livemedia] Closing the stream.");
	cleanSesssion();
	Medium::close(rtspClient);
	m_pRtspClient = NULL;
	p_log("[Access::livemedia] Stream shutdown done");
}

int LiveMediaModuleContext::start(const char* szMRL, const char* szUser, const char* szPass, bool bTCP)
{
	int iResult = 0;

	// Set default transport mode
	m_bTransportUDP = !bTCP;
	p_log("[Access::livemedia] Start with transport TCP: %d", !m_bTransportUDP);

	// For RTSP 1=verbose, 2=more verbose
	int iRTSPVerbosityLevel = 0;
	if(m_iVerbosityLevel >= 2){
		iRTSPVerbosityLevel = 2;
	}else if(m_iVerbosityLevel == 1){
		iRTSPVerbosityLevel = 1;
	}


	m_pRtspClient = CustomRTSPClient::createNew(this, szMRL, iRTSPVerbosityLevel);
	if(m_pRtspClient){
		p_log("[Access::livemedia] RTSP client created");
		p_log("[Access::livemedia] Creating authenticator");

		Authenticator* pAuth = new Authenticator(szUser, szPass);

		p_log("[Access::livemedia] Sending command OPTIONS");
		m_pRtspClient->sendOptionsCommand(CustomRTSPClient::continueAfterOPTIONS, pAuth);

		p_log("[Access::livemedia] Starting event loop: %d", m_eventLoopWatchVariable);
		m_env->taskScheduler().doEventLoop(&m_eventLoopWatchVariable);

		p_log("[Access::livemedia] End of event loop");

		if(pAuth){
			delete pAuth;
			pAuth = NULL;
		}

		shutdownStream(m_pRtspClient);
		closeStream(m_pRtspClient);
	}else{
		m_bError = true;
		p_log("[Access::livemedia] Failed to create a RTSP client for media: %s", m_env->getResultMsg());
	}

	if(m_bError){
		iResult = -1;
	}

	return iResult;
}

int main (int argc, char *argv[])
{
	const char* szRTSPUrl = NULL;
	const char* szUsername = NULL;
	const char* szPassword = NULL;
	bool bTCP = false;
	int iVerbosityLevel = 0;
	bool bWithPing = true;

	for(int i=0; i<argc; i++)
	{
		if(i==0){
			continue;
		}
		if(strcmp(argv[i], "--username") == 0 && i+1<argc){
			szUsername = argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i], "--password") == 0 && i+1<argc){
			szPassword = argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i], "--tcp") == 0){
			bTCP = true;
			continue;
		}
		if(strcmp(argv[i], "-vvv") == 0){
			iVerbosityLevel = 3;
			continue;
		}
		if(strcmp(argv[i], "-vv") == 0){
			iVerbosityLevel = 2;
			continue;
		}
		if(strcmp(argv[i], "-v") == 0){
			iVerbosityLevel = 1;
			continue;
		}
		if(strcmp(argv[i], "--no-ping") == 0){
			bWithPing = false;
			continue;
		}
		if(i == argc-1){
			szRTSPUrl = argv[i];
		}
	}

	p_log("[Access::livemedia] RTSP %s, %s, %s", szRTSPUrl, szUsername, szPassword);

	// Initiate context
	LiveMediaModuleContext* pContext = new LiveMediaModuleContext(iVerbosityLevel);
	pContext->setWithPingOptions(bWithPing);
	pContext->start(szRTSPUrl, szUsername, szPassword, bTCP);
}

