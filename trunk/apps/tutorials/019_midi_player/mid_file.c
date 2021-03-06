// $Id: mid_file.c 1118 2010-10-24 13:54:51Z tk $
/*
 * MIDI File Access Routines
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include <seq_bpm.h>
#include <seq_midi_out.h>

#include <string.h>

#include "mid_file.h"

#include <dosfs.h>


/////////////////////////////////////////////////////////////////////////////
// for optional debugging messages via MIDI
/////////////////////////////////////////////////////////////////////////////
#define DEBUG_VERBOSE_LEVEL 2
#define DEBUG_MSG MIOS32_MIDI_SendDebugMessage


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

// in which subdirectory of the SD card are the .mid files located?
// use "" for root
// use "<dir>/" for a subdirectory in root
// use "<dir>/<subdir>/" to reach a subdirectory in <dir>, etc..

#define MID_FILES_PATH ""
//#define MID_FILES_PATH "MyMidi/"



/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

// the file position and length
static u32 midifile_pos;
static u32 midifile_len;
static char ui_midifile_name[13];

// DOS FS variables
static u8 sector[SECTOR_SIZE];
static VOLINFO vi;
static DIRINFO di;
static DIRENT de;
static FILEINFO fi;


/////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 MID_FILE_Init(u32 mode)
{
  // initial midifile name and size
  strcpy(ui_midifile_name, "waiting...");
  midifile_len = 0;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Returns the filename of the .mid file for the user interface
// Contains an error message if file wasn't loaded correctly
/////////////////////////////////////////////////////////////////////////////
char *MID_FILE_UI_NameGet(void)
{
  return ui_midifile_name;
}


/////////////////////////////////////////////////////////////////////////////
// Mount the file system
// Should be called from the task which periodically checks the availability
// of SD Card (-> app.c, search for MIOS32_SDCARD_CheckAvailable)
/////////////////////////////////////////////////////////////////////////////
s32 MID_FILE_mount_fs(void)
{
  u32 pstart, psize;
  u8  pactive, ptype;

  // Obtain pointer to first partition on first (only) unit
  pstart = DFS_GetPtnStart(0, sector, 0, &pactive, &ptype, &psize);

  if( pstart == 0xffffffff ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MID_FILE] Cannot find first partition!\n");
#endif
    return -1;
  }

  // check for partition type, if we don't get one of these types, it can be assumed that the partition
  // is located at the first sector instead MBR ("superfloppy format")
  // see also http://mirror.href.com/thestarman/asm/mbr/PartTypes.htm
  if( ptype != 0x04 && ptype != 0x06 && ptype != 0x0b && ptype != 0x0c && ptype != 0x0e ) {
    pstart = 0;
#if DEBUG_VERBOSE_LEVEL >= 2
    DEBUG_MSG("[MID_FILE] Partition 0 start sector %u (invalid type, assuming superfloppy format)\n", pstart);
#endif
  } else {
#if DEBUG_VERBOSE_LEVEL >= 2
    DEBUG_MSG("[MID_FILE] Partition 0 start sector %u active 0x%02x type 0x%02x size %u\n", pstart, pactive, ptype, psize);
#endif
  }

  if (DFS_GetVolInfo(0, sector, pstart, &vi)) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MID_FILE] Error getting volume information\n");
#endif
    return -1;
  }

#if DEBUG_VERBOSE_LEVEL >= 2
  DEBUG_MSG("[MID_FILE] Volume label '%s'\n", vi.label);
  DEBUG_MSG("[MID_FILE] %u sector/s per cluster, %u reserved sector/s, volume total %u sectors.\n", vi.secperclus, vi.reservedsecs, vi.numsecs);
  DEBUG_MSG("[MID_FILE] %u sectors per FAT, first FAT at sector #%u, root dir at #%u.\n",vi.secperfat,vi.fat1,vi.rootdir);
  DEBUG_MSG("[MID_FILE] (For FAT32, the root dir is a CLUSTER number, FAT12/16 it is a SECTOR number)\n");
  DEBUG_MSG("[MID_FILE] %u root dir entries, data area commences at sector #%u.\n",vi.rootentries,vi.dataarea);
  char file_system[20];
  if( vi.filesystem == FAT12 )
    strcpy(file_system, "FAT12");
  else if (vi.filesystem == FAT16)
    strcpy(file_system, "FAT16");
  else if (vi.filesystem == FAT32)
    strcpy(file_system, "FAT32");
  else
    strcpy(file_system, "unknown FS");
  DEBUG_MSG("[MID_FILE] %u clusters (%u bytes) in data area, filesystem IDd as %s\n", vi.numclusters, vi.numclusters * vi.secperclus * SECTOR_SIZE, file_system);
#endif

  if( vi.filesystem != FAT12 && vi.filesystem != FAT16 && vi.filesystem != FAT32 )
    return -1; // unknown file system

  // enable caching
  // attention:  this feature has to be explicitely enabled, as it isn't reentrant
  // and requires to use the same buffer pointer whenever reading a file.
  DFS_CachingEnabledSet(1);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Returns the .mid file next to the given filename
// if filename == NULL, the first .mid file will be returned
// if returned filename == NULL, no .mid file has been found
// dest must point to a 13-byte buffer
/////////////////////////////////////////////////////////////////////////////
char *MID_FILE_FindNext(char *filename)
{
#if DEBUG_VERBOSE_LEVEL >= 1
  DEBUG_MSG("[MID_FILE] Opening directory '%s'\n", MID_FILES_PATH);
#endif

  di.scratch = sector;
  if( DFS_OpenDir(&vi, (uint8_t *)MID_FILES_PATH, &di) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MID_FILE] Error opening directory - try mounting the partition again\n");
#endif

    s32 error;
    if( (error = MID_FILE_mount_fs()) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MID_FILE] mounting failed with status: %d\n", error);
#endif
      strcpy(ui_midifile_name, "no SD-Card");
      return NULL; // directory not found
    }

    if( DFS_OpenDir(&vi, (uint8_t *)MID_FILES_PATH, &di) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MID_FILE] Error opening directory again - giving up!\n");
#endif
      strcpy(ui_midifile_name, "no direct.");
      return NULL; // directory not found
    }
  }

  u8 take_next;
  char search_file[13];
  if( filename == NULL ) {
    take_next = 1;
  } else {
    take_next = 0;
    DFS_CanonicalToDir(search_file, filename);
  }

  while( !DFS_GetNext(&vi, &di, &de) ) {
    if( de.name[0] ) {
      DFS_DirToCanonical((uint8_t *)ui_midifile_name, de.name);
#if DEBUG_VERBOSE_LEVEL >= 2
      DEBUG_MSG("[MID_FILE] file: '%s'\n", ui_midifile_name);
#endif
      if( strncasecmp((char *)&de.name[8], "mid", 3) == 0 ) {
	if( take_next ) {
#if DEBUG_VERBOSE_LEVEL >= 2
	  DEBUG_MSG("[MID_FILE] return it as next file\n");
#endif
	  return ui_midifile_name;
	} else if( strncasecmp((char *)&de.name[0], (char *)search_file, 8) == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 2
	  DEBUG_MSG("[MID_FILE] found file: '%s', searching for next\n", ui_midifile_name);
#endif
	  take_next = 1;
	}
      }
    }
  }

  ui_midifile_name[0] = 0;
  return NULL; // file not found
}


/////////////////////////////////////////////////////////////////////////////
// Open a .mid file with given filename
/////////////////////////////////////////////////////////////////////////////
s32 MID_FILE_open(char *filename)
{
  char filepath[MAX_PATH];
  strncpy(filepath, MID_FILES_PATH, MAX_PATH);
  strncat(filepath, filename, MAX_PATH);
  
  if( DFS_OpenFile(&vi, (uint8_t *)filepath, DFS_READ, sector, &fi)) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MID_FILE] error opening file '%s'!\n", filepath);
#endif
    strcpy(ui_midifile_name, "file error");
    midifile_len = 0;
    return -1;
  }

  // got it
  midifile_pos = 0;
  midifile_len = fi.filelen;

#if DEBUG_VERBOSE_LEVEL >= 1
  DEBUG_MSG("[MID_FILE] opened '%s' of length %u\n", filepath, midifile_len);
#endif

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// reads <len> bytes from the .mid file into <buffer>
// returns number of read bytes
/////////////////////////////////////////////////////////////////////////////
u32 MID_FILE_read(void *buffer, u32 len)
{
  u32 successcount;

  DFS_ReadFile(&fi, sector, buffer, &successcount, len);

  if( successcount != len ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MID_FILE] unexpected read error - DFS cannot access offset %u (len: %u, got: %u)\n", midifile_pos, len, successcount);
#endif
  }
  
  midifile_pos += len; // no error, just failsafe: hold the reference file position, regardless of successcount

  return successcount;
}


/////////////////////////////////////////////////////////////////////////////
// returns 1 if end of file reached
/////////////////////////////////////////////////////////////////////////////
s32 MID_FILE_eof(void)
{
  if( midifile_pos >= midifile_len )
    return 1; // end of file reached

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// sets file pointer to a specific position
// returns -1 if end of file reached
/////////////////////////////////////////////////////////////////////////////
s32 MID_FILE_seek(u32 pos)
{
  midifile_pos = pos;
  if( midifile_pos >= midifile_len )
    return -1; // end of file reached

  DFS_Seek(&fi, pos, sector);

  return 0;
}
