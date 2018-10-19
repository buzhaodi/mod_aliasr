#include <switch.h>
/* Defines module interface to FreeSWITCH */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_aliasr_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_aliasr_load);
SWITCH_MODULE_DEFINITION(mod_aliasr, mod_aliasr_load, mod_aliasr_shutdown, NULL);



typedef struct {
	char *app;
	char *data;
	char *key;
	int up;
	int tone_type;
	int total_hits;
	int hits;
	int sleep;
	int expires;
	int default_sleep;
	int default_expires;
	switch_media_bug_t *bug;
	switch_core_session_t *session;
	int bug_running;

} aliasr_container_t;






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

    switch(type) {
	case SWITCH_ABC_TYPE_INIT: {
		switch_core_session_t *session = switch_core_media_bug_get_session(bug);
		switch_codec_implementation_t read_impl = { 0 };
		switch_core_session_get_read_impl(session, &read_impl);
         switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "asr SWITCH_ABC_TYPE_INIT\n");
		break;
	}
	case SWITCH_ABC_TYPE_READ_REPLACE:
	{
		switch_frame_t *frame;
		if ((frame = switch_core_media_bug_get_read_replace_frame(bug))) {
            char *data = (char *) frame->data;
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "frame is %s \n",data);
      
          
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
    aliasr_container_t *cont = switch_channel_get_private(channel, "_fax_tone_detect_");
 
    
 


	/* are we already running? */
	bug = switch_channel_get_private(channel, "_mod_simpleamd_amd");
	if (bug) {
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "-ERR already running");
		return;
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

	SWITCH_ADD_APP(app, "aliasr_start", "Start VAD", "Start VAD", aliasr_start_app, SIMPLEVAD_START_SYNTAX, SAF_MEDIA_TAP);
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