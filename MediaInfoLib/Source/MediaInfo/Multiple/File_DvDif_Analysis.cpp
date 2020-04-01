/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
// Pre-compilation
#include "MediaInfo/PreComp.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Setup.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#if defined(MEDIAINFO_DVDIF_YES) && defined(MEDIAINFO_DVDIF_ANALYZE_YES)
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Multiple/File_DvDif.h"
#include "MediaInfo/MediaInfo_Config_MediaInfo.h"
#include "MediaInfo/MediaInfo_Events_Internal.h"
//---------------------------------------------------------------------------

namespace MediaInfoLib
{

//***************************************************************************
// Analysis
//***************************************************************************

//---------------------------------------------------------------------------
void File_DvDif::Read_Buffer_Continue()
{
    if (!Analyze_Activated)
    {
        if (Config->File_DvDif_Analysis_Get())
            Analyze_Activated=true;
        else
            return;
    }

    //Errors stats
    while (Buffer_Offset+80<=Buffer_Size)
    {
        //Quick search depends of SCT
        switch(Buffer[Buffer_Offset]&0xE0)
        {
            case 0x00 : //SCT=0 (Header)
                {
                    if (!(Buffer[Buffer_Offset  ]==0x00
                       && Buffer[Buffer_Offset+1]==0x00
                       && Buffer[Buffer_Offset+2]==0x00)) // Ignore NULL
                    {
                        if ((Buffer[Buffer_Offset+1]&0xF0)==0x00) //Dseq=0
                        {
                            if ((Buffer[Buffer_Offset+1]&0x08)==0x00) //FSC=0
                            {
                                if ((Buffer[Buffer_Offset+1]&0x04)==0x04) //FSP=1
                                {
                                    //Errors stats update
                                    if (Speed_FrameCount_StartOffset!=(int64u)-1)
                                        Errors_Stats_Update();
                                    Speed_FrameCount_StartOffset=File_Offset+Buffer_Offset;

                                    APT=Buffer[Buffer_Offset+4]&0x07;
                                    DSF=(Buffer[Buffer_Offset+3]&0x80)?true:false;
                                    Dseq_Old=DSF?11:9;
                                }
                                else
                                    FSP_WasNotSet=true;
                            }
                            else
                                FSC_WasSet=true;
                        }
                    }
                }
            case 0x20 : //SCT=1 (Subcode)
                {
                    if (!FSC && ssyb_AP3!=(int8u)-1)
                        ssyb_AP3=(Buffer[Buffer_Offset+3]>>4)&0x7;
                    for (size_t Pos=0; Pos<48; Pos+=8)
                    {
                        int8u PackType=Buffer[Buffer_Offset+3+Pos+3];
                        //dv_timecode
                        if (PackType==0x13) //Pack type=0x13 (dv_timecode)
                        {
                            bool  DropFrame                 =( Buffer[Buffer_Offset+3+Pos+3+1]&0x40)?true:false;
                            int8u Frames                    =((Buffer[Buffer_Offset+3+Pos+3+1]&0x30)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+3+1]&0x0F)   )   ;
                            int8u Seconds                   =((Buffer[Buffer_Offset+3+Pos+3+2]&0x70)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+3+2]&0x0F))      ;
                            int8u Minutes                   =((Buffer[Buffer_Offset+3+Pos+3+3]&0x70)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+3+3]&0x0F)   )   ;
                            int8u Hours                     =((Buffer[Buffer_Offset+3+Pos+3+4]&0x30)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+3+4]&0x0F)   )   ;

                            if (Frames ==0x00
                             && Seconds==0x00
                             && Minutes==0x00
                             && Hours  ==0x00
                             && Buffer[Buffer_Offset+3+Pos+3+1]==0x00
                             && Buffer[Buffer_Offset+3+Pos+3+2]==0x00
                             && Buffer[Buffer_Offset+3+Pos+3+3]==0x00
                             && Buffer[Buffer_Offset+3+Pos+3+4]==0x00
                             )
                            {
                                Frames =45;
                                Seconds=85;
                                Minutes=85;
                                Hours  =45;
                            }
                            if (Frames !=45
                             && Seconds!=85
                             && Minutes!=85
                             && Hours  !=45) //If not disabled
                            {
                                if (Speed_TimeCode_Current.IsValid
                                 && (Speed_TimeCode_Current.Time.DropFrame !=DropFrame
                                  || Speed_TimeCode_Current.Time.Frames    !=Frames
                                  || Speed_TimeCode_Current.Time.Seconds   !=Seconds
                                  || Speed_TimeCode_Current.Time.Minutes   !=Minutes
                                  || Speed_TimeCode_Current.Time.Hours     !=Hours))
                                {
                                    Speed_TimeCode_Current.MultipleValues=true; //There are 2+ different values
                                }
                                else if (!Speed_TimeCode_Current.IsValid && !Speed_TimeCode_Current.MultipleValues)
                                {
                                    Speed_TimeCode_Current.Time.DropFrame=DropFrame;
                                    Speed_TimeCode_Current.Time.Frames   =Frames;
                                    Speed_TimeCode_Current.Time.Seconds  =Seconds;
                                    Speed_TimeCode_Current.Time.Minutes  =Minutes;
                                    Speed_TimeCode_Current.Time.Hours    =Hours;
                                    Speed_TimeCode_Current.IsValid       =true;
                                }
                            }
                        }

                        //video_recdate
                        if (PackType==0x62) //Pack type=0x62 (video_rectime)
                        {
                            int8u Days                      =((Buffer[Buffer_Offset+3+Pos+2]&0x30)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+2]&0x0F)   )   ;
                            int8u Months                    =((Buffer[Buffer_Offset+3+Pos+3]&0x10)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+3]&0x0F)   )   ;
                            int8u Years                     =((Buffer[Buffer_Offset+3+Pos+4]&0xF0)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+4]&0x0F)   )   ;
                            if (Months<=12
                             && Days  <=31)
                            {
                                if (Speed_RecDate_Current.IsValid
                                 && Speed_RecDate_Current.Days      !=Days
                                 && Speed_RecDate_Current.Months     !=Months
                                 && Speed_RecDate_Current.Years     !=Years)
                                {
                                    Speed_RecDate_Current.MultipleValues=true; //There are 2+ different values
                                }
                                else if (!Speed_RecTime_Current.MultipleValues)
                                {
                                    Speed_RecDate_Current.Days     =Days;
                                    Speed_RecDate_Current.Months   =Months;
                                    Speed_RecDate_Current.Years    =Years;
                                    Speed_RecDate_Current.IsValid  =true;
                                }
                            }
                        }

                        //video_rectime
                        if (PackType==0x63) //Pack type=0x63 (video_rectime)
                        {
                            int8u Frames                    =((Buffer[Buffer_Offset+3+Pos+1]&0x30)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+1]&0x0F)   )   ;
                            int8u Seconds                   =((Buffer[Buffer_Offset+3+Pos+2]&0x70)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+2]&0x0F))      ;
                            int8u Minutes                   =((Buffer[Buffer_Offset+3+Pos+3]&0x70)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+3]&0x0F)   )   ;
                            int8u Hours                     =((Buffer[Buffer_Offset+3+Pos+4]&0x30)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+4]&0x0F)   )   ;
                            if (Seconds<61
                             && Minutes<60
                             && Hours  <24) //If not disabled
                            {
                                if (Speed_RecTime_Current.IsValid
                                 && Speed_RecTime_Current.Time.Frames    !=Frames
                                 && Speed_RecTime_Current.Time.Seconds   !=Seconds
                                 && Speed_RecTime_Current.Time.Minutes   !=Minutes
                                 && Speed_RecTime_Current.Time.Hours     !=Hours)
                                {
                                    Speed_RecTime_Current.MultipleValues=true; //There are 2+ different values
                                }
                                else if (!Speed_RecTime_Current.MultipleValues)
                                {
                                    Speed_RecTime_Current.Time.Frames   =Frames;
                                    Speed_RecTime_Current.Time.Seconds  =Seconds;
                                    Speed_RecTime_Current.Time.Minutes  =Minutes;
                                    Speed_RecTime_Current.Time.Hours    =Hours;
                                    Speed_RecTime_Current.IsValid  =true;
                                }
                            }
                        }
                    }
                }
                break;

            case 0x40 : //SCT=2 (VAUX)
                {
                    for (size_t Pos=0; Pos<15*5; Pos+=5)
                    {
                        int8u PackType=Buffer[Buffer_Offset+3+Pos];
                        //video_source
                        if (PackType==0x60) //Pack type=0x60 (video_source)
                        {
                            system=(Buffer[Buffer_Offset+3+Pos+3]&0x20)==0x20?true:false;
                            video_source_stype=Buffer[Buffer_Offset+3+Pos+3]&0x1F;
                        }
                        //video_sourcecontrol
                        if (PackType==0x61)
                        {
                            aspect=(Buffer[Buffer_Offset+3+Pos+2]&0x7);
                        }

                        //video_recdate
                        if (PackType==0x62) //Pack type=0x62 (video_rectime)
                        {
                            int8u Days                      =((Buffer[Buffer_Offset+3+Pos+2]&0x30)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+2]&0x0F)   )   ;
                            int8u Months                    =((Buffer[Buffer_Offset+3+Pos+3]&0x10)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+3]&0x0F)   )   ;
                            int8u Years                     =((Buffer[Buffer_Offset+3+Pos+4]&0xF0)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+4]&0x0F)   )   ;
                            if (Months<=12
                             && Days  <=31)
                            {
                                if (Speed_RecDate_Current.IsValid
                                 && Speed_RecDate_Current.Days      !=Days
                                 && Speed_RecDate_Current.Months     !=Months
                                 && Speed_RecDate_Current.Years     !=Years)
                                {
                                    Speed_RecDate_Current.MultipleValues=true; //There are 2+ different values
                                }
                                else if (!Speed_RecTime_Current.MultipleValues)
                                {
                                    Speed_RecDate_Current.Days     =Days;
                                    Speed_RecDate_Current.Months   =Months;
                                    Speed_RecDate_Current.Years    =Years;
                                    Speed_RecDate_Current.IsValid  =true;
                                }
                            }
                        }

                        //video_rectime
                        if (PackType==0x63) //Pack type=0x63 (video_rectime)
                        {
                            int8u Frames                    =((Buffer[Buffer_Offset+3+Pos+1]&0x30)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+1]&0x0F)   )   ;
                            int8u Seconds                   =((Buffer[Buffer_Offset+3+Pos+2]&0x70)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+2]&0x0F))      ;
                            int8u Minutes                   =((Buffer[Buffer_Offset+3+Pos+3]&0x70)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+3]&0x0F)   )   ;
                            int8u Hours                     =((Buffer[Buffer_Offset+3+Pos+4]&0x30)>>4)*10
                                                           + ((Buffer[Buffer_Offset+3+Pos+4]&0x0F)   )   ;
                            if (Seconds!=85
                             && Minutes!=85
                             && Hours  !=45) //If not disabled
                            {
                                if (Speed_RecTime_Current.IsValid
                                 && Speed_RecTime_Current.Time.Frames    !=Frames
                                 && Speed_RecTime_Current.Time.Seconds   !=Seconds
                                 && Speed_RecTime_Current.Time.Minutes   !=Minutes
                                 && Speed_RecTime_Current.Time.Hours     !=Hours)
                                {
                                    Speed_RecTime_Current.MultipleValues=true; //There are 2+ different values
                                }
                                else if (!Speed_RecTime_Current.MultipleValues)
                                {
                                    Speed_RecTime_Current.Time.Frames   =Frames;
                                    Speed_RecTime_Current.Time.Seconds  =Seconds;
                                    Speed_RecTime_Current.Time.Minutes  =Minutes;
                                    Speed_RecTime_Current.Time.Hours    =Hours;
                                    Speed_RecTime_Current.IsValid  =true;
                                }
                            }
                        }
                    }
                }
                break;

            case 0x60 : //SCT=3 (Audio)
                {
                    //audio_source
                    if (Buffer[Buffer_Offset+3+0]==0x50) //audio_source
                    {
                              QU_FSC    =(Buffer[Buffer_Offset+1  ]&0x08)?true:false; //FSC
                              QU_System =(Buffer[Buffer_Offset+3+3]&0x20)?true:false; //50/60

                        uint8_t Dseq    =Buffer[Buffer_Offset+1]>>4;
                        int8u AUDIO_MODE=Buffer[Buffer_Offset+3+2]&0x0F;
                              SMP       =(Buffer[Buffer_Offset+3+4]>>3)&0x07;
                              QU        =Buffer[Buffer_Offset+3+4]&0x07;

                        size_t ChannelGroup=(QU_FSC?1:0);
                        if (ChannelGroup>=audio_source_mode.size())
                            audio_source_mode.resize(ChannelGroup+1);
                        if (audio_source_mode[ChannelGroup].empty())
                            audio_source_mode[ChannelGroup].resize(Dseq_Count, (int8u)-1);
                        audio_source_mode[ChannelGroup][Dseq]=AUDIO_MODE;
                    }

                    //audio_source_control
                    if (Buffer[Buffer_Offset+3+0]==0x51) //audio_source_control
                    {
                        REC_ST =(Buffer[Buffer_Offset+3+2]&0x80)?true:false;
                        REC_END=(Buffer[Buffer_Offset+3+2]&0x40)?true:false;
                        REC_IsValid=true;
                    }

                    //audio_recdate
                    if (Buffer[Buffer_Offset+3+0]==0x52) //Pack type=0x52 (audio_rectime)
                    {
                        int8u Days                      =((Buffer[Buffer_Offset+3+2]&0x30)>>4)*10
                                                       + ((Buffer[Buffer_Offset+3+2]&0x0F)   )   ;
                        int8u Months                    =((Buffer[Buffer_Offset+3+3]&0x10)>>4)*10
                                                       + ((Buffer[Buffer_Offset+3+3]&0x0F)   )   ;
                        int8u Years                     =((Buffer[Buffer_Offset+3+4]&0xF0)>>4)*10
                                                       + ((Buffer[Buffer_Offset+3+4]&0x0F)   )   ;
                        if (Months<=12
                         && Days  <=31)
                        {
                            if (Speed_RecDate_Current.IsValid
                             && Speed_RecDate_Current.Days      !=Days
                             && Speed_RecDate_Current.Months     !=Months
                             && Speed_RecDate_Current.Years     !=Years)
                            {
                                Speed_RecDate_Current.MultipleValues=true; //There are 2+ different values
                            }
                            else if (!Speed_RecTime_Current.MultipleValues)
                            {
                                Speed_RecDate_Current.Days     =Days;
                                Speed_RecDate_Current.Months   =Months;
                                Speed_RecDate_Current.Years    =Years;
                                Speed_RecDate_Current.IsValid  =true;
                            }
                        }
                    }

                    //audio_rectime
                    if (Buffer[Buffer_Offset+3+0]==0x53) //Pack type=0x53 (audio_rectime)
                    {
                        int8u Frames                    =((Buffer[Buffer_Offset+3+1]&0x30)>>4)*10
                                                       + ((Buffer[Buffer_Offset+3+1]&0x0F)   )   ;
                        int8u Seconds                   =((Buffer[Buffer_Offset+3+2]&0x70)>>4)*10
                                                       + ((Buffer[Buffer_Offset+3+2]&0x0F))      ;
                        int8u Minutes                   =((Buffer[Buffer_Offset+3+3]&0x70)>>4)*10
                                                       + ((Buffer[Buffer_Offset+3+3]&0x0F)   )   ;
                        int8u Hours                     =((Buffer[Buffer_Offset+3+4]&0x30)>>4)*10
                                                       + ((Buffer[Buffer_Offset+3+4]&0x0F)   )   ;
                        if (Seconds!=85
                         && Minutes!=85
                         && Hours  !=45) //If not disabled
                        {
                            if (Speed_RecTime_Current.IsValid
                             && Speed_RecTime_Current.Time.Frames    !=Frames
                             && Speed_RecTime_Current.Time.Seconds   !=Seconds
                             && Speed_RecTime_Current.Time.Minutes   !=Minutes
                             && Speed_RecTime_Current.Time.Hours     !=Hours)
                            {
                                Speed_RecTime_Current.MultipleValues=true; //There are 2+ different values
                            }
                            else if (!Speed_RecTime_Current.MultipleValues)
                            {
                                Speed_RecTime_Current.Time.Frames   =Frames;
                                Speed_RecTime_Current.Time.Seconds  =Seconds;
                                Speed_RecTime_Current.Time.Minutes  =Minutes;
                                Speed_RecTime_Current.Time.Hours    =Hours;
                                Speed_RecTime_Current.IsValid  =true;
                            }
                        }
                    }

                    //Audio errors
                    if (Buffer[Buffer_Offset+8]==0x80)
                    {
                        bool Contains_8000=true;
                        bool Contains_800800=true;
                        for (size_t i=8; i<80; i+=2)
                            if (Buffer[Buffer_Offset+i]  !=0x80
                             || Buffer[Buffer_Offset+i+1]!=0x00)
                            {
                                Contains_8000=false;
                                break;
                            }
                        for (size_t i=8; i<80; i+=3)
                            if (Buffer[Buffer_Offset+i]  !=0x80
                             || Buffer[Buffer_Offset+i+1]!=0x80
                             || Buffer[Buffer_Offset+i+2]!=0x00)
                            {
                                Contains_800800=false;
                                break;
                            }
                        if ((/*QU==0 &&*/ Contains_8000)    //16-bit 0x8000
                         || (QU==1 && Contains_800800)  //12-bit 0x800
                         || (QU==(int8u)-1 && (Contains_8000 || Contains_800800))) //In case of QU is not already detected
                        {
                            uint8_t Channel=(Buffer[Buffer_Offset+1]&0x08)?1:0; //FSC
                            uint8_t Dseq=Buffer[Buffer_Offset+1]>>4;
                            if (Channel>=Audio_Errors.size())
                                Audio_Errors.resize(Channel+1);
                            if (Audio_Errors[Channel].empty())
                                Audio_Errors[Channel].resize(Dseq_Count);
                            Audio_Errors[Channel][Dseq]++;
                        }
                    }
                }
                break;

            case 0x80 : //SCT=4 (Video)
                {
                    //Speed_Arb_Current
                    int8u Value=Buffer[Buffer_Offset+0]&0x0F;
                    Speed_Arb_Current.Value_Counters[Value]++;
                    if (Value==0xF)
                    {
                        if (!Speed_Arb_Current.IsValid)
                        {
                            Speed_Arb_Current.Value  =0xF;
                            Speed_Arb_Current.IsValid=true;
                        }
                    }
                    else
                    {
                        if (Speed_Arb_Current.IsValid
                         && Speed_Arb_Current.Value!=0xF
                         && Speed_Arb_Current.Value!=Value)
                            Speed_Arb_Current.MultipleValues=true; //There are 2+ different values
                        else if (!Speed_Arb_Current.MultipleValues)
                        {
                            Speed_Arb_Current.Value  =Value;
                            Speed_Arb_Current.IsValid=true;
                        }
                    }

                    //STA
                    if (Buffer[Buffer_Offset+3]&0xF0)
                    {
                        if (video_source_stype!=(int8u)-1)
                        {
                            int8u STA_Error=Buffer[Buffer_Offset+3]>>4;

                            if (Video_STA_Errors.empty())
                                Video_STA_Errors.resize(16);
                            Video_STA_Errors[STA_Error]++;

                            if (Video_STA_Errors_ByDseq.empty())
                                Video_STA_Errors_ByDseq.resize(16*16); // Per Dseq and STA
                            uint8_t Dseq=Buffer[Buffer_Offset+1]>>4;
                            Video_STA_Errors_ByDseq[(Dseq<<4)|STA_Error]++;
                        }
                    }
                }
                break;
        }

        //Coherency test
        if (Buffer[Buffer_Offset  ]==0x00
         && Buffer[Buffer_Offset+1]==0x00
         && Buffer[Buffer_Offset+2]==0x00)
           Speed_Contains_NULL++;

        Buffer_Offset+=80;
    }

    if (!Status[IsAccepted])
        File__Analyze::Buffer_Offset=0;
    Config->State_Set(((float)File_Offset)/File_Size);
}

void File_DvDif::Errors_Stats_Update()
{
    if (!Analyze_Activated)
    {
        if (Config->File_DvDif_Analysis_Get())
            Analyze_Activated=true;
        else
            return;
    }

    Ztring Errors_Stats_Line;
    {
        bool Errors_AreDetected=false;
        bool Infos_AreDetected=false;
        bool Arb_AreDetected=false;

        bitset<ChannelGroup_Count*2> NewChannelInfo;
        if (QU!=(int8u)-1)
        {
            const size_t DseqSpan=QU_System?6:5;
            for (size_t ChannelGroup=0; ChannelGroup<ChannelGroup_Count; ChannelGroup++)
            {
                /* // To be used if we want detect CH1 vs CH2 (especially for 12-bit)
                for (size_t Channel=0; Channel<2; Channel++)
                {
                    const size_t Dseq_Begin=Channel?DseqSpan:0;
                    const size_t Dseq_End=Dseq_Begin+DseqSpan;
                    for (int8u Dseq=Dseq_Begin; Dseq<Dseq_End; Dseq++)
                    {
                        if (ChannelGroup<audio_source_mode.size() && !audio_source_mode[ChannelGroup].empty() && audio_source_mode[ChannelGroup][Dseq]!=(int8u)-1)
                        {
                            NewAudioChannelCount++;
                            break;
                        }
                    }
                }
                */
                const size_t Dseq_Begin=0;
                const size_t Dseq_End=Dseq_Begin+DseqSpan*2;
                for (int8u Dseq=Dseq_Begin; Dseq<Dseq_End; Dseq++)
                {
                    if (ChannelGroup<audio_source_mode.size() && !audio_source_mode[ChannelGroup].empty() && audio_source_mode[ChannelGroup][Dseq]!=(int8u)-1)
                    {
                        NewChannelInfo.set(ChannelGroup*2);
                        NewChannelInfo.set(ChannelGroup*2+1);
                        break;
                    }
                }
            }
            
            // Update channel count only if we not 0 and if we find no error
            if (NewChannelInfo.count())
                ChannelInfo=NewChannelInfo;
        }

        EVENT_BEGIN(DvDif, Change, 0)
            Event.StreamOffset=Speed_FrameCount_StartOffset;
            switch (video_source_stype)
            {
                case 0x00 :
                case 0x04 :
                            Event.Width=720;
                            Event.Height=system?576:480;
                            break;
                case 0x14 :
                case 0x15 :
                            Event.Width=system?1440:1280;
                            Event.Height=video_source_stype==0x14?1080:1035;
                            break;
                case 0x18 :
                            Event.Width=960;
                            Event.Height=720;
                            break;
                default   : 
                            Event.Width=0;
                            Event.Height=0;
            }
            if (video_source_stype!=(int8u)-1)
            {
                if (!FSC_WasSet) //Original DV 25 Mbps
                {
                    if (system==false) //NTSC
                    {
                        switch (video_source_stype)
                        {
                            case  0 : Event.VideoChromaSubsampling=0; break; //NTSC 25 Mbps
                            default : Event.VideoChromaSubsampling=(int32u)-1;
                        }
                    }
                    else //PAL
                    {
                        switch (video_source_stype)
                        {
                            case  0 : if (APT==0)
                                        Event.VideoChromaSubsampling=1;      //PAL 25 Mbps (IEC 61834)
                                      else
                                        Event.VideoChromaSubsampling=0;      //PAL 25 Mbps (SMPTE 314M)
                                      break;
                            default : Event.VideoChromaSubsampling=(int32u)-1;
                        }
                    }
                }
                else //DV 50 Mbps and 100 Mbps
                    Event.VideoChromaSubsampling=2;
            }
            else
                Event.VideoChromaSubsampling=(int32u)-1;
            Event.VideoScanType=(int32u)-1;
            switch (aspect)
            {
                case 0 :
                case 4 :
                        Event.VideoRatio_N=4;
                        Event.VideoRatio_D=3;
                        break;
                case 1 :
                case 2 :
                        Event.VideoRatio_N=16;
                        Event.VideoRatio_D=9;
                        break;
                case 7 :
                        switch (ssyb_AP3)
                            {
                                case 0 :
                                case 4 :
                                        Event.VideoRatio_N=16;
                                        Event.VideoRatio_D=9;
                                        break;
                                case 1 :
                                case 2 :
                                        Event.VideoRatio_N=4;
                                        Event.VideoRatio_D=3;
                                        break;
                                default: ;
                                        Event.VideoRatio_N=0;
                                        Event.VideoRatio_D=0;
                            }
                default: ;
                        Event.VideoRatio_N=0;
                        Event.VideoRatio_D=0;
            }
            if (video_source_stype!=(int8u)-1)
            {
                Event.VideoRate_N=system?25:30000;
                if (video_source_stype==0x18)
                    Event.VideoRate_N*=2;
                Event.VideoRate_D=system?1:1001;
            }
            else
            {
                Event.VideoRate_N=0;
                Event.VideoRate_D=0;
            }
            if (ChannelInfo.count())
            {
                switch(SMP)
                {
                    case 0:Event.AudioRate_N=48000; break;
                    case 1:Event.AudioRate_N=44100; break;
                    case 2:Event.AudioRate_N=32000; break;
                    default:Event.AudioRate_N=0;
                }
                Event.AudioRate_D=Event.AudioRate_N?1:0;
                ;
                switch(QU)
                {
                    case 0:
                            Event.AudioChannels=ChannelInfo.count();
                            Event.AudioBitDepth=16;
                            break;
                    case 1:
                            Event.AudioChannels=ChannelInfo.count()*2;
                            Event.AudioBitDepth=12;
                            break;
                    default:
                            Event.AudioChannels=0;
                            Event.AudioBitDepth=0;
                }
            }
            else
            {
                Event.AudioRate_N=0;
                Event.AudioRate_D=0;
                Event.AudioChannels=0;
                Event.AudioBitDepth=0;
            }
        EVENT_END()
        #if MEDIAINFO_EVENTS
            //Demux
            struct MediaInfo_Event_DvDif_Analysis_Frame_0 Event;
            Event.EventCode=MediaInfo_EventCode_Create(MediaInfo_Parser_DvDif, MediaInfo_Event_DvDif_Analysis_Frame, 0);
            Event.TimeCode=0;
            Event.RecordedDateTime1=0;
            Event.RecordedDateTime2=0;
            Event.Arb=0;
            Event.Verbosity=0;
            Event.Errors=NULL;
        #endif //MEDIAINFO_EVENTS

        //Framerate computing
        float64 FrameRate=29.970;
        if (video_source_stype!=(int8u)-1)
            FrameRate=system?25.000:29.970;
        else
            FrameRate=DSF?25.000:29.970;
        if (FrameRate==29.970 && Speed_TimeCode_Current.IsValid && !Speed_TimeCode_Current.Time.DropFrame)
            FrameRate=30.000;

        //Frame number
        Ztring Frame_Number_Padded=Ztring::ToZtring(Speed_FrameCount);
        if (Frame_Number_Padded.size()<8)
            Frame_Number_Padded.insert(0, 8-Frame_Number_Padded.size(), __T(' '));
        Errors_Stats_Line+=Frame_Number_Padded;
        Errors_Stats_Line+=__T('\t');

        //Time Offset
        float64 Time_Offset=(Speed_FrameCount)*1000/FrameRate;
        Errors_Stats_Line+=Ztring().Duration_From_Milliseconds((int64u)Time_Offset);
        Errors_Stats_Line+=__T('\t');

        //Timecode
        if (Speed_TimeCode_Current.IsValid)
        {
            Speed_TimeCodeZ_Last=Speed_TimeCodeZ_Current;
            Speed_TimeCodeZ_Current.clear();
            Speed_TimeCodeZ_Current.reserve(12);
            Speed_TimeCodeZ_Current.reserve(11);
            Speed_TimeCodeZ_Current+=__T('0')+Speed_TimeCode_Current.Time.Hours  /10;
            Speed_TimeCodeZ_Current+=__T('0')+Speed_TimeCode_Current.Time.Hours  %10;
            Speed_TimeCodeZ_Current+=__T(':');
            Speed_TimeCodeZ_Current+=__T('0')+Speed_TimeCode_Current.Time.Minutes/10;
            Speed_TimeCodeZ_Current+=__T('0')+Speed_TimeCode_Current.Time.Minutes%10;
            Speed_TimeCodeZ_Current+=__T(':');
            Speed_TimeCodeZ_Current+=__T('0')+Speed_TimeCode_Current.Time.Seconds/10;
            Speed_TimeCodeZ_Current+=__T('0')+Speed_TimeCode_Current.Time.Seconds%10;
            Speed_TimeCodeZ_Current+=(!DSF && Speed_TimeCode_Current.Time.DropFrame)?__T(';'):__T(':');
            Speed_TimeCodeZ_Current+=__T('0')+Speed_TimeCode_Current.Time.Frames /10;
            Speed_TimeCodeZ_Current+=__T('0')+Speed_TimeCode_Current.Time.Frames %10;
            Errors_Stats_Line+=Speed_TimeCodeZ_Current;
            if (Speed_TimeCodeZ.empty())
            {
                Speed_TimeCodeZ.resize(1);
                Speed_TimeCodeZ[0].First.FramePos=Speed_FrameCount;
                Speed_TimeCodeZ[0].First.TimeCode=Speed_TimeCodeZ_Current;
            }
            if (Speed_TimeStampsZ.empty())
            {
                Speed_TimeStampsZ.resize(1);
                Speed_TimeStampsZ[0].First.FramePos=Speed_FrameCount;
                Speed_TimeStampsZ[0].First.Time+=Ztring().Duration_From_Milliseconds((int64u)Time_Offset);
            }
            if (Speed_TimeStampsZ[0].First.FramePos==Speed_FrameCount)
                Speed_TimeStampsZ[0].First.TimeCode=Speed_TimeCodeZ_Current;
            #if MEDIAINFO_EVENTS
                int32u Seconds=Speed_TimeCode_Current.Time.Hours  *60*60
                             + Speed_TimeCode_Current.Time.Minutes   *60
                             + Speed_TimeCode_Current.Time.Seconds      ;
                Event.TimeCode|=Seconds<<8;
                Event.TimeCode|=(!DSF && Speed_TimeCode_Current.Time.DropFrame)<<7;
                Event.TimeCode|=Speed_TimeCode_Current.Time.Frames;
            #endif //MEDIAINFO_EVENTS
        }
        else
        {
            Errors_Stats_Line+=__T("XX:XX:XX:XX");
            #if MEDIAINFO_EVENTS
                Event.TimeCode|=0x7FFFF<<8;
                //Event.TimeCode|=Speed_TimeCode_Current.Time.DropFrame<<7;
                Event.TimeCode|=0x3F;
            #endif //MEDIAINFO_EVENTS
        }
        Errors_Stats_Line+=__T('\t');

        //Timecode order coherency
        if (!Speed_TimeCode_IsValid && Speed_TimeCode_Current.IsValid
         && (Speed_TimeCode_Current.Time.Hours!=0
          || Speed_TimeCode_Current.Time.Minutes!=0
          || Speed_TimeCode_Current.Time.Seconds!=0
          || Speed_TimeCode_Current.Time.Frames!=0))
            Speed_TimeCode_IsValid=true;
        bool TimeCode_Disrupted=false;
        if (Speed_TimeCode_IsValid && Speed_TimeCode_Current.IsValid && Speed_TimeCode_Last.IsValid
         && Speed_TimeCode_Current.Time.Frames ==Speed_TimeCode_Last.Time.Frames
         && Speed_TimeCode_Current.Time.Seconds==Speed_TimeCode_Last.Time.Seconds
         && Speed_TimeCode_Current.Time.Minutes==Speed_TimeCode_Last.Time.Minutes
         && Speed_TimeCode_Current.Time.Hours  ==Speed_TimeCode_Last.Time.Hours)
        {
            Errors_Stats_Line+=__T('R');
            #if MEDIAINFO_EVENTS
                Event.TimeCode|=1<<31;
            #endif //MEDIAINFO_EVENTS
            if (Speed_TimeCode_Current.Time.Hours
             || Speed_TimeCode_Current.Time.Seconds
             || Speed_TimeCode_Current.Time.Minutes)
                Errors_AreDetected=true;
        }
        else if (Speed_TimeCode_IsValid && Speed_TimeCode_Current.IsValid && Speed_TimeCode_Current_Theory.IsValid
              && (   Speed_TimeCode_Current.Time.Frames !=Speed_TimeCode_Current_Theory.Time.Frames
                  || Speed_TimeCode_Current.Time.Seconds!=Speed_TimeCode_Current_Theory.Time.Seconds
                  || Speed_TimeCode_Current.Time.Minutes!=Speed_TimeCode_Current_Theory.Time.Minutes
                  || Speed_TimeCode_Current.Time.Hours  !=Speed_TimeCode_Current_Theory.Time.Hours))
        {
            size_t Speed_TimeCodeZ_Pos=Speed_TimeCodeZ.size();
            Speed_TimeCodeZ.resize(Speed_TimeCodeZ_Pos+1);
            Speed_TimeCodeZ[Speed_TimeCodeZ_Pos].First.FramePos=Speed_FrameCount;
            Speed_TimeCodeZ[Speed_TimeCodeZ_Pos].First.TimeCode=Speed_TimeCodeZ_Current;
            Speed_TimeCodeZ[Speed_TimeCodeZ_Pos-1].Last.FramePos=Speed_FrameCount;
            Speed_TimeCodeZ[Speed_TimeCodeZ_Pos-1].Last.TimeCode=Speed_TimeCodeZ_Last;

            Errors_Stats_Line+=__T('N');
            #if MEDIAINFO_EVENTS
                Event.TimeCode|=1<<30;
            #endif //MEDIAINFO_EVENTS
            Speed_TimeCode_Current_Theory=Speed_TimeCode_Current;
            TimeCode_Disrupted=true;
            Errors_AreDetected=true;
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');

        //RecDate/RecTime
        if (Speed_RecDate_Current.IsValid)
        {
            Speed_RecDateZ_Last=Speed_RecDateZ_Current;
            Speed_RecDateZ_Current.clear();
            Speed_RecDateZ_Current.reserve(10);
            Speed_RecDateZ_Current+=Speed_RecDate_Current.Years<75?__T("20"):__T("19");
            Speed_RecDateZ_Current+=__T('0')+Speed_RecDate_Current.Years  /10;
            Speed_RecDateZ_Current+=__T('0')+Speed_RecDate_Current.Years  %10;
            Speed_RecDateZ_Current+=__T('-');
            Speed_RecDateZ_Current+=__T('0')+Speed_RecDate_Current.Months /10;
            Speed_RecDateZ_Current+=__T('0')+Speed_RecDate_Current.Months %10;
            Speed_RecDateZ_Current+=__T('-');
            Speed_RecDateZ_Current+=__T('0')+Speed_RecDate_Current.Days   /10;
            Speed_RecDateZ_Current+=__T('0')+Speed_RecDate_Current.Days   %10;
            Errors_Stats_Line+=Speed_RecDateZ_Current;
            if (Speed_RecZ.empty())
            {
                Speed_RecZ.resize(1);
                Speed_RecZ[0].First.FramePos=Speed_FrameCount;
                Speed_RecZ[0].First.Date=Speed_RecDateZ_Current;
            }
            if (Speed_TimeStampsZ.empty())
            {
                Speed_TimeStampsZ.resize(1);
                Speed_TimeStampsZ[0].First.FramePos=Speed_FrameCount;
                Speed_TimeStampsZ[0].First.Time+=Ztring().Duration_From_Milliseconds((int64u)Time_Offset);
            }
            if (Speed_TimeStampsZ[0].First.FramePos==Speed_FrameCount)
                Speed_TimeStampsZ[0].First.Date=Speed_RecDateZ_Current;
            #if MEDIAINFO_EVENTS
                Event.RecordedDateTime1|=Speed_RecDate_Current.Years<<17;
                Event.RecordedDateTime2|=Speed_RecDate_Current.Months<<12;
                Event.RecordedDateTime2|=Speed_RecDate_Current.Days<<8;
            #endif //MEDIAINFO_EVENTS
        }
        else
        {
            Errors_Stats_Line+=__T("XXXX-XX-XX");
            #if MEDIAINFO_EVENTS
                Event.RecordedDateTime1|=0x7F<<17;
                Event.RecordedDateTime2|=0x0F<<12;
                Event.RecordedDateTime2|=0x1F<<8;
            #endif //MEDIAINFO_EVENTS
        }
        Errors_Stats_Line+=__T(" ");
        if (Speed_RecTime_Current.IsValid)
        {
            Speed_RecTimeZ_Last=Speed_RecTimeZ_Current;
            Speed_RecTimeZ_Current.clear();
            Speed_RecTimeZ_Current.reserve(12);
            Speed_RecTimeZ_Current+=__T('0')+Speed_RecTime_Current.Time.Hours  /10;
            Speed_RecTimeZ_Current+=__T('0')+Speed_RecTime_Current.Time.Hours  %10;
            Speed_RecTimeZ_Current+=__T(':');
            Speed_RecTimeZ_Current+=__T('0')+Speed_RecTime_Current.Time.Minutes/10;
            Speed_RecTimeZ_Current+=__T('0')+Speed_RecTime_Current.Time.Minutes%10;
            Speed_RecTimeZ_Current+=__T(':');
            Speed_RecTimeZ_Current+=__T('0')+Speed_RecTime_Current.Time.Seconds/10;
            Speed_RecTimeZ_Current+=__T('0')+Speed_RecTime_Current.Time.Seconds%10;
            #if MEDIAINFO_EVENTS
                int32u Seconds=Speed_RecTime_Current.Time.Hours  *60*60
                             + Speed_RecTime_Current.Time.Minutes   *60
                             + Speed_RecTime_Current.Time.Seconds      ;
                Event.RecordedDateTime1|=Seconds;
            #endif //MEDIAINFO_EVENTS
            if (Speed_RecTime_Current.Time.Frames!=45)
            {
                int32u Milliseconds;
                if (video_source_stype!=(int8u)-1)
                    Milliseconds=Speed_RecTime_Current.Time.Frames*(system?40:33);
                else
                    Milliseconds=Speed_RecTime_Current.Time.Frames*(DSF?40:33);
                Speed_RecTimeZ_Current+=__T('.');
                Speed_RecTimeZ_Current+=__T('0')+(Char)(Milliseconds/100);
                Speed_RecTimeZ_Current+=__T('0')+(Char)((Milliseconds%100)/10);
                Speed_RecTimeZ_Current+=__T('0')+(Char)(Milliseconds%10);
                #if MEDIAINFO_EVENTS
                    Event.RecordedDateTime2|=Speed_RecTime_Current.Time.Frames;
                #endif //MEDIAINFO_EVENTS
            }
            else
            {
                Speed_RecTimeZ_Current+=__T("    ");
                #if MEDIAINFO_EVENTS
                    Event.RecordedDateTime2|=0x7F;
                #endif //MEDIAINFO_EVENTS
            }
            Errors_Stats_Line+=Speed_RecTimeZ_Current;
            if (Speed_RecZ.empty() || Speed_RecZ[0].First.FramePos==Speed_FrameCount) //Empty or the same frame as RecDate
            {
                if (Speed_RecZ.empty())
                    Speed_RecZ.resize(1);
                Speed_RecZ[0].First.FramePos=Speed_FrameCount;
                Speed_RecZ[0].First.Time=Speed_RecTimeZ_Current;
            }
            if (Speed_TimeStampsZ.empty())
            {
                Speed_TimeStampsZ.resize(1);
                Speed_TimeStampsZ[0].First.FramePos=Speed_FrameCount;
                Speed_TimeStampsZ[0].First.Time+=Ztring().Duration_From_Milliseconds((int64u)Time_Offset);
            }
            if (Speed_TimeStampsZ[0].First.FramePos==Speed_FrameCount) //Empty or the same frame as RecDate or the same frame as TimeCode
                Speed_TimeStampsZ[0].First.Time=Speed_RecTimeZ_Current;
        }
        else
        {
            Errors_Stats_Line+=__T("XX:XX:XX.XXX");
            #if MEDIAINFO_EVENTS
                Event.RecordedDateTime1|=0x1FFFF;
                Event.RecordedDateTime2|=0x7F;
            #endif //MEDIAINFO_EVENTS
        }
        Errors_Stats_Line+=__T('\t');

        //RecDate/RecTime coherency, Rec start/end
        bool RecTime_Disrupted=false;
        if (Speed_RecTime_Current.IsValid && Speed_RecTime_Current_Theory.IsValid
         && !(   Speed_RecTime_Current.Time.Seconds==Speed_RecTime_Current_Theory.Time.Seconds
              && Speed_RecTime_Current.Time.Minutes==Speed_RecTime_Current_Theory.Time.Minutes
              && Speed_RecTime_Current.Time.Hours  ==Speed_RecTime_Current_Theory.Time.Hours)
         && !(   Speed_RecTime_Current.Time.Seconds==Speed_RecTime_Current_Theory2.Time.Seconds
              && Speed_RecTime_Current.Time.Minutes==Speed_RecTime_Current_Theory2.Time.Minutes
              && Speed_RecTime_Current.Time.Hours  ==Speed_RecTime_Current_Theory2.Time.Hours))
        {
            size_t Speed_RecZ_Pos=Speed_RecZ.size();
            Speed_RecZ.resize(Speed_RecZ_Pos+1);
            Speed_RecZ[Speed_RecZ_Pos].First.FramePos=Speed_FrameCount;
            Speed_RecZ[Speed_RecZ_Pos].First.Date=Speed_RecDateZ_Current;
            Speed_RecZ[Speed_RecZ_Pos].First.Time=Speed_RecTimeZ_Current;
            Speed_RecZ[Speed_RecZ_Pos-1].Last.FramePos=Speed_FrameCount;
            Speed_RecZ[Speed_RecZ_Pos-1].Last.Date=Speed_RecDateZ_Last;
            Speed_RecZ[Speed_RecZ_Pos-1].Last.Time=Speed_RecTimeZ_Last;

            Errors_Stats_Line+=__T('N');
            #if MEDIAINFO_EVENTS
                Event.RecordedDateTime1|=1<<30;
            #endif //MEDIAINFO_EVENTS
            if (!REC_IsValid || REC_ST)
            {
                RecTime_Disrupted=true;
                Errors_AreDetected=true; //If there is a start, this is not an error
            }
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');

        //Speed_Arb_Current
        if (Speed_Arb_Current.IsValid)
        {
            //Searching the bigest value count
            int8u Biggest_Pos=0xF;
            size_t Biggest_Count=0;
            for (int8u Pos=0; Pos<=0xF; Pos++) //0xF is not considered as a valid value.
                if (Speed_Arb_Current.Value_Counters[Pos]>Biggest_Count)
                {
                    Biggest_Pos=Pos;
                    Biggest_Count=Speed_Arb_Current.Value_Counters[Pos];
                }
            Errors_Stats_Line+=Ztring::ToZtring(Biggest_Pos, 16);
            #if MEDIAINFO_EVENTS
                Event.Arb|=Biggest_Pos;
                Event.Arb|=1<<4;
            #endif //MEDIAINFO_EVENTS
            Speed_Arb_Current.Value=Biggest_Pos;
        }
        else
            Errors_Stats_Line+=__T('X');
        Errors_Stats_Line+=__T('\t');

        //Speed_Arb_Current coherency
        if (!Speed_Arb_IsValid && Speed_Arb_Current.IsValid && Speed_Arb_Current.Value!=0)
            Speed_Arb_IsValid=true;
        if (Speed_Arb_IsValid && Speed_Arb_Current.IsValid && Speed_Arb_Last.IsValid
         && Speed_Arb_Current.Value ==Speed_Arb_Last.Value
         && Speed_Arb_Current.Value!=0xF)
        {
            Errors_Stats_Line+=__T('R');
            #if MEDIAINFO_EVENTS
                Event.Arb|=1<<7;
            #endif //MEDIAINFO_EVENTS
            if (Speed_Arb_Current.Value!=0xF)
                Arb_AreDetected=true;

            Speed_Arb_Current_Theory.IsValid=false;
        }
        else if (Speed_Arb_IsValid && Speed_Arb_Current.IsValid && Speed_Arb_Current_Theory.IsValid
              && Speed_Arb_Current.Value   != Speed_Arb_Current_Theory.Value)
        {
            Errors_Stats_Line+=__T('N');
            #if MEDIAINFO_EVENTS
                Event.Arb|=1<<6;
            #endif //MEDIAINFO_EVENTS
            Speed_Arb_Current_Theory=Speed_Arb_Current;
            Arb_AreDetected=true;
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');

        //Start
        if (REC_IsValid && !REC_ST)
        {
            Errors_Stats_Line+=__T('S');
            #if MEDIAINFO_EVENTS
                Event.RecordedDateTime1|=1<<29;
            #endif //MEDIAINFO_EVENTS
            Infos_AreDetected=true;
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');

        //End
        if (REC_IsValid && !REC_END)
        {
            Errors_Stats_Line+=__T('E');
            #if MEDIAINFO_EVENTS
                Event.RecordedDateTime1|=1<<28;
            #endif //MEDIAINFO_EVENTS
            Infos_AreDetected=true;
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');

        //TimeStamp (RecDate/RecTime and TimeCode together)
        if (TimeCode_Disrupted || RecTime_Disrupted)
        {
            size_t Speed_TimeStampsZ_Pos=Speed_TimeStampsZ.size();
            Speed_TimeStampsZ.resize(Speed_TimeStampsZ_Pos+1);
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].First.FramePos=Speed_FrameCount;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].First.TimeCode=Speed_TimeCodeZ_Current;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].First.Date=Speed_RecDateZ_Current;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].First.Time=Speed_RecTimeZ_Current;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos-1].Last.FramePos=Speed_FrameCount;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos-1].Last.TimeCode=Speed_TimeCodeZ_Last;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos-1].Last.Date=Speed_RecDateZ_Last;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos-1].Last.Time=Speed_RecTimeZ_Last;
        }

        //Error 1: Video errors
        Ztring Errors_Stats_Line_Details;
        if (!Video_STA_Errors.empty())
        {
            if (!Stats_Total_AlreadyDetected)
            {
                Stats_Total_AlreadyDetected=true;
                Stats_Total++;
                Stats_Total_WithoutArb++;
            }
            Stats[1]++;
            Errors_Stats_Line+=__T('1');

            size_t Video_STA_Errors_Count=0;
            Ztring Video_STA_Errors_Details;
            for (size_t Pos=0; Pos<16; Pos++)
            {
                if (Video_STA_Errors[Pos])
                {
                    Video_STA_Errors_Count+=Video_STA_Errors[Pos];
                    Ztring Video_STA_Errors_Count_Padded=Ztring::ToZtring(Video_STA_Errors[Pos]);
                    if (Video_STA_Errors_Count_Padded.size()<8)
                        Video_STA_Errors_Count_Padded.insert(0, 8-Video_STA_Errors_Count_Padded.size(), __T(' '));
                    Video_STA_Errors_Details+=Video_STA_Errors_Count_Padded;
                    Video_STA_Errors_Details+=__T(" \"");
                    Video_STA_Errors_Details+=Ztring::ToZtring(Pos, 16);
                    Video_STA_Errors_Details+=__T("\" errors, ");
                    if (Video_STA_Errors_Total.empty())
                        Video_STA_Errors_Total.resize(16);
                    Video_STA_Errors_Total[Pos]+=Video_STA_Errors[Pos];
                }
            }
            if (Video_STA_Errors_Details.size()>2)
            {
                Ztring Video_STA_Errors_Count_Padded=Ztring::ToZtring(((float)Video_STA_Errors_Count)*100/(DSF?1500:1350)*(QU_FSC?2:1), 2);
                if (Video_STA_Errors_Count_Padded.size()<5)
                    Video_STA_Errors_Count_Padded.insert(0, 5-Video_STA_Errors_Count_Padded.size(), __T(' '));
                Errors_Stats_Line_Details+=Video_STA_Errors_Count_Padded+__T("%");
                Video_STA_Errors_Details.resize(Video_STA_Errors_Details.size()-2);
                Errors_Stats_Line_Details+=__T(" (")+Video_STA_Errors_Details+__T(")");
                Speed_FrameCount_Video_STA_Errors++;
                Errors_AreDetected=true;
            }
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Error 2: Audio errors
        if (QU!=(int8u)-1)
        {
            bool ErrorsAreAlreadyDetected=false;
            const size_t DseqSpan=QU_System?6:5;
            for (size_t ChannelGroup=0; ChannelGroup<ChannelGroup_Count; ChannelGroup++)
            {
                for (size_t Channel=0; Channel<2; Channel++)
                {
                    size_t ChannelTotalPos=ChannelGroup*2+Channel;
                    size_t Audio_Errors_PerChannel=0;
                    Ztring Audio_Errors_PerChannel_Details;
                    size_t Dseq_Begin=Channel?DseqSpan:0;
                    size_t Dseq_End=Dseq_Begin+DseqSpan;
                    for (int8u Dseq=Dseq_Begin; Dseq<Dseq_End; Dseq++)
                    {
                        size_t Audio_Errors_PerDseq=0;
                        if (ChannelGroup<audio_source_mode.size() && !audio_source_mode[ChannelGroup].empty() && audio_source_mode[ChannelGroup][Dseq]&0xF==0xF)
                            Audio_Errors_PerDseq=9; //We consider all audio blocks as invalid if audio mode is 0xF (15)
                        if (!Audio_Errors_PerDseq && audio_source_mode.empty() && ChannelInfo[ChannelGroup*2+Channel])
                            Audio_Errors_PerDseq=9; //We consider all audio blocks as invalid if the frame has no audio_source for this channel but had it in the previous frames
                        if (!Audio_Errors_PerDseq && ChannelGroup<Audio_Errors.size() && !Audio_Errors[ChannelGroup].empty())
                            Audio_Errors_PerDseq=Audio_Errors[ChannelGroup][Dseq];
                        if (Audio_Errors_PerDseq)
                        {
                            Audio_Errors_PerChannel+=Audio_Errors_PerDseq;
                            Ztring Audio_Errors_Count_Padded=Ztring::ToZtring(Audio_Errors_PerDseq);
                            if (Audio_Errors_Count_Padded.size()<2)
                                Audio_Errors_Count_Padded.insert(0, 2-Audio_Errors_Count_Padded.size(), __T(' '));
                            Audio_Errors_PerChannel_Details+=Audio_Errors_Count_Padded;
                            Audio_Errors_PerChannel_Details+=__T(" Dseq=");
                            Audio_Errors_PerChannel_Details+=Ztring::ToZtring(Dseq, 16);
                            Audio_Errors_PerChannel_Details+=__T(", ");
                            if (Audio_Errors_TotalPerChannel.empty())
                            {
                                Audio_Errors_TotalPerChannel.resize(4);
                                for (size_t Audio_Errors_Pos=0; Audio_Errors_Pos<4; Audio_Errors_Pos++)
                                    Audio_Errors_TotalPerChannel[Audio_Errors_Pos].resize(16);
                            }
                            Audio_Errors_TotalPerChannel[ChannelTotalPos][Dseq]+=Audio_Errors_PerDseq;
                        }
                    }
                    if (Audio_Errors_PerChannel)
                    {
                        if (!ErrorsAreAlreadyDetected)
                        {
                            if (!Stats_Total_AlreadyDetected)
                            {
                                Stats_Total_AlreadyDetected=true;
                                Stats_Total++;
                                Stats_Total_WithoutArb++;
                            }
                            Stats[2]++;
                            Errors_Stats_Line+=__T('2');
                        }

                        Ztring Audio_Errors_Count_Padded=Ztring::ToZtring(((float)Audio_Errors_PerChannel)*100/(DSF?54:45)*(QU_FSC?2:1), 2);
                        if (Audio_Errors_Count_Padded.size()<2)
                            Audio_Errors_Count_Padded.insert(0, 2-Audio_Errors_Count_Padded.size(), __T(' '));
                        if (ErrorsAreAlreadyDetected)
                            Errors_Stats_Line_Details+=__T(", ");
                        if (Audio_Errors_PerChannel<(size_t)(QU_System?54:45))
                        {
                            Errors_Stats_Line_Details+=__T("CH")+Ztring::ToZtring(ChannelTotalPos+1)+__T(": ")+Audio_Errors_Count_Padded+__T("%");
                            Audio_Errors_PerChannel_Details.resize(Audio_Errors_PerChannel_Details.size()-2);
                            Errors_Stats_Line_Details+=__T(" (")+Audio_Errors_PerChannel_Details+__T(")");
                        }
                        else
                            Errors_Stats_Line_Details+=__T("CH")+Ztring::ToZtring(ChannelTotalPos+1)+__T(": no valid DIF");

                        Speed_FrameCount_Audio_Errors[ChannelTotalPos]++;
                        ErrorsAreAlreadyDetected=true;
                        Errors_AreDetected=true;
                    }
                }
                if (!ErrorsAreAlreadyDetected)
                    Errors_Stats_Line+=__T(' ');
            }
        }
        else
            Errors_Stats_Line+=__T(' ');

        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Error 3: Timecode incoherency
        if (Speed_TimeCode_Current.MultipleValues)
        {
            if (!Stats_Total_AlreadyDetected)
            {
                Stats_Total_AlreadyDetected=true;
                Stats_Total++;
                Stats_Total_WithoutArb++;
            }
            Stats[3]++;
            Errors_Stats_Line+=__T('3');
            Errors_Stats_Line_Details+=__T("Timecode incoherency, first detected value is used");
            Speed_FrameCount_Timecode_Incoherency++;
            Errors_AreDetected=true;
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Error 4: DIF order incoherency
        if (Speed_Contains_NULL)
        {
            if (!Stats_Total_AlreadyDetected)
            {
                Stats_Total_AlreadyDetected=true;
                Stats_Total++;
                Stats_Total_WithoutArb++;
            }
            Stats[4]++;
            Errors_Stats_Line+=__T('4');
            Errors_Stats_Line_Details+=Ztring::ToZtring(Speed_Contains_NULL)+__T(" NULL DIFs");
            Speed_FrameCount_Contains_NULL++;
            Errors_AreDetected=true;
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Error 5: Speed_Arb_Current incoherency
        if (Speed_Arb_Current.MultipleValues)
        {
            if (!Stats_Total_AlreadyDetected)
            {
                Stats_Total_AlreadyDetected=true;
                Stats_Total++;
            }
            Stats[5]++;
            Errors_Stats_Line+=__T('5');
            Ztring Arb_Errors;
            for (size_t Pos=0; Pos<16; Pos++)
                if (Speed_Arb_Current.Value_Counters[Pos])
                {
                    Arb_Errors+=Ztring::ToZtring(Speed_Arb_Current.Value_Counters[Pos]);
                    Arb_Errors+=__T(" Arb bit=\"");
                    Arb_Errors+=Ztring::ToZtring(Pos, 16);
                    Arb_Errors+=__T("\", ");
                }
            if (Arb_Errors.size()>2)
            {
                Arb_Errors.resize(Arb_Errors.size()-2);
                Errors_Stats_Line_Details+=Arb_Errors;
            }
            Speed_FrameCount_Arb_Incoherency++;
            Arb_AreDetected=true;
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Error 6:
        if (Mpeg4_stts && Mpeg4_stts_Pos<Mpeg4_stts->size() && Speed_FrameCount>=Mpeg4_stts->at(Mpeg4_stts_Pos).Pos_Begin && Speed_FrameCount<Mpeg4_stts->at(Mpeg4_stts_Pos).Pos_End)
        {
            if (!Stats_Total_AlreadyDetected)
            {
                Stats_Total_AlreadyDetected=true;
                Stats_Total++;
                Stats_Total_WithoutArb++;
            }
            Stats[6]++;
            Errors_Stats_Line+=__T('6');
            Errors_Stats_Line_Details+=__T("stts flucuation");
            Speed_FrameCount_Stts_Fluctuation++;
            Errors_AreDetected=true;
        }
        else
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Error 7:
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Error 8:
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Error 9:
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Error 0:
            Errors_Stats_Line+=__T(' ');
        Errors_Stats_Line+=__T('\t');
        Errors_Stats_Line_Details+=__T('\t');

        //Filling the main text if needed
        {
            #if MEDIAINFO_EVENTS
            if (!Config->Event_CallBackFunction_IsSet())
            {
                Errors_Stats_10+=Errors_Stats_Line;
                Errors_Stats_10+=Errors_Stats_Line_Details;
                Errors_Stats_10+=__T("&");
            }
                Event.Verbosity=10;
            #endif //MEDIAINFO_EVENTS
        }
        if (Speed_FrameCount==0
         || Status[IsFinished]
         || Errors_AreDetected
         || Infos_AreDetected
         || Arb_AreDetected)
        {
            #if MEDIAINFO_EVENTS
            if (!Config->Event_CallBackFunction_IsSet())
            {
                Errors_Stats_09+=Errors_Stats_Line;
                Errors_Stats_09+=Errors_Stats_Line_Details;
                Errors_Stats_09+=__T("&");
            }
                Event.Verbosity=9;
            #endif //MEDIAINFO_EVENTS

            if (Speed_FrameCount==0
             || Status[IsFinished]
             || Errors_AreDetected
             || Infos_AreDetected)
            {
                #if MEDIAINFO_EVENTS
                if (!Config->Event_CallBackFunction_IsSet())
                {
                    Errors_Stats_05+=Errors_Stats_Line;
                    Errors_Stats_05+=Errors_Stats_Line_Details;
                    Errors_Stats_05+=__T("&");
                }
                    Event.Verbosity=5;
                #endif //MEDIAINFO_EVENTS

                if (Speed_FrameCount==0
                 || Status[IsFinished]
                 || Errors_AreDetected)
                {
                    #if MEDIAINFO_EVENTS
                    if (!Config->Event_CallBackFunction_IsSet())
                    {
                        Errors_Stats_03+=Errors_Stats_Line;
                        Errors_Stats_03+=Errors_Stats_Line_Details;
                        Errors_Stats_03+=__T("&");
                    }
                        Event.Verbosity=3;
                    #endif //MEDIAINFO_EVENTS
                }
             }
        }

        #if MEDIAINFO_EVENTS
            std::string Errors;
            if (Errors_Stats_Line_Details.size()>10)
            {
                Errors=Errors_Stats_Line_Details.To_Local();
                Event.Errors=(char*)Errors.c_str();
            }
            struct MediaInfo_Event_DvDif_Analysis_Frame_1 Event1;
            Event_Prepare((struct MediaInfo_Event_Generic*)&Event1);
            Event1.EventCode=MediaInfo_EventCode_Create(MediaInfo_Parser_DvDif, MediaInfo_Event_DvDif_Analysis_Frame, 1);
            Event1.StreamOffset=Speed_FrameCount_StartOffset;
            Event1.TimeCode=Event.TimeCode;
            Event1.RecordedDateTime1=Event.RecordedDateTime1;
            Event1.RecordedDateTime2=Event.RecordedDateTime2;
            Event1.Arb=Event.Arb;
            Event1.Verbosity=Event.Verbosity;
            Event1.Errors=Event.Errors;
            Event1.Video_STA_Errors_Count=Video_STA_Errors_ByDseq.size();
            Event1.Video_STA_Errors=Video_STA_Errors_ByDseq.empty()?NULL:&Video_STA_Errors_ByDseq[0];
            if (Audio_Errors.empty())
            {
                Event1.Audio_Data_Errors_Count=0;
                Event1.Audio_Data_Errors=NULL;
            }
            else
            {
                size_t Audio_Errors_PerDseq[16]; //Per Dseq
                memset(Audio_Errors_PerDseq, 0, sizeof(Audio_Errors_PerDseq));
                for (size_t ChannelGroup=0; ChannelGroup<Audio_Errors.size(); ChannelGroup++)
                    for (size_t Dseq=0; Dseq<Dseq_Count; Dseq++)
                        Audio_Errors_PerDseq[Dseq]+=Audio_Errors[ChannelGroup][Dseq];
                Event1.Audio_Data_Errors_Count=16;
                Event1.Audio_Data_Errors=Audio_Errors_PerDseq;
            }
            Config->Event_Send(NULL, (const int8u*)&Event1, sizeof(MediaInfo_Event_DvDif_Analysis_Frame_1));
            Config->Event_Send(NULL, (const int8u*)&Event, sizeof(MediaInfo_Event_DvDif_Analysis_Frame_0));
        #endif //MEDIAINFO_EVENTS
    }

    //Speed_TimeCode_Current
    if (!Speed_TimeCode_Current_Theory.IsValid)
        Speed_TimeCode_Current_Theory=Speed_TimeCode_Current;
    if (Speed_TimeCode_Current_Theory.IsValid)
    {
        int8u Frames_Max;
        if (video_source_stype!=(int8u)-1)
            Frames_Max=system?25:30;
        else
            Frames_Max=DSF?25:30;

        Speed_TimeCode_Current_Theory.Time.Frames++;
        if (Speed_TimeCode_Current_Theory.Time.Frames>=Frames_Max)
        {
            Speed_TimeCode_Current_Theory.Time.Seconds++;
            Speed_TimeCode_Current_Theory.Time.Frames=0;
            if (Speed_TimeCode_Current_Theory.Time.Seconds>=60)
            {
                Speed_TimeCode_Current_Theory.Time.Seconds=0;
                Speed_TimeCode_Current_Theory.Time.Minutes++;

                if (!DSF && Speed_TimeCode_Current_Theory.Time.DropFrame && Speed_TimeCode_Current_Theory.Time.Minutes%10)
                    Speed_TimeCode_Current_Theory.Time.Frames=2; //frames 0 and 1 are dropped for every minutes except 00 10 20 30 40 50

                if (Speed_TimeCode_Current_Theory.Time.Minutes>=60)
                {
                    Speed_TimeCode_Current_Theory.Time.Minutes=0;
                    Speed_TimeCode_Current_Theory.Time.Hours++;
                    if (Speed_TimeCode_Current_Theory.Time.Hours>=24)
                    {
                        Speed_TimeCode_Current_Theory.Time.Hours=0;
                    }
                }
            }
        }
    }

    //Speed_RecTime_Current_Theory
    Speed_RecTime_Current_Theory=Speed_RecTime_Current;
    Speed_RecTime_Current_Theory2=Speed_RecTime_Current; //Don't change it
    if (Speed_RecTime_Current_Theory.IsValid)
    {
        Speed_RecTime_Current_Theory.Time.Seconds++;
        if (Speed_RecTime_Current_Theory.Time.Seconds>=60)
        {
            Speed_RecTime_Current_Theory.Time.Seconds=0;
            Speed_RecTime_Current_Theory.Time.Minutes++;
            if (Speed_RecTime_Current_Theory.Time.Seconds>=60)
            {
                Speed_RecTime_Current_Theory.Time.Minutes=0;
                Speed_RecTime_Current_Theory.Time.Hours++;
                if (Speed_RecTime_Current_Theory.Time.Hours>=24)
                {
                    Speed_RecTime_Current_Theory.Time.Hours=0;
                }
            }
        }
    }

    //Speed_Arb_Current_Theory
    if (!Speed_Arb_Current_Theory.IsValid && Speed_Arb_Current.Value!=0xF)
        Speed_Arb_Current_Theory=Speed_Arb_Current;
    if (Speed_Arb_Current_Theory.IsValid && Speed_Arb_Current.Value!=0xF)
    {
        Speed_Arb_Current_Theory.Value++;
        if (Speed_Arb_Current_Theory.Value>=12)
        {
            Speed_Arb_Current_Theory.Value=0;
        }
    }

    FSC_WasSet=false;
    FSP_WasNotSet=false;
    video_source_stype=(int8u)-1;
    aspect=(int8u)-1;
    ssyb_AP3=(int8u)-1;
    Speed_TimeCode_Last=Speed_TimeCode_Current;
    Speed_TimeCode_Current.Clear();
    Speed_RecDate_Current.IsValid=false;
    Speed_RecDate_Current.MultipleValues=false;
    Speed_RecTime_Current.IsValid=false;
    Speed_RecTime_Current.MultipleValues=false;
    Speed_Arb_Last=Speed_Arb_Current;
    Speed_Arb_Current.Clear();
    Speed_FrameCount++;
    REC_IsValid=false;
    audio_source_mode.clear();
    Speed_Contains_NULL=0;
    Video_STA_Errors.clear();
    Video_STA_Errors_ByDseq.clear();
    Audio_Errors.clear();
    Stats_Total_AlreadyDetected=false;
}

void File_DvDif::Errors_Stats_Update_Finnish()
{
    if (!Analyze_Activated)
    {
        if (Config->File_DvDif_Analysis_Get())
            Analyze_Activated=true;
        else
            return;
    }
    Errors_Stats_Update();

    //Preparing next frame
    Ztring Errors_Stats_End_03;
    Ztring Errors_Stats_End_05;
    Ztring Errors_Stats_End_Lines;

    //Frames
    if (Speed_FrameCount)
        Errors_Stats_End_Lines+=__T("Frame Count: ")+Ztring::ToZtring(Speed_FrameCount)+__T('&');

    //One block
    if (!Errors_Stats_End_Lines.empty())
    {
        Errors_Stats_End_05+=Errors_Stats_End_Lines;
        Errors_Stats_End_05+=__T('&');
        Errors_Stats_End_Lines.clear();
    }

    //Error 1: Video error concealment
    if (Speed_FrameCount_Video_STA_Errors)
        Errors_Stats_End_Lines+=__T("Frame count with video error concealment: ")+Ztring::ToZtring(Speed_FrameCount_Video_STA_Errors)+__T(" frames &");
    if (!Video_STA_Errors_Total.empty())
    {
        Ztring Errors_Details;
        size_t Errors_Count=0;
        for (size_t Pos=0; Pos<16; Pos++)
        {
            if (Video_STA_Errors_Total[Pos])
            {
                Errors_Count+=Video_STA_Errors_Total[Pos];
                Ztring Errors_Count_Padded=Ztring::ToZtring(Video_STA_Errors_Total[Pos]);
                if (Errors_Count_Padded.size()<8)
                    Errors_Count_Padded.insert(0, 8-Errors_Count_Padded.size(), __T(' '));
                Errors_Details+=Errors_Count_Padded;
                Errors_Details+=__T(" \"");
                Errors_Details+=Ztring::ToZtring(Pos, 16);
                Errors_Details+=__T("\" errors, ");
            }
        }
        if (Errors_Details.size()>2)
        {
            Errors_Stats_End_Lines+=__T("Total video error concealment: ");
            Ztring Errors_Count_Padded=Ztring::ToZtring(Errors_Count);
            if (Errors_Count_Padded.size()<8)
                Errors_Count_Padded.insert(0, 8-Errors_Count_Padded.size(), __T(' '));
            Errors_Stats_End_Lines+=__T(" ")+Errors_Count_Padded+__T(" errors");
            Errors_Details.resize(Errors_Details.size()-2);
            Errors_Stats_End_Lines+=__T(" (")+Errors_Details+__T(")")+__T('&');
        }
    }

    //Error 2: Audio error code
    if (!Audio_Errors_TotalPerChannel.empty())
    {
        for (size_t Channel=0; Channel<4; Channel++)
        {
            if (Speed_FrameCount_Audio_Errors[Channel])
                Errors_Stats_End_Lines+=__T("Frame count with CH")+Ztring::ToZtring(Channel+1)+__T(" audio error code: ")+Ztring::ToZtring(Speed_FrameCount_Audio_Errors[Channel])+__T(" frames &");

            Ztring Errors_Details;
            size_t Errors_Count=0;
            for (size_t Pos=0; Pos<16; Pos++)
            {
                if (Audio_Errors_TotalPerChannel[Channel][Pos])
                {
                    Errors_Count+=Audio_Errors_TotalPerChannel[Channel][Pos];
                    Ztring Errors_Count_Padded=Ztring::ToZtring(Audio_Errors_TotalPerChannel[Channel][Pos]);
                    if (Errors_Count_Padded.size()<8)
                        Errors_Count_Padded.insert(0, 8-Errors_Count_Padded.size(), __T(' '));
                    Errors_Details+=Errors_Count_Padded;
                    Errors_Details+=__T(" Dseq=");
                    Errors_Details+=Ztring::ToZtring(Pos, 16);
                    Errors_Details+=__T(", ");
                }
            }
            if (Errors_Details.size()>2)
            {
                Errors_Stats_End_Lines+=__T("Total audio error code for CH")+Ztring::ToZtring(Channel+1)+__T(": ");
                Ztring Errors_Count_Padded=Ztring::ToZtring(Errors_Count);
                if (Errors_Count_Padded.size()<8)
                    Errors_Count_Padded.insert(0, 8-Errors_Count_Padded.size(), __T(' '));
                Errors_Stats_End_Lines+=__T(" ")+Errors_Count_Padded+__T(" errors");
                Errors_Details.resize(Errors_Details.size()-2);
                Errors_Stats_End_Lines+=__T(" (")+Errors_Details+__T(")")+__T('&');
            }
        }
    }

    //Error 3: Timecode incoherency
    if (Speed_FrameCount_Timecode_Incoherency)
        Errors_Stats_End_Lines+=__T("Frame count with DV timecode incoherency: ")+Ztring::ToZtring(Speed_FrameCount_Timecode_Incoherency)+__T(" frames &");

    //Error 4: DIF incohereny
    if (Speed_FrameCount_Contains_NULL)
        Errors_Stats_End_Lines+=__T("Frame count with DIF incoherency: ")+Ztring::ToZtring(Speed_FrameCount_Contains_NULL)+__T(" frames &");

    //Error 5: Arbitrary bit inconsistency
    if (Speed_FrameCount_Arb_Incoherency)
        Errors_Stats_End_Lines+=__T("Frame count with Arbitrary bit inconsistency: ")+Ztring::ToZtring(Speed_FrameCount_Arb_Incoherency)+__T(" frames &");

    //Error 6: Stts fluctuation
    if (Speed_FrameCount_Stts_Fluctuation)
        Errors_Stats_End_Lines+=__T("Frame count with stts fluctuation: ")+Ztring::ToZtring(Speed_FrameCount_Stts_Fluctuation)+__T(" frames &");

    //One block
    if (!Errors_Stats_End_Lines.empty())
    {
        Errors_Stats_End_03+=Errors_Stats_End_Lines;
        Errors_Stats_End_03+=__T('&');
        Errors_Stats_End_05+=Errors_Stats_End_Lines;
        Errors_Stats_End_05+=__T('&');
        Errors_Stats_End_Lines.clear();
    }

    //TimeStamps (RecDate/RecTime and TimeCode)
    if (!Speed_RecDateZ_Current.empty() || !Speed_RecTimeZ_Current.empty()) //Date and Time must be both available
    {
        size_t Speed_TimeStampsZ_Pos=Speed_TimeStampsZ.size();
        if (Speed_TimeStampsZ_Pos)
        {
            Speed_TimeStampsZ_Pos--;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.FramePos=Speed_FrameCount;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.FramePos=Speed_FrameCount;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.TimeCode=Speed_TimeCodeZ_Current;
            if (Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.FramePos-(Speed_TimeStampsZ_Pos?Speed_TimeStampsZ[Speed_TimeStampsZ_Pos-1].Last.FramePos:0)==1) //Only one frame, the "Last" part is not filled
                Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.TimeCode=Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].First.TimeCode;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.Date=Speed_RecDateZ_Current;
            Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.Time=Speed_RecTimeZ_Current;
            if (Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.FramePos-(Speed_TimeStampsZ_Pos?Speed_TimeStampsZ[Speed_TimeStampsZ_Pos-1].Last.FramePos:0)==1)
            {
                //Only one frame, the "Last" part is not filled
                Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.Date=Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].First.Date;
                Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].Last.Time=Speed_TimeStampsZ[Speed_TimeStampsZ_Pos].First.Time;
            }

            //Framerate computing
            float64 FrameRate=29.970;
            if (video_source_stype!=(int8u)-1)
                FrameRate=system?25.000:29.970;
            else
                FrameRate=DSF?25.000:29.970;
            if (FrameRate==29.970 && Speed_TimeCode_Current.IsValid && !Speed_TimeCode_Current.Time.DropFrame)
                FrameRate=30.000;

            Errors_Stats_End_Lines+=__T("Absolute time\tDV timecode range        \tRecorded date/time range                         \tFrame range&");
            for (size_t Pos=0; Pos<Speed_TimeStampsZ.size(); Pos++)
            {
                //Time
                float64 Time_Offset=(Pos?Speed_TimeStampsZ[Pos-1].Last.FramePos:0)*1000/FrameRate;
                Errors_Stats_End_Lines+=Ztring().Duration_From_Milliseconds((int64u)Time_Offset);

                Errors_Stats_End_Lines+=__T("\t");

                //TimeCode_range
                Errors_Stats_End_Lines+=Speed_TimeStampsZ[Pos].First.TimeCode.empty()?Ztring(__T("XX:XX:XX:XX")):Speed_TimeStampsZ[Pos].First.TimeCode;

                Errors_Stats_End_Lines+=__T(" - ");

                Errors_Stats_End_Lines+=Speed_TimeStampsZ[Pos].Last.TimeCode.empty()?Ztring(__T("XX:XX:XX:XX")):Speed_TimeStampsZ[Pos].Last.TimeCode;

                Errors_Stats_End_Lines+=__T("\t");

                //Recorded date/time_range
                Errors_Stats_End_Lines+=Speed_TimeStampsZ[Pos].First.Date.empty()?Ztring(__T("XXXX-XX-XX")):Speed_TimeStampsZ[Pos].First.Date;
                Errors_Stats_End_Lines+=__T(' ');
                Errors_Stats_End_Lines+=Speed_TimeStampsZ[Pos].First.Time.empty()?Ztring(__T("XX:XX:XX:XX")):Speed_TimeStampsZ[Pos].First.Time;

                Errors_Stats_End_Lines+=__T(" - ");

                Errors_Stats_End_Lines+=Speed_TimeStampsZ[Pos].Last.Date.empty()?Ztring(__T("XXXX-XX-XX")):Speed_TimeStampsZ[Pos].Last.Date;
                Errors_Stats_End_Lines+=__T(' ');
                Errors_Stats_End_Lines+=Speed_TimeStampsZ[Pos].Last.Time.empty()?Ztring(__T("XX:XX:XX:XX")):Speed_TimeStampsZ[Pos].Last.Time;

                Errors_Stats_End_Lines+=__T("\t");

                //Frame range
                int64u Start=Pos?Speed_TimeStampsZ[Pos-1].Last.FramePos:0;
                Ztring Start_Padded=Ztring::ToZtring(Start);
                if (Start_Padded.size()<8)
                    Start_Padded.insert(0, 8-Start_Padded.size(), __T(' '));

                Errors_Stats_End_Lines+=Start_Padded;

                int64u End=Speed_TimeStampsZ[Pos].Last.FramePos-1;
                Ztring End_Padded=Ztring::ToZtring(End);
                if (End_Padded.size()<8)
                    End_Padded.insert(0, 8-End_Padded.size(), __T(' '));
                Errors_Stats_End_Lines+=__T(" - ")+End_Padded;

                Errors_Stats_End_Lines+=__T('&');
            }
        }
    }

    //One block
    if (!Errors_Stats_End_Lines.empty())
    {
        Errors_Stats_End_05+=Errors_Stats_End_Lines;
        Errors_Stats_End_05+=__T('&');
        Errors_Stats_End_Lines.clear();
    }

    //Stats
    if (Stats_Total)
    {
        Errors_Stats_End_Lines+=__T("Percent of frames with Error: ");
        Errors_Stats_End_Lines+=Ztring::ToZtring(((float)Stats_Total_WithoutArb*100)/Speed_FrameCount, 2);
        Errors_Stats_End_Lines+=__T("%");
        Errors_Stats_End_Lines+=__T('&');
        Errors_Stats_End_Lines+=__T("Percent of frames with Error (including Arbitrary bit inconsistency): ");
        Errors_Stats_End_Lines+=Ztring::ToZtring(((float)Stats_Total*100)/Speed_FrameCount, 2);
        Errors_Stats_End_Lines+=__T("%");
        Errors_Stats_End_Lines+=__T('&');

        if (Stats[1])
        {
            Errors_Stats_End_Lines+=__T("Percent of frames with Video Error Concealment: ");
            Errors_Stats_End_Lines+=Ztring::ToZtring(((float)Stats[1]*100)/Speed_FrameCount, 2);
            Errors_Stats_End_Lines+=__T("%");
            Errors_Stats_End_Lines+=__T('&');
        }

        if (Stats[2])
        {
            Errors_Stats_End_Lines+=__T("Percent of frames with Audio Errors: ");
            Errors_Stats_End_Lines+=Ztring::ToZtring(((float)Stats[2]*100)/Speed_FrameCount, 2);
            Errors_Stats_End_Lines+=__T("%");
            Errors_Stats_End_Lines+=__T('&');
        }

        if (Stats[3])
        {
            Errors_Stats_End_Lines+=__T("Percent of frames with Timecode Incoherency: ");
            Errors_Stats_End_Lines+=Ztring::ToZtring(((float)Stats[3]*100)/Speed_FrameCount, 2);
            Errors_Stats_End_Lines+=__T("%");
            Errors_Stats_End_Lines+=__T('&');
        }

        if (Stats[4])
        {
            Errors_Stats_End_Lines+=__T("Percent of frames with DIF Incoherency: ");
            Errors_Stats_End_Lines+=Ztring::ToZtring(((float)Stats[4]*100)/Speed_FrameCount, 2);
            Errors_Stats_End_Lines+=__T("%");
            Errors_Stats_End_Lines+=__T('&');
        }

        if (Stats[5])
        {
            Errors_Stats_End_Lines+=__T("Percent of frames with Arbitrary bit inconsistency: ");
            Errors_Stats_End_Lines+=Ztring::ToZtring(((float)Stats[5]*100)/Speed_FrameCount, 2);
            Errors_Stats_End_Lines+=__T("%");
            Errors_Stats_End_Lines+=__T('&');
        }

        if (Stats[6])
        {
            Errors_Stats_End_Lines+=__T("Percent of frames with Stts Fluctuation: ");
            Errors_Stats_End_Lines+=Ztring::ToZtring(((float)Stats[6]*100)/Speed_FrameCount, 2);
            Errors_Stats_End_Lines+=__T("%");
            Errors_Stats_End_Lines+=__T('&');
        }
    }

    //One block
    if (!Errors_Stats_End_Lines.empty())
    {
        Errors_Stats_End_05+=Errors_Stats_End_Lines;
        Errors_Stats_End_05+=__T('&');
        Errors_Stats_End_Lines.clear();
    }

    //
    if (Errors_Stats_End_03.size()>2)
        Errors_Stats_End_03.resize(Errors_Stats_End_03.size()-2); //Removing last carriage returns
    if (Errors_Stats_End_05.size()>2)
        Errors_Stats_End_05.resize(Errors_Stats_End_05.size()-2); //Removing last carriage returns

    if (Errors_Stats_End_03.empty())
    {
        Errors_Stats_End_03+=__T("No identified errors");
        Errors_Stats_End_05+=__T("&&No identified errors");
    }

    //Filling
    if (Count_Get(Stream_Video)==0)
        Stream_Prepare(Stream_Video);
    Fill(Stream_Video, 0, "Errors_Stats_Begin", "Frame # \tAbsolute time\tDV timecode\tN\tRecorded date/time     \tN\tA\tN\tS\tE\t1\t2\t3\t4\t5\t6\t7\t8\t9\t0\t1\t2\t3\t4\t5\t6\t7\t8\t9\t0");
    Fill_SetOptions(Stream_Video, 0, "Errors_Stats_Begin", "N NT");
    Fill(Stream_Video, 0, "Errors_Stats_03", Errors_Stats_03);
    Fill_SetOptions(Stream_Video, 0, "Errors_Stats_03", "N NT");
    Fill(Stream_Video, 0, "Errors_Stats_05", Errors_Stats_05);
    Fill_SetOptions(Stream_Video, 0, "Errors_Stats_05", "N NT");
    Fill(Stream_Video, 0, "Errors_Stats_09", Errors_Stats_09);
    Fill_SetOptions(Stream_Video, 0, "Errors_Stats_09", "N NT");
    Fill(Stream_Video, 0, "Errors_Stats_10", Errors_Stats_10);
    Fill_SetOptions(Stream_Video, 0, "Errors_Stats_10", "N NT");
    if (MediaInfoLib::Config.Verbosity_Get()>=(float32)1.0)
        Fill(Stream_Video, 0, "Errors_Stats", Errors_Stats_10);
    else if (MediaInfoLib::Config.Verbosity_Get()>=(float32)0.5)
        Fill(Stream_Video, 0, "Errors_Stats", Errors_Stats_09);
    else if (MediaInfoLib::Config.Verbosity_Get()>=(float32)0.9)
        Fill(Stream_Video, 0, "Errors_Stats", Errors_Stats_05);
    else
        Fill(Stream_Video, 0, "Errors_Stats", Errors_Stats_03);
    Fill_SetOptions(Stream_Video, 0, "Errors_Stats", "N NT");
    Fill(Stream_Video, 0, "Errors_Stats_End_03", Errors_Stats_End_03);
    Fill_SetOptions(Stream_Video, 0, "Errors_Stats_End_03", "N NT");
    Fill(Stream_Video, 0, "Errors_Stats_End_05", Errors_Stats_End_05);
    Fill_SetOptions(Stream_Video, 0, "Errors_Stats_End_05", "N NT");
    if (MediaInfoLib::Config.Verbosity_Get()>=(float32)0.5)
        Fill(Stream_Video, 0, "Errors_Stats_End", Errors_Stats_End_05);
    else
        Fill(Stream_Video, 0, "Errors_Stats_End", Errors_Stats_End_03);
    Fill_SetOptions(Stream_Video, 0, "Errors_Stats_End", "N NT");
    Fill(Stream_Video, 0, "FrameCount_Speed", Speed_FrameCount);
    Fill_SetOptions(Stream_Video, 0, "FrameCount_Speed", "N NT");
}

} //NameSpace

#endif //MEDIAINFO_DV_YES

