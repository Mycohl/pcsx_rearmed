#include <stdint.h>
#include <unistd.h>

#include "jniapi.h"
#include "logger.h"
#include "settings.h"
#include "../../../frontend/ouya.h"

static bool _settings_updated;
static char _bios_dir[256], _save_dir[256];
static const char *pBD, *pSD;

extern void _set_pcsx_message(const char *text, unsigned frames);

void Settings_Update()
{
	// TODO: handle non-PCSX-core settings changes
	_settings_updated = true;
	return;
}

void _clear_settings_updated()
{
	_settings_updated = false;
}

bool _check_pcsx_variables_updated()
{
	if (_settings_updated)
		return !(_settings_updated = false);
	return false;
}

void _get_pcsx_variable(struct ouya_variable *var)
{
	char key[128] = "pref_";
	strncat(key, var->key, 122);
	static char val[64];
	var->value = val;
	JNI_Get_Pref((const char*)key, var->value, sizeof(val));
}

void _get_pcsx_bios_directory(const char** dir)
{	
	JNI_Get_Pref("pref_pcsx_rearmed_bios_dir", _bios_dir, sizeof(_bios_dir));	
	pBD = (const char*)&_bios_dir;
	*dir = pBD;
}

void _get_pcsx_save_directory(const char** dir)
{	
	JNI_Get_Pref("pref_pcsx_rearmed_save_dir", _save_dir, sizeof(_save_dir));	
	pSD = (const char*)&_save_dir;
	*dir = pSD;
}

bool _settings_callback(unsigned cmd, void *data)
{
	bool status = false;

	switch (cmd)
	{
		case SETTINGS_SET_VARIABLES:
			// we already know the variables
			status = true;
			break;
		case SETTINGS_GET_VARIABLE:
			// cast to ouya_variable
			_get_pcsx_variable((struct ouya_variable*)data);
			status = true;
			break;
		case SETTINGS_GET_VARIABLE_UPDATE:
			// cast to bool
			*((bool*)data) = _check_pcsx_variables_updated();
			status = true;
			break;
		case SETTINGS_GET_BIOS_DIRECTORY:
			_get_pcsx_bios_directory((const char**)data);
			status = true;
			break;			
		case SETTINGS_GET_SAVE_DIRECTORY:
			_get_pcsx_save_directory((const char**)data);
			status = true;
			break;			
		//~ case SETTINGS_SET_DISK_CONTROL_INTERFACE:
			//~ break;
		case SETTINGS_SET_MESSAGE:
			_set_pcsx_message(((struct ouya_message*)data)->msg, ((struct ouya_message*)data)->frames);
			status = true;
			break;
		default:
			break;
	}
	
	usleep(10);
	
	return status;
}


