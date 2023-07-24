/*
 * modify RDB flags
 *
 * Copyright © 1994 by Andreas M. Kirchwitz
 *                     Seesener Straﬂe 69
 *                     D-10709 Berlin, Germany
 *                     UUCP/Internet: amk@zikzak.in-berlin.de
 *
 * Thanks to Ralph Babel (author of "The Amiga Guru Book")
 * and Matthias Scheler (owner of a copy of "The Amiga Guru Book").
 */

#define __USE_SYSBASE

#include <exec/types.h>
#include <exec/alerts.h>
#include <exec/memory.h>
#include <devices/trackdisk.h>
#include <devices/hardblocks.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>

extern struct ExecBase *__far AbsExecBase;

/* defines */

#define PROGRAMNAME_MAX 32

#define DEFAULT_DEVICE		"scsi.device"
#define DEFAULT_UNIT		0
#define DEFAULT_SECTORSIZE	TD_SECTOR
#define DEFAULT_FLAGS		0

#define RDBFLAGS_TEMPLATE "Device/K,Unit/N,SectorSize/N,LastDisk/S,NoLastDisk/S,LastLun/S,NoLastLun/S,LastTarget/S,NoLastTarget/S,Reselect/S,NoReselect/S,Sync/S,NoSync/S"

struct Args
{
	char *device;
	LONG *pUnit;
	LONG *pSectorSize;
	LONG lastDisk;
	LONG noLastDisk;
	LONG lastLun;
	LONG noLastLun;
	LONG lastTarget;
	LONG noLastTarget;
	LONG reselection;
	LONG noReselection;
	LONG synchronous;
	LONG noSynchronous;
	LONG defaultUnit;
	LONG defaultSectorSize;
};

/* globals */

static const char version[] = "$VER: RDBFlags 1.3 " __AMIGADATE__;

LONG main(void)
{
	LONG result;
	struct ExecBase *SysBase;
	struct Library *DOSBase;
	struct Process *pr;
	struct Message *mn;
	struct RDArgs *rda;
	struct MsgPort *mp;
	struct IOStdReq *io;
	struct RigidDiskBlock *rdb;
	char *name;
	char *device;
	ULONG unit;
	ULONG sectorSize, i;
	ULONG sector;
	ULONG flags;
	LONG *p, sum;
	struct Args args;
	struct DriveGeometry DG;
	char programName[PROGRAMNAME_MAX];

	SysBase = AbsExecBase;

	pr = (struct Process *) FindTask(NULL);

	result = RETURN_FAIL;

	if (pr->pr_CLI != NULL)
	{
		if ((DOSBase = OpenLibrary(DOSNAME, 37)) != NULL)
		{
			if (!GetProgramName(name = programName, PROGRAMNAME_MAX))
				name = "RDBFlags";

			args.device            = DEFAULT_DEVICE;
			args.pUnit             = &args.defaultUnit;
			args.pSectorSize       = &args.defaultSectorSize;
			args.lastDisk          = FALSE;
			args.noLastDisk        = FALSE;
			args.lastLun           = FALSE;
			args.noLastLun         = FALSE;
			args.lastTarget        = FALSE;
			args.noLastTarget      = FALSE;
			args.reselection       = FALSE;
			args.noReselection     = FALSE;
			args.synchronous       = FALSE;
			args.noSynchronous     = FALSE;
			args.defaultUnit       = DEFAULT_UNIT;
			args.defaultSectorSize = DEFAULT_SECTORSIZE;

			if (rda = ReadArgs(RDBFLAGS_TEMPLATE, (LONG *) &args, NULL))
			{
				device = args.device;
				unit   = *args.pUnit;

				result = RETURN_ERROR;

				if ((mp = CreateMsgPort()) != NULL)
				{
					if ((io = CreateIORequest(mp, sizeof(struct IOStdReq))) != NULL)
					{
						if (OpenDevice(device, unit, (struct IORequest *) io, DEFAULT_FLAGS) == 0)
						{
							Printf("device \"%s\", unit %lu\n", device, unit);

							io->io_Command = TD_GETGEOMETRY;
							io->io_Length  = sizeof(struct DriveGeometry);
							io->io_Data    = &DG;

							if (DoIO((struct IORequest *) io) != 0) {
								Printf("%s: error %ld while reading drive geometry.\n", name, (LONG) io->io_Error);
								Printf("%s: (use option \"SectorSize\" to override default sector size)\n", name);
								DG.dg_SectorSize = *args.pSectorSize;
								DG.dg_BufMemType = MEMF_CHIP;
							}
							else if (args.pSectorSize != &args.defaultSectorSize) {
								DG.dg_SectorSize = *args.pSectorSize;
								DG.dg_BufMemType = MEMF_CHIP;
							}

							sectorSize = DG.dg_SectorSize;

							Printf("sector size is %lu bytes\n", sectorSize);

							if (sectorSize >= sizeof(struct RigidDiskBlock))
							{
								if (rdb = AllocMem(sectorSize, DG.dg_BufMemType))
								{
									for (sector = 0; sector < RDB_LOCATION_LIMIT; ++sector)
									{
										io->io_Command = CMD_READ;
										io->io_Length  = sectorSize;
										io->io_Data    = rdb;
										io->io_Offset  = sector * sectorSize;

										if (DoIO((struct IORequest *) io) == 0
										    && rdb->rdb_ID == IDNAME_RIGIDDISK
										    && rdb->rdb_SummedLongs * sizeof(LONG) <= sectorSize)
										{
											for (sum = 0, p = (LONG *) rdb, i = rdb->rdb_SummedLongs; i != 0; --i)
												sum += *p++;

											if (sum == 0 && rdb->rdb_BlockBytes == sectorSize)
												break;
										}
									}

									if (sector != RDB_LOCATION_LIMIT)
									{
										Printf("found RDB at sector %lu\n", sector);
										Printf("drive %-.8s %-.16s %-.4s\n", rdb->rdb_DiskVendor, rdb->rdb_DiskProduct, rdb->rdb_DiskRevision);

										flags = rdb->rdb_Flags;

										if ( args.lastDisk    && args.noLastDisk
										  || args.lastLun     && args.noLastLun
										  || args.lastTarget  && args.noLastTarget
										  || args.reselection && args.noReselection
										  || args.synchronous && args.noSynchronous )
										{
											Printf("%s: contradictory options, won't alter RDB.\n", name);
										}
										else
										{
											if (args.lastDisk)      flags |=  RDBFF_LAST;
											if (args.noLastDisk)    flags &= ~RDBFF_LAST;
											if (args.lastLun)       flags |=  RDBFF_LASTLUN;
											if (args.noLastLun)     flags &= ~RDBFF_LASTLUN;
											if (args.lastTarget)    flags |=  RDBFF_LASTTID;
											if (args.noLastTarget)  flags &= ~RDBFF_LASTTID;
											if (args.reselection)   flags &= ~RDBFF_NORESELECT;
											if (args.noReselection) flags |=  RDBFF_NORESELECT;
											if (args.synchronous)   flags |=  RDBFF_SYNCH;
											if (args.noSynchronous) flags &= ~RDBFF_SYNCH;
										}

										Printf("\n last disk:   %s\n", flags & RDBFF_LAST       ? "yes" : "no");
										Printf(" last lun:    %s\n",   flags & RDBFF_LASTLUN    ? "yes" : "no");
										Printf(" last target: %s\n",   flags & RDBFF_LASTTID    ? "yes" : "no");
										Printf(" reselection: %s\n",   flags & RDBFF_NORESELECT ? "no"  : "yes");
										Printf(" synchronous: %s\n\n", flags & RDBFF_SYNCH      ? "yes" : "no");

										result = RETURN_OK;

										if (flags != rdb->rdb_Flags)
										{
											rdb->rdb_Flags = flags;

											rdb->rdb_ChkSum = 0;
											for (sum = 0, p = (LONG *) rdb, i = rdb->rdb_SummedLongs; i != 0; --i)
												sum -= *p++;
											rdb->rdb_ChkSum = sum;

											Printf("RDB modified, saving to disk!\n");

											io->io_Command = CMD_WRITE;
											io->io_Length  = sectorSize;
											io->io_Data    = rdb;
											io->io_Offset  = sector * sectorSize;

											if (DoIO((struct IORequest *) io) == 0)
											{
												io->io_Command = CMD_UPDATE;
												(void) DoIO((struct IORequest *) io);
											}

											if (io->io_Error != 0)
											{
												result = RETURN_ERROR;
												io->io_Command = CMD_CLEAR;
												(void) DoIO((struct IORequest *) io);
												Printf("%s: error %ld while writing RDB.\n", name, (LONG) io->io_Error);
											}
										}
									}
									else
										Printf("%s: no RDB found.\n", name);

									io->io_Command = TD_MOTOR;
									io->io_Length  = 0;
									(void) DoIO((struct IORequest *) io);

									FreeMem(rdb, sectorSize);
								}
								else
									PrintFault(ERROR_NO_FREE_STORE, name);
							}
							else
								Printf("%s: sector size %lu too small for RDB.\n", name, sectorSize);

							CloseDevice((struct IORequest *) io);
						}
						else
							Printf("%s: error %ld opening \"%s\", unit %lu.\n", name, (LONG) io->io_Error, device, unit);

						DeleteMsgPort(mp);
					}
					DeleteIORequest(io);
				}
				FreeArgs(rda);
			}
			else
				PrintFault(IoErr(), name);

			CloseLibrary(DOSBase);
		}
		else
			Alert(AT_Recovery | AG_OpenLib | AO_DOSLib);
	}
	else
	{
		(void) WaitPort(&pr->pr_MsgPort);
		mn = GetMsg(&pr->pr_MsgPort);
		Forbid();
		ReplyMsg(mn);
	}

	return result;
}

