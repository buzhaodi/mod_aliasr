#include <switch.h>
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
	
} aliasr_session_t;


// struct aliasr_session_t {
// 	switch_core_session_t *session;
// 	switch_port_t read_rtp_port;
// 	switch_port_t write_rtp_port;
// 	switch_rtp_t *read_rtp_stream;
// 	switch_rtp_t *write_rtp_stream;
// 	switch_codec_implementation_t read_impl;
// 	switch_codec_implementation_t write_impl;
// 	uint32_t read_cnt;
// 	uint32_t write_cnt;
// 	switch_media_bug_t *read_bug;
// 	switch_event_t *invite_extra_headers;
// 	switch_event_t *bye_extra_headers;
// 	int usecnt;
//     switch_audio_resampler_t *read_resampler;
//     switch_audio_resampler_t *write_resampler;
//     int voicecount;
// 	int testvoicedata;
	
// };








#define SIMPLEVAD_START_SYNTAX "{threshold_adjust_ms=200,max_threshold=1300,threshold=130,voice_ms=60,voice_end_ms=850}"



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
		break;
	}
	case SWITCH_ABC_TYPE_READ_REPLACE:
	{
		switch_frame_t *frame;
		if ((frame = switch_core_media_bug_get_read_replace_frame(bug))) {
            char *data = (char *) frame->data;
		
			
			if (aliasr_t->testvoicedata<strlen(data)){



					// char *json;
	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		// switch_event_serialize_json(event, &json);
		// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s\n" ,json);

		channle_uuid= switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"Channel-Call-UUID"));
		create_time = switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"Caller-Profile-Created-Time"));
		answer_time = switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"Caller-Channel-Answered-Time"));
		tocall = switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"Caller-Destination-Number"));
		caller = switch_core_strdup(switch_core_session_get_pool(session), switch_event_get_header_nil(event,"variable_sip_to_user"));
		switch_event_destroy(&event);
	}










				voicecount = switch_channel_get_variable(channel, "voicecount");				
              	if (voicecount) {
					  intvoicecount=atoi(voicecount)+1;
					//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "voicecount is %s \n",intvoicecount);
		                switch_channel_set_variable(channel, "voicecount",switch_mprintf("%d", intvoicecount));       
						if(intvoicecount>10){
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channels answer_time is  %s\n" ,answer_time);
		                    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channels create_time is  %s\n" ,create_time);
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "now is %ld" ,switch_micro_time_now());
								
								strcpy(cfilename,caller);
								strcat(cfilename,".txt");
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, " caller  is %s  \n",caller);
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, " c is %s  \n",cfilename);
								fp = fopen(cfilename, "a+");
								sprintf(endline,"caller:%s,tocall:%s,create time : %s,answer_time : %s,testvoicetime : %ld \n",caller,tocall,create_time,answer_time,switch_micro_time_now());
								fputs(endline, fp);
								fclose(fp);


							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, " test end  \n");							    
    							SWITCH_STANDARD_STREAM(stream);
							// channle_uuid = switch_channel_get_variable(channel, "Core-UUID");
							switch_api_execute("uuid_kill", channle_uuid, NULL, &stream);
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, " kill uuid %s  \n",channle_uuid);
							// switch_core_session_hangup_state(aliasr_t->session,SWITCH_FALSE);
						}
              	}
				 else{
					 switch_channel_set_variable(channel, "voicecount", "0" );
				 } 
				 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "voicecount is %s \n",voicecount);
              
			}
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










/**
 * Called when FreeSWITCH loads the module
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_aliasr_load)
{
	switch_application_interface_t *app;


	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app, "aliasr_start", "test 10086voice app", "Start VAD", aliasr_start_app, SIMPLEVAD_START_SYNTAX, SAF_MEDIA_TAP);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod aliasr aload!\n");



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