#include <unistd.h> // sbrk , size_t
#include <sys/mman.h> // mmap
#include <cstring> // std::memset , std::memcpy
#include <stdlib.h>

const int N = 128;
const size_t KB = 1024;
const int cookies = rand();

typedef char Byte;

struct MallocMetadata {
    int _cookies = cookies;
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
    MallocMetadata* hist_next;
    MallocMetadata* hist_prev;
};

int ceil(int numerator, int denominator)
{
    // numerator/denominator is the floor
    if(numerator % denominator == 0) {
        return numerator/denominator;
    }
    else {
        return numerator/denominator + 1;
    }
}

size_t roundToNearest8Mult(size_t size)
{
	//answer is size+ (size- size%8)%8
	
	//or
    //answer is ceiling(size/PageSize)*PageSize
    // int numOfPages_floor = (int)(size/PageSize);
    // int numOfPages_ceiling = 
    int numOf8s = ceil(size,8);
    return numOf8s*8;
}

MallocMetadata* getBlockMeta(void* block);
void* getBlockAddress(void* arr);

class Hist { 
    MallocMetadata* arr[N];
    Hist() {
        for(int i = 0; i < N; i++) {
            arr[i] = nullptr;
        }
    }

public: 
    static Hist& get_instance() {
        static Hist hist;
        return hist;
    }

    int get_index_by_size(size_t size) {
        return size / KB;
    }

    size_t min_size_of_index(int index) {
        return index * KB;
    }

    size_t max_size_of_index(int index) {
        return (index + 1) * KB - 1;
    }

    void insert(MallocMetadata* new_block) {
        int index = get_index_by_size(new_block->size);
        
        if(arr[index] == nullptr) {
            arr[index] = new_block;
            return;
        }
        
        for (MallocMetadata* block = arr[index]; block != nullptr; block = block->hist_next) {
            if (block->size > new_block->size) {
                new_block->hist_next = block;
                new_block->hist_prev = block->hist_prev;
                if (block->hist_prev != nullptr) {
                    block->hist_prev->hist_next = new_block;
                }
                block->hist_prev = new_block;
                if (block == arr[index]) {
					arr[index] = new_block;
				}
                break;
            } else if (block->hist_next == nullptr) {
                new_block->hist_prev = block;
                block->hist_next = new_block;
                break;
            }
        }
    }

    void remove_node(MallocMetadata* node) {
        if (node == NULL) {
            return;
		}
        if(node->hist_prev != nullptr) {
            node->hist_prev->hist_next = node->hist_next;
        } else {
            int index = get_index_by_size(node->size);
            if(arr[index] == node) {
                arr[index] = node->hist_next;
            }
        }
        if(node->hist_next != nullptr) {
            node->hist_next->hist_prev = node->hist_prev;
        }
        
        node->hist_next = nullptr;
        node->hist_prev = nullptr;
    }

    void* get_first_free_block(size_t size) {
        int index = get_index_by_size(size);
        for(int i = index; i < N; i++) {
            for (MallocMetadata* block = arr[i]; block != nullptr; block = block->hist_next) {
                if(block->size >= size) { 
                    remove_node(block);
                    block->is_free = false;
                    return getBlockAddress(block);
                } 
            }
        }
        return NULL;
    }
    
    /*
    void print()
    {
		cout<<"the hist"<<endl;
		for (int i=0; i<N; ++i) {
			if (arr[i] != nullptr) {
				cout <<i<<": "<<arr[i]->size;
				for (MallocMetadata* block = arr[i]->hist_next; block != nullptr; block = block->hist_next) {
					cout <<", "<<block->size;
				}
				cout<<endl;
			}
		}
		cout<<endl;
	}
	*/
};  
 
MallocMetadata* metaDataList = nullptr;
MallocMetadata* metaDataTail = nullptr;

size_t mmap_block_counter = 0;
size_t mmap_byte_counter = 0;

MallocMetadata* allocateMetaData(void* address, size_t size)
{ 
    MallocMetadata* new_meta = (MallocMetadata*)address;
    new_meta->size = size;
    new_meta->is_free = false;
    new_meta->prev = nullptr;
    new_meta->next = nullptr;
    new_meta->hist_prev = nullptr;
    new_meta->hist_next = nullptr;
    return new_meta;
}

void* allocate_new_pages(size_t size)
{
    size= roundToNearest8Mult(size);
	sbrk((8-(long)sbrk(0)%8)%8);
    void* arr = NULL;
    
    if(metaDataTail != nullptr && metaDataTail->is_free) {
        size -= (sizeof(MallocMetadata)+ metaDataTail->size);  
        arr = sbrk(size); 
        if (arr != (void*)(-1)) {  
            Hist& hist = Hist::get_instance(); 
            hist.remove_node(metaDataTail);  
            return (void*)metaDataTail;
        } 
        return NULL;
    }

    arr= sbrk(size);
    if (arr != (void*)(-1)) {
        return arr;
    } 
    return NULL;
}

void* allocate_new_large_pages(size_t size)
{ 
    size = roundToNearest8Mult(size);
    
    void* arr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0); 
    if (arr != (void*)(-1)) {
        return arr;
    }
    return NULL;
}

MallocMetadata* getBlockMeta(void* block)
{
    if (block == nullptr) {
        return nullptr;
    }
    MallocMetadata* ret = (MallocMetadata*)((Byte*)block - sizeof(MallocMetadata));
    if(ret->_cookies != cookies)
        exit(0xdeadbeef);
    return ret;
}

void* getBlockAddress(void* arr)
{
    if (arr == nullptr) {
        return nullptr;
    }
    return (void*)((Byte*)arr + sizeof(MallocMetadata));
}  

void combine_next_and_insert(MallocMetadata* block)
{
    if(block == NULL) {
		return;
	}
    if(!block->is_free) {
        return;
    }
    
    Hist& hist = Hist::get_instance();
    hist.remove_node(block);
	if (!block->next || !block->next->is_free) {
		hist.insert(block);
        return;
    }
    hist.remove_node(block->next); 

    MallocMetadata* next_block = block->next;
    block->next = next_block->next;
    if(next_block->next) {
        next_block->next->prev = block;
	}
    block->size += next_block->size + sizeof(MallocMetadata);
  
    hist.insert(block);
    
    if(next_block == metaDataTail) {
        metaDataTail = block;
    }

}

void split(void* arr, size_t size)
{
    if(arr == NULL || size == 0) {
        return;
    }
    MallocMetadata* block = getBlockMeta(arr);
    size_t left_over_size = block->size - size; 
    
    if(left_over_size >= 128 + sizeof(MallocMetadata)) {
        block->size -= left_over_size;
        void* left_over_ptr = (Byte*)arr + size;
        MallocMetadata* left_over_block = allocateMetaData(left_over_ptr, left_over_size-sizeof(MallocMetadata));
        left_over_block->is_free = true;
		left_over_block->next = block->next;
		if (block->next) {
			block->next->prev = left_over_block;
		}
		left_over_block->prev = block;
		block->next = left_over_block;
        
		Hist& hist = Hist::get_instance();
		hist.insert(left_over_block);

		combine_next_and_insert(left_over_block);
        
		if (block == metaDataTail) {
			metaDataTail = left_over_block;
		}
    }
}

void* smalloc(size_t size)
{
    if (size == 0 || size > 1e8) {
        return NULL;
    }
    
    size= roundToNearest8Mult(size);

    void* arr = nullptr;
    size_t sizeWithMeta = size + sizeof(MallocMetadata);

    if(size >= 128 * KB) { 
        arr = allocate_new_large_pages(sizeWithMeta);
        if(arr == NULL) {
            return NULL;
        }
        MallocMetadata* new_block = allocateMetaData(arr, size);
        ++mmap_block_counter;
        mmap_byte_counter += size;
        return getBlockAddress(new_block); 
    } 
    
    //if first smalloc:
    if (metaDataList == nullptr) {
        arr = allocate_new_pages(sizeWithMeta);
        //if failed:
        if (arr == NULL) {
            return NULL;
        }
        //if succeeded: 
        metaDataList = allocateMetaData(arr,size);
        metaDataTail = metaDataList;  
        return getBlockAddress(arr);
    }   

    Hist& hist = Hist::get_instance();
    arr = hist.get_first_free_block(size);

    //if succeeded:
    if (arr != NULL) {
        split(arr, size);
        return arr; 
    } 

    //if we didn't find free_block
    arr = allocate_new_pages(sizeWithMeta);

    //if failed:
    if (arr == NULL) {
        return NULL;
    } 

    //if succeeded:  
    MallocMetadata* new_block = allocateMetaData(arr,size);
    for (MallocMetadata* block = metaDataList; block!=nullptr; block= block->next) {
        if (block > new_block) {
            new_block->next = block;
            new_block->prev = block->prev;
            if (block->prev != nullptr) {
                block->prev->next = new_block;
                block->prev = new_block;
            }
            break;
        } else if (block->next == nullptr && new_block != metaDataTail) {
            metaDataTail = new_block;
            new_block->prev = block;
            block->next = new_block;
            break;
        }
    }
    return getBlockAddress(arr);  
}

void* scalloc(size_t num, size_t size)
{
    if((num==0) || (size == 0) || (num*size > 1e8)) {
        return NULL;
    }

    void* new_block = smalloc(num*size);
    if(new_block == NULL) {
        return NULL;
    }
    std::memset(new_block,0,num*size); 
    return new_block;
}

void sfree(void* p)
{
    if (p == NULL) {
        return;
    }
    MallocMetadata* block = getBlockMeta(p); 
    if(block->size >= 128*KB){
        --mmap_block_counter;
        mmap_byte_counter -= block->size;
        munmap((void*) block, block->size+sizeof(MallocMetadata));
        return;
    }
 
    block->is_free = true;
    combine_next_and_insert(block);
    combine_next_and_insert(block->prev);  
}

void* srealloc(void* oldp, size_t size)
{
    if (size == 0 || size>1e8) {
        return NULL;
    }

	size= roundToNearest8Mult(size);

    if (oldp == NULL) {
        return smalloc(size);
    }
    
    MallocMetadata* oldBlock = getBlockMeta(oldp); 

	//if mmaped
    if(oldBlock->size >= 128*KB){
		size_t size_to_copy = oldBlock->size;
        if (oldBlock->size > size) {
            size_to_copy = size;
        }
        void* newp = smalloc(size);
        if(newp == NULL) {
            return NULL;
        }
        std::memcpy(newp, oldp, size_to_copy);
        --mmap_block_counter;
        mmap_byte_counter -= oldBlock->size;
        munmap((void*)oldBlock, oldBlock->size+sizeof(MallocMetadata));
        return newp;
    }

    // 1) a
    if (oldBlock->size >= size) { 
        split(oldp, size);
        return oldp;
    } 

    Hist& hist = Hist::get_instance();

    // 1) b
    if(oldBlock->prev && oldBlock->prev->is_free && oldBlock->prev->size + oldBlock->size + sizeof(MallocMetadata) >= size) {
        void* prev_addr = getBlockAddress(oldBlock->prev);
        oldBlock->is_free = true;
        combine_next_and_insert(oldBlock->prev);
        hist.remove_node(oldBlock->prev);
        oldBlock->prev->is_free = false;
        std::memcpy(prev_addr, oldp, oldBlock->size);
        split(prev_addr, size);
        return prev_addr;
    } 

    // 1) c
    if(oldBlock->next && oldBlock->next->is_free && oldBlock->next->size + oldBlock->size + sizeof(MallocMetadata) >= size) { 
        oldBlock->is_free = true;
        combine_next_and_insert(oldBlock);
        hist.remove_node(oldBlock);
        oldBlock->is_free = false; 
        split(oldp, size);
        return oldp;
    } 

    // 1) d
    if(oldBlock->prev && oldBlock->prev->is_free && oldBlock->next && oldBlock->next->is_free && oldBlock->prev->size + oldBlock->next->size + oldBlock->size + 2*sizeof(MallocMetadata)>= size) {
        void* prev_addr = getBlockAddress(oldBlock->prev);
        oldBlock->is_free = true;
        combine_next_and_insert(oldBlock);
        combine_next_and_insert(oldBlock->prev);
        hist.remove_node(oldBlock->prev);
        oldBlock->prev->is_free = false;
        std::memcpy(prev_addr, oldp, oldBlock->size);
        split(prev_addr, size);
        return prev_addr;
    } 

    // 1) e + f
    if(oldBlock == metaDataTail) { // if oldp is the wilderness
		if (oldBlock->prev && oldBlock->prev->is_free) {
			void* from = oldp;
			void* to = getBlockAddress(oldBlock->prev);
			size_t size = oldBlock->size;
			oldBlock->is_free = true;
			combine_next_and_insert(oldBlock->prev);
			hist.remove_node(oldBlock->prev);
			oldBlock->prev->is_free = false;
			std::memcpy(to,from,size); 
			
			oldBlock = oldBlock->prev;
		}
        if(sbrk(size - oldBlock->size) == (void *)(-1)) {
            return NULL;
        }
        oldBlock->size = size;
        return getBlockAddress(oldBlock);
    }

    void* newp = smalloc(size);
    if (newp == NULL) {
        return NULL;
    }
	std::memcpy(newp,oldp,oldBlock->size); 
    sfree(oldp);
    return newp;
}

size_t _num_free_blocks()
{
    size_t numOfFreeBlocks = 0;
    for (MallocMetadata* block = metaDataList; block!=nullptr; block= block->next) {
        if ((block->is_free) && (block->size != 0)) {
            ++numOfFreeBlocks;
        }
    }
    return numOfFreeBlocks;
}

size_t _num_free_bytes()
{
    size_t totalFreeSize = 0;
    for (MallocMetadata* block = metaDataList; block!=nullptr; block= block->next) {
        if (block->is_free) {
            totalFreeSize+= block->size;
        }
    }
    return totalFreeSize;
}

size_t _num_allocated_blocks()
{
    size_t numOfBlocks = 0;
    for (MallocMetadata* block = metaDataList; block!=nullptr; block= block->next) {
        if (block->size != 0) {
            ++numOfBlocks; 
        }
    }
    return numOfBlocks + mmap_block_counter;
}

size_t _num_allocated_bytes()
{
    size_t totalBlockSize = 0;
    for (MallocMetadata* block = metaDataList; block!=nullptr; block= block->next) {
        totalBlockSize+= block->size;
    }
    return totalBlockSize + mmap_byte_counter;
}

size_t _num_meta_data_bytes()
{
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}

size_t _size_meta_data()
{
    return sizeof(MallocMetadata);
}
