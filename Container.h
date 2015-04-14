///============================================================================
///======	class CContainer
///============================================================================

template<typename T>
class CContainer
{
    char * start;
    unsigned int bufferSize;
    unsigned int maxCount;
    unsigned int count;
    unsigned bitmaskLength;
    unsigned bitmaskBytes;
    char * toc;
    
public:
    
    
    // Use a data structure in the buffer to serve as a table of contents,
    // showing where free spaces exist in the buffer, as opposed to spaces that are filled.
    // What can we use to accomplish this?:
    // 1) a bitmask
    // 2) a list of indexes
    //
    // Or, should we keep a pointer to the first empty space?
    // Or, perform a linear search for empty space? - that would be compute heavy but not require any buffer space, and it would be necessary to mark a space as empty somehow to be able to identify it, which I don't even think we can do since this the storage object is generic.
    // It is only necessary to consult a Table of Contents data structure while empty spaces exist in the middle of filled spaces. As long as the empty spaces are all at the end, all adds are pushed onto the end without consulting the table of contents.
    //
    // 2) If there are 2048 bytes in buffer
    // sizeof CTestClass is 16 bytes
    // We can have 2048/16=128 spaces
    // But the worst case scenario is if the first 127 spaces become empty, and space 128 is filled
    // So we need a list of 127 empty spaces, and that takes 127 8-bit numbers, or half the space in our buffer. That's pretty bad.
    
    // 1) A bit mask is a better approach. To represent 128 spaces, we just need a binary string 128 spaces
    // long, or the space of 4 32-bit inegers (16 bytes) leaving 2048-16 = 2032 bytes for storage
    // If 0 represents an empty space and 1 represents a filled space,
    // then when adding a new element, just search the bitmask for the first 0 and take its index
    // as the index of the free space in the buffer. This is efficient in both time and space.
    // It gives us space to store 2032/16=127 objects, only 1 fewer objects than not having a bitmask at all!
    
    // If the buffer size is 200KB = 200*1024=204800 bytes, you can store 204800/16=12800  16 byte objects
    // You need a bitmask of length 12800, or 12800/8=1600 bytes, or 800 16-bit ints
    // This lets you store 100 fewer 16-byte objects because of space occupied by the data structure.
    // You can store 12700 objects
    // Formula:
    //  bitmaskLength = bufferSizeBytes/objectSizeBytes
    //  bitmaskBytes = bitmaskLength/8;
    //  capacity = (bufferSizeBytes/objectSizeBytes) - (bitmaskBytes/objectSizeBytes)
    // That's it!
    // Now how does one create such a bitmask and search it for the first zero?
    
    // Since the first 1600 bytes are used for the bitmask, you actually only have space for:
    // 204800-1600=203200 bytes in the buffer, or 203200/16=12700 objects
    // So you only actually need a bitmask of length 12700/8=1586 bytes
    // You could free up 14 bytes, but that isn't enough to add another object. I don't think we can do much better.
    
    ///======	Constructs the container from a pre-defined buffer.
    CContainer(char * pBuffer, unsigned nBufferSize) {
        // Set up Table of contents structure: a bitmask
        // This will be the first thing in the buffer
        toc = pBuffer;
        bufferSize = nBufferSize;
        bitmaskLength = bufferSize / sizeof(T);
        bitmaskBytes = bitmaskLength / 8;
        
        // Keep a pointer to the start of the object storage, immediately after the toc in the buffer
        start = pBuffer + bitmaskBytes;
        maxCount = (bitmaskLength) - (bitmaskBytes/sizeof(T));
        count = 0;
    }
    
    ~CContainer() {}
    
    ///======	Adds an element to the container, constructs it and returns it to the caller.
    ///======	Note that the return address needs to be persistent during the lifetime of the object.
    T *				Add() {
        // There are two ways to do this:
        // 1) Using placement new, directly on the buffer
        // or 2) By allocating the object on the stack and then copying it to our buffer
        // The benefit of using (1) is that no copying is needed
        
        // This does a linear search for an empty space
        /*unsigned i = 0;
         T *x = (T*)(start+(i*sizeof(T)));
         unsigned *y = (unsigned*)x;
         while (*y != 0) {
         i++;
         x = (T*)(start+(i*sizeof(T)));
         y = (unsigned*)x;
         }
         printf("empty space was at %d\n", i);
         */
        
        int firstEmptyIndex = -1;
        unsigned firstEmptyIndexByte = 0;
        unsigned firstEmptyIndexBitOffset = 0;
        bool bit = true;
        
        // Find the index of first 0 in the bitmask
        for (unsigned currentByte = 0; currentByte < bitmaskBytes; currentByte++) {
            char *byte = toc+currentByte;
            for (int i = 7; i >= 0; i--) {
                bit = *byte & (1 << i);
                //That will put the value of bit x into the variable bit.
                //The MSB is on the left, but indexing is from the right,
                //So the index of the leftmost bit is 7
                
                if (!bit) {
                    firstEmptyIndexByte = currentByte;
                    firstEmptyIndexBitOffset = i;
                    firstEmptyIndex = (currentByte*8)+(7-i);
                    break;
                }
            }
            if (firstEmptyIndex != -1) {
                break;
            }
        }
        
        
        //Allocate an object in the buffer at the empty slot
        T* newGuy = new (start+(firstEmptyIndex*sizeof(T))) T();
        //Fill the bitmask for this slot
        //number |= 1 << x;
        //That will set bit x.
        *(toc+firstEmptyIndexByte) |= 1 << firstEmptyIndexBitOffset;
        
        count++;
        return newGuy;
    }
    /*
     void			check(unsigned checkIndex)		///======	Print a string of the bitmask
     {
     printf("Check:\n");
     bool bit;
     unsigned currentIndex = 0;
     bool checkIndexValue = true;
     for (unsigned currentByte = 0; currentByte < bitmaskBytes; currentByte++) {
     char *byte = toc+currentByte;
     for (int i = 7; i >= 0; i--) {
     bit = *byte & (1 << i);
     if (bit) {
     printf("1");
     }
     else {
     printf("0");
     }
     if (checkIndex == currentIndex) {
     checkIndexValue = bit;
     }
     currentIndex++;
     }
     }
     printf("\n");
     printf("Check index bitmask value is %d\n", checkIndexValue);
     }
     */
    void			Remove(T *object)		///======	Removes an object from the container.
    {
        //Clear the bitmask slot for this index
        //number &= ~(1 << x);
        //That will clear bit x. You must invert the bit string with the bitwise NOT operator (~), then AND it.
        unsigned long index = ((char*)object-start) / sizeof(T);
        unsigned byteIndex = index / 8;
        unsigned bitIndex = 7 - (index % 8);
        *(toc+byteIndex) &= ~(1 << bitIndex);
        
        // Delete the object
        object->~T();
        count--;
    }
    
    unsigned		Count() const		///======	Number of elements currently in the container.
    {
        return count;
    }
    
    ///======	Max number of elements the container can contain. You can limit the capacity to 65536 elements if this makes your implementation easier.
    unsigned		Capacity() const
    {
        return maxCount;
    }
    
    bool			IsEmpty() const	///======	Is container empty?
    {
        return count == 0;
    }
    bool			IsFull() const		///======	Is container full?
    {
        return count == maxCount;
    }
    
    char const *	GetAuthor() const
    {
        return "Zachary Carter submitted on March 1, 2015";	///======	put your first and last name as well as the date you took the test.
    }
    
    T const *		operator [] (unsigned nIndex) const	///======	Returns the n th element of the container [0..Count -1].
    {
        unsigned indexOfNthElement =  getNthValidElementIndex(nIndex);
        return (const T*)(start+(sizeof(T)*indexOfNthElement));
    }
    
    T *				operator [] (unsigned nIndex)			///======	Returns the n th element of the container [0..Count -1].
    {
        unsigned indexOfNthElement =  getNthValidElementIndex(nIndex);
        return (T*)(start+(sizeof(T)*indexOfNthElement));
    }
    
private:
    
    unsigned	getNthValidElementIndex(unsigned n) const
    {
        // This needs to only return valid elements, skipping over empty slots
        bool bit;
        unsigned onesCount = 0;
        unsigned currentIndex = 0;
        
        for (unsigned currentByte = 0; currentByte < bitmaskBytes; currentByte++) {
            char *byte = toc+currentByte;
            for (int i = 7; i >= 0; i--) {
                bit = *byte & (1 << i);
                if (bit) {
                    if (onesCount == n) {
                        return currentIndex;
                    } else {
                        onesCount++;
                    }
                }
                currentIndex++;
            }
        }
        
        return NULL;
    }
    
};
