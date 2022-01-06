/*
See LICENSE folder for this sample’s licensing information.

Abstract:
A minimal user-space driver.
*/

/*==================================================================================================
	SV_IOKit.cpp
==================================================================================================*/

//==================================================================================================
//	Includes
//==================================================================================================

//	Self Include
#include "SV_IOKit.h"

//	PublicUtility Includes
#include "CACFArray.h"
#include "CACFDictionary.h"
#include "CACFNumber.h"
#include "CACFString.h"
#include "CADebugMacros.h"
#include "CAException.h"

//	Local Includes
#include <CoreAudio/AudioHardwareBase.h>

//==================================================================================================
//	SV_IOKitObject
//==================================================================================================

SV_IOKitObject::SV_IOKitObject()
:
	mObject(IO_OBJECT_NULL),
	mConnection(IO_OBJECT_NULL),
	mProperties(static_cast<CFMutableDictionaryRef>(NULL), true),
	mUserProperties(static_cast<CFMutableDictionaryRef>(NULL), true),
	mAlwaysLoadPropertiesFromRegistry(true),
	mIsAlive(true)
{
}

SV_IOKitObject::SV_IOKitObject(io_object_t inObject)
:
	mObject(inObject),
	mConnection(IO_OBJECT_NULL),
	mProperties(static_cast<CFMutableDictionaryRef>(NULL), true),
	mUserProperties(static_cast<CFMutableDictionaryRef>(NULL), true),
	mAlwaysLoadPropertiesFromRegistry(true),
	mIsAlive(true)
{
	//	Note that we don't retain anything here as this constructor will consume a reference. In
	//	other words, this constructor essentially takes ownership of the object.
}

SV_IOKitObject::SV_IOKitObject(const SV_IOKitObject& inObject)
:
	mObject(inObject.mObject),
	mConnection(IO_OBJECT_NULL),
	mProperties(inObject.mProperties),
	mUserProperties(inObject.mUserProperties),
	mAlwaysLoadPropertiesFromRegistry(inObject.mAlwaysLoadPropertiesFromRegistry),
	mIsAlive(inObject.mIsAlive)
{
	Retain();
}

SV_IOKitObject&	SV_IOKitObject::operator=(const SV_IOKitObject& inObject)
{
	if (mObject != inObject.mObject)
	{
		CloseConnection();
		Release();
		mObject = inObject.mObject;
		mAlwaysLoadPropertiesFromRegistry = inObject.mAlwaysLoadPropertiesFromRegistry;
		mIsAlive = inObject.mIsAlive;
		mProperties = inObject.mProperties;
		if(mProperties.IsValid())
		{
			CFRelease(mProperties.GetCFDictionary());
		}
		mUserProperties = inObject.mUserProperties;
		if(mUserProperties.IsValid())
		{
			CFRelease(mUserProperties.GetCFDictionary());
		}
		Retain();
	}
	return *this;
}

SV_IOKitObject::~SV_IOKitObject()
{		
	CloseConnection();
	Release();
}

io_object_t	SV_IOKitObject::GetObject() const
{
	return mObject;
}

io_object_t	SV_IOKitObject::CopyObject()
{
	Retain();
	return mObject;
}

bool	SV_IOKitObject::IsValid() const
{
	return mObject != IO_OBJECT_NULL;
}

bool	SV_IOKitObject::IsEqualTo(io_object_t inObject) const
{
	return IOObjectIsEqualTo(mObject, inObject);
}

bool	SV_IOKitObject::ConformsTo(const io_name_t inClassName)
{
	return IOObjectConformsTo(mObject, inClassName);
}

bool	SV_IOKitObject::IsServiceAlive() const
{
	return mIsAlive;
}

void	SV_IOKitObject::ServiceWasTerminated()
{
	mIsAlive = false;
}

bool	SV_IOKitObject::TestForLiveness(io_object_t inObject)
{
	bool theAnswer = false;
	if(inObject != IO_OBJECT_NULL)
	{
		CFMutableDictionaryRef theProperties = NULL;
		kern_return_t theError = IORegistryEntryCreateCFProperties(inObject, &theProperties, NULL, 0);
		if(theProperties != NULL)
		{
			CFRelease(theProperties);
		}
		theAnswer = theError == 0;
	}
	return theAnswer;
}

bool	SV_IOKitObject::HasProperty(CFStringRef inKey, bool inIsInUserDictionary) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		return mUserProperties.HasKey(inKey);
	}
	return mProperties.HasKey(inKey);
}

bool	SV_IOKitObject::CopyProperty_bool(CFStringRef inKey, bool inIsInUserDictionary, bool& outValue) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		return mUserProperties.GetBool(inKey, outValue);
	}
	return mProperties.GetBool(inKey, outValue);
}

bool	SV_IOKitObject::CopyProperty_SInt32(CFStringRef inKey, bool inIsInUserDictionary, SInt32& outValue) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		return mUserProperties.GetSInt32(inKey, outValue);
	}
	return mProperties.GetSInt32(inKey, outValue);
}

bool	SV_IOKitObject::CopyProperty_UInt32(CFStringRef inKey, bool inIsInUserDictionary, UInt32& outValue) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		return mUserProperties.GetUInt32(inKey, outValue);
	}
	return mProperties.GetUInt32(inKey, outValue);
}

bool	SV_IOKitObject::CopyProperty_UInt64(CFStringRef inKey, bool inIsInUserDictionary, UInt64& outValue) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		return mUserProperties.GetUInt64(inKey, outValue);
	}
	return mProperties.GetUInt64(inKey, outValue);
}

bool	SV_IOKitObject::CopyProperty_Fixed32(CFStringRef inKey, bool inIsInUserDictionary, Float32& outValue) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		return mUserProperties.GetFixed32(inKey, outValue);
	}
	return mProperties.GetFixed32(inKey, outValue);
}

bool	SV_IOKitObject::CopyProperty_Fixed64(CFStringRef inKey, bool inIsInUserDictionary, Float64& outValue) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		return mUserProperties.GetFixed64(inKey, outValue);
	}
	return mProperties.GetFixed64(inKey, outValue);
}

bool	SV_IOKitObject::CopyProperty_CFString(CFStringRef inKey, bool inIsInUserDictionary, CFStringRef& outValue) const
{
	CacheProperties();
	bool theAnswer = false;
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		theAnswer = mUserProperties.GetString(inKey, outValue);
	}
	else
	{
		theAnswer = mProperties.GetString(inKey, outValue);
	}
	if(outValue != NULL)
	{
		CFRetain(outValue);
	}
	return theAnswer;
}

bool	SV_IOKitObject::CopyProperty_CFArray(CFStringRef inKey, bool inIsInUserDictionary, CFArrayRef& outValue) const
{
	CacheProperties();
	bool theAnswer = false;
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		theAnswer = mUserProperties.GetArray(inKey, outValue);
	}
	else
	{
		theAnswer = mProperties.GetArray(inKey, outValue);
	}
	if(outValue != NULL)
	{
		CFRetain(outValue);
	}
	return theAnswer;
}

bool	SV_IOKitObject::CopyProperty_CFDictionary(CFStringRef inKey, bool inIsInUserDictionary, CFDictionaryRef& outValue) const
{
	CacheProperties();
	bool theAnswer = false;
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		theAnswer = mUserProperties.GetDictionary(inKey, outValue);
	}
	else
	{
		theAnswer = mProperties.GetDictionary(inKey, outValue);
	}
	if(outValue != NULL)
	{
		CFRetain(outValue);
	}
	return theAnswer;
}

bool	SV_IOKitObject::CopyProperty_CFType(CFStringRef inKey, bool inIsInUserDictionary, CFTypeRef& outValue) const
{
	CacheProperties();
	bool theAnswer = false;
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		theAnswer = mUserProperties.GetCFType(inKey, outValue);
	}
	else
	{
		theAnswer = mProperties.GetCFType(inKey, outValue);
	}
	if(outValue != NULL)
	{
		CFRetain(outValue);
	}
	return theAnswer;
}

void	SV_IOKitObject::CopyProperty_CACFString(CFStringRef inKey, bool inIsInUserDictionary, CACFString& outValue) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		mUserProperties.GetCACFString(inKey, outValue);
	}
	else
	{
		mProperties.GetCACFString(inKey, outValue);
	}
}

void	SV_IOKitObject::CopyProperty_CACFArray(CFStringRef inKey, bool inIsInUserDictionary, CACFArray& outValue) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		mUserProperties.GetCACFArray(inKey, outValue);
	}
	else
	{
		mProperties.GetCACFArray(inKey, outValue);
	}
}

void	SV_IOKitObject::CopyProperty_CACFDictionary(CFStringRef inKey, bool inIsInUserDictionary, CACFDictionary& outValue) const
{
	CacheProperties();
	if (inIsInUserDictionary && mUserProperties.IsValid())
	{
		mUserProperties.GetCACFDictionary(inKey, outValue);
	}
	else
	{
		mProperties.GetCACFDictionary(inKey, outValue);
	}
}

void	SV_IOKitObject::PropertiesChanged()
{
	mProperties = static_cast<CFMutableDictionaryRef>(NULL);
	mUserProperties = static_cast<CFMutableDictionaryRef>(NULL);
}

void	SV_IOKitObject::CacheProperties() const
{
	//	One if the biggest differences between using a KEXT and a DEXT is how driver-specific Registry properties are handled.
	//	In a KEXT, they are just normal Registry entries and they show up in the main list so you can use
	//	IORegistryEntryCreateCFProperties and friends to directly access them. In a DEXT however, all the driver-specific
	//	properties are gathered together in a dicitionary in the registry entry with the key, kIOUserServicePropertiesKey. So
	//	looking up such a property is a two step process. Here, we cache the user-sercice properties dicitionary for easy
	//	access and have added an arguement to the fetching function to control where the code looks for the property.
	if((mObject != IO_OBJECT_NULL) && (!mProperties.IsValid() || mAlwaysLoadPropertiesFromRegistry))
	{
		CFMutableDictionaryRef theProperties = NULL;
		kern_return_t theError = IORegistryEntryCreateCFProperties(mObject, &theProperties, NULL, 0);
		AssertNoKernelError(theError, "SV_IOKitObject::CacheProperties: failed to get the properties from the IO Registry");
		const_cast<SV_IOKitObject*>(this)->mProperties = theProperties;
		if(theProperties != NULL)
		{
			CFRelease(theProperties);
		}

		mProperties.GetCACFDictionary(CFSTR(kIOUserServicePropertiesKey), const_cast<SV_IOKitObject*>(this)->mUserProperties);
	}
}

bool	SV_IOKitObject::CopyProperty_bool(io_object_t inObject, CFStringRef inKey, bool inIsInUserDictionary, bool& outValue)
{
	IOObjectRetain(inObject);
	SV_IOKitObject theObject{inObject};
	return theObject.CopyProperty_bool(inKey, inIsInUserDictionary, outValue);
}

bool	SV_IOKitObject::CopyProperty_SInt32(io_object_t inObject, CFStringRef inKey, bool inIsInUserDictionary, SInt32& outValue)
{
	IOObjectRetain(inObject);
	SV_IOKitObject theObject{inObject};
	return theObject.CopyProperty_SInt32(inKey, inIsInUserDictionary, outValue);
}

bool	SV_IOKitObject::CopyProperty_UInt32(io_object_t inObject, CFStringRef inKey, bool inIsInUserDictionary, UInt32& outValue)
{
	IOObjectRetain(inObject);
	SV_IOKitObject theObject{inObject};
	return theObject.CopyProperty_UInt32(inKey, inIsInUserDictionary, outValue);
}

bool	SV_IOKitObject::CopyProperty_CFString(io_object_t inObject, CFStringRef inKey, bool inIsInUserDictionary, CFStringRef& outValue)
{
	IOObjectRetain(inObject);
	SV_IOKitObject theObject{inObject};
	return theObject.CopyProperty_CFString(inKey, inIsInUserDictionary, outValue);
}

bool	SV_IOKitObject::CopyProperty_CFArray(io_object_t inObject, CFStringRef inKey, bool inIsInUserDictionary, CFArrayRef& outValue)
{
	IOObjectRetain(inObject);
	SV_IOKitObject theObject{inObject};
	return theObject.CopyProperty_CFArray(inKey, inIsInUserDictionary, outValue);
}

bool	SV_IOKitObject::CopyProperty_CFDictionary(io_object_t inObject, CFStringRef inKey, bool inIsInUserDictionary, CFDictionaryRef& outValue)
{
	IOObjectRetain(inObject);
	SV_IOKitObject theObject{inObject};
	return theObject.CopyProperty_CFDictionary(inKey, inIsInUserDictionary, outValue);
}

void	SV_IOKitObject::CopyProperty_CACFString(io_object_t inObject, CFStringRef inKey, bool inIsInUserDictionary, CACFString& outValue)
{
	IOObjectRetain(inObject);
	SV_IOKitObject theObject{inObject};
	theObject.CopyProperty_CACFString(inKey, inIsInUserDictionary, outValue);
}

bool	SV_IOKitObject::IsConnectionOpen() const
{
	return mConnection != IO_OBJECT_NULL;
}

void	SV_IOKitObject::OpenConnection(UInt32 inUserClientType)
{
	if((mObject != IO_OBJECT_NULL) && (mConnection == IO_OBJECT_NULL))
	{
		kern_return_t theKernelError = IOServiceOpen(mObject, mach_task_self(), inUserClientType, &mConnection);
		ThrowIfKernelError(theKernelError, CAException(theKernelError), "SV_IOKitObject::OpenConnection: failed to open a connection");
	}
}

void	SV_IOKitObject::CloseConnection()
{
	if(mConnection != IO_OBJECT_NULL)
	{
		IOServiceClose(mConnection);
		mConnection = IO_OBJECT_NULL;
	}
}

void	SV_IOKitObject::SetConnectionNotificationPort(UInt32 inType, mach_port_t inPort, void* inUserData)
{
	if(mConnection != IO_OBJECT_NULL)
	{
		kern_return_t theKernelError = IOConnectSetNotificationPort(mConnection, inType, inPort, reinterpret_cast<uintptr_t>(inUserData));
		if(inPort != MACH_PORT_NULL)
		{
			ThrowIfKernelError(theKernelError, CAException(theKernelError), "SV_IOKitObject::SetConnectionNotificationPort: Cannot set the connection's's notification port.");
		}
	}
}

void*	SV_IOKitObject::MapMemory(UInt32 inType, IOOptionBits inOptions, UInt32& outSize)
{
	void* theAnswer = NULL;
	if((mConnection != IO_OBJECT_NULL) && mIsAlive)
	{
		mach_vm_address_t	theAddress;
		mach_vm_size_t		theSize;
		kern_return_t theKernelError = IOConnectMapMemory64(mConnection, inType, mach_task_self(), &theAddress, &theSize, inOptions);
		ThrowIfKernelError(theKernelError, CAException(theKernelError), "SV_IOKitObject::MapMemory: failed to map in the memory");
		theAnswer = reinterpret_cast<void*>(theAddress);
		ThrowIfNULL(theAnswer, CAException(kAudioHardwareIllegalOperationError), "SV_IOKitObject::MapMemory: mapped in a NULL pointer");
		outSize = static_cast<UInt32>(theSize);
	}
	return theAnswer;
}

void	SV_IOKitObject::ReleaseMemory(void* inMemory, UInt32 inType)
{
	if((mConnection != IO_OBJECT_NULL) && (inMemory != NULL))
	{
		IOConnectUnmapMemory64(mConnection, inType, mach_task_self(), reinterpret_cast<mach_vm_address_t>(inMemory));
//		AssertNoKernelError(theKernelError, "SV_IOKitObject::ReleaseMemory: failed to release the memory");
	}
}

kern_return_t	SV_IOKitObject::CallMethod(UInt32 inSelector, const UInt64* inInputItems, UInt32 inNumberInputItems, const void* inRawInput, size_t inRawInputSize, UInt64* outOutputItems, UInt32* outNumberOutputItems, void* outRawOutput, size_t* outRawOutputSize)
{
	kern_return_t theKernelError;
	if((mConnection != IO_OBJECT_NULL) && mIsAlive)
	{
		theKernelError = IOConnectCallMethod(mConnection, inSelector, inInputItems, inNumberInputItems, inRawInput, inRawInputSize, outOutputItems, reinterpret_cast<uint32_t*>(outNumberOutputItems), outRawOutput, outRawOutputSize);
	}
	else
	{
		theKernelError = kAudioHardwareNotRunningError;
	}
	return theKernelError;
}

kern_return_t	SV_IOKitObject::CallTrap(UInt32 inSelector)
{
	kern_return_t theKernelError;
	if((mConnection != IO_OBJECT_NULL) && mIsAlive)
	{
		theKernelError = IOConnectTrap0(mConnection, inSelector);
	}
	else
	{
		theKernelError = kAudioHardwareNotRunningError;
	}
	return theKernelError;
}

void	SV_IOKitObject::Retain()
{
	if(mObject != IO_OBJECT_NULL)
	{
		IOObjectRetain(mObject);
	}
}

void	SV_IOKitObject::Release()
{
	if(mObject != IO_OBJECT_NULL)
	{
		IOObjectRelease(mObject);
		mObject = IO_OBJECT_NULL;
	}
	mProperties = static_cast<CFMutableDictionaryRef>(NULL);
}

//==================================================================================================
//	SV_IOKitIterator
//==================================================================================================

SV_IOKitIterator::SV_IOKitIterator()
:
	mIterator(IO_OBJECT_NULL),
	mWillRelease(true)
{
}

SV_IOKitIterator::SV_IOKitIterator(io_iterator_t inIterator, bool inWillRelease)
:
	mIterator(inIterator),
	mWillRelease(inWillRelease)
{
}

SV_IOKitIterator::SV_IOKitIterator(const SV_IOKitIterator& inIterator)
:
	mIterator(inIterator.mIterator),
	mWillRelease(inIterator.mWillRelease)
{
	Retain();
}

SV_IOKitIterator::SV_IOKitIterator(io_object_t inParent, const io_name_t inPlane)
:
	mIterator(IO_OBJECT_NULL),
	mWillRelease(true)
{
	if(IORegistryEntryGetChildIterator(inParent, inPlane, &mIterator) != 0)
	{
		mIterator = IO_OBJECT_NULL;
	}
}

SV_IOKitIterator::SV_IOKitIterator(io_object_t inChild, const io_name_t inPlane, bool)
:
	mIterator(IO_OBJECT_NULL),
	mWillRelease(true)
{
	if(IORegistryEntryGetParentIterator(inChild, inPlane, &mIterator) != 0)
	{
		mIterator = IO_OBJECT_NULL;
	}
}

SV_IOKitIterator::SV_IOKitIterator(CFMutableDictionaryRef inMatchingDictionary)
:
	mIterator(IO_OBJECT_NULL),
	mWillRelease(true)
{
	//	note that IOServiceGetMatchingServices will consume one reference on inMatchingDictionary
    if(IOServiceGetMatchingServices(kIOMainPortDefault, inMatchingDictionary, &mIterator) != 0)
	{
		mIterator = IO_OBJECT_NULL;
	}
}

SV_IOKitIterator&	SV_IOKitIterator::operator=(io_iterator_t inIterator)
{
	Release();
	mIterator = inIterator;
	Retain();
	return *this;
}

SV_IOKitIterator&	SV_IOKitIterator::operator=(const SV_IOKitIterator& inIterator)
{
	Release();
	mIterator = inIterator.mIterator;
	Retain();
	return *this;
}

SV_IOKitIterator::~SV_IOKitIterator()
{
	Release();
}

io_iterator_t	SV_IOKitIterator::GetIterator() const
{
	return mIterator;
}

bool	SV_IOKitIterator::IsValid() const
{
	return mIterator != IO_OBJECT_NULL;
}

SV_IOKitObject	SV_IOKitIterator::Next()
{
	return SV_IOKitObject{IOIteratorNext(mIterator)};
}

void	SV_IOKitIterator::SetWillRelease(bool inWillRelease)
{
	mWillRelease = inWillRelease;
}

void	SV_IOKitIterator::Retain()
{
	if(mIterator != IO_OBJECT_NULL)
	{
		IOObjectRetain(mIterator);
	}
}

void	SV_IOKitIterator::Release()
{
	if(mWillRelease && (mIterator != IO_OBJECT_NULL))
	{
		IOObjectRelease(mIterator);
		mIterator = IO_OBJECT_NULL;
	}
}
