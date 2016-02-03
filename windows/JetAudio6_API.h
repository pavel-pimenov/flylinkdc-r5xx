#ifndef __jetaudio6_api_h
#define __jetaudio6_api_h

#pragma once


//////////////////////////////////////////////////////////////////////////
//
// How to get JetAudio window handle
// -> m_hWndJetAudio = ::FindWindow("COWON Jet-Audio Remocon Class", "Jet-Audio Remote Control");
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Change current mode
// wParam : CMP_CDP (Disc Mode), CMP_DGA (File Mode)
// lParam : None
// -> SendMessage(m_hWndJetAudio, WM_REMOCON_CHANGE_COMPONENT, CMP_DGA, 0); // change to file mode
#define CMP_CDP                 (2)
#define CMP_DGA                 (3)

#define WM_REMOCON_CHANGE_COMPONENT     WM_APP+750


//////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////
#define MAX_REVERB_MODE         (4)

#define MAX_EQ_MODE             (14)

#define MAX_REPEAT_METHOD       (3)

#define MAX_RANDOM_METHOD       (3)

#define MAX_TIME_METHOD         (2)

//////////////////////////////////////////////////////////////////////////
// Command
//////////////////////////////////////////////////////////////////////////

// Message for command. wParam is not used. For lParam, refer to Action commands below
#define WM_REMOCON_SENDCOMMAND          WM_APP+741

//////////////////////////////////////////////////////////////////////////
//
// Action command
//
// Usage -> SendMessage(m_hWndJetAudio, WM_REMOCON_SENDCOMMAND, 0, MAKELPARAM(command, para));
//
//////////////////////////////////////////////////////////////////////////

// Stop Playback
// Para : not used
// Return : N/A
#define JRC_ID_STOP                 5102

// Start playback
// Para : Track Number ( >= 1 ). Use 0 for normal play/pause.
// Return : N/A
#define JRC_ID_PLAY                 5104

// Play previous track
// Para : not used
// Return : N/A
#define JRC_ID_PREV_TRACK           5107

// Play next track
// Para : not used
// Return : N/A
#define JRC_ID_NEXT_TRACK           5108

// RepeatMethod
// Para : desired repeat mode (1 - 3) or 0 for next repeat mode
// Return : current repeat mode
#define JRC_ID_REPEATMODE           5112

// Move 10 seconds backward.
// Para : not used
// Return : N/A
#define JRC_ID_BACKWARD             5115

// Move 10 seconds forward
// Para : not used
// Return : N/A
#define JRC_ID_FORWARD              5116

// RandomMode
// Para : desired random mode (1 - 3) or 0 for next random mode
// Return : current random mode
#define JRC_ID_RANDOMMODE           5117

// Slower playback.
// Para : not used
// Return : N/A
#define JRC_ID_PLAY_SLOW            5128

// Faster playback.
// Para : not used
// Return : N/A
#define JRC_ID_PLAY_FAST            5129

// Volume down.
// Para : not used
// Return : current volume (0 - 32)
#define JRC_ID_VOL_DOWN             5134

// Volume up.
// Para : not used
// Return : current volume (0 - 32)
#define JRC_ID_VOL_UP               5135

// This command is not used anymore.
#define JRC_ID_VIDEOCD              5168

// Exit Jet-Audio.
// Para : not used
// Return : N/A
#define JRC_ID_EXIT                 5171

// Mute sound
// Para : 0(Toggle), 1(ON), -1(OFF)
// Return : current mute status
#define JRC_ID_ATT                  5172

// Change main volume.
// Para : Volume Value (0 - 100)
// Return : current volume
#define JRC_ID_SET_VOLUME           5180

// Change timer.
// Para : second (0 is timer off)
// Return : N/A
#define JRC_ID_SET_TIMER            5181

// Set screen size as 1x.
// Para : not used
// Return : N/A
#define JRC_ID_SCREEN_1X            5188

// Set screen size as 2x.
// Para : not used
// Return : N/A
#define JRC_ID_SCREEN_2X            5189

// Set screen to full screen mode.
// Para : not used
// Return : N/A
#define JRC_ID_SCREEN_FULL          5190

// Change equalizer mode.
// Para : desired eq mode (0 - 13) or -1 for next eq
// Return : current eq mode
#define JRC_ID_CHANGE_EQ            5191

// Move playback position.
// Para : New position (second)
// Return : N/A
#define JRC_ID_SEEK                 5192

// Change Reverb mode
// Para : desired reverb mode (0 - 3) or -1 for next reverb mode
//        To turn off/on, send current reverb mode as parameter
// Return : current reverb mode (0 - 3) or -1 if turned off
#define JRC_ID_CHANGE_SFX_REVERB    5200

// GoUp/Return (for VideoCD/DVD)
// Para : not used
// Return : N/A
#define JRC_ID_GOUP                 5202

// Toggle screen mode between windowed / full screen mode
// Para : not used
// Return : 1 if full screen, 0 if windowed mode
#define JRC_ID_CHANGE_SCREEN_SIZE   5205

// Change Album sequentially
// Para : 1 (next album) or -1 (previous album)
//        For disc mode, change drive
// Return : N/A
#define JRC_ID_CHANGE_ALBUM         5206

// Minimize/Restore Jet-Audio
// Para : not used
// Return : 0:minimized or hidden, 1:normal
#define JRC_ID_MINIMIZE             5701

// Change Wide mode
// Para : 0(Toggle), 1(ON), -1(OFF)
// Return : current status
#define JRC_ID_CHANGE_SFX_WIDE      6000

// Change XBass mode
// Para : 0(Toggle), 1(ON), -1(OFF)
// Return : current status
#define JRC_ID_CHANGE_SFX_XBASS     6001

// Change BBE mode
// Para : 0(Toggle), 1(ON), -1(OFF)
// Return : current status
#define JRC_ID_CHANGE_SFX_BBE       6002

// Change B3D(BBE ViVA) mode
// Para : 0(Toggle), 1(ON), -1(OFF)
// Return : current status
#define JRC_ID_CHANGE_SFX_B3D       6003

// Show Open dialog box
// Para : not used
// Return : N/A
#define JRC_ID_OPEN_FILES           6004

// Eject current CD-ROM drive
// Para : not used
// Return : N/A
#define JRC_ID_EJECT_DRIVE          6005

// Change current CD-ROM drive
// Para : drive letter
// Return : N/A
#define JRC_ID_SELECT_DRIVE         6006

// Change to tray-only mode
// Para : not used
// Return : N/A
#define JRC_ID_GOTO_TRAY            6007

// Load album
// Para : album index
// Return : N/A
#define JRC_ID_SELECT_ALBUM         6008

// Refresh contents of current album
// Para : not used
// Return : N/A
#define JRC_ID_ALBUM_REFRESH        6009

// Sort contents of current album
// Para : not used
// Return : N/A
#define JRC_ID_ALBUM_SORT           6010

// Change Crossfade mode
// Para : 0(Toggle), 1(ON), -1(OFF)
// Return : current status
#define JRC_ID_CHANGE_SFX_XFADE     6011

// Change X-Surround mode
// Para : 0(Toggle), 1(ON), -1(OFF)
// Return : current status
#define JRC_ID_CHANGE_SFX_XSR       6012


//===============================DVD====================================//

// Change DVD Language sequentially
// Para : not used
// Return : N/A
#define JRC_ID_DVD_CHANGE_LANGUAGE  7030

// Change DVD Subtitle sequentially
// Para : not used
// Return : N/A
#define JRC_ID_DVD_CHANGE_SUBTITLE  7031

// Change DVD Angle sequentially
// Para : not used
// Return : N/A
#define JRC_ID_DVD_CHANGE_ANGLE     7032

// Call DVD Title Menu
// Para : not used
// Return : N/A
#define JRC_ID_DVDMENU_TITLE        7050

// Call DVD Root Menu
// Para : not used
// Return : N/A
#define JRC_ID_DVDMENU_ROOT         7051


//////////////////////////////////////////////////////////////////////////
// Status
//////////////////////////////////////////////////////////////////////////

// Message for status. wParam is not used. For lParam, refer to commands below
#define WM_REMOCON_GETSTATUS                (WM_APP+740)

//////////////////////////////////////////////////////////////////////////
//
// Status command
//
// Usage -> return = SendMessage(m_hWndJetAudio, WM_REMOCON_GETSTATUS, 0, command);
//
//////////////////////////////////////////////////////////////////////////

#define GET_STATUS_START                    (0)     // Indicator for component commands

// Return : MCI_MODE_PAUSE(529) or MCI_MODE_PLAY(526) or MCI_MODE_STOP(525)
#define GET_STATUS_STATUS                   (1)

// Return : 0:Elapsed, 1:Remaining
#define GET_STATUS_TIME_MODE                (2)

// Return : 0:Normal, 1:Random, 2:Program
#define GET_STATUS_RANDOM_MODE              (3)

// Return : 0:Normal, 1:Repeat This, 2:Repeat All
#define GET_STATUS_REPEAT_MODE              (4)

// Get track information of current track
// string will be returned using WM_COPYDATA message
#define GET_STATUS_TRACK_INFO               (5)

// Return : current speed
#define GET_STATUS_SPEED                    (6)

// Return : number of tracks in current album
#define GET_STATUS_MAX_TRACK                (7)

// Return : current track number
#define GET_STATUS_CUR_TRACK                (8)

// Return : current playback time
#define GET_STATUS_CUR_TIME                 (9)

// Return : length of current track (in msec)
#define GET_STATUS_MAX_TIME                 (10)

// Get file name of current track
// string will be returned using WM_COPYDATA message
#define GET_STATUS_TRACK_FILENAME           (11)

// Get title info of current track
// string will be returned using WM_COPYDATA message
#define GET_STATUS_TRACK_TITLE              (12)

// Get artist info of current track
// string will be returned using WM_COPYDATA message
#define GET_STATUS_TRACK_ARTIST             (13)

// Get all tracks of current album
// string will be returned using WM_COPYDATA message
#define GET_STATUS_TRACK_LIST               (14)

// Get id of current album
// string will be returned using WM_COPYDATA message
#define GET_STATUS_ALBUMID                  (15)

// Return : current drive letter
#define GET_STATUS_CUR_DRIVE                (16)

// Get all drive letters of CD-ROM drives
// string will be returned using WM_COPYDATA message
#define GET_STATUS_DRIVE_LIST               (17)

// Get all albums of jetaudio
// string will be returned using WM_COPYDATA message
#define GET_STATUS_ALBUM_LIST               (18)

// Get album information of current album
// string will be returned using WM_COPYDATA message
#define GET_STATUS_ALBUM_INFO               (19)

// for VideoCD/DVD
// 0:Normal, 1:VCD Menu, 2:DVD Menu
#define GET_STATUS_MENU_MODE                (20)


// This command is not used anymore
#define GET_STATUS_RUNTIME_FLAG             (101)

// This command is not used anymore
#define GET_STATUS_QUICKWINDOW              (102)


// Get Mute status
// Return : 0:Mute OFF, 1:Mute ON
#define GET_STATUS_ATT                      (110)

// Get EQ mode
// Return : 0 - 13
#define GET_STATUS_EQ_MODE                  (111)

// Get Reverb status
// Return : 0:OFF, 1:ON
#define GET_STATUS_REVERB_FLAG              (120)

// Get Reverb status
// Return : 0 - 4
#define GET_STATUS_REVERB_MODE              (121)

// Get screen mode
// Return : 0:Windowed, 1:Full screen
#define GET_STATUS_SCREEN_MODE              (122)

// Get Wide status
// Return : 0:OFF, 1:ON
#define GET_STATUS_WIDE_FLAG                (123)

// Get X-Bass status
// Return : 0:OFF, 1:ON
#define GET_STATUS_XBASS_FLAG               (124)

// Get BBE status
// Return : 0:OFF, 1:ON
#define GET_STATUS_BBE_FLAG                 (125)

// Get BBE ViVA status
// Return : 0:OFF, 1:ON
#define GET_STATUS_B3D_FLAG                 (126)

// Get Volume
// Return : 0 - 100
#define GET_STATUS_VOLUME                   (127)

// Get current mode
// Return : 1:Disc mode, 0:File mode
#define GET_STATUS_PLAY_MODE                (128)

// Get current video disc playback mode
// for VCD/DVD
#define GET_STATUS_VCD_MODE                 (130)   // 0:None, 1:VCD, 2:DVD

// Get X-Surround status
// Return : 0:OFF, 1:ON
#define GET_STATUS_XSR_FLAG                 (131)

// Get Crossfade status
// Return : 0:OFF, 1:ON
#define GET_STATUS_XFADE_FLAG               (132)

// Get JetAudio's version info
#define GET_STATUS_JETAUDIO_VER1            (995)   // major version number -> 6 for 6.1.7
#define GET_STATUS_JETAUDIO_VER2            (996)   // minor version number -> 1 for 6.1.7
#define GET_STATUS_JETAUDIO_VER3            (997)   // build version number -> 7 for 6.1.7

// Get JetAudio supported API version
// 0 : FirstVersion,
// 1 : DVD & Album Name & Track Name supported
// 2 : JetAudio 5
// 3 : JetAudio 5.2 or later
#define GET_STATUS_SUPPORTED_VER            (999)



//////////////////////////////////////////////////////////////////////////
// WM_COPYDATA is used for receive string information from Jet-Audio
// For more information, refer to example source code
//////////////////////////////////////////////////////////////////////////
#define JRC_COPYDATA_ID_ALBUMNAME           (0x1000)
#define JRC_COPYDATA_ID_DRIVENAME           (0x1001)
#define JRC_COPYDATA_ID_GETVER              (0x1002)
#define JRC_COPYDATA_ID_ALBUM_CHANGED       (0x1003)
#define JRC_COPYDATA_ID_ALBUM_REMOVED       (0x1004)
#define JRC_COPYDATA_ID_ALBUM_ADDED         (0x1004)

#define JRC_COPYDATA_ID_DVD_CHAPTER         (0x2000)
#define JRC_COPYDATA_ID_DVD_LANGUAGE        (0x2001)
#define JRC_COPYDATA_ID_DVD_SUBTITLE        (0x2002)
#define JRC_COPYDATA_ID_DVD_ANGLE           (0x2003)

#define JRC_COPYDATA_ID_TRACK_FILENAME      (0x3000)
#define JRC_COPYDATA_ID_TRACK_TITLE         (0x3001)
#define JRC_COPYDATA_ID_TRACK_ARTIST        (0x3002)
#define JRC_COPYDATA_ID_TRACK_LIST          (0x3003)
#define JRC_COPYDATA_ID_ALBUMID             (0x3004)
#define JRC_COPYDATA_ID_DRIVE_LIST          (0x3005)
#define JRC_COPYDATA_ID_ALBUM_LIST          (0x3006)
#define JRC_COPYDATA_ID_ALBUM_INFO          (0x3007)
#define JRC_COPYDATA_ID_TRACK_INFO          (0x3008)

#endif

