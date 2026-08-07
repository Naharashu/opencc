#ifndef ARENA_H
#define ARENA_H

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#else
#error "Unsupported OS"
#endif

class Arena {
    private:
    char* mem;
    bool used_mmap = false;
    bool used_virtualalloc = false;
    bool used_malloc = false;
    size_t arena_size=0;
    size_t arena_offset=0;
    size_t deleter_count=0;
    public:

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) = delete;
    Arena& operator=(Arena&&) = delete;

    explicit Arena(size_t size) {
        arena_size = size;
        #ifdef _WIN32
        mem = static_cast<char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if(mem == nullptr) {
            DWORD err = GetLastError();
            std::cerr << "VirtualAlloc: failed to allocate memory(code: " << err << ")\n";
            std::cerr << "Fallback to malloc()\n";
            mem = static_cast<char*>(std::malloc(size));
            if(mem == nullptr) {
                std::cerr << "Cannot allocate memory even with malloc()\n";
                std::exit(EXIT_FAILURE);
            }
            memset(mem, 0, size);
            used_malloc = true;
            return;
        }
        used_virtualalloc = true;
        #else
        mem = static_cast<char*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if(mem == MAP_FAILED) {
            perror("mmap");
            std::cerr << "Fallback to malloc()\n";
            mem = static_cast<char*>(std::malloc(size));
            if(mem == nullptr) {
                std::cerr << "Cannot allocate memory even with malloc()\n";
                std::exit(EXIT_FAILURE);
            }
            memset(mem, 0, size);
            used_malloc = true;
            return;
        }
        used_mmap = true;
        #endif
    }

    struct destructor {
        using deleter_fn = void(void*);
        deleter_fn* fn;
        void* addr;
    };

    destructor* topDeleter() {
	    if (deleter_count > 0) {
			return reinterpret_cast<destructor*>(mem + arena_size - deleter_count * sizeof(destructor));
		} else {
			return nullptr;
		}
    }

    /*
    raw allocation
    */
    void* raw_alloc(size_t _size, size_t alignment) {
        size_t offset = (arena_offset + alignment - 1) & ~(alignment - 1);
        size_t dtor_stack_size = deleter_count * sizeof(destructor);
        if(offset + _size > arena_size - dtor_stack_size) return nullptr;
        arena_offset = offset + _size;
        return mem+offset;
    }

    /*
    allocates memory for T* inside arena
    */
    template<typename T>
    void* alloc(size_t _size, size_t alignment) {
        size_t offset = (arena_offset + alignment - 1) & ~(alignment - 1);
        size_t dtor_stack_size = deleter_count * sizeof(destructor);

        if constexpr (!std::is_trivially_destructible_v<T>) {
			dtor_stack_size += sizeof(destructor);
		}
        if(offset + _size > arena_size - dtor_stack_size) return nullptr;
        arena_offset = offset + _size;
        if constexpr (!std::is_trivially_destructible_v<T>) {
			deleter_count++;
			auto* dtor = topDeleter();
			dtor->fn = +[](void* object) noexcept { reinterpret_cast<T*>(object)->~T(); };
			dtor->addr = mem+offset;
		}
        return mem+offset;
    }

    /*
    creates pointer
    */
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* ptr = alloc<T>(sizeof(T), alignof(T));
        return ptr ? new (ptr) T(std::forward<Args>(args)...) : nullptr;
    }
    /*
    Resets arena, but doesnt frees Arena
    */
    void reset() noexcept {
        for (; deleter_count > 0; deleter_count--) {
			auto* dtor = topDeleter();
			(*dtor->fn)(dtor->addr);
		}
        arena_offset = 0;
    }

    ~Arena() noexcept {
        for (; deleter_count > 0; deleter_count--) {
			auto* dtor = topDeleter();
			(*dtor->fn)(dtor->addr);
		}
        if(used_malloc) {
            std::free(mem);
        } else if(used_mmap) {
            #if defined(__linux__) || defined(__APPLE__)
            munmap(mem, arena_size);
            #endif
        } else if (used_virtualalloc) {
            #ifdef _WIN32
            VirtualFree(mem, 0, MEM_RELEASE);
            #endif
        }
    }
};

#endif

