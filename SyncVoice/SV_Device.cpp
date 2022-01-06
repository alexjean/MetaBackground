/*
See LICENSE folder for this sample’s licensing information.

Abstract:
A minimal user-space driver.
*/

/*==================================================================================================
	SV_Device.cpp
==================================================================================================*/

//==================================================================================================
//	Includes
//==================================================================================================

//	Self Include
#include "SV_Device.h"

//	Local Includes
#include "SV_PlugIn.h"
#include "SyncVoiceDriverTypes.h"

//	PublicUtility Includes
#include "CACFDictionary.h"
#include "CADebugMacros.h"
#include "CADispatchQueue.h"
#include "CAException.h"

//==================================================================================================
//	SV_Device
//==================================================================================================

#pragma mark Construction/Destruction

SV_Device::SV_Device(AudioObjectID inObjectID, io_object_t inIOKitObject)
:
	SV_Object(inObjectID, kAudioDeviceClassID, kAudioObjectClassID, kAudioObjectPlugInObject),
	mStateMutex("Device State"),
	mIOMutex("Device IO"),
	mIOKitObject(inIOKitObject),
	mDeviceUID(HW_CopyDeviceUID(inIOKitObject)),
	mStartCount(0),
	mSampleRateShadow(0),
	mRingBufferFrameSize(0),
	mDriverStatus(NULL),
	mInputStreamObjectID(SV_ObjectMap::GetNextObjectID()),
	mInputStreamIsActive(true),
	mInputStreamRingBuffer(NULL),
	mOutputStreamObjectID(SV_ObjectMap::GetNextObjectID()),
	mOutputStreamIsActive(true),
	mOutputStreamRingBuffer(NULL),
	mInputMasterVolumeControlObjectID(SV_ObjectMap::GetNextObjectID()),
	mInputMasterVolumeControlRawValueShadow(kSyncVoiceDriver_Control_MinRawVolumeValue),
	mOutputMasterVolumeControlObjectID(SV_ObjectMap::GetNextObjectID()),
	mOutputMasterVolumeControlRawValueShadow(kSyncVoiceDriver_Control_MinRawVolumeValue),
	mVolumeCurve()
{
	//	Setup the volume curve with the one range
	mVolumeCurve.AddRange(kSyncVoiceDriver_Control_MinRawVolumeValue, kSyncVoiceDriver_Control_MaxRawVolumeValue, kSyncVoiceDriver_Control_MinDBVolumeValue, kSyncVoiceDriver_Control_MaxDbVolumeValue);
}

void	SV_Device::Activate()
{
	//	Open the connection to the driver and initialize things.
	_HW_Open();
	
	//	map the subobject IDs to this object
	SV_ObjectMap::MapObject(mInputStreamObjectID, this);
	SV_ObjectMap::MapObject(mOutputStreamObjectID, this);
	SV_ObjectMap::MapObject(mInputMasterVolumeControlObjectID, this);
	SV_ObjectMap::MapObject(mOutputMasterVolumeControlObjectID, this);
	
	//	call the super-class, which just marks the object as active
	SV_Object::Activate();
}

void	SV_Device::Deactivate()
{
	//	When this method is called, the obejct is basically dead, but we still need to be thread
	//	safe. In this case, we also need to be safe vs. any IO threads, so we need to take both
	//	locks.
	CAMutex::Locker theStateLocker(mStateMutex);
	CAMutex::Locker theIOLocker(mIOMutex);
	
	//	mark the object inactive by calling the super-class
	SV_Object::Deactivate();
	
	//	unmap the subobject IDs
	SV_ObjectMap::UnmapObject(mInputStreamObjectID, this);
	SV_ObjectMap::UnmapObject(mOutputStreamObjectID, this);
	SV_ObjectMap::UnmapObject(mInputMasterVolumeControlObjectID, this);
	SV_ObjectMap::UnmapObject(mOutputMasterVolumeControlObjectID, this);
	
	//	close the connection to the driver
	_HW_Close();
}

SV_Device::~SV_Device()
{
}

#pragma mark Property Operations

bool	SV_Device::HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	This object implements several API-level objects. So the first thing to do is to figure out
	//	which object this request is really for. Note that mSubObjectID is an invariant as this
	//	driver's structure does not change dynamically. It will always have the parts it has.
	bool theAnswer = false;
	if(inObjectID == mObjectID)
	{
		theAnswer = Device_HasProperty(inObjectID, inClientPID, inAddress);
	}
	else if((inObjectID == mInputStreamObjectID) || (inObjectID == mOutputStreamObjectID))
	{
		theAnswer = Stream_HasProperty(inObjectID, inClientPID, inAddress);
	}
	else if((inObjectID == mInputMasterVolumeControlObjectID) || (inObjectID == mOutputMasterVolumeControlObjectID))
	{
		theAnswer = Control_HasProperty(inObjectID, inClientPID, inAddress);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
	return theAnswer;
}

bool	SV_Device::IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	bool theAnswer = false;
	if(inObjectID == mObjectID)
	{
		theAnswer = Device_IsPropertySettable(inObjectID, inClientPID, inAddress);
	}
	else if((inObjectID == mInputStreamObjectID) || (inObjectID == mOutputStreamObjectID))
	{
		theAnswer = Stream_IsPropertySettable(inObjectID, inClientPID, inAddress);
	}
	else if((inObjectID == mInputMasterVolumeControlObjectID) || (inObjectID == mOutputMasterVolumeControlObjectID))
	{
		theAnswer = Control_IsPropertySettable(inObjectID, inClientPID, inAddress);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
	return theAnswer;
}

UInt32	SV_Device::GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	UInt32 theAnswer = 0;
	if(inObjectID == mObjectID)
	{
		theAnswer = Device_GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
	}
	else if((inObjectID == mInputStreamObjectID) || (inObjectID == mOutputStreamObjectID))
	{
		theAnswer = Stream_GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
	}
	else if((inObjectID == mInputMasterVolumeControlObjectID) || (inObjectID == mOutputMasterVolumeControlObjectID))
	{
		theAnswer = Control_GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
	return theAnswer;
}

void	SV_Device::GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const
{
	if(inObjectID == mObjectID)
	{
		Device_GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
	}
	else if((inObjectID == mInputStreamObjectID) || (inObjectID == mOutputStreamObjectID))
	{
		Stream_GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
	}
	else if((inObjectID == mInputMasterVolumeControlObjectID) || (inObjectID == mOutputMasterVolumeControlObjectID))
	{
		Control_GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
}

void	SV_Device::SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
	if(inObjectID == mObjectID)
	{
		Device_SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
	}
	else if((inObjectID == mInputStreamObjectID) || (inObjectID == mOutputStreamObjectID))
	{
		Stream_SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
	}
	else if((inObjectID == mInputMasterVolumeControlObjectID) || (inObjectID == mOutputMasterVolumeControlObjectID))
	{
		Control_SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
}

#pragma mark Device Property Operations

bool	SV_Device::Device_HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Device_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyName:
		case kAudioObjectPropertyManufacturer:
		case kAudioDevicePropertyDeviceUID:
		case kAudioDevicePropertyModelUID:
		case kAudioDevicePropertyTransportType:
		case kAudioDevicePropertyRelatedDevices:
		case kAudioDevicePropertyClockDomain:
		case kAudioDevicePropertyDeviceIsAlive:
		case kAudioDevicePropertyDeviceIsRunning:
		case kAudioObjectPropertyControlList:
		case kAudioDevicePropertyNominalSampleRate:
		case kAudioDevicePropertyAvailableNominalSampleRates:
		case kAudioDevicePropertyIsHidden:
		case kAudioDevicePropertyZeroTimeStampPeriod:
		case kAudioDevicePropertyStreams:
			theAnswer = true;
			break;
			
		case kAudioDevicePropertyLatency:
		case kAudioDevicePropertySafetyOffset:
		case kAudioDevicePropertyPreferredChannelsForStereo:
		case kAudioDevicePropertyPreferredChannelLayout:
		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
			theAnswer = (inAddress.mScope == kAudioObjectPropertyScopeInput) || (inAddress.mScope == kAudioObjectPropertyScopeOutput);
			break;
			
		default:
			theAnswer = SV_Object::HasProperty(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

bool	SV_Device::Device_IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Device_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyName:
		case kAudioObjectPropertyManufacturer:
		case kAudioDevicePropertyDeviceUID:
		case kAudioDevicePropertyModelUID:
		case kAudioDevicePropertyTransportType:
		case kAudioDevicePropertyRelatedDevices:
		case kAudioDevicePropertyClockDomain:
		case kAudioDevicePropertyDeviceIsAlive:
		case kAudioDevicePropertyDeviceIsRunning:
		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
		case kAudioDevicePropertyLatency:
		case kAudioDevicePropertyStreams:
		case kAudioObjectPropertyControlList:
		case kAudioDevicePropertySafetyOffset:
		case kAudioDevicePropertyAvailableNominalSampleRates:
		case kAudioDevicePropertyIsHidden:
		case kAudioDevicePropertyPreferredChannelsForStereo:
		case kAudioDevicePropertyPreferredChannelLayout:
		case kAudioDevicePropertyZeroTimeStampPeriod:
			theAnswer = false;
			break;
		
		case kAudioDevicePropertyNominalSampleRate:
			theAnswer = true;
			break;
		
		default:
			theAnswer = SV_Object::IsPropertySettable(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

UInt32	SV_Device::Device_GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Device_GetPropertyData() method.
	
	UInt32 theAnswer = 0;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyName:
			theAnswer = sizeof(CFStringRef);
			break;
			
		case kAudioObjectPropertyManufacturer:
			theAnswer = sizeof(CFStringRef);
			break;
			
		case kAudioObjectPropertyOwnedObjects:
			switch(inAddress.mScope)
			{
				case kAudioObjectPropertyScopeGlobal:
					theAnswer = kNumberOfSubObjects * sizeof(AudioObjectID);
					break;
					
				case kAudioObjectPropertyScopeInput:
					theAnswer = kNumberOfInputSubObjects * sizeof(AudioObjectID);
					break;
					
				case kAudioObjectPropertyScopeOutput:
					theAnswer = kNumberOfOutputSubObjects * sizeof(AudioObjectID);
					break;
			};
			break;

		case kAudioDevicePropertyDeviceUID:
			theAnswer = sizeof(CFStringRef);
			break;

		case kAudioDevicePropertyModelUID:
			theAnswer = sizeof(CFStringRef);
			break;

		case kAudioDevicePropertyTransportType:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyRelatedDevices:
			theAnswer = sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertyClockDomain:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceIsAlive:
			theAnswer = sizeof(AudioClassID);
			break;

		case kAudioDevicePropertyDeviceIsRunning:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyLatency:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyStreams:
			switch(inAddress.mScope)
			{
				case kAudioObjectPropertyScopeGlobal:
					theAnswer = kNumberOfStreams * sizeof(AudioObjectID);
					break;
					
				case kAudioObjectPropertyScopeInput:
					theAnswer = kNumberOfInputStreams * sizeof(AudioObjectID);
					break;
					
				case kAudioObjectPropertyScopeOutput:
					theAnswer = kNumberOfOutputStreams * sizeof(AudioObjectID);
					break;
			};
			break;

		case kAudioObjectPropertyControlList:
			theAnswer = kNumberOfControls * sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertySafetyOffset:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyNominalSampleRate:
			theAnswer = sizeof(Float64);
			break;

		case kAudioDevicePropertyAvailableNominalSampleRates:
			theAnswer = 2 * sizeof(AudioValueRange);
			break;
		
		case kAudioDevicePropertyIsHidden:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyPreferredChannelsForStereo:
			theAnswer = 2 * sizeof(UInt32);
			break;

		case kAudioDevicePropertyPreferredChannelLayout:
			theAnswer = offsetof(AudioChannelLayout, mChannelDescriptions) + (2 * sizeof(AudioChannelDescription));
			break;

		case kAudioDevicePropertyZeroTimeStampPeriod:
			theAnswer = sizeof(UInt32);
			break;

		default:
			theAnswer = SV_Object::GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
			break;
	};
	return theAnswer;
}

void	SV_Device::Device_GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required.
	//	Also, since most of the data that will get returned is static, there are few instances where
	//	it is necessary to lock the state mutex.

	UInt32 theNumberItemsToFetch;
	UInt32 theItemIndex;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyName:
			//	This is the human readable name of the device. Note that in this case we return a
			//	value that is a key into the localizable strings in this bundle. This allows us to
			//	return a localized name for the device.
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioObjectPropertyManufacturer for the device");
			*reinterpret_cast<CFStringRef*>(outData) = CFSTR("DeviceName");
			outDataSize = sizeof(CFStringRef);
			break;
			
		case kAudioObjectPropertyManufacturer:
			//	This is the human readable name of the maker of the plug-in. Note that in this case
			//	we return a value that is a key into the localizable strings in this bundle. This
			//	allows us to return a localized name for the manufacturer.
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioObjectPropertyManufacturer for the device");
			*reinterpret_cast<CFStringRef*>(outData) = CFSTR("ManufacturerName");
			outDataSize = sizeof(CFStringRef);
			break;
			
		case kAudioObjectPropertyOwnedObjects:
			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);
			
			//	The device owns its streams and controls. Note that what is returned here
			//	depends on the scope requested.
			switch(inAddress.mScope)
			{
				case kAudioObjectPropertyScopeGlobal:
					//	global scope means return all objects
					if(theNumberItemsToFetch > kNumberOfSubObjects)
					{
						theNumberItemsToFetch = kNumberOfSubObjects;
					}
					
					//	fill out the list with as many objects as requested, which is everything
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = mInputStreamObjectID;
					}
					if(theNumberItemsToFetch > 1)
					{
						reinterpret_cast<AudioObjectID*>(outData)[1] = mOutputStreamObjectID;
					}
					if(theNumberItemsToFetch > 2)
					{
						reinterpret_cast<AudioObjectID*>(outData)[2] = mInputMasterVolumeControlObjectID;
					}
					if(theNumberItemsToFetch > 3)
					{
						reinterpret_cast<AudioObjectID*>(outData)[3] = mOutputMasterVolumeControlObjectID;
					}
					break;
					
				case kAudioObjectPropertyScopeInput:
					//	input scope means just the objects on the input side
					if(theNumberItemsToFetch > kNumberOfInputSubObjects)
					{
						theNumberItemsToFetch = kNumberOfInputSubObjects;
					}
					
					//	fill out the list with the right objects
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = mInputStreamObjectID;
					}
					if(theNumberItemsToFetch > 1)
					{
						reinterpret_cast<AudioObjectID*>(outData)[1] = mInputMasterVolumeControlObjectID;
					}
					break;
					
				case kAudioObjectPropertyScopeOutput:
					//	output scope means just the objects on the output side
					if(theNumberItemsToFetch > kNumberOfOutputSubObjects)
					{
						theNumberItemsToFetch = kNumberOfOutputSubObjects;
					}
					
					//	fill out the list with the right objects
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = mOutputStreamObjectID;
					}
					if(theNumberItemsToFetch > 1)
					{
						reinterpret_cast<AudioObjectID*>(outData)[1] = mOutputMasterVolumeControlObjectID;
					}
					break;
			};
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertyDeviceUID:
			//	This is a CFString that is a persistent token that can identify the same
			//	audio device across boot sessions. Note that two instances of the same
			//	device must have different values for this property.
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceUID for the device");
			*reinterpret_cast<CFStringRef*>(outData) = mDeviceUID.CopyCFString();
			outDataSize = sizeof(CFStringRef);
			break;

		case kAudioDevicePropertyModelUID:
			//	This is a CFString that is a persistent token that can identify audio
			//	devices that are the same kind of device. Note that two instances of the
			//	save device must have the same value for this property.
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyModelUID for the device");
			*reinterpret_cast<CFStringRef*>(outData) = CFSTR(kDeviceModelUID);
			outDataSize = sizeof(CFStringRef);
			break;

		case kAudioDevicePropertyTransportType:
			//	This value represents how the device is attached to the system. This can be
			//	any 32 bit integer, but common values for this property are defined in
			//	<CoreAudio/AudioHardwareBase.h>
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyTransportType for the device");
			*reinterpret_cast<UInt32*>(outData) = kAudioDeviceTransportTypeVirtual;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyRelatedDevices:
			//	The related devices property identifies device objects that are very closely
			//	related. Generally, this is for relating devices that are packaged together
			//	in the hardware such as when the input side and the output side of a piece
			//	of hardware can be clocked separately and therefore need to be represented
			//	as separate AudioDevice objects. In such case, both devices would report
			//	that they are related to each other. Note that at minimum, a device is
			//	related to itself, so this list will always be at least one item long.

			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);
			
			//	we only have the one device...
			if(theNumberItemsToFetch > 1)
			{
				theNumberItemsToFetch = 1;
			}
			
			//	Write the devices' object IDs into the return value
			if(theNumberItemsToFetch > 0)
			{
				reinterpret_cast<AudioObjectID*>(outData)[0] = GetObjectID();
			}
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertyClockDomain:
			//	This property allows the device to declare what other devices it is
			//	synchronized with in hardware. The way it works is that if two devices have
			//	the same value for this property and the value is not zero, then the two
			//	devices are synchronized in hardware. Note that a device that either can't
			//	be synchronized with others or doesn't know should return 0 for this
			//	property.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyClockDomain for the device");
			*reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceIsAlive:
			//	This property returns whether or not the device is alive. Note that it is
			//	note uncommon for a device to be dead but still momentarily availble in the
			//	device list. In the case of this device, it will always be alive.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceIsAlive for the device");
			*reinterpret_cast<UInt32*>(outData) = 1;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceIsRunning:
			//	This property returns whether or not IO is running for the device.
			{
				ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceIsRunning for the device");
				
				//	The IsRunning state is protected by the state lock
				CAMutex::Locker theStateLocker(mStateMutex);
				
				//	return the state and how much data we are touching
				*reinterpret_cast<UInt32*>(outData) = mStartCount > 0;
				outDataSize = sizeof(UInt32);
			}
			break;

		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
			//	This property returns whether or not the device wants to be able to be the
			//	default device for content. This is the device that iTunes and QuickTime
			//	will use to play their content on and FaceTime will use as it's microhphone.
			//	Nearly all devices should allow for this.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceCanBeDefaultDevice for the device");
			*reinterpret_cast<UInt32*>(outData) = 1;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
			//	This property returns whether or not the device wants to be the system
			//	default device. This is the device that is used to play interface sounds and
			//	other incidental or UI-related sounds on. Most devices should allow this
			//	although devices with lots of latency may not want to.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceCanBeDefaultSystemDevice for the device");
			*reinterpret_cast<UInt32*>(outData) = 1;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyLatency:
			//	This property returns the presentation latency of the device. For this,
			//	device, the value is 0 due to the fact that it always vends silence.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyLatency for the device");
			*reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyStreams:
			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);
			
			//	Note that what is returned here depends on the scope requested.
			switch(inAddress.mScope)
			{
				case kAudioObjectPropertyScopeGlobal:
					//	global scope means return all streams
					if(theNumberItemsToFetch > kNumberOfStreams)
					{
						theNumberItemsToFetch = kNumberOfStreams;
					}
					
					//	fill out the list with as many objects as requested
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = mInputStreamObjectID;
					}
					if(theNumberItemsToFetch > 1)
					{
						reinterpret_cast<AudioObjectID*>(outData)[1] = mOutputStreamObjectID;
					}
					break;
					
				case kAudioObjectPropertyScopeInput:
					//	input scope means just the objects on the input side
					if(theNumberItemsToFetch > kNumberOfInputStreams)
					{
						theNumberItemsToFetch = kNumberOfInputStreams;
					}
					
					//	fill out the list with as many objects as requested
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = mInputStreamObjectID;
					}
					break;
					
				case kAudioObjectPropertyScopeOutput:
					//	output scope means just the objects on the output side
					if(theNumberItemsToFetch > kNumberOfOutputStreams)
					{
						theNumberItemsToFetch = kNumberOfOutputStreams;
					}
					
					//	fill out the list with as many objects as requested
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = mOutputStreamObjectID;
					}
					break;
			};
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
			break;

		case kAudioObjectPropertyControlList:
			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);
			if(theNumberItemsToFetch > kNumberOfControls)
			{
				theNumberItemsToFetch = kNumberOfControls;
			}
			
			//	fill out the list with as many objects as requested, which is everything
			if(theNumberItemsToFetch > 0)
			{
				reinterpret_cast<AudioObjectID*>(outData)[0] = mInputMasterVolumeControlObjectID;
			}
			if(theNumberItemsToFetch > 1)
			{
				reinterpret_cast<AudioObjectID*>(outData)[1] = mOutputMasterVolumeControlObjectID;
			}
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertySafetyOffset:
			//	This property returns the how close to now the HAL can read and write. For
			//	this, device, the value is 0 due to the fact that it always vends silence.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertySafetyOffset for the device");
			*reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyNominalSampleRate:
			//	This property returns the nominal sample rate of the device. Note that we
			//	only need to take the state lock to get this value.
			{
				ThrowIf(inDataSize < sizeof(Float64), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyNominalSampleRate for the device");
			
				//	The sample rate is protected by the state lock
				CAMutex::Locker theStateLocker(mStateMutex);
					
				//	need to lock around fetching the sample rate
				*reinterpret_cast<Float64*>(outData) = static_cast<Float64>(_HW_GetSampleRate());
				outDataSize = sizeof(Float64);
			}
			break;

		case kAudioDevicePropertyAvailableNominalSampleRates:
			//	This returns all nominal sample rates the device supports as an array of
			//	AudioValueRangeStructs. Note that for discrete sampler rates, the range
			//	will have the minimum value equal to the maximum value.
			
			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioValueRange);
			
			//	clamp it to the number of items we have
			if(theNumberItemsToFetch > 2)
			{
				theNumberItemsToFetch = 2;
			}
			
			//	fill out the return array
			if(theNumberItemsToFetch > 0)
			{
				((AudioValueRange*)outData)[0].mMinimum = 44100.0;
				((AudioValueRange*)outData)[0].mMaximum = 44100.0;
			}
			if(theNumberItemsToFetch > 1)
			{
				((AudioValueRange*)outData)[1].mMinimum = 48000.0;
				((AudioValueRange*)outData)[1].mMaximum = 48000.0;
			}
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioValueRange);
			break;
		
		case kAudioDevicePropertyIsHidden:
			//	This returns whether or not the device is visible to clients.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyIsHidden for the device");
			*reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyPreferredChannelsForStereo:
			//	This property returns which two channesl to use as left/right for stereo
			//	data by default. Note that the channel numbers are 1-based.
			ThrowIf(inDataSize < (2 * sizeof(UInt32)), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyPreferredChannelsForStereo for the device");
			((UInt32*)outData)[0] = 1;
			((UInt32*)outData)[1] = 2;
			outDataSize = 2 * sizeof(UInt32);
			break;

		case kAudioDevicePropertyPreferredChannelLayout:
			//	This property returns the default AudioChannelLayout to use for the device
			//	by default. For this device, we return a stereo ACL.
			{
				//	calcualte how big the
				UInt32 theACLSize = offsetof(AudioChannelLayout, mChannelDescriptions) + (2 * sizeof(AudioChannelDescription));
				ThrowIf(inDataSize < theACLSize, CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyPreferredChannelLayout for the device");
				((AudioChannelLayout*)outData)->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
				((AudioChannelLayout*)outData)->mChannelBitmap = 0;
				((AudioChannelLayout*)outData)->mNumberChannelDescriptions = 2;
				for(theItemIndex = 0; theItemIndex < 2; ++theItemIndex)
				{
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mChannelLabel = kAudioChannelLabel_Left + theItemIndex;
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mChannelFlags = 0;
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mCoordinates[0] = 0;
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mCoordinates[1] = 0;
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mCoordinates[2] = 0;
				}
				outDataSize = theACLSize;
			}
			break;

		case kAudioDevicePropertyZeroTimeStampPeriod:
			//	This property returns how many frames the HAL should expect to see between
			//	successive sample times in the zero time stamps this device provides.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyZeroTimeStampPeriod for the device");
			*reinterpret_cast<UInt32*>(outData) = mRingBufferFrameSize;
			outDataSize = sizeof(UInt32);
			break;

		default:
			SV_Object::GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
			break;
	};
}

void	SV_Device::Device_SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Device_GetPropertyData() method.
	
	switch(inAddress.mSelector)
	{
		case kAudioDevicePropertyNominalSampleRate:
			//	Changing the sample rate needs to be handled via the RequestConfigChange/PerformConfigChange machinery.
			{
				//	check the arguments
				ThrowIf(inDataSize != sizeof(Float64), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Device_SetPropertyData: wrong size for the data for kAudioDevicePropertyNominalSampleRate");
				ThrowIf((*((const Float64*)inData) != 44100.0) && (*((const Float64*)inData) != 48000.0), CAException(kAudioHardwareIllegalOperationError), "SV_Device::Device_SetPropertyData: unsupported value for kAudioDevicePropertyNominalSampleRate");
				
				//	we need to lock around getting the current sample rate to compare against the new rate
				UInt64 theOldSampleRate = 0;
				{
					CAMutex::Locker theStateLocker(mStateMutex);
					theOldSampleRate = _HW_GetSampleRate();
				}
				
				//	make sure that the new value is different than the old value
				UInt64 theNewSampleRate = static_cast<UInt64>(*reinterpret_cast<const Float64*>(inData));
				if(theNewSampleRate != theOldSampleRate)
				{
					//	we dispatch this so that the change can happen asynchronously
					AudioObjectID theDeviceObjectID = GetObjectID();
					CADispatchQueue::GetGlobalSerialQueue().Dispatch(false,	^{
																				SV_PlugIn::Host_RequestDeviceConfigurationChange(theDeviceObjectID, theNewSampleRate, NULL);
																			});
				}
			}
			break;
		
		default:
			SV_Object::SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
			break;
	};
}

#pragma mark Stream Property Operations

bool	SV_Device::Stream_HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Stream_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioStreamPropertyIsActive:
		case kAudioStreamPropertyDirection:
		case kAudioStreamPropertyTerminalType:
		case kAudioStreamPropertyStartingChannel:
		case kAudioStreamPropertyLatency:
		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats:
			theAnswer = true;
			break;
			
		default:
			theAnswer = SV_Object::HasProperty(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

bool	SV_Device::Stream_IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Stream_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioStreamPropertyDirection:
		case kAudioStreamPropertyTerminalType:
		case kAudioStreamPropertyStartingChannel:
		case kAudioStreamPropertyLatency:
		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats:
			theAnswer = false;
			break;
		
		case kAudioStreamPropertyIsActive:
		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
			theAnswer = true;
			break;
		
		default:
			theAnswer = SV_Object::IsPropertySettable(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

UInt32	SV_Device::Stream_GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Stream_GetPropertyData() method.
	
	UInt32 theAnswer = 0;
	switch(inAddress.mSelector)
	{
		case kAudioStreamPropertyIsActive:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioStreamPropertyDirection:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioStreamPropertyTerminalType:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioStreamPropertyStartingChannel:
			theAnswer = sizeof(UInt32);
			break;
		
		case kAudioStreamPropertyLatency:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
			theAnswer = sizeof(AudioStreamBasicDescription);
			break;

		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats:
			theAnswer = 2 * sizeof(AudioStreamRangedDescription);
			break;

		default:
			theAnswer = SV_Object::GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
			break;
	};
	return theAnswer;
}

void	SV_Device::Stream_GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required.
	//	Also, since most of the data that will get returned is static, there are few instances where
	//	it is necessary to lock the state mutex.
	
	UInt32 theNumberItemsToFetch;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyBaseClass:
			//	The base class for kAudioStreamClassID is kAudioObjectClassID
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_GetPropertyData: not enough space for the return value of kAudioObjectPropertyBaseClass for the volume control");
			*reinterpret_cast<AudioClassID*>(outData) = kAudioObjectClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyClass:
			//	Streams are of the class, kAudioStreamClassID
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_GetPropertyData: not enough space for the return value of kAudioObjectPropertyClass for the volume control");
			*reinterpret_cast<AudioClassID*>(outData) = kAudioStreamClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyOwner:
			//	The stream's owner is the device object
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_GetPropertyData: not enough space for the return value of kAudioObjectPropertyOwner for the volume control");
			*reinterpret_cast<AudioObjectID*>(outData) = GetObjectID();
			outDataSize = sizeof(AudioObjectID);
			break;
			
		case kAudioStreamPropertyIsActive:
			//	This property tells the device whether or not the given stream is going to
			//	be used for IO. Note that we need to take the state lock to examine this
			//	value.
			{
				ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyIsActive for the stream");
				
				//	lock the state mutex
				CAMutex::Locker theStateLocker(mStateMutex);
				
				//	return the requested value
				*reinterpret_cast<UInt32*>(outData) = (inAddress.mScope == kAudioObjectPropertyScopeInput) ? mInputStreamIsActive : mOutputStreamIsActive;
				outDataSize = sizeof(UInt32);
			}
			break;

		case kAudioStreamPropertyDirection:
			//	This returns whether the stream is an input stream or an output stream.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyDirection for the stream");
			*reinterpret_cast<UInt32*>(outData) = (inObjectID == mInputStreamObjectID) ? 1 : 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioStreamPropertyTerminalType:
			//	This returns a value that indicates what is at the other end of the stream
			//	such as a speaker or headphones, or a microphone. Values for this property
			//	are defined in <CoreAudio/AudioHardwareBase.h>
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyTerminalType for the stream");
			*reinterpret_cast<UInt32*>(outData) = (inObjectID == mInputStreamObjectID) ? kAudioStreamTerminalTypeMicrophone : kAudioStreamTerminalTypeSpeaker;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioStreamPropertyStartingChannel:
			//	This property returns the absolute channel number for the first channel in
			//	the stream. For exmaple, if a device has two output streams with two
			//	channels each, then the starting channel number for the first stream is 1
			//	and ths starting channel number fo the second stream is 3.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyStartingChannel for the stream");
			*reinterpret_cast<UInt32*>(outData) = 1;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioStreamPropertyLatency:
			//	This property returns any additonal presentation latency the stream has.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyStartingChannel for the stream");
			*reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
			//	This returns the current format of the stream in an AudioStreamBasicDescription.
			//	For devices that don't override the mix operation, the virtual format has to be the
			//	same as the physical format.
			{
				ThrowIf(inDataSize < sizeof(AudioStreamBasicDescription), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyVirtualFormat for the stream");
				
				//	lock the state mutex
				CAMutex::Locker theStateLocker(mStateMutex);
				
				//	This particular device always vends  16 bit native endian signed integers
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mSampleRate = static_cast<Float64>(_HW_GetSampleRate());
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mFormatID = kAudioFormatLinearPCM;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mBytesPerPacket = 4;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mFramesPerPacket = 1;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mBytesPerFrame = 4;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mChannelsPerFrame = 2;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mBitsPerChannel = 16;
				outDataSize = sizeof(AudioStreamBasicDescription);
			}
			break;

		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats:
			//	This returns an array of AudioStreamRangedDescriptions that describe what
			//	formats are supported.

			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioStreamRangedDescription);
			
			//	clamp it to the number of items we have
			if(theNumberItemsToFetch > 2)
			{
				theNumberItemsToFetch = 2;
			}
			
			//	fill out the return array
			if(theNumberItemsToFetch > 0)
			{
				((AudioStreamRangedDescription*)outData)[0].mFormat.mSampleRate = 44100.0;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mFormatID = kAudioFormatLinearPCM;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mBytesPerPacket = 4;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mFramesPerPacket = 1;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mBytesPerFrame = 4;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mChannelsPerFrame = 2;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mBitsPerChannel = 16;
				((AudioStreamRangedDescription*)outData)[0].mSampleRateRange.mMinimum = 44100.0;
				((AudioStreamRangedDescription*)outData)[0].mSampleRateRange.mMaximum = 44100.0;
			}
			if(theNumberItemsToFetch > 1)
			{
				((AudioStreamRangedDescription*)outData)[1].mFormat.mSampleRate = 48000.0;
				((AudioStreamRangedDescription*)outData)[1].mFormat.mFormatID = kAudioFormatLinearPCM;
				((AudioStreamRangedDescription*)outData)[1].mFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
				((AudioStreamRangedDescription*)outData)[1].mFormat.mBytesPerPacket = 4;
				((AudioStreamRangedDescription*)outData)[1].mFormat.mFramesPerPacket = 1;
				((AudioStreamRangedDescription*)outData)[1].mFormat.mBytesPerFrame = 4;
				((AudioStreamRangedDescription*)outData)[1].mFormat.mChannelsPerFrame = 2;
				((AudioStreamRangedDescription*)outData)[1].mFormat.mBitsPerChannel = 16;
				((AudioStreamRangedDescription*)outData)[1].mSampleRateRange.mMinimum = 48000.0;
				((AudioStreamRangedDescription*)outData)[1].mSampleRateRange.mMaximum = 48000.0;
			}
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioStreamRangedDescription);
			break;

		default:
			SV_Object::GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
			break;
	};
}

void	SV_Device::Stream_SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Stream_GetPropertyData() method.
	
	switch(inAddress.mSelector)
	{
		case kAudioStreamPropertyIsActive:
			{
				//	Changing the active state of a stream doesn't affect IO or change the structure
				//	so we can just save the state and send the notification.
				ThrowIf(inDataSize != sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_SetPropertyData: wrong size for the data for kAudioDevicePropertyNominalSampleRate");
				bool theNewIsActive = *reinterpret_cast<const UInt32*>(inData) != 0;
				
				CAMutex::Locker theStateLocker(mStateMutex);
				if(inObjectID == mInputStreamObjectID)
				{
					if(mInputStreamIsActive != theNewIsActive)
					{
						mInputStreamIsActive = theNewIsActive;
					}
				}
				else
				{
					if(mOutputStreamIsActive != theNewIsActive)
					{
						mOutputStreamIsActive = theNewIsActive;
					}
				}
			}
			break;
			
		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
			{
				//	Changing the stream format needs to be handled via the
				//	RequestConfigChange/PerformConfigChange machinery. Note that because this
				//	device only supports 2 channel 16 bit integer data, the only thing that can
				//	change is the sample rate.
				ThrowIf(inDataSize != sizeof(AudioStreamBasicDescription), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Stream_SetPropertyData: wrong size for the data for kAudioStreamPropertyPhysicalFormat");
				
				const AudioStreamBasicDescription* theNewFormat = reinterpret_cast<const AudioStreamBasicDescription*>(inData);
				ThrowIf(theNewFormat->mFormatID != kAudioFormatLinearPCM, CAException(kAudioDeviceUnsupportedFormatError), "SV_Device::Stream_SetPropertyData: unsupported format ID for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mFormatFlags != (kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked), CAException(kAudioDeviceUnsupportedFormatError), "SV_Device::Stream_SetPropertyData: unsupported format flags for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mBytesPerPacket != 4, CAException(kAudioDeviceUnsupportedFormatError), "SV_Device::Stream_SetPropertyData: unsupported bytes per packet for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mFramesPerPacket != 1, CAException(kAudioDeviceUnsupportedFormatError), "SV_Device::Stream_SetPropertyData: unsupported frames per packet for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mBytesPerFrame != 4, CAException(kAudioDeviceUnsupportedFormatError), "SV_Device::Stream_SetPropertyData: unsupported bytes per frame for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mChannelsPerFrame != 2, CAException(kAudioDeviceUnsupportedFormatError), "SV_Device::Stream_SetPropertyData: unsupported channels per frame for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mBitsPerChannel != 16, CAException(kAudioDeviceUnsupportedFormatError), "SV_Device::Stream_SetPropertyData: unsupported bits per channel for kAudioStreamPropertyPhysicalFormat");
				ThrowIf((theNewFormat->mSampleRate != 44100.0) && (theNewFormat->mSampleRate != 48000.0), CAException(kAudioDeviceUnsupportedFormatError), "SV_Device::Stream_SetPropertyData: unsupported sample rate for kAudioStreamPropertyPhysicalFormat");
			
				//	we need to lock around getting the current sample rate to compare against the new rate
				UInt64 theOldSampleRate = 0;
				{
					CAMutex::Locker theStateLocker(mStateMutex);
					theOldSampleRate = _HW_GetSampleRate();
				}
				
				//	make sure that the new value is different than the old value
				UInt64 theNewSampleRate = static_cast<UInt64>(*reinterpret_cast<const Float64*>(inData));
				if(theNewSampleRate != theOldSampleRate)
				{
					//	we dispatch this so that the change can happen asynchronously
					AudioObjectID theDeviceObjectID = GetObjectID();
					CADispatchQueue::GetGlobalSerialQueue().Dispatch(false,	^{
																				SV_PlugIn::Host_RequestDeviceConfigurationChange(theDeviceObjectID, theNewSampleRate, NULL);
																			});
				}
			}
			break;
		
		default:
			SV_Object::SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
			break;
	};
}

#pragma mark Control Property Operations

bool	SV_Device::Control_HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Control_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioControlPropertyScope:
		case kAudioControlPropertyElement:
		case kAudioLevelControlPropertyScalarValue:
		case kAudioLevelControlPropertyDecibelValue:
		case kAudioLevelControlPropertyDecibelRange:
		case kAudioLevelControlPropertyConvertScalarToDecibels:
		case kAudioLevelControlPropertyConvertDecibelsToScalar:
			theAnswer = true;
			break;
		
		default:
			theAnswer = SV_Object::HasProperty(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

bool	SV_Device::Control_IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Control_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioControlPropertyScope:
		case kAudioControlPropertyElement:
		case kAudioLevelControlPropertyDecibelRange:
		case kAudioLevelControlPropertyConvertScalarToDecibels:
		case kAudioLevelControlPropertyConvertDecibelsToScalar:
			theAnswer = false;
			break;
		
		case kAudioLevelControlPropertyScalarValue:
		case kAudioLevelControlPropertyDecibelValue:
			theAnswer = true;
			break;
		
		default:
			theAnswer = SV_Object::IsPropertySettable(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

UInt32	SV_Device::Control_GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Control_GetPropertyData() method.
	
	UInt32 theAnswer = 0;
	switch(inAddress.mSelector)
	{
		case kAudioControlPropertyScope:
			theAnswer = sizeof(AudioObjectPropertyScope);
			break;

		case kAudioControlPropertyElement:
			theAnswer = sizeof(AudioObjectPropertyElement);
			break;

		case kAudioLevelControlPropertyScalarValue:
			theAnswer = sizeof(Float32);
			break;

		case kAudioLevelControlPropertyDecibelValue:
			theAnswer = sizeof(Float32);
			break;

		case kAudioLevelControlPropertyDecibelRange:
			theAnswer = sizeof(AudioValueRange);
			break;

		case kAudioLevelControlPropertyConvertScalarToDecibels:
			theAnswer = sizeof(Float32);
			break;

		case kAudioLevelControlPropertyConvertDecibelsToScalar:
			theAnswer = sizeof(Float32);
			break;

		default:
			theAnswer = SV_Object::GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
			break;
	};
	return theAnswer;
}

void	SV_Device::Control_GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required.
	//	Also, since most of the data that will get returned is static, there are few instances where
	//	it is necessary to lock the state mutex.
	
	SInt32 theControlRawValue;
	Float32 theVolumeValue;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyBaseClass:
			//	The base class for kAudioVolumeControlClassID is kAudioLevelControlClassID
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioObjectPropertyBaseClass for the volume control");
			*reinterpret_cast<AudioClassID*>(outData) = kAudioLevelControlClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyClass:
			//	Volume controls are of the class, kAudioVolumeControlClassID
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioObjectPropertyClass for the volume control");
			*reinterpret_cast<AudioClassID*>(outData) = kAudioVolumeControlClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyOwner:
			//	The control's owner is the device object
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioObjectPropertyOwner for the volume control");
			*reinterpret_cast<AudioObjectID*>(outData) = GetObjectID();
			outDataSize = sizeof(AudioObjectID);
			break;
			
		case kAudioControlPropertyScope:
			//	This property returns the scope that the control is attached to.
			ThrowIf(inDataSize < sizeof(AudioObjectPropertyScope), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioControlPropertyScope for the volume control");
			*reinterpret_cast<AudioObjectPropertyScope*>(outData) = (inObjectID == mInputMasterVolumeControlObjectID) ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput;
			outDataSize = sizeof(AudioObjectPropertyScope);
			break;

		case kAudioControlPropertyElement:
			//	This property returns the element that the control is attached to.
			ThrowIf(inDataSize < sizeof(AudioObjectPropertyElement), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioControlPropertyElement for the volume control");
            *reinterpret_cast<AudioObjectPropertyElement*>(outData) = kAudioObjectPropertyElementMain;
			outDataSize = sizeof(AudioObjectPropertyElement);
			break;

		case kAudioLevelControlPropertyScalarValue:
			//	This returns the value of the control in the normalized range of 0 to 1.
			{
				ThrowIf(inDataSize < sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyScalarValue for the volume control");
				CAMutex::Locker theStateLocker(mStateMutex);
				theControlRawValue = _HW_GetVolumeControlValue((inObjectID == mInputMasterVolumeControlObjectID) ? kSyncVoiceDriver_Control_MasterInputVolume : kSyncVoiceDriver_Control_MasterOutputVolume);
				*reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertRawToScalar(theControlRawValue);
				outDataSize = sizeof(Float32);
			}
			break;

		case kAudioLevelControlPropertyDecibelValue:
			//	This returns the dB value of the control.
			{
				ThrowIf(inDataSize < sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyDecibelValue for the volume control");
				CAMutex::Locker theStateLocker(mStateMutex);
				theControlRawValue = _HW_GetVolumeControlValue((inObjectID == mInputMasterVolumeControlObjectID) ? kSyncVoiceDriver_Control_MasterInputVolume : kSyncVoiceDriver_Control_MasterOutputVolume);
				*reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertRawToDB(theControlRawValue);
				outDataSize = sizeof(Float32);
			}
			break;

		case kAudioLevelControlPropertyDecibelRange:
			//	This returns the dB range of the control.
			ThrowIf(inDataSize < sizeof(AudioValueRange), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyDecibelRange for the volume control");
			reinterpret_cast<AudioValueRange*>(outData)->mMinimum = mVolumeCurve.GetMinimumDB();
			reinterpret_cast<AudioValueRange*>(outData)->mMaximum = mVolumeCurve.GetMaximumDB();
			outDataSize = sizeof(AudioValueRange);
			break;

		case kAudioLevelControlPropertyConvertScalarToDecibels:
			//	This takes the scalar value in outData and converts it to dB.
			ThrowIf(inDataSize < sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyDecibelValue for the volume control");
			
			//	clamp the value to be between 0 and 1
			theVolumeValue = *reinterpret_cast<Float32*>(outData);
			theVolumeValue = std::min(1.0f, std::max(0.0f, theVolumeValue));
			
			//	do the conversion
			*reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertScalarToDB(theVolumeValue);
			
			//	report how much we wrote
			outDataSize = sizeof(Float32);
			break;

		case kAudioLevelControlPropertyConvertDecibelsToScalar:
			//	This takes the dB value in outData and converts it to scalar.
			ThrowIf(inDataSize < sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "SV_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyDecibelValue for the volume control");
			
			//	clamp the value to be between kVolume_MinDB and kVolume_MaxDB
			theVolumeValue = *reinterpret_cast<Float32*>(outData);
			theVolumeValue = std::min(kSyncVoiceDriver_Control_MaxDbVolumeValue, std::max(kSyncVoiceDriver_Control_MinDBVolumeValue, theVolumeValue));
			
			//	do the conversion
			*reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertDBToScalar(theVolumeValue);
			
			//	report how much we wrote
			outDataSize = sizeof(Float32);
			break;

		default:
			SV_Object::GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
			break;
	};
}

void	SV_Device::Control_SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Control_GetPropertyData() method.
	
	bool sendNotifications = false;
	kern_return_t theError = 0;
	Float32 theNewVolumeValue;
	SInt32 theNewRawVolumeValue;
	switch(inAddress.mSelector)
	{
		case kAudioLevelControlPropertyScalarValue:
			//	For the scalar volume, we clamp the new value to [0, 1]. Note that if this
			//	value changes, it implies that the dB value changed too.
			{
				ThrowIf(inDataSize != sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "NullAudio_SetControlPropertyData: wrong size for the data for kAudioLevelControlPropertyScalarValue");
				theNewVolumeValue = *((const Float32*)inData);
				theNewVolumeValue = std::min(1.0f, std::max(0.0f, theNewVolumeValue));
				theNewRawVolumeValue = mVolumeCurve.ConvertScalarToRaw(theNewVolumeValue);
				CAMutex::Locker theStateLocker(mStateMutex);
				theError = _HW_SetVolumeControlValue((inObjectID == mInputMasterVolumeControlObjectID) ? kSyncVoiceDriver_Control_MasterInputVolume : kSyncVoiceDriver_Control_MasterOutputVolume, theNewRawVolumeValue);
				sendNotifications = theError == 0;
			}
			break;
		
		case kAudioLevelControlPropertyDecibelValue:
			//	For the dB value, we first convert it to a scalar value since that is how
			//	the value is tracked. Note that if this value changes, it implies that the
			//	scalar value changes as well.
			{
				ThrowIf(inDataSize != sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "NullAudio_SetControlPropertyData: wrong size for the data for kAudioLevelControlPropertyScalarValue");
				theNewVolumeValue = *((const Float32*)inData);
				theNewVolumeValue = std::min(kSyncVoiceDriver_Control_MaxDbVolumeValue, std::max(kSyncVoiceDriver_Control_MinDBVolumeValue, theNewVolumeValue));
				theNewRawVolumeValue = mVolumeCurve.ConvertDBToRaw(theNewVolumeValue);
				CAMutex::Locker theStateLocker(mStateMutex);
				theError = _HW_SetVolumeControlValue((inObjectID == mInputMasterVolumeControlObjectID) ? kSyncVoiceDriver_Control_MasterInputVolume : kSyncVoiceDriver_Control_MasterOutputVolume, theNewRawVolumeValue);
				sendNotifications = theError == 0;
			}
			break;
		
		default:
			SV_Object::SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
			break;
	};
	
	if(sendNotifications)
	{
		CADispatchQueue::GetGlobalSerialQueue().Dispatch(false,	^{
            AudioObjectPropertyAddress theChangedProperties[] = { { kAudioLevelControlPropertyScalarValue, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain }, { kAudioLevelControlPropertyDecibelValue, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain } };
																	SV_PlugIn::Host_PropertiesChanged(inObjectID, 2, theChangedProperties);
																});
	}
}

#pragma mark IO Operations

void	SV_Device::StartIO()
{
	//	Starting/Stopping IO needs to be reference counted due to the possibility of multiple clients starting IO
	CAMutex::Locker theStateLocker(mStateMutex);
	
	//	make sure we can start
	ThrowIf(mStartCount == UINT64_MAX, CAException(kAudioHardwareIllegalOperationError), "SV_Device::StartIO: failed to start because the ref count was maxxed out already");
	
	//	we only tell the hardware to start if this is the first time IO has been started
	if(mStartCount == 0)
	{
		kern_return_t theError = _HW_StartIO();
		ThrowIfKernelError(theError, CAException(theError), "SV_Device::StartIO: failed to start because of an error calling down to the driver");
	}
	++mStartCount;
}

void	SV_Device::StopIO()
{
	//	Starting/Stopping IO needs to be reference counted due to the possibility of multiple clients starting IO
	CAMutex::Locker theStateLocker(mStateMutex);
	
	//	we tell the hardware to stop if this is the last stop call
	if(mStartCount == 1)
	{
		_HW_StopIO();
		mStartCount = 0;
	}
	else if(mStartCount > 1)
	{
		--mStartCount;
	}
}

void	SV_Device::GetZeroTimeStamp(Float64& outSampleTime, UInt64& outHostTime, UInt64& outSeed) const
{
	//	accessing the mapped memory requires holding the IO mutex
	CAMutex::Locker theIOLocker(mIOMutex);
	
	//	read from the engine status struct in a loop to guarantee consistency
	UInt64 theSampleTime1;
	UInt64 theSampleTime2;
	UInt64 theHostTime1;
	UInt64 theHostTime2;
	do
	{
		theHostTime1 = mDriverStatus->mHostTime;
		theSampleTime1 = mDriverStatus->mSampleTime;
		theHostTime2 = mDriverStatus->mHostTime;
		theSampleTime2 = mDriverStatus->mSampleTime;
	}
	while((theSampleTime1 != theSampleTime2) || (theHostTime1 != theHostTime2));
	
	//	set the return values
	outSampleTime = theSampleTime1;
	outHostTime = theHostTime1;
	outSeed = 0;
}

void	SV_Device::WillDoIOOperation(UInt32 inOperationID, bool& outWillDo, bool& outWillDoInPlace) const
{
	switch(inOperationID)
	{
		case kAudioServerPlugInIOOperationReadInput:
		case kAudioServerPlugInIOOperationWriteMix:
			outWillDo = true;
			outWillDoInPlace = true;
			break;
			
		case kAudioServerPlugInIOOperationThread:
		case kAudioServerPlugInIOOperationCycle:
		case kAudioServerPlugInIOOperationConvertInput:
		case kAudioServerPlugInIOOperationProcessInput:
		case kAudioServerPlugInIOOperationProcessOutput:
		case kAudioServerPlugInIOOperationMixOutput:
		case kAudioServerPlugInIOOperationProcessMix:
		case kAudioServerPlugInIOOperationConvertMix:
		default:
			outWillDo = false;
			outWillDoInPlace = true;
			break;
			
	};
}

void	SV_Device::BeginIOOperation(UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo& inIOCycleInfo)
{
	#pragma unused(inOperationID, inIOBufferFrameSize, inIOCycleInfo)
}

void	SV_Device::DoIOOperation(AudioObjectID inStreamObjectID, UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo& inIOCycleInfo, void* ioMainBuffer, void* ioSecondaryBuffer)
{
	#pragma unused(inStreamObjectID, ioSecondaryBuffer)
	switch(inOperationID)
	{
		case kAudioServerPlugInIOOperationReadInput:
			ReadInputData(inIOBufferFrameSize, inIOCycleInfo.mInputTime.mSampleTime, ioMainBuffer);
			break;
			
		case kAudioServerPlugInIOOperationWriteMix:
			WriteOutputData(inIOBufferFrameSize, inIOCycleInfo.mOutputTime.mSampleTime, ioMainBuffer);
			break;
	};
}

void	SV_Device::EndIOOperation(UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo& inIOCycleInfo)
{
	#pragma unused(inOperationID, inIOBufferFrameSize, inIOCycleInfo)
}

void	SV_Device::ReadInputData(UInt32 inIOBufferFrameSize, Float64 inSampleTime, void* outBuffer)
{
	//	we need to be holding the IO lock to do this
	CAMutex::Locker theIOLocker(mIOMutex);
	
	//	figure out where we are starting
	SInt64 theSampleTime = static_cast<SInt64>(inSampleTime);
	SInt64 theStartFrameOffset = theSampleTime % mRingBufferFrameSize;
	if (theStartFrameOffset < 0)
		theStartFrameOffset += mRingBufferFrameSize;
	
	//	figure out how many frames we need to copy
	SInt64 theNumberFramesToCopy1 = inIOBufferFrameSize;
	SInt64 theNumberFramesToCopy2 = 0;
	if((theStartFrameOffset + theNumberFramesToCopy1) > mRingBufferFrameSize)
	{
		theNumberFramesToCopy1 = mRingBufferFrameSize - theStartFrameOffset;
		theNumberFramesToCopy2 = inIOBufferFrameSize - theNumberFramesToCopy1;
	}
	
	//	do the copying (the byte sizes here assume a 16 bit stereo sample format)
	Byte* theDestination = reinterpret_cast<Byte*>(outBuffer);
	memcpy(theDestination, mInputStreamRingBuffer + (theStartFrameOffset * 4), theNumberFramesToCopy1 * 4);
	if(theNumberFramesToCopy2 > 0)
	{
		memcpy(theDestination + (theNumberFramesToCopy1 * 4), mInputStreamRingBuffer, theNumberFramesToCopy2 * 4);
	}
}

void	SV_Device::WriteOutputData(UInt32 inIOBufferFrameSize, Float64 inSampleTime, const void* inBuffer)
{
	//	we need to be holding the IO lock to do this
	CAMutex::Locker theIOLocker(mIOMutex);
	
	//	figure out where we are starting
	UInt64 theSampleTime = static_cast<UInt64>(inSampleTime);
	UInt32 theStartFrameOffset = theSampleTime % mRingBufferFrameSize;
	
	//	figure out how many frames we need to copy
	UInt32 theNumberFramesToCopy1 = inIOBufferFrameSize;
	UInt32 theNumberFramesToCopy2 = 0;
	if((theStartFrameOffset + theNumberFramesToCopy1) > mRingBufferFrameSize)
	{
		theNumberFramesToCopy1 = mRingBufferFrameSize - theStartFrameOffset;
		theNumberFramesToCopy2 = inIOBufferFrameSize - theNumberFramesToCopy1;
	}
	
	//	do the copying (the byte sizes here assume a 16 bit stereo sample format)
	const Byte* theSource = reinterpret_cast<const Byte*>(inBuffer);
	memcpy(mOutputStreamRingBuffer + (theStartFrameOffset * 4), theSource, theNumberFramesToCopy1 * 4);
	if(theNumberFramesToCopy2 > 0)
	{
		memcpy(mOutputStreamRingBuffer, theSource + (theNumberFramesToCopy1 * 4), theNumberFramesToCopy2 * 4);
	}
}

#pragma mark Hardware Accessors

CFStringRef	SV_Device::HW_CopyDeviceUID(io_object_t inIOObject)
{
	CFStringRef theAnswer = NULL;
	SV_IOKitObject::CopyProperty_CFString(inIOObject, CFSTR(kSyncVoiceDriver_RegistryKey_DeviceUID), true, theAnswer);
	return theAnswer;
}

void	SV_Device::_HW_Open()
{
	//	open the connection to the IOKit object
	mIOKitObject.OpenConnection();
	
	//	open the user-client
	mIOKitObject.CallMethod(kSyncVoiceDriver_Method_Open, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
	
	//	map in the buffers
	UInt32 theBufferSize = 0;
	mDriverStatus = reinterpret_cast<SyncVoiceDriverStatus*>(mIOKitObject.MapMemory(kSyncVoiceDriver_Buffer_Status, kIOMapAnywhere, theBufferSize));
	mInputStreamRingBuffer = reinterpret_cast<Byte*>(mIOKitObject.MapMemory(kSyncVoiceDriver_Buffer_Input, kIOMapAnywhere, theBufferSize));
	mOutputStreamRingBuffer = reinterpret_cast<Byte*>(mIOKitObject.MapMemory(kSyncVoiceDriver_Buffer_Output, kIOMapAnywhere, theBufferSize));
	
	//	get the sample rate, ring buffer size, and control values to prime the shadows
	_HW_GetSampleRate();
	_HW_GetRingBufferFrameSize();
	_HW_GetVolumeControlValue(kSyncVoiceDriver_Control_MasterInputVolume);
	_HW_GetVolumeControlValue(kSyncVoiceDriver_Control_MasterOutputVolume);
}

void	SV_Device::_HW_Close()
{
	//	release the buffers
	mIOKitObject.ReleaseMemory(mDriverStatus, kSyncVoiceDriver_Buffer_Status);
	mIOKitObject.ReleaseMemory(mInputStreamRingBuffer, kSyncVoiceDriver_Buffer_Input);
	mIOKitObject.ReleaseMemory(mOutputStreamRingBuffer, kSyncVoiceDriver_Buffer_Output);
	
	//	close the user client
	mIOKitObject.CallMethod(kSyncVoiceDriver_Method_Close, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
	
	//	close the connection
	mIOKitObject.CloseConnection();
}

kern_return_t	SV_Device::_HW_StartIO()
{
	return mIOKitObject.CallMethod(kSyncVoiceDriver_Method_StartHardware, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
}

void	SV_Device::_HW_StopIO()
{
	mIOKitObject.CallMethod(kSyncVoiceDriver_Method_StopHardware, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
}

UInt64	SV_Device::_HW_GetSampleRate() const
{
	mIOKitObject.CopyProperty_UInt64(CFSTR(kSyncVoiceDriver_RegistryKey_SampleRate), true, const_cast<SV_Device*>(this)->mSampleRateShadow);
	return mSampleRateShadow;
}

kern_return_t	SV_Device::_HW_SetSampleRate(UInt64 inNewSampleRate)
{
	return mIOKitObject.CallMethod(kSyncVoiceDriver_Method_SetSampleRate, &inNewSampleRate, 1, NULL, 0, NULL, NULL, NULL, NULL);
}

UInt32	SV_Device::_HW_GetRingBufferFrameSize() const
{
	mIOKitObject.CopyProperty_UInt32(CFSTR(kSyncVoiceDriver_RegistryKey_RingBufferFrameSize), true, const_cast<SV_Device*>(this)->mRingBufferFrameSize);
	return mRingBufferFrameSize;
}

SInt32	SV_Device::_HW_GetVolumeControlValue(int inControlID) const
{
	//	get the value from the kernel
	UInt64 theControlID = static_cast<UInt64>(inControlID);
	UInt64 theControlValue = 0;
	UInt32 theNumberOutputArguments = 1;
	const_cast<SV_Device*>(this)->mIOKitObject.CallMethod(kSyncVoiceDriver_Method_GetControlValue, &theControlID, 1, NULL, 0, &theControlValue, &theNumberOutputArguments, NULL, NULL);

	//	store the new value in the shadow
	switch(inControlID)
	{
		case kSyncVoiceDriver_Control_MasterInputVolume:
			const_cast<SV_Device*>(this)->mInputMasterVolumeControlRawValueShadow = static_cast<SInt32>(theControlValue);
			break;
			
		case kSyncVoiceDriver_Control_MasterOutputVolume:
			const_cast<SV_Device*>(this)->mOutputMasterVolumeControlRawValueShadow = static_cast<SInt32>(theControlValue);
			break;
	};
	
	//	return the value
	return static_cast<SInt32>(theControlValue);
}

kern_return_t	SV_Device::_HW_SetVolumeControlValue(int inControlID, SInt32 inNewControlValue)
{
	UInt64 theInputArguments[] = { static_cast<UInt64>(inControlID), static_cast<UInt64>(inNewControlValue) };
	kern_return_t theError = mIOKitObject.CallMethod(kSyncVoiceDriver_Method_SetControlValue, theInputArguments, 2, NULL, 0, NULL, NULL, NULL, NULL);
	
	//	make sure the new value is in the proper range
	inNewControlValue = std::min(std::max(kSyncVoiceDriver_Control_MinRawVolumeValue, inNewControlValue), kSyncVoiceDriver_Control_MaxRawVolumeValue);
	
	//	if there wasn't an error, the new value was applied, so we need to update the shadow
	if(theError == 0)
	{
		switch(inControlID)
		{
			case kSyncVoiceDriver_Control_MasterInputVolume:
				mInputMasterVolumeControlRawValueShadow = inNewControlValue;
				break;
				
			case kSyncVoiceDriver_Control_MasterOutputVolume:
				mOutputMasterVolumeControlRawValueShadow = inNewControlValue;
				break;
		};
	}
	
	return theError;
}

#pragma mark Implementation

void	SV_Device::PerformConfigChange(UInt64 inChangeAction, void* inChangeInfo)
{
	#pragma unused(inChangeInfo)
	
	//	this device only supports chagning the sample rate, which is stored in inChangeAction
	UInt64 theNewSampleRate = inChangeAction;
	
	//	make sure we support the new sample rate
	if((theNewSampleRate == 44100) || (theNewSampleRate == 48000))
	{
		//	we need to lock the state lock around telling the hardware about the new sample rate
		CAMutex::Locker theStateLocker(mStateMutex);
		_HW_SetSampleRate(theNewSampleRate);
	}
}

void	SV_Device::AbortConfigChange(UInt64 inChangeAction, void* inChangeInfo)
{
	#pragma unused(inChangeAction, inChangeInfo)
	
	//	this device doesn't need to do anything special if a change request gets aborted
}
