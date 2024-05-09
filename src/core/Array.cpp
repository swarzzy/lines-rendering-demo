#include "Array.h"

template <typename T, typename Derived>
ArrayRef<T> ArrayBase<T, Derived>::AsRef()
{
    return ArrayRef<T>(_DataPtr(), _Count());
}

template <typename T, typename Derived>
T* ArrayBase<T, Derived>::_DataPtr()
{
    T* result = ((Derived*)this)->Data();
    return result;
}

template <typename T, typename Derived>
u32 ArrayBase<T, Derived>::_Count()
{
    u32 result = ((Derived*)this)->Count();
    return result;
}

template <typename T, typename Derived>
T& ArrayBase<T, Derived>::operator[](u32 i)
{
    Assert(i < _Count());
    return _DataPtr()[i];
}

template <typename T, typename Derived>
T* ArrayBase<T, Derived>::At(u32 i)
{
    Assert(i < _Count());
    return _DataPtr() + i;
}

template <typename T, typename Derived>
T* ArrayBase<T, Derived>::Last()
{
    T* result = nullptr;
    if (_Count())
    {
        result = _DataPtr() + (_Count() - 1);
    }
    return result;
}

template <typename T, typename Derived>
u32 ArrayBase<T, Derived>::IndexFromPtr(const T* it)
{
    auto data = _DataPtr();
    Assert(it >= data && it < data + _Count());
    uptr off = (uptr)it - (uptr)data;
    return (u32)off / sizeof(T);
}

template <typename T, typename Derived>
void ArrayBase<T, Derived>::Fill(T& value)
{
    for (u32 i = 0; i < _Count(); i++)
    {
        memcpy(_DataPtr() + i, &value, sizeof(T));
    }
}

template <typename T, typename Derived>
void ArrayBase<T, Derived>::Reverse()
{
    auto data = _DataPtr();
    for (u32 i = 0, j = _Count() - 1; i < j; i++, j--)
    {
        T tmp = data[i];
        data[i] = data[j];
        data[j] = tmp;
    }
}

template <typename T, typename Derived>
template <typename Fn>
u32 ArrayBase<T, Derived>::CountIf(Fn callback)
{
    u32 n = 0;
    ForEach(this, it) {
        if (callback(it)) n++;
    } EndEach;
    return n;
}

template <typename T, typename Derived>
template <typename Fn>
void ArrayBase<T, Derived>::Each(Fn callback)
{
    ForEach(this, it) {
        callback(it);
    } EndEach;
}

template <typename T, typename Derived>
template <typename Fn>
T* ArrayBase<T, Derived>::FindFirst(Fn callback)
{
    T* result = nullptr;
    ForEach(this, it) {
        if (callback(it))
        {
            result = it;
            break;
        }
    } EndEach;
    return result;
}

template <typename T>
DArray<T> DArray<T>::Clone()
{
    DArray<T> clone (allocator);
    clone.Resize(count);
    memcpy(clone.data, data, (size_t)count * sizeof(T));
    return clone;
}

template <typename T>
void DArray<T>::CopyTo(DArray<T>* other)
{
    CopyTo(other, count);
}

template <typename T>
void DArray<T>::CopyTo(DArray<T>* other, u32 copyCount)
{
    if (copyCount > other->capacity)
    {
        other->Reserve(other->_GrowCapacity(copyCount));
    }
    memcpy(other->data, data, sizeof(T) * copyCount);
    other->count = copyCount;
}

template <typename T>
void DArray<T>::Append(const DArray<T>* other)
{
    Append(other.data, other.count);
}

template <typename T>
void DArray<T>::Append(T* data, u32 n)
{
    if (n > 0)
    {
        T* mem = PushBackN(n);
        memcpy(mem, data, sizeof(T) * n);
    }
}

template <typename T>
void DArray<T>::Prepend(const DArray<T>* other)
{
    Prepend(other.data, other.count);
}

template <typename T>
void DArray<T>::Prepend(T* data, u32 n)
{
    if (n > 0)
    {
        T* mem = PushFrontN(n);
        memcpy(mem, data, sizeof(T) * n);
    }
}


template <typename T>
void DArray<T>::FreeBuffers()
{
    if (data)
    {
        allocator->Dealloc(data);
    }
    count = 0;
    capacity = 0;
    data = nullptr;
}

template <typename T>
void DArray<T>::Clear()
{
    count = 0;
}

template <typename T>
void DArray<T>::Resize(u32 newSize)
{
    if (newSize > capacity)
    {
        Reserve(_GrowCapacity(newSize));
    }
    count = newSize;
}

template <typename T>
void DArray<T>::Reserve(u32 newCapacity)
{
    if (newCapacity > capacity)
    {
        T* newData = allocator->Alloc<T>(newCapacity, false);
        if (data)
        {
            memcpy(newData, data, (size_t)count * sizeof(T));
            allocator->Dealloc(data);
        }
        data = newData;
        capacity = newCapacity;
    }
}

template <typename T>
void DArray<T>::Shrink(u32 newSize)
{
    Assert(newSize <= size);
    count = newSize;
}

template <typename T>
void DArray<T>::ShrinkBuffers(u32 newSize)
{
    unreachable();
}

template <typename T>
T* DArray<T>::PushBack()
{
    if (count == capacity)
    {
        Reserve(_GrowCapacity(count + 1));
    }
    return data + count++;
}

template <typename T>
T* DArray<T>::PushBackN(u32 num)
{
    if ((count + num) >= capacity)
    {
        Reserve(_GrowCapacity(count + num));
    }

    T* result = data + count;
    count += num;
    return result;
}


template <typename T>
void DArray<T>::PushBack(const T& v)
{
    auto entry = PushBack();
    memcpy(entry, &v, sizeof(v));
}

template <typename T>
T* DArray<T>::PushFront()
{
    T* result = nullptr;
    if (count == 0)
    {
        result = PushBack();
    }
    else
    {
        result = Insert(0);
    }
    return result;
}

template <typename T>
T* DArray<T>::PushFrontN(u32 n)
{
    T* result = nullptr;
    if (count == 0)
    {
        result = PushBackN(n);
    }
    else
    {
        result = InsertN(0, n);
    }
    return result;
}

template <typename T>
void DArray<T>::PushFront(const T& v)
{
    auto entry = PushFront();
    memcpy(entry, &v, sizeof(v));
}

template <typename T>
void DArray<T>::Pop()
{
    Assert(count > 0);
    count--;
}

template <typename T>
void DArray<T>::Erase(const T* it)
{
    Assert(it >= data && it < data + count);
    uptr off = it - data;
    memmove(data + off, data + off + 1, ((size_t)count - (size_t)off - 1) * sizeof(T));
    count--;
}

template <typename T>
void DArray<T>::EraseUnsorted(const T* it)
{
    Assert(it >= data && it < data + count);
    uptr off = it - data;
    if (it < data + count - 1)
    {
        memcpy(data + off, data + count - 1, sizeof(T));
    }
    count--;
}

template <typename T>
void DArray<T>::Erase(u32 index)
{
    Assert(index < count);
    Erase(data + index);
}

template <typename T>
void DArray<T>::EraseUnsorted(u32 index)
{
    Assert(index < count);
    EraseUnsorted(data + index);
}

template <typename T>
T* DArray<T>::InsertN(u32 index, u32 n)
{
    Assert(index < count);
    if ((count + n) >= capacity)
    {
        Reserve(_GrowCapacity(count + n));
    }

    if (index < count)
    {
        memmove(data + index + n, data + index, ((size_t)count - (size_t)index) * sizeof(T));
    }
    count += n;
    return data + index;
}

template <typename T>
T* DArray<T>::Insert(u32 index)
{
    Assert(index < count);
    if (count == capacity)
    {
        Reserve(_GrowCapacity(count + 1));
    }
    if (index < count)
    {
        memmove(data + index + 1, data + index, ((size_t)count - (size_t)index) * sizeof(T));
    }
    count++;
    return data + index;
}

template <typename T>
void DArray<T>::Insert(u32 index, const T& v)
{
    auto entry = Insert(index);
    memcpy(entry, &v, sizeof(v));
}

template <typename T>
void DArray<T>::KillDuplicatesUnsorted()
{
    for (u32 i = 0; i < count; i++)
    {
        for (u32 j = i + 1; j < count;)
        {
            if (data[i] == data[j])
            {
                EraseUnsorted(data + j);
            }
            else
            {
                j++;
            }
        }
    }
}


template <typename T>
u32 DArray<T>::_GrowCapacity(u32 sz)
{
    u32 newCapacity = capacity ? (capacity + capacity / 2) : 8;
    return newCapacity > sz ? newCapacity : sz;
}
