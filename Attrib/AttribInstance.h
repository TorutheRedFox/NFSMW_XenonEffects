#ifndef ATTRIBINSTANCE_H
#define ATTRIBINSTANCE_H

#include "AttribCore.h"

namespace Attrib
{
	class Attribute;
	class Instance
	{
	public: 
		void *GetLayoutPointer() const
		{
			return mLayoutPtr;
		}
		void *GetLayoutPointer()
		{
			return mLayoutPtr;
		}
		inline void* GetAttributePointer(uint32_t attribkey, uint32_t index)
		{
			return ((void *(__thiscall *)(Instance *, uint32_t, uint32_t))0x454810)(this, attribkey, index);
		}

		Instance(Instance* src)
		{
			((void(__thiscall*)(Instance*, Instance*))0x4523C0)(this, src);
		}

		Instance(Collection *collection, uint32_t msgPort, void* owner)
		{
			((void(__thiscall *)(Instance *, Collection *, uint32_t, void*))0x452380)(this, collection, msgPort, owner);
		}

		Instance(RefSpec& refspec, uint32_t msgPort, void* owner)
		{
			Collection* collection = ((Collection * (__thiscall*)(RefSpec&))0x4560D0)(refspec);
			Instance(collection, msgPort, owner);
		}

		class Attrib::Attribute* Get(void *unk, uint32_t attributeKey)
		{
			return ((class Attrib::Attribute * (__thiscall*)(Instance*, void*, uint32_t))0x4546C0)(this, unk, attributeKey);
		}

		~Instance()
		{
			((void *(__thiscall *)(Instance *))0x45A430)(this);
		}
	
		void *mOwner = NULL;
		class Collection *const mCollection = NULL;
		void *mLayoutPtr = NULL;
		uint32_t mMsgPort = NULL;
		uint16_t mFlags = NULL;
		uint16_t mLocks = NULL;
	
		enum Flags
		{
			kDynamic = 1
		};
	};
}

#endif
