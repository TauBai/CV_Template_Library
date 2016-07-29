/*
    因为free-list 这种内存分配方式占用内存较大(sgi stl每次分配两倍所需内存, 一半用来使用一半用于下一次分配),  
    但是它减少了内存碎片, 在所以这种分配方式暂时保留, 遇到新的方法时再添加
    暂时不考虑多线程


    因为目前还是菜鸡, 虽然能写简单的空间配置器, 但是很多地方想不到位, 不能独立写一个像STL一样完整的空间配置器, 
    基本是照着<<STL源码剖析>> 打下的代码, 版权应属于SGI STL. 侯捷版代码不完全, 所以还参考了SGI STL源码
    刚刚编译过, 一大票的错误, 暂时先这样
*/

#ifndef __ALLOC_H
#define __ALLOC_H

#ifndef __THROW_BAD_ALLOC
#  if defined(__STL_NO_BAD_ALLOC) || !defined(__STL_USE_EXCEPTIONS)
#    include <stdio.h>
#    include <stdlib.h>
#    define __THROW_BAD_ALLOC fprintf(stderr, "out of memory\n"); exit(1)
#  else /* Standard conforming out-of-memory handling */
#    include <new>
#    define __THROW_BAD_ALLOC throw std::bad_alloc()
#  endif
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef __RESTRICT
#  define __RESTRICT
#endif

#include "__alloc.h"

namespace tau {
template <class T>
class allocator{
public:
    typedef T              value_type;
    typedef T*                pointer;
    typedef const T*    const_pointer;
    typedef size_t          size_type;
    typedef ptrdiff_t difference_type;
    typedef T&              reference;
    typedef const T&  const_reference;
    
    template<class U>
    struct rebind
    {
        typedef allocator<U> other;
    };
//设计allocator, 首先要有构造,析构函数,复制


    allocator();
    allocator(const allocator& x){
        
    }
    ~allocator();
    


    pointer address(reference x){
        return (pointer)&x;
    }
    const_pointer const_address(const_reference x) {
        return (const_pointer)&x;
    }
    

    pointer allocate(size_type n, const void *hint = 0){
        return _allocate((difference_type)n, (pointer)0);
    }
    
    void deallocate(pointer p,size_type n){
        _deallocate(p);
    }

    
    

    size_type max_size() const {
        return size_type(UINT_MAX/sizeof(value_type));
    }




};
///////////////////////////////////////////////////////////
//////////////    constructor, destructor   //////////////////////////
template <class T1,class T2>
inline void construct(T1 *p,const T2 value){
    new (p) T1(value);
}
// normal version
template <class T>
inline void destroy(T* pointer){
    pointer-> ~T();
}
// iterator version
template <class ForwardIterator>
inline void destroy(ForwardIterator first, ForwardIterator last){
    __destroy(first,last,value_type(first));
}
template <class ForwardIterator, class T>
inline void __destroy(ForwardIterator first, ForwardIterator last, T*){
    typedef typename __type_traits<T>::has_trivial_destructor trivial_destructor
    __destroy_aux(ForwardIterator first, ForwardIterator last, trivial_destructor());
}
// has no trivial destructor
template <class ForwardIterator>
inline void __destroy_aux(ForwardIterator first,ForwardIterator last, __false_type){
    for(;first < last; ++first){
        destroy(&*first);
    }
}
// has trivial destructor
template <class ForwardIterator>
inline void __destroy_aux(ForwardIterator, ForwardIterator, __true_type){ }
// partial specialization for char, wchar_t
inline void destroy(char*,char*){}
inline void destroy(wchar_t*,wchar_t*){}
////////////////////////////////////////////////////////////////

template <class T, class Alloc>
class simple_alloc{
public :
    static T *allocate(size_t n){
        return 0 == n ? (T*)tau::allocate(n*sizeof(T));
    }
    static T *allocate(void){
        return (T*) tau::alocate(sizeof(T));
    }
    static void deallocate(T* p , size_t n){
        if(n != 0 && p != NULL)
            tau::deallocate(p, n*sizeof(T));
    }
    static void deallocate(T *p){
        if(p != NULL)
             tau::deallocate(p,sizeof(T));
    }

}

/////////////////////////////////////////////////////////////////////

template <int inst>
class __malloc_alloc_template{
private :
    static void *oom_malloc(size_t);
    static void *oom_realloc(void*, size_t);
    static void (* __malloc_alloc_oom_handler)();

public:
    //为什么不用判断n是否为0?
    static void *allocate(size_t n){
        void *result = malloc(n);
        if(result == 0)
            result = oom_malloc(n);
        return result;
    }

    static void deallocate(void *p, size_t){
        free(p);
    }
    static void *reallocate(void *p,size_t,size_t new_sz){
        void* result =  realloc(p,new_sz);
        if(0 == result) result  = oom_realloc(p,new_sz);  //当realloc不出时使用oom
        return result;
    }
    static void (* set_malloc_handler(void (*f)())) (){
        void(* old)() = __malloc_alloc_oom_handler;
        __malloc_alloc_oom_handler = f;
        return (old);
    }

};

template <int inst>
void (* __malloc_alloc_template<inst>::__malloc_alloc_oom_handler) () = 0;
template <int inst>
void *__malloc_alloc_template<inst>::oom_malloc(size_t n){
    void (* my_malloc_handler)();
    void *result;
    for(;;){
        my_malloc_handler = __malloc_alloc_oom_handler;
        if(my_malloc_handler == 0){
            __THROW_BAD_ALLOC;
        }
        (*my_malloc_handler)();
        result = malloc(n);
        if(result)
            return result;
    }

}
template <int inst>
void* __malloc_alloc_template<inst>::oom_realloc(void* p, size_t n ){
    void (* my_malloc_handler)();
    void *result;
    for(;;){
        my_malloc_handler = __malloc_alloc_oom_handler;
        if(my_malloc_handler == 0){
            __THROW_BAD_ALLOC;
        }
        (*my_malloc_handler)();
        result = realloc(p,n);
        if(result)
            return result;
    }

}

typedef __malloc_alloc_template<0> malloc_alloc;


////////////////////////////////////////////////////////////////////

#ifdef __USE_MALLOC

typedef malloc_alloc alloc;
typedef malloc_alloc single_client_alloc;

#else

// Default node allocator.
// With a reasonable compiler, this should be roughly as fast as the
// original STL class-specific allocators, but with less fragmentation.
// Default_alloc_template parameters are experimental and MAY
// DISAPPEAR in the future.  Clients should just use alloc for now.
//
// Important implementation properties:
// 1. If the client request an object of size > _MAX_BYTES, the resulting
//    object will be obtained directly from malloc.
// 2. In all other cases, we allocate an object of size exactly
//    _S_round_up(requested_size).  Thus the client has enough size
//    information that we can return the object to the proper free list
//    without permanently losing part of the object.
//

// The first template parameter specifies whether more than one thread
// may use this allocator.  It is safe to allocate an object from
// one instance of a default_alloc and deallocate it with another
// one.  This effectively transfers its ownership to the second one.
// This may have undesirable effects on reference locality.
// The second parameter is unreferenced and serves only to allow the
// creation of multiple default_alloc instances.
// Node that containers built on different allocator instances have
// different types, limiting the utility of this approach.
#if defined(__SUNPRO_CC) || defined(__GNUC__)
// breaks if we make these template class members:
  enum {_ALIGN = 8};
  enum {_MAX_BYTES = 128};
  enum {_NFREELISTS = 16}; // _MAX_BYTES/_ALIGN
#endif

template <bool threads, int inst>
class __default_alloc_template {

private:
    static size_t ROUND_UP(size_t bytes){
        return ((bytes) + _ALIGN -1) & ~(__ALIGH - 1);
    }
private:
    union obj{
        union obj* free_list_link;
        char client_data[1];
    }
private:
    static obj* volatile free_list[_NFREELISTS];
    static size_t FREELIST_INDEX(size_t bytes){
        return ( (bytes) + _ALIGN - 1)/_ALIGN -1 );
    }
    
    static void *refill(size_t n);
    static char* chunk_alloc(size_t size, int& nobjs);

    //chunk allocation state
    static char* start_free;
    static char* end_free;
    static size_t heap_size;
public:
    static void* allocate(size_t n){
        obj* volatile * my_free_list;
        obj* result;
//////////  if n > _MAX_BYTES  use malloc/////////////
        if(n > (size_t)_MAX_BYTES){
            return (malloc_alloc::allocate(n));
        }
        my_free_list = free_list + FREELIST_INDEX(n);
/////   result
        result = *my_free_list;
/////   if no free_list left, refill  //////////
        if(result == 0){
            void* r =refill(ROUND_UP(n));
            return r;
        }
/////   else
        *my_free_list = result -> free_list_link;
        return (result);
    }
    static void deallocate(void* p, size_t n){
        obj* q = (obj*)p;
        obj* volatile * my_free_list;

        if( n > _MAX_BYTES ){
            malloc_alloc::deallocate(p,n);
            return;
        }
        
        my_free_list = free_list + FREELIST_INDEX(n);
        q -> free_list_link = *my_free_list;
        *my_free_list = q;

    }
    static void* reallocate(void* p, size_t old_sz, size_t new_sz){
        void* result;
        size_t copy_sz;

        if (old_sz > (size_t) _MAX_BYTES && new_sz > (size_t) _MAX_BYTES) {
            return(realloc(p, new_sz));
        }
        if (ROUND_UP(old_sz) == ROUND_UP(new_sz)) 
            return(p);
        result = allocate(new_sz);
        copy_sz = new_sz > old_sz? old_sz : new_sz;
        memcpy(result, p, copy_sz);
        deallocate(p, old_sz);
        return (result);

    }
};


/////     下面是类中各元素的初值, 在新版的SGI STL未出现              ////
template <bool threads, int inst>
char* __default_alloc_template<threads, inst>::start_free = 0;

template <bool threads, int inst>
char* __default_alloc_template<threads,inst>::end_free = 0;

template <bool threads, int inst>
size_t __default_alloc_template<threads,inst>::heap_size = 0;

template <bool threads, int inst>
__default_alloc_template<threads,inst>::free_list[_NFREELISTS] = 
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
///////////                                         ////////////////////////

static void* __default_alloc_template::refill(size_t n){
    int nobjs = 20;
    char* chunk = chunk_alloc(n, nobjs);
    Obj* volatile my_free_list;
    Obj* result;
    Obj* current_obj;
    Obj* next_obj;
    int i;

    if (1 == nobjs)
        return(chunk);
    my_free_list = free_list + FREELIST_INDEX(n);

    /* Build free list in chunk */
      result = (Obj*)chunk;
      *my_free_list = next_obj = (Obj*)(chunk + n);
      for (i = 1; ; ++i) {
        current_obj = next_obj;
        next_obj = (Obj*)((char*)next_obj + n);
        if (nobjs - 1 == i) {
            current_obj -> free_list_link = 0;
            break;
        } else {
            current_obj -> free_list_link = next_obj;
        }
      }
    return (result);
}
static void* __default_alloc_template::chunk_alloc(size_t size, int& nobjs){
    int nobjs = 20;
    char* chunk = chunk_alloc(n, nobjs);
    Obj* volatile my_free_list;
    Obj* result;
    Obj* current_obj;
    Obj* next_obj;
    int i;

    if (1 == nobjs) 
        return(chunk);
    my_free_list = free_list + FREELISTS_INDEX(n);

      result = (Obj*)chunk;
      *my_free_list = next_obj = (Obj*)(chunk + n);
      for (i = 1; ; ++i) {
          current_obj = next_obj;
          next_obj = (Obj*)((char*)next_obj + n);
          if (nobjs - 1 == i) {
              current_obj -> free_list_link = 0;
              break;
          } else {
              current_obj -> free_list_link = next_obj;
          }
      }
    return (result);
}

#endif
#endif 
