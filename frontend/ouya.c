/*
 * (C) notaz, 2012,2014,2015
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#define _GNU_SOURCE 1 // strcasestr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sched.h>

#include "../libpcsxcore/misc.h"
#include "../libpcsxcore/psxcounters.h"
#include "../libpcsxcore/psxmem_map.h"
#include "../libpcsxcore/new_dynarec/new_dynarec.h"
#include "../libpcsxcore/cdrom.h"
#include "../libpcsxcore/cdriso.h"
#include "../libpcsxcore/cheat.h"
#include "../plugins/dfsound/out.h"
#include "../plugins/dfsound/spu_config.h"
#include "../plugins/dfinput/externals.h"
#include "cspace.h"
#include "main.h"
#include "plugin.h"
#include "plugin_lib.h"
#include "libpicofe/arm/neon_scale2x.h"
#include "libpicofe/arm/neon_eagle2x.h"
//#include "revision.h"
#include "ouya.h"

static ouya_render_t video_cb;
static ouya_input_poll_t input_poll_cb;
static ouya_input_state_t input_state_cb;
static ouya_audio_batch_t audio_batch_cb;
static ouya_settings_t settings_cb;

static void *rend_ptr;
//~ static void *aud_ptr;

static void *vout_buf;
static int vout_width, vout_height;
static int vout_scale_w, vout_scale_h, vout_yoffset;
static int vout_doffs_old, vout_fb_dirty;
static bool vout_can_dupe;
static bool duping_enable;
static int soft_filter, scanlines, scanline_level;


static int plugins_opened;
static int is_pal_mode;

/* memory card data */
extern char Mcd1Data[MCD_SIZE];
extern char Mcd2Data[MCD_SIZE];
extern char McdDisable[2];

/* PCSX ReARMed core calls and stuff */
int in_type1, in_type2;
int in_a1[2] = { 127, 127 }, in_a2[2] = { 127, 127 }, in_a3[2] = { 127, 127 }, in_a4[2] = { 127, 127 };
int in_keystate;
int in_enable_vibration = 0;

#define MAXPATHLEN 256

static char rom_fname[MAXPATHLEN];

static void init_memcard(char *mcd_data)
{
	unsigned off = 0;
	unsigned i;

	memset(mcd_data, 0, MCD_SIZE);

	mcd_data[off++] = 'M';
	mcd_data[off++] = 'C';
	off += 0x7d;
	mcd_data[off++] = 0x0e;

	for (i = 0; i < 15; i++) {
		mcd_data[off++] = 0xa0;
		off += 0x07;
		mcd_data[off++] = 0xff;
		mcd_data[off++] = 0xff;
		off += 0x75;
		mcd_data[off++] = 0xa0;
	}

	for (i = 0; i < 20; i++) {
		mcd_data[off++] = 0xff;
		mcd_data[off++] = 0xff;
		mcd_data[off++] = 0xff;
		mcd_data[off++] = 0xff;
		off += 0x04;
		mcd_data[off++] = 0xff;
		mcd_data[off++] = 0xff;
		off += 0x76;
	}
}

static int vout_open(void)
{
	return 0;
}

static inline int resolution_ok(int w, int h)
{
	return w <= 1024 && h <= 512;
}

static void vout_set_mode(int w, int h, int raw_w, int raw_h, int bpp)
{
	vout_yoffset = 0;
	
	vout_width = w;
	vout_height = h;

	vout_scale_w = vout_scale_h = 1;
#ifdef __ARM_NEON__
	if (soft_filter) {
		if (resolution_ok(w * 2, h * 2) && bpp == 16) {
			vout_scale_w = 2;
			vout_scale_h = 2;
		}
	}
	else if (scanlines != 0 && scanline_level != 100 && bpp == 16) {
		//~ if (h <= 256) {
			//~ vout_scale_h = 2;
			//~ SysPrintf("Setting vout_scale_h to 2\n");
		//~ }
	}
#endif
	vout_width *= vout_scale_w;
	vout_height *= vout_scale_h;

	//~ if (vout_buf != NULL)
		//~ vout_buf = (char *)vout_buf + vout_yoffset * vout_width * bpp / 8;
}

static void vout_flip(const void *vram, int stride, int bgr24, int w, int h)
{
	unsigned short *dest = vout_buf;
	const unsigned short *src = vram;
	int dstride = vout_width, h1 = h;
	int doffs;

	if (vram == NULL) {
		// blanking
		memset(vout_buf, 0, dstride * h * 2);
		goto out;
	}

	doffs = (vout_height - h * vout_scale_h) * dstride;
	doffs += (dstride - w * vout_scale_w) / 2 & ~1;
	if (doffs != vout_doffs_old) {
		// clear borders
		memset(vout_buf, 0, dstride * h * 2);
		vout_doffs_old = doffs;
	}
	dest += doffs;

	if (bgr24)
	{
		// XXX: could we switch to RETRO_PIXEL_FORMAT_XRGB8888 here?
		for (; h1-- > 0; dest += dstride, src += stride)
		{
			bgr888_to_rgb565(dest, src, w * 3);
		}
	}
#ifdef __ARM_NEON__
	else if (soft_filter == SOFT_FILTER_SCALE2X && vout_scale_w == 2)
	{
		neon_scale2x_16_16(src, (void *)dest, w, stride * 2, dstride * 2, h);
	}
	else if (soft_filter == SOFT_FILTER_EAGLE2X && vout_scale_w == 2)
	{
		neon_eagle2x_16_16(src, (void *)dest, w, stride * 2, dstride * 2, h);
	}
	else if (scanlines != 0 && scanline_level != 100)
	{
		int l = scanline_level * 2048 / 100;
		int stride_0 = vout_scale_h >= 2 ? 0 : stride;

		h1 *= vout_scale_h;
		for (; h1 >= 2; h1 -= 2)
		{
			bgr555_to_rgb565(dest, src, w * 2);
			dest += dstride * 2, src += stride_0;

			bgr555_to_rgb565_b(dest, src, w * 2, l);
			dest += dstride * 2, src += stride;
		}
	}
#endif
	else
	{
		for (; h1-- > 0; dest += dstride, src += stride)
		{
			bgr555_to_rgb565(dest, src, w * 2);
		}
	}

out:
	vout_fb_dirty = 1;
	pl_rearmed_cbs.flip_cnt++;
	
//	if (pl_rearmed_cbs.flip_cnt % 100 == 0)
//		SysPrintf("100 flips.\n");
}

static void vout_close(void)
{
}

static void *pl_mmap(unsigned int size)
{
	return psxMap(0, size, 0, MAP_TAG_VRAM);
}

static void pl_munmap(void *ptr, unsigned int size)
{
	psxUnmap(ptr, size, MAP_TAG_VRAM);
}

struct rearmed_cbs pl_rearmed_cbs = {
	.pl_vout_open = vout_open,
	.pl_vout_set_mode = vout_set_mode,
	.pl_vout_flip = vout_flip,
	.pl_vout_close = vout_close,
	.mmap = pl_mmap,
	.munmap = pl_munmap,
	/* from psxcounters */
	.gpu_hcnt = &hSyncCount,
	.gpu_frame_count = &frame_counter,
};

void pl_frame_limit(void)
{
	/* called once per frame, make psxCpu->Execute() above return */
	stop = 1;
}

void pl_timing_prepare(int is_pal)
{
	is_pal_mode = is_pal;
}

void plat_trigger_vibrate(int pad, int low, int high)
{
//    rumble.set_rumble_state(pad, RETRO_RUMBLE_STRONG, high << 8);
//    rumble.set_rumble_state(pad, RETRO_RUMBLE_WEAK, low ? 0xffff : 0x0);
}

void pl_update_gun(int *xn, int *yn, int *xres, int *yres, int *in)
{
}

/* sound calls */
static int snd_init(void)
{
	return 0;
}

static void snd_finish(void)
{
}

static int snd_busy(void)
{
	return 0;
}

static void snd_feed(void *buf, int bytes)
{
	if (audio_batch_cb != NULL)
		audio_batch_cb(buf, bytes);
}

void out_register_ouya(struct out_driver *drv)
{
	drv->name = "ouya";
	drv->init = snd_init;
	drv->finish = snd_finish;
	drv->busy = snd_busy;
	drv->feed = snd_feed;
}

void pcsx_ouya_prep_settings(ouya_settings_t cb)
{
   static const struct ouya_variable vars[] = {
      { "pcsx_rearmed_frameskip", "Frameskip; 0|1|2|3" },
      { "pcsx_rearmed_region", "Region; Auto|NTSC|PAL" },
      { "pcsx_rearmed_pad1type", "Pad 1 Type; standard|analog" },
      { "pcsx_rearmed_pad2type", "Pad 2 Type; standard|analog" },
#ifndef DRC_DISABLE
      { "pcsx_rearmed_drc", "Dynamic recompiler; enabled|disabled" },
#endif
#ifdef __ARM_NEON__
      { "pcsx_rearmed_neon_interlace_enable", "Enable interlacing mode(s); disabled|enabled" },
      { "pcsx_rearmed_neon_enhancement_enable", "Enhanced resolution (slow); disabled|enabled" },
      { "pcsx_rearmed_neon_enhancement_no_main", "Enhanced resolution speed hack; disabled|enabled" },
#endif
      { "pcsx_rearmed_duping_enable", "Frame duping; on|off" },
      { "pcsx_rearmed_spu_reverb", "Sound: Reverb; on|off" },
      { "pcsx_rearmed_spu_interpolation", "Sound: Interpolation; simple|gaussian|cubic|off" },
      { NULL, NULL },
   };

   settings_cb = cb;

   settings_cb(SETTINGS_SET_VARIABLES, (void*)vars);
}

void pcsx_ouya_set_video_callback(ouya_render_t cb, void *rp) { video_cb = cb; rend_ptr = rp;}
void pcsx_ouya_set_audio_callback(ouya_audio_batch_t cb) { audio_batch_cb = cb;}
void pcsx_ouya_set_input_poll_callback(ouya_input_poll_t cb) { input_poll_cb = cb; }
void pcsx_ouya_set_input_state_callback(ouya_input_state_t cb) { input_state_cb = cb; }


/* cheats */
void pcsx_ouya_cheat_clear(void)
{
	ClearAllCheats();
}

void pcsx_ouya_cheat_set(unsigned index, bool enabled, const char *code)
{
	char buf[256];
	int ret;

	// cheat funcs are destructive, need a copy..
	strncpy(buf, code, sizeof(buf));
	buf[sizeof(buf) - 1] = 0;

	if (index < NumCheats)
		ret = EditCheat(index, "", buf);
	else
		ret = AddCheat("", buf);

	if (ret != 0)
		SysPrintf("Failed to set cheat %#u\n", index);
	else if (index < NumCheats)
		Cheats[index].Enabled = enabled;
}

/* multidisk support */
/*
static bool disk_ejected;
static unsigned int disk_current_index;
static unsigned int disk_count;
static struct disks_state {
	char *fname;
	int internal_index; // for multidisk eboots
} disks[8];

static bool disk_set_eject_state(bool ejected)
{
	// weird PCSX API..
	SetCdOpenCaseTime(ejected ? -1 : (time(NULL) + 2));
	LidInterrupt();

	disk_ejected = ejected;
	return true;
}

static bool disk_get_eject_state(void)
{
	// can't be controlled by emulated software 
	return disk_ejected;
}

static unsigned int disk_get_image_index(void)
{
	return disk_current_index;
}

static bool disk_set_image_index(unsigned int index)
{
	if (index >= sizeof(disks) / sizeof(disks[0]))
		return false;

	CdromId[0] = '\0';
	CdromLabel[0] = '\0';

	if (disks[index].fname == NULL) {
		SysPrintf("missing disk #%u\n", index);
		CDR_shutdown();

		// RetroArch specifies "no disk" with index == count,
		// so don't fail here..
		disk_current_index = index;
		return true;
	}

	SysPrintf("switching to disk %u: \"%s\" #%d\n", index,
		disks[index].fname, disks[index].internal_index);

	cdrIsoMultidiskSelect = disks[index].internal_index;
	set_cd_image(disks[index].fname);
	if (ReloadCdromPlugin() < 0) {
		SysPrintf("failed to load cdr plugin\n");
		return false;
	}
	if (CDR_open() < 0) {
		SysPrintf("failed to open cdr plugin\n");
		return false;
	}

	if (!disk_ejected) {
		SetCdOpenCaseTime(time(NULL) + 2);
		LidInterrupt();
	}

	disk_current_index = index;
	return true;
}

static unsigned int disk_get_num_images(void)
{
	return disk_count;
}

static bool disk_replace_image_index(unsigned index, const struct ouya_game_info *info)
{
	char *old_fname;
	bool ret = true;

	if (index >= sizeof(disks) / sizeof(disks[0]))
		return false;

	old_fname = disks[index].fname;
	disks[index].fname = NULL;
	disks[index].internal_index = 0;

	if (info != NULL) {
		disks[index].fname = strdup(info->path);
		if (index == disk_current_index)
			ret = disk_set_image_index(index);
	}

	if (old_fname != NULL)
		free(old_fname);

	return ret;
}

static bool disk_add_image_index(void)
{
	if (disk_count >= 8)
		return false;

	disk_count++;
	return true;
}
*/

//~ static struct retro_disk_control_callback disk_control = {
	//~ .set_eject_state = disk_set_eject_state,
	//~ .get_eject_state = disk_get_eject_state,
	//~ .get_image_index = disk_get_image_index,
	//~ .set_image_index = disk_set_image_index,
	//~ .get_num_images = disk_get_num_images,
	//~ .replace_image_index = disk_replace_image_index,
	//~ .add_image_index = disk_add_image_index,
//~ };
//~ static struct ouya_disk_control_callback disk_control = {
	//~ .set_eject_state = disk_set_eject_state,
	//~ .get_eject_state = disk_get_eject_state,
	//~ .get_image_index = disk_get_image_index,
	//~ .set_image_index = disk_set_image_index,
	//~ .get_num_images = disk_get_num_images,
	//~ .replace_image_index = disk_replace_image_index,
	//~ .add_image_index = disk_add_image_index,
//~ };

// just in case, maybe a win-rt port in the future?
//~ #ifdef _WIN32
//~ #define SLASH '\\'
//~ #else
#define SLASH '/'
//~ #endif

static char base_dir[PATH_MAX];

/*
static bool read_m3u(const char *file)
{
	char line[PATH_MAX];
	char name[PATH_MAX];
	FILE *f = fopen(file, "r");
	if (!f)
		return false;

	while (fgets(line, sizeof(line), f) && disk_count < sizeof(disks) / sizeof(disks[0])) {
		if (line[0] == '#')
			continue;
		char *carrige_return = strchr(line, '\r');
		if (carrige_return)
			*carrige_return = '\0';
		char *newline = strchr(line, '\n');
		if (newline)
			*newline = '\0';

		if (line[0] != '\0')
		{
			snprintf(name, sizeof(name), "%s%c%s", base_dir, SLASH, line);
			disks[disk_count++].fname = strdup(name);
		}
	}

	fclose(f);
	return (disk_count != 0);
}
*/

static void extract_filename(char *buf, const char *path, size_t size)
{
   char *base = strrchr(path, '/');
   strncpy(buf, base, size - 1);

   base = strrchr(buf, '.');
   if (base)
      *base = '\0';
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   char *base;
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
   {
      buf[0] = '.';
      buf[1] = '\0';
   }
}

bool pcsx_ouya_change_disc(const char *path)
{
	if (path == NULL || strlen(path) == 0) {
		if (Config.HLE == 1) {
			SysPrintf("no BIOS, prohibiting empty cd image.\n");
			char msgtext[64];
			JNI_Get_AppString(OUYA_APP_STRING_NOEMPTYHLE, msgtext, 64);
			struct ouya_message msg = {msgtext, 180};
			settings_cb(SETTINGS_SET_MESSAGE, &msg);
			return false;
		} else {
			CdromId[0] = '\0';
			CdromLabel[0] = '\0';
			CDR_shutdown();
			set_cd_image(NULL);
			strncpy(rom_fname, "BIOS", sizeof(rom_fname));
			return true;
		}
	}

	CdromId[0] = '\0';
	CdromLabel[0] = '\0';


	set_cd_image(path);
	if (ReloadCdromPlugin() < 0) {
		SysPrintf("failed to load cdr plugin\n");
		return false;
	}
	if (CDR_open() < 0) {
		SysPrintf("failed to open cdr plugin\n");
		return false;
	}

	SetCdOpenCaseTime(time(NULL) + 2);
	LidInterrupt();

	// save rom name so we can generate state file names
	extract_filename(rom_fname, path, sizeof(rom_fname));

	return true;
}

bool pcsx_ouya_load_exe(const char *path)
{
	if (plugins_opened) {
		ClosePlugins();
		plugins_opened = 0;
	}

	set_cd_image(NULL);
	
	if (LoadPlugins() == -1) {
		SysPrintf("failed to load plugins\n");
		return false;
	}

	plugins_opened = 1;
	NetOpened = 0;

	if (OpenPlugins() == -1) {
		SysPrintf("failed to open plugins\n");
		return false;
	}

	plugin_call_rearmed_cbs();
	dfinput_activate();

	SysReset();
	if (Load(path) != 0) {
		SysPrintf("exe load failed, bad file?\n");
		return false;
	}

	// save rom name so we can generate state file names
	extract_filename(rom_fname, path, sizeof(rom_fname));

	return true;
}

bool pcsx_ouya_load_game(const char *path)
{
	if (plugins_opened) {
		ClosePlugins();
		plugins_opened = 0;
	}

	extract_directory(base_dir, path, sizeof(base_dir));

	set_cd_image(path);

	/* have to reload after set_cd_image for correct cdr plugin */
	if (LoadPlugins() == -1) {
		SysPrintf("failed to load plugins\n");
		return false;
	}

	plugins_opened = 1;
	NetOpened = 0;

	if (OpenPlugins() == -1) {
		SysPrintf("failed to open plugins\n");
		return false;
	}

	plugin_call_rearmed_cbs();
	dfinput_activate();

	if (CheckCdrom() == -1) {
		SysPrintf("unsupported/invalid CD image: %s\n", path);
		return false;
	}

	SysReset();

	if (LoadCdrom() == -1) {
		SysPrintf("could not load CD-ROM!\n");
		return false;
	}
	emu_on_new_cd(0);

	// save rom name so we can generate state file names
	extract_filename(rom_fname, path, sizeof(rom_fname));

	return true;
}

bool pcsx_ouya_run_bios()
{
	if (Config.HLE)
		return false;

	if (plugins_opened) {
		ClosePlugins();
		plugins_opened = 0;
	}

	set_cd_image(NULL);

	if (LoadPlugins() == -1) {
		SysPrintf("failed to load plugins\n");
		return false;
	}

	plugins_opened = 1;
	NetOpened = 0;

	if (OpenPlugins() == -1) {
		SysPrintf("failed to open plugins\n");
		return false;
	}

	plugin_call_rearmed_cbs();
	dfinput_activate();

	SysReset();

	// fake rom name in case they want to savestate the BIOS :-/
	strncpy(rom_fname, "BIOS", sizeof(rom_fname));

	return true;
}

/*
//void *retro_get_memory_data(unsigned id)
void *ouya_get_memory_data(unsigned id)
{
	//if (id == RETRO_MEMORY_SAVE_RAM)
	if (id == OUYA_MEMORY_SAVE_RAM)
		return Mcd1Data;
	else
		return NULL;
}
*/

/*
//size_t retro_get_memory_size(unsigned id)
size_t ouya_get_memory_size(unsigned id)
{
	//if (id == RETRO_MEMORY_SAVE_RAM)
	if (id == OUYA_MEMORY_SAVE_RAM)
		return MCD_SIZE;
	else
		return 0;
}
*/

bool check_bios();

void pcsx_ouya_reset(void)
{
	// check whether BIOS has changed
	if (check_bios()) {
		SysPrintf("found BIOS file: %s\n", Config.Bios);
		Config.HLE = false;
	}
	else
		Config.HLE = true;
	
	ClosePlugins();
	OpenPlugins();
	SysReset();
	if (CheckCdrom() != -1) {
		LoadCdrom();
	}
}


static const unsigned short ouya_psx_map[] = {
	[OUYA_PAD_O]		= 1 << DKEY_CROSS,
	[OUYA_PAD_U]		= 1 << DKEY_SQUARE,
	[OUYA_PAD_Y]		= 1 << DKEY_TRIANGLE,
	[OUYA_PAD_A]		= 1 << DKEY_CIRCLE,
	[OUYA_PAD_UP]		= 1 << DKEY_UP,
	[OUYA_PAD_DOWN]		= 1 << DKEY_DOWN,
	[OUYA_PAD_LEFT]		= 1 << DKEY_LEFT,
	[OUYA_PAD_RIGHT]	= 1 << DKEY_RIGHT,
	[OUYA_PAD_L1]		= 1 << DKEY_L1,
	[OUYA_PAD_R1]		= 1 << DKEY_R1,
	[OUYA_PAD_L2]		= 1 << DKEY_L2,
	[OUYA_PAD_R2]		= 1 << DKEY_R2,
	[OUYA_PAD_L3]		= 1 << DKEY_L3,
	[OUYA_PAD_R3]		= 1 << DKEY_R3,
	[OUYA_PAD_SELECT]	= 1 << DKEY_SELECT,
	[OUYA_PAD_START]	= 1 << DKEY_START,
};
#define OUYA_PSX_MAP_LEN (sizeof(ouya_psx_map) / sizeof(ouya_psx_map[0]))

bool check_memcards();

static void update_variables(bool in_flight)
{
   struct ouya_variable var;
   
   var.value = NULL;
   var.key = "pcsx_rearmed_frameskip";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
      pl_rearmed_cbs.frameskip = atoi(var.value);

   var.value = NULL;
   var.key = "pcsx_rearmed_region";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      Config.PsxAuto = 0;
      if (strcmp(var.value, "Automatic") == 0)
         Config.PsxAuto = 1;
      else if (strcmp(var.value, "NTSC") == 0)
         Config.PsxType = 0;
      else if (strcmp(var.value, "PAL") == 0)
         Config.PsxType = 1;
   }

   var.value = NULL;
   var.key = "pcsx_rearmed_pad1type";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      in_type1 = PSE_PAD_TYPE_STANDARD;
      if (strcmp(var.value, "analog") == 0)
         in_type1 = PSE_PAD_TYPE_ANALOGPAD;
   }

   var.value = NULL;
   var.key = "pcsx_rearmed_pad2type";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      in_type2 = PSE_PAD_TYPE_STANDARD;
      if (strcmp(var.value, "analog") == 0)
         in_type2 = PSE_PAD_TYPE_ANALOGPAD;
   }

#ifdef __ARM_NEON__
   var.value = "NULL";
   var.key = "pcsx_rearmed_neon_interlace_enable";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "disabled") == 0)
         pl_rearmed_cbs.gpu_neon.allow_interlace = 0;
      else if (strcmp(var.value, "enabled") == 0)
         pl_rearmed_cbs.gpu_neon.allow_interlace = 1;
   }

   /*
   var.value = NULL;
   var.key = "pcsx_rearmed_neon_enhancement_enable";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "disabled") == 0)
         pl_rearmed_cbs.gpu_neon.enhancement_enable = 0;
      else if (strcmp(var.value, "enabled") == 0)
         pl_rearmed_cbs.gpu_neon.enhancement_enable = 1;
   }

   var.value = NULL;
   var.key = "pcsx_rearmed_neon_enhancement_no_main";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "disabled") == 0)
         pl_rearmed_cbs.gpu_neon.enhancement_no_main = 0;
      else if (strcmp(var.value, "enabled") == 0)
         pl_rearmed_cbs.gpu_neon.enhancement_no_main = 1;
   }
   */

   var.value = NULL;
   var.key = "pcsx_rearmed_neon_scaling";
   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "none") == 0) {
         soft_filter = SOFT_FILTER_NONE;
         pl_rearmed_cbs.gpu_neon.enhancement_enable = 0;
         pl_rearmed_cbs.gpu_neon.enhancement_no_main = 0;
	  } else if (strcmp(var.value, "scale2x") == 0) {
         soft_filter = SOFT_FILTER_SCALE2X;
         pl_rearmed_cbs.gpu_neon.enhancement_enable = 0;
         pl_rearmed_cbs.gpu_neon.enhancement_no_main = 0;
	  } else if (strcmp(var.value, "eagle2x") == 0) {
         soft_filter = SOFT_FILTER_EAGLE2X;
         pl_rearmed_cbs.gpu_neon.enhancement_enable = 0;
         pl_rearmed_cbs.gpu_neon.enhancement_no_main = 0;
	  } else if (strcmp(var.value, "doubleres") == 0) {
         soft_filter = SOFT_FILTER_NONE;
         pl_rearmed_cbs.gpu_neon.enhancement_enable = 1;
         pl_rearmed_cbs.gpu_neon.enhancement_no_main = 1;
	  }
   }

   //~ var.value = NULL;
   //~ var.key = "pcsx_rearmed_scanlines";
   //~ if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   //~ {
      //~ if (strcmp(var.value, "none") == 0) {
         scanlines = 0;
         scanline_level = 0;
	  //~ } else {
         //~ scanlines = 1;
         //~ scanline_level = atoi(var.value);
	  //~ }
   //~ }
#endif

   var.value = "NULL";
   var.key = "pcsx_rearmed_duping_enable";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "off") == 0)
         duping_enable = false;
      else if (strcmp(var.value, "on") == 0)
         duping_enable = true;
   }

#ifndef DRC_DISABLE
   var.value = NULL;
   var.key = "pcsx_rearmed_drc";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      R3000Acpu *prev_cpu = psxCpu;

      if (strcmp(var.value, "disabled") == 0)
         Config.Cpu = CPU_INTERPRETER;
      else if (strcmp(var.value, "enabled") == 0)
         Config.Cpu = CPU_DYNAREC;

      psxCpu = (Config.Cpu == CPU_INTERPRETER) ? &psxInt : &psxRec;
      if (psxCpu != prev_cpu) {
         prev_cpu->Shutdown();
         psxCpu->Init();
         psxCpu->Reset(); // not really a reset..
      }
   }
#endif

   var.value = "NULL";
   var.key = "pcsx_rearmed_spu_reverb";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "off") == 0)
         spu_config.iUseReverb = false;
      else if (strcmp(var.value, "on") == 0)
         spu_config.iUseReverb = true;
   }

   var.value = "NULL";
   var.key = "pcsx_rearmed_spu_interpolation";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "simple") == 0)
         spu_config.iUseInterpolation = 1;
      else if (strcmp(var.value, "gaussian") == 0)
         spu_config.iUseInterpolation = 2;
      else if (strcmp(var.value, "cubic") == 0)
         spu_config.iUseInterpolation = 3;
      else if (strcmp(var.value, "off") == 0)
         spu_config.iUseInterpolation = 0;
   }

   var.value = NULL;
   var.key = "pcsx_rearmed_spu_irq_always_on";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "on") == 0)
         Config.SpuIrq = 1;
      else 
         Config.SpuIrq = 0;
   }

   var.value = NULL;
   var.key = "pcsx_rearmed_disable_xa";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "on") == 0)
         Config.Xa = 1;
      else 
         Config.Xa = 0;
   }

   var.value = NULL;
   var.key = "pcsx_rearmed_disable_cdda";

   if (settings_cb(SETTINGS_GET_VARIABLE, &var) || var.value)
   {
      if (strcmp(var.value, "on") == 0)
         Config.Cdda = 1;
      else 
         Config.Cdda = 0;
   }

   // memory card changes
   check_memcards();

   if (in_flight) {
      // inform core things about possible config changes
      plugin_call_rearmed_cbs();

      if (GPU_open != NULL && GPU_close != NULL) {
         GPU_close();
         GPU_open(&gpuDisp, "PCSX", NULL);
      }

      dfinput_activate();
   }
}

//void retro_run(void) 
void pcsx_ouya_step()
{
	int i;

	input_poll_cb();

	bool updated = false;
	//if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
	if (settings_cb(SETTINGS_GET_VARIABLE_UPDATE, &updated) && updated)
		update_variables(true);

	in_keystate = 0;

	for (i = 0; i < OUYA_PSX_MAP_LEN; i++)
		if (input_state_cb(1, OUYA_DEVICE_JOYPAD, 0, i))
			in_keystate |= ouya_psx_map[i];
	in_keystate <<= 16;
	for (i = 0; i < OUYA_PSX_MAP_LEN; i++)
		if (input_state_cb(0, OUYA_DEVICE_JOYPAD, 0, i))
			in_keystate |= ouya_psx_map[i];

	if (in_type1 == PSE_PAD_TYPE_ANALOGPAD)
	{
		in_a1[0] = input_state_cb(0, OUYA_DEVICE_ANALOG, 0, OUYA_ANALOG_LX);
		in_a1[1] = input_state_cb(0, OUYA_DEVICE_ANALOG, 0, OUYA_ANALOG_LY);
		in_a2[0] = input_state_cb(0, OUYA_DEVICE_ANALOG, 0, OUYA_ANALOG_RX);
		in_a2[1] = input_state_cb(0, OUYA_DEVICE_ANALOG, 0, OUYA_ANALOG_RY);
		//~ in_a1[0] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 256) + 128;
		//~ in_a1[1] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 256) + 128;
		//~ in_a2[0] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / 256) + 128;
		//~ in_a2[1] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / 256) + 128;
	}

	if (in_type2 == PSE_PAD_TYPE_ANALOGPAD)
	{
		in_a3[0] = input_state_cb(1, OUYA_DEVICE_ANALOG, 0, OUYA_ANALOG_LX);
		in_a3[1] = input_state_cb(1, OUYA_DEVICE_ANALOG, 0, OUYA_ANALOG_LY);
		in_a4[0] = input_state_cb(1, OUYA_DEVICE_ANALOG, 0, OUYA_ANALOG_RX);
		in_a4[1] = input_state_cb(1, OUYA_DEVICE_ANALOG, 0, OUYA_ANALOG_RY);
	}
	
	stop = 0;
	//SysPrintf("Stepping PSX CPU...\n");
	psxCpu->Execute();
	
	//~ usleep(10);
	sched_yield();

	video_cb((vout_fb_dirty || !duping_enable) ? vout_buf : NULL, vout_width, vout_height, vout_width * 2, rend_ptr);
	vout_fb_dirty = 0;
}

static bool try_use_bios(const char *path)
{
	FILE *f;
	long size;
	const char *name;

	f = fopen(path, "rb");
	if (f == NULL)
		return false;

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);

	if (size != 512 * 1024)
		return false;

	name = strrchr(path, SLASH);
	if (name++ == NULL)
		name = path;
	snprintf(Config.Bios, sizeof(Config.Bios), "%s", name);
	return true;
}

#if 1
#include <sys/types.h>
#include <dirent.h>

static bool find_any_bios(const char *dirpath, char *path, size_t path_size)
{
	DIR *dir;
	struct dirent *ent;
	bool ret = false;

	dir = opendir(dirpath);
	if (dir == NULL)
		return false;

	while ((ent = readdir(dir))) {
		if (strncasecmp(ent->d_name, "scph", 4) != 0)
			continue;

		snprintf(path, path_size, "%s/%s", dirpath, ent->d_name);
		ret = try_use_bios(path);
		if (ret)
			break;
	}
	closedir(dir);
	return ret;
}
#else
#define find_any_bios(...) false
#endif

/*
static void check_system_specs(void)
{
   unsigned level = 6;
   //environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
   settings_cb(SETTINGS_SET_PERFORMANCE_LEVEL, &level);
}
* */

bool check_bios()
{
	const char *bios[] = { "scph1001", "scph5501", "scph7001" };
	const char *dir;
	char path[MAXPATHLEN];
	int i;
	bool found_bios = false;

	if (settings_cb(SETTINGS_GET_BIOS_DIRECTORY, &dir) && dir)
	{
		snprintf(Config.BiosDir, sizeof(Config.BiosDir), "%s", dir);
		SysPrintf("Got BIOS directory: %s\n", Config.BiosDir);

		// first try BIOS file from prefs
		struct ouya_variable bios_file;
		bios_file.value = NULL;
		bios_file.key = "pcsx_rearmed_bios_file";
		if (settings_cb(SETTINGS_GET_VARIABLE, &bios_file) || bios_file.value) {
			snprintf(path, sizeof(path), "%s%s", dir, bios_file.value);
			found_bios = try_use_bios(path);
		}

		// next try hardcoded BIOS names
		if (!found_bios) {
			for (i = 0; i < sizeof(bios) / sizeof(bios[0]); i++) {
				snprintf(path, sizeof(path), "%s/%s.bin", dir, bios[i]);
				found_bios = try_use_bios(path);
				if (found_bios)
					break;
			}
		}

		// lastly try anything
		if (!found_bios)
			found_bios = find_any_bios(dir, path, sizeof(path));
	}
	return found_bios;
}

bool check_memcards()
{
	const char *dir;
	if (settings_cb(SETTINGS_GET_SAVE_DIRECTORY, &dir) && dir)
	{
		struct ouya_variable card;

		card.value = NULL;
		card.key = "pcsx_rearmed_memcard1";
		if (settings_cb(SETTINGS_GET_VARIABLE, &card) || card.value) 
			snprintf(Config.Mcd1, sizeof(Config.Mcd1), "%s%s", dir, card.value);
		else 
			snprintf(Config.Mcd1, sizeof(Config.Mcd1), "%spcsx-mc1.mcd", dir);
		
		card.value = NULL;
		card.key = "pcsx_rearmed_memcard2";
		if (settings_cb(SETTINGS_GET_VARIABLE, &card) || card.value) 
			snprintf(Config.Mcd2, sizeof(Config.Mcd2), "%s%s", dir, card.value);
		else 
			snprintf(Config.Mcd2, sizeof(Config.Mcd2), "%spcsx-mc2.mcd", dir);		

		LoadMcd(1, Config.Mcd1);
		LoadMcd(2, Config.Mcd2);
		return true;
	}
	return false;
}

void pcsx_ouya_init(void)
{
	int ret;
	bool found_bios = false;

	ret = emu_core_preinit();

	if (check_memcards())
		SysPrintf("Got Saves directory.\n");

	ret |= emu_core_init();

	if (ret != 0) {
		SysPrintf("PCSX init failed.\n");
		exit(1);
	}

#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)
	posix_memalign(&vout_buf, 16, VOUT_MAX_WIDTH * VOUT_MAX_HEIGHT * 2);
#else
	vout_buf = malloc(VOUT_MAX_WIDTH * VOUT_MAX_HEIGHT * 2);
#endif

	found_bios = check_bios();
	if (found_bios) {
		SysPrintf("found BIOS file: %s\n", Config.Bios);
		Config.HLE = false;
	}
	else
	{
		SysPrintf("no BIOS files found.\n");
		char msgtext[64];
		JNI_Get_AppString(OUYA_APP_STRING_NOBIOS, msgtext, 64);
		struct ouya_message msg = {msgtext, 180};
		settings_cb(SETTINGS_SET_MESSAGE, &msg);
		Config.HLE = true;
	}

	//environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE, &vout_can_dupe);
	//environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &disk_control);
	//environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble);
	vout_can_dupe = true;
	//settings_cb(SETTINGS_SET_DISK_CONTROL_INTERFACE, &disk_control);

	/* Set how much slower PSX CPU runs * 100 (so that 200 is 2 times)
	 * we have to do this because cache misses and some IO penalties
	 * are not emulated. Warning: changing this may break compatibility. */
#if !defined(__arm__) || defined(__ARM_ARCH_7A__)
	cycle_multiplier = 175;
#else
	cycle_multiplier = 200;
#endif
	pl_rearmed_cbs.gpu_peops.iUseDither = 1;
	spu_config.iUseFixedUpdates = 1;

	McdDisable[0] = 0;
	McdDisable[1] = 0;
	init_memcard(Mcd1Data);
	init_memcard(Mcd2Data);
	
	//~ SaveFuncs.open = save_open;
	//~ SaveFuncs.read = save_read;
	//~ SaveFuncs.write = save_write;
	//~ SaveFuncs.seek = save_seek;
	//~ SaveFuncs.close = save_close;

	update_variables(false);
	//check_system_specs();
}

void pcsx_ouya_deinit(void)
{
	SysClose();
	free(vout_buf);
	vout_buf = NULL;
}

int get_state_file(char *fname, size_t len, int slot)
{
	if (slot < 0 || slot > 9)
		return -1;
	
	const char *dir;

	if (settings_cb(SETTINGS_GET_SAVE_DIRECTORY, &dir) && dir)
	{
		snprintf(fname, len, "%s%s.rs%d", dir, rom_fname, slot);
		return 0;
	}
	return -1;
}

bool pcsx_ouya_save_state(int slot)
{
	char fname[MAXPATHLEN];
	int ret;

	ret = get_state_file(fname, sizeof(fname), slot);
	if (ret != 0)
		return false;

	ret = SaveState(fname);

	SysPrintf("* %s \"%s\" [%d]\n", ret == 0 ? "saved" : "failed to save", fname, slot);
	return (ret == 0);
}

bool pcsx_ouya_load_state(int slot)
{
	char fname[MAXPATHLEN];
	int ret;

	hud_msg[0] = 0;

	ret = get_state_file(fname, sizeof(fname), slot);
	if (ret != 0)
		return false;

	return (LoadState(fname) == 0);
}
