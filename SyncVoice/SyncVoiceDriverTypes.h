/*
See LICENSE folder for this sample’s licensing information.

Abstract:
A minimal user-space driver.
*/

/*==================================================================================================
	SyncVoiceDriverTypes.h
==================================================================================================*/
#if !defined(__SyncVoiceDriverTypes_h__)
#define __SyncVoiceDriverTypes_h__

#include <cstdint>

//==================================================================================================
//	Constants
//==================================================================================================

//	the class name for the part of the driver for which a matching notificaiton will be created
#define kSyncVoiceDriverClassName	"SyncVoiceDriver"

//	IORegistry keys that have the basic info about the driver
#define kSyncVoiceDriver_RegistryKey_SampleRate			"sample rate"
#define kSyncVoiceDriver_RegistryKey_RingBufferFrameSize	"buffer frame size"
#define kSyncVoiceDriver_RegistryKey_DeviceUID			"device UID"

//	memory types
enum
{
	kSyncVoiceDriver_Buffer_Status,
	kSyncVoiceDriver_Buffer_Input,
	kSyncVoiceDriver_Buffer_Output
};

//	user client method selectors
enum
{
	kSyncVoiceDriver_Method_Open,				//	No arguments
	kSyncVoiceDriver_Method_Close,			//	No arguments
	kSyncVoiceDriver_Method_StartHardware,	//	No arguments
	kSyncVoiceDriver_Method_StopHardware,		//	No arguments
	kSyncVoiceDriver_Method_SetSampleRate,	//	One input: the new sample rate as a 64 bit integer
	kSyncVoiceDriver_Method_GetControlValue,	//	One input: the control ID, One output: the control value
	kSyncVoiceDriver_Method_SetControlValue,	//	Two inputs, the control ID and the new value
	kSyncVoiceDriver_Method_NumberOfMethods
};

//	control IDs
enum
{
	kSyncVoiceDriver_Control_MasterInputVolume,
	kSyncVoiceDriver_Control_MasterOutputVolume
};

//	volume control ranges
#define kSyncVoiceDriver_Control_MinRawVolumeValue	0
#define kSyncVoiceDriver_Control_MaxRawVolumeValue	96
#define kSyncVoiceDriver_Control_MinDBVolumeValue		-96.0f
#define kSyncVoiceDriver_Control_MaxDbVolumeValue		0.0f

//	the struct in the status buffer
struct SyncVoiceDriverStatus
{
	volatile uint64_t	mSampleTime;
	volatile uint64_t	mHostTime;
};
typedef struct SyncVoiceDriverStatus	SyncVoiceDriverStatus;

#endif	//	__SyncVoiceDriverTypes_h__
