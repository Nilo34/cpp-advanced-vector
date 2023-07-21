#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }
    
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept 
        : buffer_(std::exchange(other.buffer_, nullptr))
        , capacity_(std::exchange(other.capacity_, 0))
    {
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            
            buffer_ = std::move(rhs.buffer_);
            capacity_ = rhs.capacity_;
            
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        
        return *this;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    
    using iterator = T*;
    using const_iterator = const T*;
    
    Vector() = default;
    
    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0))  //
    {
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        
        RawMemory<T> new_data(new_capacity);
        // Конструируем элементы в new_data
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
        // При выходе из метода старая память будет возвращена в кучу
    }
    
    size_t Size() const noexcept {
        return size_;
    }
    
    size_t Capacity() const noexcept {
        return data_.Capacity();
    }
    
    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }
    
    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(other.size_, size_);
    }
    
    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /* Применить copy-and-swap */
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                if (rhs.size_ < size_) {
                    // Размер вектора-источника меньше размера вектора-приёмника
                    std::copy(rhs.data_.GetAddress(),
                              rhs.data_.GetAddress()+rhs.size_,
                              data_.GetAddress());
                    
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                } else {
                    // Размер вектора-источника больше или равен размеру вектора-приёмника
                    std::copy(rhs.data_.GetAddress(),
                              rhs.data_.GetAddress() + size_,
                              data_.GetAddress());
                    
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_,
                                              rhs.size_ - size_,
                                              data_.GetAddress() + size_);
                }
                
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_.Swap(rhs.data_);
            size_ = rhs.size_;
            
            rhs.size_ = 0;
        }
        
        return *this;
    }
    
    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        
        size_ = new_size;
    }
    
    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    void PushBack(T&& value) {
        EmplaceBack(std::forward<T>(value));
    }
    
    void PopBack() noexcept {
        assert(size_ != 0);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            
            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
            try {
                // Конструируем элементы в new_data
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
                }
            } catch(...) {
                std::destroy_n(new_data.GetAddress(), size_);
                
                throw;
            }
            
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
            // При выходе из метода старая память будет возвращена в кучу
            
        } else {
            new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        }
        
        ++size_;
        return data_[size_ - 1];
    }
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return cbegin();
    }
    const_iterator end() const noexcept {
        return cend();
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert((begin() <= pos) && (pos <= end()));
        
        int num_pos = pos - begin(); // ибо итератор при изменении вектора теряет актуальность
        
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            // вызываем конструктор нового элемента
            new (new_data.GetAddress() + num_pos) T(std::forward<Args>(args)...);
            
            // Конструируем элементы в new_data
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), num_pos, new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + num_pos,
                                          size_ - num_pos,
                                          new_data.GetAddress() + num_pos + 1);

            } else {
                    try {
                        std::uninitialized_copy_n(data_.GetAddress(), num_pos, new_data.GetAddress());
                    } catch(...) {
                        std::destroy_at(new_data.GetAddress() + num_pos);
                        throw;
                    }
                    try {
                        std::uninitialized_copy_n(data_.GetAddress() + num_pos,
                                                  size_ - num_pos,
                                                  new_data.GetAddress() + num_pos + 1);
                    } catch(...) {
                        std::destroy_n(new_data.GetAddress(), num_pos);
                        throw;
                    }
                }
            
            
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
            // При выходе из метода старая память будет возвращена в кучу
            
        } else {
            
            if (pos == end()) {
                    new (end()) T(std::forward<Args>(args)...);
            } else {
                T temporary_value(std::forward<Args>(args)...);
                
                new (end()) T(std::forward<T>(data_[0]));
                
                try {    
                    std::move_backward(begin() + num_pos, end() - 1, end());
                } catch(...) {
                    std::destroy_at(end());
                    throw;
                }
                
                *(begin() + num_pos) = std::forward<T>(temporary_value);
            }
            
        }
        
        ++size_;
        return begin() + num_pos;
    }
    
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        assert((begin() <= pos) && (pos <= end()));
        
        auto num_pos = pos - begin(); // ибо итератор при изменении вектора теряет актуальность
        
        std::move(begin() + num_pos + 1, 
                  end(), 
                  begin() + num_pos);
        
        PopBack();
        
        return begin() + num_pos;
    }
    
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }
    
private:
    
    RawMemory<T> data_;
    size_t size_ = 0;
};