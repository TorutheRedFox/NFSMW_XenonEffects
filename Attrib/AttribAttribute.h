#ifndef ATTRIBATTRIBUTE_H
#define ATTRIBATTRIBUTE_H

#include "AttribCore.h"
#include "AttribInstance.h"
#include "AttribCollection.h"
#include "AttribNode.h"

namespace Attrib
{
	class Attribute
	{
		Attrib::Instance* mInstance = NULL;
		Attrib::Collection* mCollection = NULL;
		Attrib::Node* mInternal = NULL;
		void* mDataPointer = NULL;
	public:
		Attribute() {};
	
		Attribute(Attribute* src)
		{
			if (mInstance)
				mInstance->mLocks--;
			*this = *src;
			if (mInstance)
				mInstance->mLocks++;
		}
	
		Attribute(Instance *instance, Collection* collection, Node* node)
		{
			this->mCollection = collection;
			this->mInstance = instance;
			this->mInternal = node;
			this->mDataPointer = NULL;
	
			if (!node->IsArray())
				this->mDataPointer = node->GetPointer(mInstance->mLayoutPtr);
	
			if (instance)
				++instance->mLocks;
		}
	
		~Attribute()
		{
			if (mInstance)
				mInstance->mLocks--;
		}
	
		inline uint32_t GetLength()
		{
			if (mInternal)
				return mInternal->GetCount(mInstance->mLayoutPtr);
			else
				return 0;
		}
	};
}

#endif
