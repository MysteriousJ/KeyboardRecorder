#include <stdlib.h>
#include <assert.h>

#define ASSERT(condition, message) if(!(condition)){assert(!(message));}

template <class T> struct DynamicArray
{
	unsigned int count, allocatedCount;
	T* data;

	unsigned int size() { return count; }

	void push_back(T element) {
		if (count + 1 > allocatedCount) {
			allocatedCount = (allocatedCount + 1) * 2;
			data = (T*)realloc(data, allocatedCount * sizeof(T));
		}
		data[count] = element;
		++count;
	}

	T& pop_back() {
		ASSERT(count > 0, "Trying to pop last element in empty DynamicArray");
		--count;
		return data[count];
	}

	void remove(unsigned int index) {
		for (unsigned int i = index; i < count - 1; ++i) {
			data[i] = data[i + 1];
		}
		--count;
	}

	DynamicArray<T> deepCopy() {
		DynamicArray<T> result;
		result.count = count;
		result.allocatedCount = allocatedCount;
		result.data = (T*)malloc(allocatedCount * sizeof(T));
		for (unsigned int i = 0; i < count; ++i) {
			result.data[i] = data[i];
		}
		return result;
	}

	void clear() { count = 0; }

	void freeMemory() {
		free(data);
		data = 0;
		allocatedCount = count = 0;
	}

	T& last() {
		ASSERT(count > 0, "Trying to access last element in empty DynamicArray");
		return data[count - 1];
	}

	T& operator[](unsigned int index) {
		ASSERT(index < count, "DynamicArray access out of range");
		return data[index];
	}
};