/*
See LICENSE folder for this sample’s licensing information.

Abstract:
A minimal user-space driver.
*/

/*==================================================================================================
	SV_ObjectMap.h
==================================================================================================*/
#if !defined(__SV_ObjectMap_h__)
#define __SV_ObjectMap_h__

//==================================================================================================
//	Includes
//==================================================================================================

//	PublicUtility Includes
#include "CAMutex.h"

//	System Includes
#include <CoreAudio/AudioServerPlugIn.h>

//	Standard Library Includes
#include <vector>

//==================================================================================================
//	Types
//==================================================================================================

//==================================================================================================
//	SV_Object
//
//	This is the base class for objects managed by SV_ObjectMap. It's only job is to ensure that
//	objects of this type have the proper external semantics for a reference counted object. This
//	means that the desctructor is protected so that these objects cannot be deleted directly. Also,
//	these objects many not make a copy of another object or be assigned from another object. Note
//	that the reference count of the object is tracked and owned by the SV_ObjectMap.
//
//	These objects provide RTTI information tied to the constants describing the HAL's API class
//	hierarchy as described in the headers. The class ID and base class IDs passed in to the
//	constructor must operate with the semantics described in AudioObjectBase.h where the base class
//	has to always be one of the standard classes. The class ID can be a custom value or a standard
//	value however. If it is a standard value, the base class should be the proper standard base
//	class. So for example, a standard volume control object will say that it's class is
//	kAudioVolumeControlClassID and that its base class is kAudioLevelControlClassID. In the case of
//	a custom boolean control, it would say that it's class is a custom value like 'MYBL' and that
//	its base class is kAudioBooleanControlClassID.
//
//	Subclasses of this class must implement Activate(). This method is called after an object has
//	been constructed and inserted into the object map. Until Activate() is called, a constructed
//	object may not do anything active such as sending/receiving notifications or creating other
//	objects. Active operations may be performed in the Activate() method proper however. Note that
//	Activate() is called prior to any references to the object being handed out. As such, it does
//	not need to worry about being thread safe while Activate() is in progress.
//
//	Subclasses of this class must also implement Deactivate(). This method is called when the object
//	is at the end of it's lifecycle. Once Deactivate() has been called, the object may no longer
//	perform active opertions, including Deactivating other objects. This is based on the notion that
//	all the objects have a definite point at which they are considered dead to the outside world.
//	For example, an AudioDevice object is dead if it's hardware is unplugged. The point of death is
//	the notification the owner of the device gets to signal that it has been unplugged. Note that it
//	is both normal and expected that a dead object might still have outstanding references. Thus, an
//	object has to put in some care to do the right thing when these zombie references are used. The
//	best thing to do is to just have those queries return appropriate errors.
//
//	Deactivate() itself needs to be thread safe with respect to other opertions taking place on the
//	object. This also means taking care to handle the Deactivation of owned objects. For example, an
//	AudioDevice object will almost always own one or more AudioStream objects. If the stream is in a
//	separate lock domain from it's owning device, then the device has to be very careful about how
//	it deactivates the stream such that it doesn't try to lock the stream's lock while holding the
//	device's lock which will inevitably lead to a deadlock situation. There are two reasonable
//	approaches to dealing with this kind of situation. The first is to just not get into it by
//	making the device share a lock domain with all it's owned objects like streams and controls. The
//	other approach is to use dispatch queues to make the work of Deactivating owned objects take
//	place outside of the device's lock domain. For example, if the device needs to deactivate a
//	stream, it can remove the stream from any tracking in the device object and then dispatch
//	asynchronously the Deactivate() call on the stream and the release of the reference the device
//	has on the stream.
//
//	Note that both Activate() and Deactiveate() are called by objects at large. Typically,
//	Activate() is called by the creator of the object, usually right after the object has been
//	allocated. Deactivate() will usually be called by the owner of the object upon recognizing that
//	the object is dead to the outside world. Going back to the example of an AudioDevice getting
//	unplugged, the Deactivate() method will be called by whomever receives the notification about
//	the hardware going away, which is often the owner of the object.
//
//	This class also defines methods to implement the portion of the
//	AudioServerPlugInDriverInterface that deals with properties. The five methods all have the same
//	basic arguments and semantics. The class also provides the implementation for
//	the minimum required properties for all AudioObjects. There is a detailed commentary about each
//	specific property in the GetPropertyData() method.
//
//	It is important that a thread retain and hold a reference while it is using an SV_Object and
//	that the reference be released promptly when the thread is finished using the object. By
//	assuming this, an SV_Objects can minimize the amount of locking it needs to do. In particular,
//	purely static or invariant data can be handled without any locking at all.
//==================================================================================================

class SV_Object
{

#pragma mark Construction/Destruction
public:
						SV_Object(AudioObjectID inObjectID, AudioClassID inClassID, AudioClassID inBaseClassID, AudioObjectID inOwnerObjectID);
					
	virtual void		Activate();
	virtual void		Deactivate();

protected:
	virtual				~SV_Object();

private:
						SV_Object(const SV_Object&);
	SV_Object&			operator=(const SV_Object&);

#pragma mark Attributes
public:
	AudioObjectID		GetObjectID() const			{ return mObjectID; }
	void*				GetObjectIDAsPtr() const	{ uintptr_t thePtr = mObjectID; return reinterpret_cast<void*>(thePtr); }
	AudioClassID		GetClassID() const			{ return mClassID; }
	AudioClassID		GetBaseClassID() const		{ return mBaseClassID; }
	AudioObjectID		GetOwnerObjectID() const	{ return mOwnerObjectID; }
	bool				IsActive() const			{ return mIsActive; }

#pragma mark Property Operations
public:
	virtual bool		HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	virtual bool		IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	virtual UInt32		GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const;
	virtual void		GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const;
	virtual void		SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData);

#pragma mark Implementation
protected:
	friend class		SV_ObjectMap;
	
	AudioObjectID		mObjectID;
	AudioClassID		mClassID;
	AudioClassID		mBaseClassID;
	AudioObjectID		mOwnerObjectID;
	bool				mIsActive;

};

//==================================================================================================
//	SV_ObjectMap
//
//	SV_ObjectMap is a singleton object that maintains the mapping between AudioObjectIDs and
//	SV_Objects. In the process, it also manages the reference counting mechanism as well. The same
//	object can be mapped to multiple IDs.
//
//	The creator of an SV_Object must register the object with this map prior to calling Activate()
//	on the new object. The typical sequence of events for creating an object go like this:
//		- Get an AudioObjectID for the new object (via GetNextObjectID())
//		- Create the new object
//		- Register the object with the map (via MapObject())
//		- Activate the new object
//==================================================================================================

class SV_ObjectMap
{

#pragma mark Construction/Destruction
private:
										SV_ObjectMap();
	virtual								~SV_ObjectMap();
	
	static void							StaticInitializer();
										SV_ObjectMap(const SV_ObjectMap&);
	SV_ObjectMap&						operator=(const SV_ObjectMap&);

#pragma mark External Methods
public:
	static AudioObjectID				GetNextObjectID();
	static bool							MapObject(AudioObjectID inObjectID, SV_Object* inObject);
	static void							UnmapObject(AudioObjectID inObjectID, SV_Object* inObject);
	static SV_Object*					CopyObjectByObjectID(AudioObjectID inObjectID);
	template <class T> static T*		CopyObjectOfClassByObjectID(AudioObjectID inObjectID)			{ return reinterpret_cast<T*>(CopyObjectByObjectID(inObjectID)); }
	static UInt64						RetainObject(SV_Object* inObject);
	static UInt64						ReleaseObject(SV_Object* inObject);
	static void							Dump();

private:
	void								DestroyObject(SV_Object* inObject);

#pragma mark Internal Methods
private:
	AudioObjectID						_GetNextObjectID();
	bool								_MapObject(AudioObjectID inObjectID, SV_Object* inObject);
	void								_UnmapObject(AudioObjectID inObjectID, SV_Object* inObject);
	SV_Object*							_CopyObjectByObjectID(AudioObjectID inObjectID);
	UInt64								_RetainObject(SV_Object* inObject);
	UInt64								_ReleaseObject(SV_Object* inObject);
	void								_Dump();	

#pragma mark Implemenatation
private:
	typedef std::vector<AudioObjectID>	ObjectIDList;
	struct ObjectInfo
	{
		SV_Object*						mObject;
		UInt64							mReferenceCount;
		ObjectIDList					mObjectIDList;

										ObjectInfo(AudioObjectID inObjectID, SV_Object* inObject)	: mObject(inObject), mReferenceCount(1), mObjectIDList(1, inObjectID) {}
		bool							operator==(AudioObjectID inObjectID) const					{ return std::find(mObjectIDList.begin(), mObjectIDList.end(), inObjectID) != mObjectIDList.end(); }
		bool							operator==(const SV_Object* inObject) const					{ return mObject == inObject; }
	};
	typedef std::vector<ObjectInfo>		ObjectInfoList;
	
	CAMutex								mMutex;
	AudioObjectID						mNextObjectID;
	ObjectInfoList						mObjectInfoList;
	
	static pthread_once_t				sStaticInitializer;
	static SV_ObjectMap*				sInstance;
	
};

//==================================================================================================
//	SV_ObjectReleaser
//==================================================================================================

template <typename T>
class SV_ObjectReleaser
{

#pragma mark Construction/Destruction
public:
	explicit			SV_ObjectReleaser(T* inObject)							: mObject(inObject)	{}
						SV_ObjectReleaser(const SV_ObjectReleaser& inObject)	: mObject(inObject.mObject) { SV_ObjectMap::RetainObject(mObject); }
						~SV_ObjectReleaser()									{ SV_ObjectMap::ReleaseObject(mObject); }
	SV_ObjectReleaser&	operator=(const SV_ObjectReleaser& inObject)			{ if(mObject != inObject.mObject) { SV_ObjectMap::ReleaseObject(mObject); } mObject = inObject.mObject; SV_ObjectMap::RetainObject(mObject); return *this; }
	

#pragma mark Operations
public:
					operator T*() const			{ return mObject; }
	T*				operator ->() const			{ return mObject; }
	bool			IsValid() const				{ return mObject != NULL; }
	T*				GetObject() const			{ return mObject; }

#pragma mark Implementation
private:
	T*				mObject;
	
};

#endif	//	__SV_ObjectMap_h__
