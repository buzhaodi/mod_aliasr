#include <switch.h>
#if defined(_WIN32)
#include <windows.h>
#include "pthread.h"
#else
#include <unistd.h>
#include <pthread.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctime>
#include <map>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include "include/nlsClient.h"
#include "include/nlsEvent.h"
#include "include/speechTranscriberSyncRequest.h"
#include "include/speechRecognizerSyncRequest.h"

#include "include/nlsCommonSdk/Token.h"


using std::map;
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;





using namespace AlibabaNlsCommon;

using AlibabaNls::NlsClient;
using AlibabaNls::NlsEvent;
using AlibabaNls::LogDebug;
using AlibabaNls::LogInfo;
using AlibabaNls::AudioDataStatus;
using AlibabaNls::AUDIO_FIRST;
using AlibabaNls::AUDIO_MIDDLE;
using AlibabaNls::AUDIO_LAST;
using AlibabaNls::SpeechRecognizerSyncRequest;






#define SIMPLEVAD_START_SYNTAX "{threshold_adjust_ms=200,max_threshold=1300,threshold=130,voice_ms=60,voice_end_ms=850}"
#define FRAME_SIZE 3200
#define SAMPLE_RATE 8000
#define ALIASR_EVENT_RESULT "mod_aliasr::asrresult"







/* Defines module interface to FreeSWITCH */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_aliasr_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_aliasr_load);
SWITCH_MODULE_DEFINITION(mod_aliasr, mod_aliasr_load, mod_aliasr_shutdown, NULL);



typedef struct{
	int fristcount;

} recordvoice;


typedef struct aliasr_session_s {
	switch_core_session_t *session;
	switch_port_t read_rtp_port;
	switch_port_t write_rtp_port;
	switch_rtp_t *read_rtp_stream;
	switch_rtp_t *write_rtp_stream;
	switch_codec_implementation_t read_impl;
	switch_codec_implementation_t write_impl;
	uint32_t read_cnt;
	uint32_t write_cnt;
	switch_media_bug_t *read_bug;
	switch_event_t *invite_extra_headers;
	switch_event_t *bye_extra_headers;
	int usecnt;
    switch_audio_resampler_t *read_resampler;
    switch_audio_resampler_t *write_resampler;
    int voicecount;
	int testvoicedata;
	SpeechRecognizerSyncRequest* request;
	
} aliasr_session_t;



// 自定义线程参数
struct ParamStruct {
    string fileName;
    string token;
    string appkey;
	SpeechRecognizerSyncRequest* request;
};



char * appkey;
char * g_akId;
char * g_akSecret;
string  g_token = "";
long g_expireTime = -1;





/**
    * @brief 获取sendAudio发送延时时间
    * @param dataSize 待发送数据大小
    * @param sampleRate 采样率 16k/8K
    * @param compressRate 数据压缩率，例如压缩比为10:1的16k opus编码，此时为10；非压缩数据则为1
    * @return 返回sendAudio之后需要sleep的时间
    * @note 对于8k pcm 编码数据, 16位采样，建议每发送1600字节 sleep 100 ms.
            对于16k pcm 编码数据, 16位采样，建议每发送3200字节 sleep 100 ms.
            对于其它编码格式的数据, 用户根据压缩比, 自行估算, 比如压缩比为10:1的 16k opus,
            需要每发送3200/10=320 sleep 100ms.
*/
unsigned int getSendAudioSleepTime(const int dataSize,
                                   const int sampleRate,
                                   const int compressRate) {
    // 仅支持16位采样
    const int sampleBytes = 16;
    // 仅支持单通道
    const int soundChannel = 1;

    // 当前采样率，采样位数下每秒采样数据的大小
    int bytes = (sampleRate * sampleBytes * soundChannel) / 8;

    // 当前采样率，采样位数下每毫秒采样数据的大小
    int bytesMs = bytes / 1000;

    // 待发送数据大小除以每毫秒采样数据大小，以获取sleep时间
    int sleepMs = (dataSize * compressRate) / bytesMs;

    return sleepMs;
}




/**
 * 根据AccessKey ID和AccessKey Secret重新生成一个token，并获取其有效期时间戳
 */
int generateToken(string akId, string akSecret, string* token, long* expireTime) {
    NlsToken nlsTokenRequest;
    nlsTokenRequest.setAccessKeyId(akId);
    nlsTokenRequest.setKeySecret(akSecret);

    if (-1 == nlsTokenRequest.applyNlsToken()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Failed:%s.\n", nlsTokenRequest.getErrorMsg());

        return -1;
    }

    *token = nlsTokenRequest.getToken();
    *expireTime = nlsTokenRequest.getExpireTime();

    return 0;
}






/**
 * Process a buffer of audio data for AMD events
 *
 * @param bug the session's media bug
 * @param user_data the detector
 * @param type the type of data available from the bug
 * @return SWITCH_TRUE
 */
static switch_bool_t amd_process_buffer(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	aliasr_session_t *aliasr_t = (aliasr_session_t *)user_data;
	switch_core_session_t *session = aliasr_t->session;
	char *create_time = NULL;
	char *answer_time = NULL;
	char *caller = NULL;
	char *tocall = NULL;
	const char *voicecount=NULL;
	const char * channle_uuid=NULL;
	int intvoicecount;
	switch_stream_handle_t stream = { 0 };
	FILE *fp = NULL; 
	char  endline[100];
	char cfilename[200];
	const char *var;
	SpeechRecognizerSyncRequest* request = aliasr_t->request;
	

	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_event_t *event;		

        // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channels answer_time is  %s\n" ,answer_time);
		// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channels create_time is  %s\n" ,create_time);





    switch(type) {
	case SWITCH_ABC_TYPE_INIT: {
		switch_core_session_t *session = switch_core_media_bug_get_session(bug);
		switch_codec_implementation_t read_impl = {(switch_codec_type_t)0};
		switch_core_session_get_read_impl(session, &read_impl);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "asr SWITCH_ABC_TYPE_INIT\n");
		switch_channel_set_variable(channel, "initany", "ture");
		break;
	}
	case SWITCH_ABC_TYPE_READ_REPLACE:
	{
		switch_frame_t *frame;
		if ((frame = switch_core_media_bug_get_read_replace_frame(bug))) {
            char *data = (char *) frame->data;



		AudioDataStatus status;
		var=switch_channel_get_variable(channel, "initany");

		if (strcasecmp(var,"ture")==0) {
		
			status = AUDIO_FIRST;  // 发送第一块音频数据
			switch_channel_set_variable(channel, "initany", "false");
		}
		// else if (!strcasecmp(var,"false")) {
		// 	status = AUDIO_MIDDLE; // 发送中间音频数据
		// }
		// else {
		// 	status = AUDIO_LAST; // 发送最后一块音频数据
		// }
			else {
			status = AUDIO_MIDDLE; // 发送最后一块音频数据
		}
			
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "initany is %d\n",status);		
		request->sendSyncAudio(data, 320, status);





		/*
		* 4: 获取识别结果
		* 接收到EventType为TaskFailed, closed, completed事件类型时，停止发送数据
		* 部分错误可收到多次TaskFailed事件，只要发生TaskFailed事件，请停止发送数据
		*/
		bool isFinished = false;
		std::queue<NlsEvent> eventQueue;
		request->getRecognizerResult(&eventQueue);
		while (!eventQueue.empty()) {
			NlsEvent _event = eventQueue.front();
			eventQueue.pop();

			NlsEvent::EventType type = _event.getMsgType();
			switch (type)
			{
			case NlsEvent::RecognitionStarted:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "************* Recognizer started *************\n");
			
				break;
			case NlsEvent::RecognitionResultChanged:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "************* Recognizer has middle result *************\n");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "result:%s\n", _event.getResult());

					if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, ALIASR_EVENT_RESULT) == SWITCH_STATUS_SUCCESS) {
						switch_channel_event_set_basic_data(channel, event);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "aliasr_result", "%s", _event.getResult());
						switch_event_fire(&event);
					}


			
				break;
			case NlsEvent::RecognitionCompleted:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "************* Recognizer completed *************\n");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "result:%s\n", _event.getResult());				
				isFinished = true;
				break;
			case NlsEvent::TaskFailed:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "************* TaskFailed *************\n");


				    SWITCH_STANDARD_STREAM(stream);
							// channle_uuid = switch_channel_get_variable(channel, "Core-UUID");
							switch_api_execute("uuid_kill", switch_core_session_get_uuid(session), NULL, &stream);
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, " kill uuid %s  \n",switch_core_session_get_uuid(session));
					

		
				isFinished = true;
				break;
			case NlsEvent::Close:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "************* Closed *************\n");
			
				isFinished = true;
				break;
			default:
				break;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "allMessage:%s\n", _event.getAllResponse());	
	
		}


















	// 				// char *json;
	// if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
	// 	switch_channel_event_set_data(channel, event);
	// 	// switch_event_serialize_json(event, &json);
	// 	// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s\n" ,json);

	// 	channle_uuid= switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"Channel-Call-UUID"));
	// 	create_time = switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"Caller-Profile-Created-Time"));
	// 	answer_time = switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"Caller-Channel-Answered-Time"));
	// 	tocall = switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"Caller-Destination-Number"));
	// 	caller = switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"variable_sip_to_user"));
	// 	switch_event_destroy(&event);
	// }




				//  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "voicecount is %s \n",voicecount);
              
			
			// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, " data lentg is %s  \n",strlen(data));
			// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, " data lentg is %zu  \n",strlen(data));
      
          
		}
		break;
	}
	case SWITCH_ABC_TYPE_CLOSE:
		  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "asr end \n");
		break;
	default:
		break;
	}

	return SWITCH_TRUE;
}


   
    // if(cont->default_expires){
    //     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod aliasr aload! %d \n",cont->default_expires);
    // }else{
    //       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "no data\n");
    // }
   


















/**
 * APP interface to start AMD
 */
SWITCH_STANDARD_APP(aliasr_start_app)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug = NULL;
    aliasr_session_t *cont = NULL;




	

    cont =(aliasr_session_t *)switch_core_session_alloc(session, sizeof(*cont));
	switch_assert(cont);
	memset(cont, 0, sizeof(*cont));
	
	// const char *create_time = NULL;
	// const char *answer_time = NULL;
	// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channels create_time is  %s\n" ,(char *)channel);
    // create_time=switch_channel_get_variable(channel, "Core-UUID");
	// answer_time=switch_channel_get_variable(channel, "Answered-Time");
 


	// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channels create_time is  %s\n" ,create_time);
	// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channels answer_time is  %s\n" ,answer_time);

	/* are we already running? */
	bug = (switch_media_bug_t*)switch_channel_get_private(channel, "_mod_simpleamd_amd");
	if (bug) {
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "-ERR already running");
		return;
	}
	
    cont->session = session;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "no testvoicedata found set defult value 200.\n");
		cont->testvoicedata=200;
	}else{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "testvoicedata is  %s\n" ,data);
		cont->testvoicedata=atoi(data);
	}




    std::time_t curTime = std::time(0);
    if (g_expireTime - curTime < 10) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "the token will be expired, please generate new token by AccessKey-ID and AccessKey-Secret.\n");
         generateToken(g_akId, g_akSecret, &g_token, &g_expireTime);
    }


   



	SpeechRecognizerSyncRequest* request = NlsClient::getInstance()->createRecognizerSyncRequest();
    if (request == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "createRecognizerSyncRequest failed.\n");	
		return ;
    }

	request->setAppKey(appkey); // 设置AppKey, 必填参数, 请参照官网申请
	request->setFormat("pcm"); // 设置音频数据编码格式, 可选参数, 目前支持pcm, opu, opus, speex. 默认是pcm
	request->setSampleRate(SAMPLE_RATE); // 设置音频数据采样率, 可选参数, 目前支持16000, 8000. 默认是16000
	request->setIntermediateResult(true); // 设置是否返回中间识别结果, 可选参数. 默认false
	request->setPunctuationPrediction(true); // 设置是否在后处理中添加标点, 可选参数. 默认false
	request->setInverseTextNormalization(true); // 设置是否在后处理中执行ITN, 可选参数. 默认false
    request->setToken(g_token.c_str()); // 设置账号校验token, 必填参数
	cont->request=request;

 	// ParamStruct pa;
    // pa.token = g_token;
    // pa.appkey = appkey;
	// pa.request = request;


	// pthreadFunc((void *)&pa);




















	/* Add media bug */
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Starting aliasr\n");
	switch_core_media_bug_add(session, "_mod_simpleamd_amd", NULL, amd_process_buffer, cont, 0, SMBF_NO_PAUSE | SMBF_READ_REPLACE, &bug);
	if (!bug) {

		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "-ERR can't create media bug");
		return;
	}
	switch_channel_set_private(channel, "_mod_simpleamd_amd", bug);

	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "+OK started");
}






static switch_status_t load_config(void)
{
	const char *cf = "aliasr.conf";
	switch_xml_t cfg, xml = NULL, param, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "appkey")) {
				if (!zstr(val)) appkey = strdup(val);
			} else if (!strcasecmp(var, "g_akId")) {
				if (!zstr(val)) g_akId = strdup(val);
			} else if (!strcasecmp(var, "g_akSecret")) {
				if (!zstr(val)) g_akSecret = strdup(val);
			} 
		}
	}

  done:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

















/**
 * Called when FreeSWITCH loads the module
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_aliasr_load)
{
	switch_application_interface_t *app;


	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app, "aliasr_start", "test 10086voice app", "Start VAD", aliasr_start_app, SIMPLEVAD_START_SYNTAX, SAF_MEDIA_TAP);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod aliasr aload!\n");


	load_config();



  int ret = NlsClient::getInstance()->setLogConfig("log-recognizer.txt", LogInfo);
    if (-1 == ret) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "set log failed.\n");
          return (switch_status_t)-1;
    }

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Called when FreeSWITCH stops the module
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_aliasr_shutdown)
{
	return SWITCH_STATUS_SUCCESS;
}