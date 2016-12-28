#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <3ds.h>

#include "archive.h"
#include "log.h"

#include "modules_common.h"

Result bossbannerhax_install(char *menuhax_basefn, s16 menuversion);
Result bossbannerhax_delete();

static u32 bossbannerhax_NsDataId = 0x58484e42;

void register_module_bossbannerhax()
{
	module_entry module = {
		.name = "bossbannerhax",
		.haxinstall = bossbannerhax_install,
		.haxdelete = bossbannerhax_delete,
		.themeflag = false
	};

	register_module(&module);
}

Result bossbannerhax_install(char *menuhax_basefn, s16 menuversion)
{
	Result ret=0;

	bossContext ctx;
	u8 status=0;
	u32 tmp=0;
	u8 tmpbuf[4] = {0};
	char *taskID = "tmptask";

	Handle fshandle=0;
	FS_Path archpath;
	FS_ExtSaveDataInfo extdatainfo;
	u32 extdataID = 0x217;//TODO: Load this properly.
	u32 extdata_exists = 0;

	u32 numdirs = 0, numfiles = 0;
	u8 *smdh = NULL;
	u32 smdh_size = 0x36c0;

	char payload_filepath[256];
	char tmpstr[256];

	memset(payload_filepath, 0, sizeof(payload_filepath));
	memset(tmpstr, 0, sizeof(tmpstr));

	snprintf(payload_filepath, sizeof(payload_filepath)-1, "romfs:/finaloutput/stage1_bossbannerhax.zip@%s.bin", menuhax_basefn);
	snprintf(tmpstr, sizeof(tmpstr)-1, "sdmc:/menuhax/stage1/%s.bin", menuhax_basefn);

	log_printf(LOGTAR_ALL, "Copying stage1 to SD...\n");
	log_printf(LOGTAR_LOG, "Src path = '%s', dst = '%s'.\n", payload_filepath, tmpstr);

	unlink(tmpstr);
	ret = archive_copyfile(SDArchive, SDArchive, payload_filepath, tmpstr, filebuffer, 0, 0x1000, 0, "stage1");
	if(ret!=0)
	{
		log_printf(LOGTAR_ALL, "Failed to copy stage1 to SD: 0x%08x.\n", (unsigned int)ret);
		return ret;
	}

	log_printf(LOGTAR_ALL, "Running extdata deletion+creation if required, for the currently running title...\n");

	archpath.type = PATH_BINARY;
	archpath.data = &extdatainfo;
	archpath.size = 0xc;

	memset(&extdatainfo, 0, sizeof(extdatainfo));
	extdatainfo.mediaType = MEDIATYPE_SD;
	extdatainfo.saveId = extdataID;

	//TODO: Add notes about running the <target application> at least once in error messages related to extdata-reading failing.

	//For whatever reason deleting/creating this extdata with the default *hax fsuser session(from Home Menu) fails.
	if (R_FAILED(ret = srvGetServiceHandleDirect(&fshandle, "fs:USER"))) return ret;//This code is based on the code from sploit_installer for this.
        if (R_FAILED(ret = FSUSER_Initialize(fshandle))) return ret;
	fsUseSession(fshandle);

	ret = FSUSER_GetFormatInfo(NULL, &numdirs, &numfiles, NULL, ARCHIVE_EXTDATA, archpath);
	if(R_FAILED(ret))
	{
		log_printf(LOGTAR_ALL, "FSUSER_GetFormatInfo() failed: 0x%08x.\n", (unsigned int)ret);
		//return ret;
	}
	else
	{
		log_printf(LOGTAR_ALL, "FSUSER_GetFormatInfo: numdirs = 0x%08x, numfiles = 0x%08x\n", (unsigned int)numdirs, (unsigned int)numfiles);
		extdata_exists = 1;
	}
	

	if(numdirs!=10 || numfiles!=10)
	{
		smdh = malloc(smdh_size);
		if(smdh==NULL)
		{
			log_printf(LOGTAR_ALL, "Failed to allocate memory for the SMDH.\n");
			ret = -12;
			fsEndUseSession();
			return ret;
		}
		memset(smdh, 0, smdh_size);

		//TODO: Load "extdata:/ExBanner/COMMON.bin", then write it after creating the extdata.

		if(extdata_exists)
		{
			ret = FSUSER_ReadExtSaveDataIcon(&tmp, extdatainfo, smdh_size, smdh);
			if(R_SUCCEEDED(ret) && tmp!=smdh_size)ret = -13;
			if(R_FAILED(ret))
			{
				log_printf(LOGTAR_ALL, "Extdata icon reading failed: 0x%08x.\n", (unsigned int)ret);
				free(smdh);
				fsEndUseSession();
				return ret;
			}

			ret = FSUSER_DeleteExtSaveData(extdatainfo);
			if(R_FAILED(ret))
			{
				log_printf(LOGTAR_ALL, "Extdata deletion failed: 0x%08x.\n", (unsigned int)ret);
				free(smdh);
				fsEndUseSession();
				return ret;
			}
		}

		ret = FSUSER_CreateExtSaveData(extdatainfo, 10, 10, ~0, smdh_size, smdh);
		free(smdh);
		if(R_FAILED(ret))
		{
			log_printf(LOGTAR_ALL, "Extdata creation failed: 0x%08x.\n", (unsigned int)ret);
			fsEndUseSession();
			return ret;
		}
	}

	fsEndUseSession();

	log_printf(LOGTAR_ALL, "Running BOSS setup...\n");

	ret = bossInit(0, false);
	if(R_FAILED(ret))
	{
		log_printf(LOGTAR_ALL, "bossInit() failed: 0x%08x.\n", (unsigned int)ret);
		return ret;
	}

	ret = bossReinit(0x0004001000021700ULL);//TODO: Load this programID properly.
	//TODO: Run this again except with the proper Home Menu programID once BOSS usage is finished.
	if(R_FAILED(ret))
	{
		log_printf(LOGTAR_ALL, "bossReinit() failed: 0x%08x.\n", (unsigned int)ret);
		bossExit();
		return ret;
	}

	ret = bossSetStorageInfo(extdataID, 0x400000, MEDIATYPE_SD);//TODO: Load the actual extdataID for this region.
	if(R_FAILED(ret))
	{
		log_printf(LOGTAR_ALL, "bossSetStorageInfo() failed: 0x%08x.\n", (unsigned int)ret);
		bossExit();
		return ret;
	}

	memset(tmpstr, 0, sizeof(tmpstr));
	snprintf(tmpstr, sizeof(tmpstr)-1, "http://10.0.0.23/menuhax/bossbannerhax/%s_bossbannerhax.bin" /*"http://yls8.mtheall.com/menuhax/bossbannerhax/%s_bossbannerhax.bin"*/, menuhax_basefn);
	//HTTP is used here since it's currently unknown how to setup a non-default rootCA cert for BOSS.

	bossSetupContextDefault(&ctx, 60, tmpstr);

	ret = bossSendContextConfig(&ctx);
	if(R_FAILED(ret))log_printf(LOGTAR_ALL, "bossSendContextConfig returned 0x%08x.\n", (unsigned int)ret);

	if(R_SUCCEEDED(ret))
	{
		ret = bossDeleteTask(taskID, 0);
		ret = bossDeleteNsData(bossbannerhax_NsDataId);

		ret = bossRegisterTask(taskID, 0, 0);
		if(R_FAILED(ret))log_printf(LOGTAR_ALL, "bossRegisterTask returned 0x%08x.\n", (unsigned int)ret);

		if(R_SUCCEEDED(ret))
		{
			ret = bossStartTaskImmediate(taskID);
			if(R_FAILED(ret))log_printf(LOGTAR_ALL, "bossStartTaskImmediate returned 0x%08x.\n", (unsigned int)ret);

			if(R_SUCCEEDED(ret))
			{
				log_printf(LOGTAR_ALL, "Waiting for the task to run...\n");

				while(1)
				{
					ret = bossGetTaskState(taskID, 0, &status, NULL ,NULL);
					if(R_FAILED(ret))
					{
						log_printf(LOGTAR_ALL, "bossGetTaskState returned 0x%08x.\n", (unsigned int)ret);
						break;
					}
					if(R_SUCCEEDED(ret))log_printf(LOGTAR_ALL, "...\n");//printf("bossGetTaskState: tmp0=0x%x, tmp2=0x%x, tmp1=0x%x.\n", (unsigned int)tmp0, (unsigned int)tmp2, (unsigned int)tmp1);

					if(status!=BOSSTASKSTATUS_STARTED)break;

					svcSleepThread(1000000000LL);//Delay 1s.
				}
			}

			if(R_SUCCEEDED(ret) && status==BOSSTASKSTATUS_ERROR)
			{
				log_printf(LOGTAR_ALL, "BOSS task failed.\n");
				ret = -9;
			}

			if(R_SUCCEEDED(ret))
			{
				log_printf(LOGTAR_ALL, "Reading BOSS content...\n");

				tmp = 0;
				ret = bossReadNsData(bossbannerhax_NsDataId, 0, tmpbuf, sizeof(tmpbuf), &tmp, NULL);
				if(R_FAILED(ret))log_printf(LOGTAR_ALL, "bossReadNsData returned 0x%08x, transfer_total=0x%x.\n", (unsigned int)ret, (unsigned int)tmp);

				if(R_SUCCEEDED(ret) && tmp!=sizeof(tmpbuf))ret = -10;

				if(R_SUCCEEDED(ret) && memcmp(tmpbuf, "CBMD", 4))ret = -11;

				if(R_FAILED(ret))log_printf(LOGTAR_ALL, "BOSS data reading failed: 0x%08x.\n", (unsigned int)ret);
			}

			bossDeleteTask(taskID, 0);
		}
	}

	bossExit();

	return ret;
}

Result bossbannerhax_delete()
{
	Result ret=0;

	ret = bossInit(0, true);
	if(R_FAILED(ret))
	{
		log_printf(LOGTAR_ALL, "bossInit() failed: 0x%08x.\n", (unsigned int)ret);
		return ret;
	}

	ret = bossReinit(0x0004001000021700ULL);//TODO: Load this programID properly.
	//TODO: Run this again except with the proper Home Menu programID once BOSS usage is finished.
	if(R_FAILED(ret))
	{
		log_printf(LOGTAR_ALL, "bossReinit() failed: 0x%08x.\n", (unsigned int)ret);
		bossExit();
		return ret;
	}

	ret = bossDeleteNsData(bossbannerhax_NsDataId);
	if(R_FAILED(ret))log_printf(LOGTAR_ALL, "bossDeleteNsData() returned: 0x%08x.\n", (unsigned int)ret);

	bossUnregisterStorage();

	bossExit();

	return 0;
}

