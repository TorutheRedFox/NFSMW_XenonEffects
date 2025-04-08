#ifndef ATTRIBCOLLECTION_H
#define ATTRIBCOLLECTION_H

#include "AttribCore.h"
#include "AttribHashMap.h"

namespace Attrib
{
	class Collection
	{
	public:
		class Node *GetNode(uint32_t attributeKey, const Collection *&container);
		void *GetData(uint32_t attributeKey, uint32_t index);
	
	private:
		HashMap mTable;
		const Collection *mParent;
		class Class *mClass;
		void *mLayout;
		uint32_t mRefCount;
		uint32_t mKey;
		class Vault *mSource;
		const char *mNamePtr;
	};
	
	inline Collection * FindCollection(ClassKey classKey, CollectionKey collectionKey)
	{
		return ((Collection *(*)(ClassKey, CollectionKey))0x455FD0)(classKey, collectionKey);
	}
}

#endif