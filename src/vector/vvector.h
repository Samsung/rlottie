/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef VVECTOR_H
#define VVECTOR_H

#include <memory>
#include <algorithm>
#include <cstring>
#include "config.h"

namespace lottie
{

#ifndef CUSTOM_LOTTIE_ALLOCATOR
	struct LottieAllocator
	{
		static void * alloc(size_t n) { return ::malloc(n); }
		static void free(void * p)    { return ::free(p); }
		static void * realloc(void * p , size_t n) { return ::realloc(p, n); }
	};

#endif
	/** An memory efficient vector class that implement std::vector interface.
	  	Unlike std::vector, this implementation does not grow exponentially, so appending to 
		the vector is a O(N) operation 
		
		@param T 	The type stored by the vector
		@param LottieAllocator  An allocator that must implement:
								 -  static void * alloc(size_t)
								 -  static void free(void *)
								 -  static void * realloc(void *, size_t)
		*/
    template <typename T>
	class vector
	{
		typedef vector<T>                   				  this_type;

	public:
		typedef T                                             value_type;
		typedef T*                                            pointer;
		typedef const T*                                      const_pointer;
		typedef T&                                            reference;
		typedef const T&                                      const_reference;  
		typedef T*                                            iterator;         
		typedef const T*                                      const_iterator;   
		typedef std::reverse_iterator<iterator>               reverse_iterator;
		typedef std::reverse_iterator<const_iterator>         const_reverse_iterator;    
		typedef size_t   					                  size_type;
		typedef ptrdiff_t						              difference_type;

		const static bool trivial = std::is_trivially_copyable<T>::value;

	public:
		vector() noexcept : first(nullptr), last(nullptr) {}
		explicit vector(size_type n) { allocate(n); }
		vector(size_type n, const value_type& value) { allocate(n, value); }
		vector(const this_type& x) { allocate(x.size()); assign(x.first, x.last); }

		template <typename InputIterator>
		vector(InputIterator first, InputIterator last) {
			allocate(last - first); assign(first, last);
		}

	    ~vector() {
			deallocate();
		}

		this_type& operator=(const this_type& x) {
			if (this != &x) {
				deallocate();
				allocate(x.size());
				assign(x.first, x.last);
			}
			return *this;
		}

		iterator       begin() noexcept { return first; }
		const_iterator begin() const noexcept { return first; }
		const_iterator cbegin() const noexcept { return first; }

		iterator       end() noexcept { return last; }
		const_iterator end() const noexcept { return last; }
		const_iterator cend() const noexcept { return last; }

		reverse_iterator       rbegin() noexcept { return reverse_iterator(end()); }
		const_reverse_iterator rbegin() const noexcept{ return const_reverse_iterator(end()); }
		const_reverse_iterator crbegin() const noexcept{ return const_reverse_iterator(cend()); }

		reverse_iterator       rend() noexcept { return reverse_iterator(begin()); }
		const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
		const_reverse_iterator crend() const noexcept { return const_reverse_iterator(crbegin()); }

		bool      empty() const noexcept { return size() == 0; }
		size_type size() const noexcept { return (size_t)(last - first); }
		size_type capacity() const noexcept { return cap; }

		void resize(size_type n, const value_type& value) { resize_impl<T>(n, value); }
		void resize(size_type n) { resize_impl<T>(n); }
		void reserve(size_type n) { if (n > cap) reserve_impl<T>(n); }

		pointer       data() noexcept { return first; }
		const_pointer data() const noexcept { return first; }

		reference       operator[](size_type n) { return first[n]; }
		const_reference operator[](size_type n) const { return first[n]; }

		reference       front() { return *first; }
		const_reference front() const { return *first; }

		reference       back() { return *(last - 1); }
		const_reference back() const { return *(last - 1); }

		void      push_back(const value_type& value) { if (size() == cap) reserve(cap + 1); new(last++) value_type(value); }
		reference push_back() { if (size() == cap) resize(cap + 1); return *(last - 1); }
		void      push_back(value_type&& value) { if (size() == cap) reserve(cap + 1); new(last++) value_type(std::move(value)); }
		void      pop_back() { free(last - 1, last != nullptr ? 1 : 0); } // Don't reduce the capacity here?

		template<class... Args>
		reference emplace_back(Args&&... args) {
			if (size() == cap) reserve(cap + 1);
			new(last++) value_type(std::forward<Args>(args)...);
			return back();
		}

		reference emplace_back() {
			if (size() == cap) reserve(cap + 1);
			new(last++) value_type();
			return back();
		}


		// iterator erase(const_iterator position);
		iterator erase(const_iterator begin, const_iterator end) {
			if (begin == end) return first;
			move(const_cast<pointer>(end), const_cast<pointer>(begin));
			free(begin, (end - begin));
			last -= (end - begin);
			return first;
		}

		// reverse_iterator erase(const_reverse_iterator position);
		// reverse_iterator erase(const_reverse_iterator first, const_reverse_iterator last);

		void clear() noexcept { deallocate(); }

	private:
		pointer  first, last;
		size_t   cap {0};

		void allocate(size_t nelem) {
			first = (value_type*)LottieAllocator::alloc(nelem * sizeof(value_type));
			last = &first[nelem];
			set(first, nelem);
			cap = nelem;
		}

		void allocate(size_t nelem, const_reference value) {
			first = (value_type*)LottieAllocator::alloc(nelem * sizeof(value_type));
			last = &first[nelem];
			set(first, nelem, value);
			cap = nelem;
		}

		void assign(const_iterator b, const_iterator e) {
			if ((size_t)(e - b) > size()) 
				e = b + size(); 
			copy(first, b, e - b);
		}

		void deallocate() {
			free(first, last - first);
			LottieAllocator::free(first);
			first = last = nullptr;
			cap = 0;
		}

		// Optimization functions for trivial types
	private:
		template<class U> typename std::enable_if<std::is_trivially_copyable<U>::value>::type copy(U* dest, const U* src, size_t size)
		{
			memcpy(dest, src, sizeof(U) * size);
		}

		template<class U> typename std::enable_if<!std::is_trivially_copyable<U>::value>::type copy(U* dest, const U* src, size_t size)
		{
			std::copy_n(src, size, dest);
		}

		void move(pointer start, pointer into, const_pointer end = 0) {
			if (!end) end = last;
			for (pointer f = start; f != end; ++f, ++into) {
				new(into) value_type(std::move(*f)); 
				f->~T();
			}
		}

		template<class U> typename std::enable_if<std::is_trivially_copyable<U>::value>::type reserve_impl(size_t size)
		{
			// Try to realloc first
			pointer p = (pointer)LottieAllocator::realloc(first, size * sizeof(value_type));
			if (p == nullptr) return; // Should throw here
			// No memcpy required, it's done in realloc
			cap = size;
			last = p + (last - first);
			first = p;
		}

		template<class U> typename std::enable_if<!std::is_trivially_copyable<U>::value>::type reserve_impl(size_t size)
		{
			// Can't realloc here, need the new size and copy the objects here
			pointer p = (pointer)LottieAllocator::alloc(size * sizeof(value_type));
			if (p == nullptr) return; // Should throw here
			move(first, p);
			cap = size;
			last = p + (last - first);
			LottieAllocator::free(first);
			first = p;
		}

		template<class U> typename std::enable_if<std::is_trivially_copyable<U>::value>::type resize_impl(size_t size)
		{
			size_t cur_size = (last - first);
			if (cur_size == size) return;
			// Try to realloc first
			pointer p = (pointer)LottieAllocator::realloc(first, size * sizeof(value_type));
			if (p == nullptr) return; // Should throw here
			// No memcpy required, it's done in realloc
			cap = size;
			if (cur_size < size) {
				set(p + cur_size, size - cur_size);
			}
			last = p + size;
			first = p;
		}

		template<class U> typename std::enable_if<!std::is_trivially_copyable<U>::value>::type resize_impl(size_t size)
		{
			size_t cur_size = (last - first);
			if (cur_size == size) return;
			// Try to realloc first
			pointer p = (pointer)LottieAllocator::alloc(size * sizeof(value_type));
			if (p == nullptr) return; // Should throw here
			std::copy_n(std::make_move_iterator(first), std::min(cur_size, size), p);
			free(first, cur_size);
			cap = size;
			if (cur_size < size) {
				set(p + cur_size, size - cur_size);
			}
			last = p + size;
			LottieAllocator::free(first);
			first = p;
		}

		template<class U> typename std::enable_if<std::is_trivially_copyable<U>::value>::type resize_impl(size_t size, const U & value)
		{
			size_t cur_size = (last - first);
			if (cur_size == size) return;
			// Try to realloc first
			pointer p = (pointer)LottieAllocator::realloc(first, size * sizeof(value_type));
			if (p == nullptr) return; // Should throw here
			// No memcpy required, it's done in realloc
			cap = size;
			if (cur_size < size) {
				set(p + cur_size, size - cur_size, value);
			}
			last = p + size;
			first = p;
		}

		template<class U> typename std::enable_if<!std::is_trivially_copyable<U>::value>::type resize_impl(size_t size, const U & value)
		{
			size_t cur_size = (last - first);
			if (cur_size == size) return;
			// Try to realloc first
			pointer p = (pointer)LottieAllocator::alloc(size * sizeof(value_type));
			if (p == nullptr) return; // Should throw here
			free(first + size, cur_size > size ? cur_size - size : 0);
			std::copy_n(std::make_move_iterator(first), std::min(cur_size, size), p);
			cap = size;
			if (cur_size < size) {
				set(p + cur_size, size - cur_size, value);
			}
			last = p + size;
			LottieAllocator::free(first);
			first = p;
		}

		template<class U> typename std::enable_if<std::is_trivially_copyable<U>::value>::type set(U* dest, size_t size, const U & value)
		{
			while (size--)
				memcpy(dest++, &value, sizeof(U));
		}

		template<class U> typename std::enable_if<!std::is_trivially_copyable<U>::value>::type set(U* dest, size_t size, const U & value)
		{
			while (size--)
				new(dest++) U(value);
		}
		template<class U> typename std::enable_if<std::is_trivially_copyable<U>::value>::type set(U* dest, size_t size)
		{
			U u{};
			while (size--)
				memcpy(dest++, &u, sizeof(U));
		}

		template<class U> typename std::enable_if<!std::is_trivially_copyable<U>::value>::type set(U* dest, size_t size)
		{
			while (size--)
				new(dest++) U();
		}

		template<class U> typename std::enable_if<std::is_trivially_copyable<U>::value>::type free(const U* dest, size_t size)
		{
		}
		template<class U> typename std::enable_if<!std::is_trivially_copyable<U>::value>::type free(const U* dest, size_t size)
		{
			while (size--)
				(dest++)->~U();
		}



	private:
		/* Below is the missing interface from a std::vector that's not used in lottie, hence, not implemented */

		// void set_capacity(size_type n = 0);   // Revises the capacity to the user-specified value. Resizes the container to match the capacity if the requested capacity n is less than the current size. If n == npos then the capacity is reallocated (if necessary) such that capacity == size.
		// void shrink_to_fit();                               // C++11 function which is the same as set_capacity().
		// explicit vector(const allocator_type& allocator) noexcept;
		// vector(const this_type& x, const allocator_type& allocator);
		// vector(this_type&& x) noexcept;
		// vector(this_type&& x, const allocator_type& allocator);
		// vector(std::initializer_list<value_type> ilist);
		// this_type& operator=(std::initializer_list<value_type> ilist);
		// this_type& operator=(this_type&& x); // TODO(c++17): noexcept(allocator_traits<Allocator>::propagate_on_container_move_assignment::value || allocator_traits<Allocator>::is_always_equal::value)

		// void swap(this_type& x); // TODO(c++17): noexcept(allocator_traits<Allocator>::propagate_on_container_move_assignment::value || allocator_traits<Allocator>::is_always_equal::value)

		// void assign(size_type n, const value_type& value);

		// template <typename InputIterator>
		// void assign(InputIterator first, InputIterator last);
		// template<class... Args>
		// iterator emplace(const_iterator position, Args&&... args);

		// void assign(std::initializer_list<value_type> ilist);
		// iterator insert(const_iterator position, const value_type& value);
		// iterator insert(const_iterator position, size_type n, const value_type& value);
		// iterator insert(const_iterator position, value_type&& value);
		// iterator insert(const_iterator position, std::initializer_list<value_type> ilist);

		// template <typename InputIterator>
		// iterator insert(const_iterator position, InputIterator first, InputIterator last);

		// iterator erase_first(const T& value);
		// iterator erase_first_unsorted(const T& value); // Same as erase, except it doesn't preserve order, but is faster because it simply copies the last item in the vector over the erased position.
		// reverse_iterator erase_last(const T& value);
		// reverse_iterator erase_last_unsorted(const T& value); // Same as erase, except it doesn't preserve order, but is faster because it simply copies the last item in the vector over the erased position.
		// iterator erase_unsorted(const_iterator position);         // Same as erase, except it doesn't preserve order, but is faster because it simply copies the last item in the vector over the erased position.
		// reverse_iterator erase_unsorted(const_reverse_iterator position);
		// void reset_lose_memory() noexcept;                       // This is a unilateral reset to an initially empty state. No destructors are called, no deallocation occurs.

		// bool validate() const noexcept;
		// int  validate_iterator(const_iterator i) const noexcept;
		// reference       at(size_type n);
		// const_reference at(size_type n) const;
		// void*     push_back_uninitialized();
	};

	template <typename T>
	inline bool operator==(const vector<T>& a, const vector<T>& b)
	{
		return ((a.size() == b.size()) && std::equal(a.begin(), a.end(), b.begin()));
	}


	template <typename T>
	inline bool operator!=(const vector<T>& a, const vector<T>& b)
	{
		return ((a.size() != b.size()) || !std::equal(a.begin(), a.end(), b.begin()));
	}


	template <typename T>
	inline bool operator<(const vector<T>& a, const vector<T>& b)
	{
		return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
	}


	template <typename T>
	inline bool operator>(const vector<T>& a, const vector<T>& b)
	{
		return b < a;
	}


	template <typename T>
	inline bool operator<=(const vector<T>& a, const vector<T>& b)
	{
		return !(b < a);
	}


	template <typename T>
	inline bool operator>=(const vector<T>& a, const vector<T>& b)
	{
		return !(a < b);
	}


	template <typename T>
	inline void swap(vector<T>& a, vector<T>& b)
	{
		a.swap(b);
	}
}

#ifdef LOTTIE_MEMSHRINK_SUPPORT
    template <typename T>
    using VVector = lottie::vector<T>;
#else
    #include <vector>
    template <typename T>
    using VVector = std::vector<T>;
#endif

#endif
