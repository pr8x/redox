/*
redox
-----------
MIT License

Copyright (c) 2018 Luis von der Eltz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once
#include "core\core.h"
#include "core\non_copyable.h"
#include "core\allocation\default_allocator.h"
#include "core\allocation\growth_policy.h"

#include <initializer_list> //std::initializer_list
#include <type_traits> //std::move
#include <new> //placement-new
#include <string.h> //std::memcpy

namespace redox {
	template<class ValueType, 
		class Allocator = allocation::DefaultAllocator<ValueType>,
		class GrowthPolicy = allocation::DefaultGrowth>
	class Buffer : public NonCopyable {
	public:
		using size_type = std::size_t;

		Buffer() : _data(nullptr), _reserved(0), _size(0) {}

		_RDX_INLINE Buffer(size_type size) : Buffer() {
			resize(size);
		}

		_RDX_INLINE Buffer(const ValueType* start, const size_type length) : Buffer() {
			reserve(length);
			std::memcpy(_data, start, length);
			_size = length;
		}

		_RDX_INLINE Buffer(std::initializer_list<ValueType> values) : Buffer() {
			reserve(values.size());

			//Unfortunately, initializer_list is a static container
			//that does not provide non-const access to it's data
			//This makes moving impossible and copying the only viable option.
			for (const auto& v : values)
				_push_no_checks(v);
		}

		_RDX_INLINE Buffer(Buffer&& ref) :
			_data(ref._data),
			_reserved(ref._reserved),
			_size(ref._size) {
			ref._data = nullptr;
			ref._size = 0;
			ref._reserved = 0;
		}

		_RDX_INLINE Buffer& operator=(Buffer&& ref) {
			_data = ref._data;
			_size = ref._size;
			_reserved = ref._reserved;
			ref._data = nullptr;
			ref._size = 0;
			ref._reserved = 0;
			return *this;
		}

		_RDX_INLINE void push(const ValueType& element) {
			_grow_if_needed();
			_push_no_checks(element);
		}

		_RDX_INLINE void push(ValueType&& element) {
			_grow_if_needed();
			_push_no_checks(std::move(element));
		}

		template<class...Args>
		_RDX_INLINE void emplace(Args&&...args) {
			_grow_if_needed();
			_emplace_no_checks(std::forward<Args>(args)...);
		}

		_RDX_INLINE void clear() {
			_size = 0;
			_destruct();
		}

		_RDX_INLINE ~Buffer() {
			_destruct();
			_dealloc();
		}

		_RDX_INLINE ValueType& operator[](size_type index) {
#ifdef RDX_DEBUG
			if (index >= _size)
				throw Exception("index out of bounds");
#endif
			return _data[index];
		}

		_RDX_INLINE const ValueType& operator[](size_type index) const {
#ifdef RDX_DEBUG
			if (index >= _size)
				throw Exception("index out of bounds");
#endif
			return _data[index];
		}

		_RDX_INLINE void reserve(size_type reserve) {
			if (reserve > _reserved) {
				_data = _realloc(reserve);
				_reserved = reserve;
			}
		}

		_RDX_INLINE void resize(size_type size) {
			reserve(size);

			static_assert(std::is_default_constructible_v<ValueType>);
			for (size_type elm = _size; elm < size; ++elm)
				new (_data + elm) ValueType();

			_size = size;
		}

		_RDX_INLINE ValueType* data() const {
			return _data;
		}

		_RDX_INLINE size_type size() const {
			return _size;
		}

		_RDX_INLINE size_type capacity() const {
			return _reserved;
		}

		_RDX_INLINE size_type byte_size() const {
			return _size * sizeof(ValueType);
		}

		_RDX_INLINE size_type empty() const {
			return _size == 0;
		}

		_RDX_INLINE auto begin() const {
			return _data;
		}

		_RDX_INLINE auto end() const {
			return _data + _size;
		}

	private:
		_RDX_INLINE auto _realloc(size_type reserve) {
			static_assert(std::is_move_constructible_v<ValueType>);
			auto dest = Allocator::allocate(reserve);

			//we need to copy/move the old data to the
			//newly allocated buffer.
			for (size_type elm = 0; elm < _size; ++elm)
				new (dest + elm) ValueType(std::move(_data[elm]));

			_destruct();
			_dealloc();
			return dest;
		}

		_RDX_INLINE void _grow_if_needed() {
			if (_size == _reserved) {
				GrowthPolicy fn;
				reserve(fn(_size));
			}
		}

		_RDX_INLINE void _destruct() {
			if constexpr(!std::is_trivially_destructible_v<ValueType>) {
				for (auto& c : *this)
					c.~ValueType();
			}
		}

		template<class T>
		_RDX_INLINE void _push_no_checks(T&& element) {
			new (_data + _size++) ValueType(std::forward<T>(element));
		}

		template<class...Args>
		_RDX_INLINE void _emplace_no_checks(Args&&...args) {
			new (_data + _size++) ValueType(std::forward<Args>(args)...);
		}

		_RDX_INLINE void _dealloc() {
			Allocator::deallocate(_data);
		}

		size_type _reserved;
		size_type _size;
		ValueType* _data;
	};
}