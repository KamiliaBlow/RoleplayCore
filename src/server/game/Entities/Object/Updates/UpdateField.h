/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UpdateField_h__
#define UpdateField_h__

#include "ObjectGuid.h"
#include "Optional.h"
#include "UpdateMask.h"
#include <algorithm>
#include <memory>
#include <vector>

class ByteBuffer;
class Object;

namespace UF
{
    enum class UpdateFieldFlag : uint8
    {
        None = 0,
        Owner = 0x01,
        PartyMember = 0x02,
        UnitAll = 0x04,
        Empath = 0x08
    };

    DEFINE_ENUM_FLAG(UpdateFieldFlag);

    template<typename T>
    class UpdateFieldBase;

    template<typename T, int32 BlockBit, uint32 Bit>
    class UpdateField;

    template<typename T>
    class UpdateFieldArrayBaseWithoutSize;

    template<typename T, std::size_t Size>
    class UpdateFieldArrayBase;

    template<typename T, std::size_t Size, uint32 Bit, int32 FirstElementBit>
    class UpdateFieldArray;

    template<typename T>
    class DynamicUpdateFieldBase;

    template<typename T, int32 BlockBit, uint32 Bit>
    class DynamicUpdateField;

    template<typename T>
    class OptionalUpdateFieldBase;

    template<typename T, int32 BlockBit, uint32 Bit>
    class OptionalUpdateField;

    template<typename T, bool PublicSet>
    struct MutableFieldReferenceWithChangesMask;

    template<typename T, bool PublicSet>
    struct MutableFieldReferenceNoChangesMask;

    template<typename T, bool PublicSet>
    struct MutableNestedFieldReference;

    struct IsUpdateFieldStructureTag
    {
    };
    struct HasChangesMaskTag
    {
    };
    struct IsUpdateFieldHolderTag
    {
    };

    template<typename T>
    struct UpdateFieldPrivateSetter
    {
        using value_type = T;

        template<typename F>
        friend bool SetUpdateFieldValue(UpdateFieldPrivateSetter<F>& setter, typename UpdateFieldPrivateSetter<F>::value_type&& value);

        UpdateFieldPrivateSetter(T& value) : _value(value)
        {
        }

        T GetValue() const
        {
            return _value;
        }

    private:
        bool SetValue(T value)
        {
            if (_value != value)
            {
                _value = std::move(value);
                return true;
            }
            return false;
        }

        T& _value;
    };

    // Same as UpdateFieldSetter but with public setter, used to set member fields for values added to dynamic fields
    template<typename T>
    struct UpdateFieldPublicSetter
    {
        using value_type = T;

        UpdateFieldPublicSetter(T& value) : _value(value)
        {
        }

        T GetValue() const
        {
            return _value;
        }

        void SetValue(T value)
        {
            _value = std::move(value);
        }

    private:
        T& _value;
    };

    template<typename T, bool PublicSet>
    using UpdateFieldSetter = std::conditional_t<PublicSet, UpdateFieldPublicSetter<T>, UpdateFieldPrivateSetter<T>>;

    template<typename T, bool PublicSet>
    using MutableFieldReference = std::conditional_t<std::is_base_of_v<HasChangesMaskTag, T>,
        MutableFieldReferenceWithChangesMask<T, PublicSet>,
        MutableFieldReferenceNoChangesMask<T, PublicSet>>;

    template<typename T>
    struct DynamicUpdateFieldSetter
    {
        using value_type = T;
        using insert_result = std::conditional_t<std::is_base_of_v<HasChangesMaskTag, T>, MutableFieldReference<T, true>, T&>;

        template<typename F>
        friend typename DynamicUpdateFieldSetter<F>::insert_result AddDynamicUpdateFieldValue(DynamicUpdateFieldSetter<F>& setter);

        template<typename F>
        friend typename DynamicUpdateFieldSetter<F>::insert_result InsertDynamicUpdateFieldValue(DynamicUpdateFieldSetter<F>& setter, uint32 index);

        template<typename F>
        friend void RemoveDynamicUpdateFieldValue(DynamicUpdateFieldSetter<F>& setter, uint32 index);

        template<typename F>
        friend void ClearDynamicUpdateFieldValues(DynamicUpdateFieldSetter<F>& setter);

        DynamicUpdateFieldSetter(std::vector<T>& values, std::vector<uint32>& updateMask) : _values(values), _updateMask(updateMask)
        {
        }

    private:
        insert_result AddValue()
        {
            MarkChanged(_values.size());
            T& value = _values.emplace_back();
            if constexpr (std::is_base_of_v<HasChangesMaskTag, T>)
                value._changesMask.SetAll();
            return { value };
        }

        insert_result InsertValue(std::size_t index)
        {
            _values.emplace(_values.begin() + index);
            for (std::size_t i = index; i < _values.size(); ++i)
            {
                MarkChanged(i);
                // also mark all fields of value as changed
                if constexpr (std::is_base_of_v<HasChangesMaskTag, T>)
                    _values[i]._changesMask.SetAll();
            }
            return { _values[index] };
        }

        void RemoveValue(std::size_t index)
        {
            // remove by shifting entire container - client might rely on values being sorted for certain fields
            _values.erase(_values.begin() + index);
            for (std::size_t i = index; i < _values.size(); ++i)
            {
                MarkChanged(i);
                // also mark all fields of value as changed
                if constexpr (std::is_base_of_v<HasChangesMaskTag, T>)
                    _values[i]._changesMask.SetAll();
            }
            if (_values.size() % 32)
                _updateMask[UpdateMaskHelpers::GetBlockIndex(_values.size())] &= ~UpdateMaskHelpers::GetBlockFlag(_values.size());
            else
                _updateMask.pop_back();
        }

        void Clear()
        {
            _values.clear();
            _updateMask.clear();
        }

        void MarkChanged(std::size_t index)
        {
            std::size_t block = UpdateMaskHelpers::GetBlockIndex(index);
            if (block >= _updateMask.size())
                _updateMask.resize(block + 1);

            _updateMask[block] |= UpdateMaskHelpers::GetBlockFlag(index);
        }

        std::vector<T>& _values;
        std::vector<uint32>& _updateMask;
    };

    template<typename T>
    struct OptionalUpdateFieldSetter
    {
        template<typename F>
        friend void RemoveOptionalUpdateFieldValue(OptionalUpdateFieldSetter<F>& setter);

        OptionalUpdateFieldSetter(OptionalUpdateFieldBase<T>& field) : _field(field) { }

    private:
        void RemoveValue()
        {
            if (_field.has_value())
                _field.DestroyValue();
        }

        OptionalUpdateFieldBase<T>& _field;
    };

    template<typename T, bool PublicSet>
    struct MutableFieldReferenceWithChangesMask
    {
        MutableFieldReferenceWithChangesMask(T& value) : _value(value)
        {
        }

        template<typename V, int32 BlockBit, uint32 Bit>
        std::conditional_t<std::is_base_of_v<IsUpdateFieldStructureTag, V>,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldHolderTag, V>,
            MutableNestedFieldReference<V, PublicSet>,
            UpdateFieldSetter<V, PublicSet>>>
            ModifyValue(UpdateField<V, BlockBit, Bit>(T::* field))
        {
            if constexpr (BlockBit >= 0)
                _value._changesMask.Set(BlockBit);

            _value._changesMask.Set(Bit);
            return { (_value.*field)._value };
        }

        template<typename V, std::size_t Size, uint32 Bit, int32 FirstElementBit>
        std::conditional_t<std::is_base_of_v<IsUpdateFieldStructureTag, V>,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldHolderTag, V>,
            MutableNestedFieldReference<V, PublicSet>,
            UpdateFieldSetter<V, PublicSet>>>
            ModifyValue(UpdateFieldArray<V, Size, Bit, FirstElementBit>(T::* field), uint32 index)
        {
            _value._changesMask.Set(Bit);
            if constexpr (FirstElementBit >= 0)
            {
                if constexpr (!std::is_base_of_v<IsUpdateFieldHolderTag, V>)
                    _value._changesMask.Set(FirstElementBit + index);
                else
                    _value._changesMask.Set(FirstElementBit);
            }

            return { (_value.*field)._values[index] };
        }

        template<typename V, int32 BlockBit, uint32 Bit>
        DynamicUpdateFieldSetter<V> ModifyValue(DynamicUpdateField<V, BlockBit, Bit>(T::* field))
        {
            if constexpr (BlockBit >= 0)
                _value._changesMask.Set(BlockBit);

            _value._changesMask.Set(Bit);
            return { (_value.*field)._values, (_value.*field)._updateMask };
        }

        template<typename V, int32 BlockBit, uint32 Bit>
        std::conditional_t<std::is_base_of_v<IsUpdateFieldStructureTag, V>,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldHolderTag, V>,
            MutableNestedFieldReference<V, PublicSet>,
            UpdateFieldSetter<V, PublicSet>>>
            ModifyValue(DynamicUpdateField<V, BlockBit, Bit>(T::* field), uint32 index)
        {
            if (index >= (_value.*field).size())
            {
                // fill with zeros until reaching desired slot
                (_value.*field)._values.resize(index + 1);
                (_value.*field)._updateMask.resize((index + 1 + 31) / 32);
            }

            if constexpr (BlockBit >= 0)
                _value._changesMask.Set(BlockBit);

            _value._changesMask.Set(Bit);
            (_value.*field).MarkChanged(index);
            return { (_value.*field)._values[index] };
        }

        template<typename V, int32 BlockBit, uint32 Bit>
        OptionalUpdateFieldSetter<V> ModifyValue(OptionalUpdateField<V, BlockBit, Bit>(T::* field))
        {
            if constexpr (BlockBit >= 0)
                _value._changesMask.Set(BlockBit);

            _value._changesMask.Set(Bit);
            return { _value.*field };
        }

        template<typename V, int32 BlockBit, uint32 Bit>
        std::conditional_t<std::is_base_of_v<IsUpdateFieldStructureTag, V>,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldHolderTag, V>,
            MutableNestedFieldReference<V, PublicSet>,
            UpdateFieldSetter<V, PublicSet>>>
            ModifyValue(OptionalUpdateField<V, BlockBit, Bit>(T::* field), uint32 /*dummy*/)
        {
            if (!(_value.*field).has_value())
                (_value.*field).ConstructValue();

            if constexpr (BlockBit >= 0)
                _value._changesMask.Set(BlockBit);

            _value._changesMask.Set(Bit);
            return { *((_value.*field)._value) };
        }

    private:
        T& _value;
    };

    template<typename T, bool PublicSet>
    struct MutableFieldReferenceNoChangesMask
    {
        MutableFieldReferenceNoChangesMask(T& value) : _value(value)
        {
        }

        template<typename V>
        UpdateFieldSetter<V, PublicSet> ModifyValue(V(T::* field))
        {
            return { _value.*field };
        }

        template<typename V, std::size_t Size>
        UpdateFieldSetter<V, PublicSet> ModifyValue(V(T::* field)[Size], uint32 index)
        {
            return { (_value.*field)[index] };
        }

    private:
        T& _value;
    };

    template<typename T, bool PublicSet>
    struct MutableNestedFieldReference
    {
        using value_type = typename T::value_type;

        MutableNestedFieldReference(T& value) : _value(value)
        {
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of_v<UpdateFieldBase<value_type>, U>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldStructureTag, value_type>,
            MutableFieldReference<value_type, PublicSet>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldHolderTag, value_type>,
            MutableNestedFieldReference<value_type, PublicSet>,
            UpdateFieldSetter<value_type, PublicSet>>>>
            ModifyValue()
        {
            return { _value._value };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of_v<UpdateFieldArrayBaseWithoutSize<value_type>, U>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldStructureTag, value_type>,
            MutableFieldReference<value_type, PublicSet>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldHolderTag, value_type>,
            MutableNestedFieldReference<value_type, PublicSet>,
            UpdateFieldSetter<value_type, PublicSet>>>>
            ModifyValue(uint32 index)
        {
            return { _value._values[index] };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of_v<DynamicUpdateFieldBase<value_type>, U>, DynamicUpdateFieldSetter<value_type>>
            ModifyValue()
        {
            return { _value._values, _value._updateMask };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of_v<DynamicUpdateFieldBase<value_type>, U>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldStructureTag, value_type>,
            MutableFieldReference<value_type, PublicSet>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldHolderTag, value_type>,
            MutableNestedFieldReference<value_type, PublicSet>,
            UpdateFieldSetter<value_type, PublicSet>>>>
            ModifyValue(uint32 index)
        {
            if (index >= _value.size())
            {
                // fill with zeros until reaching desired slot
                _value._values.resize(index + 1);
                _value._updateMask.resize((index + 1 + 31) / 32);
            }

            _value.MarkChanged(index);
            return { _value._values[index] };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of_v<OptionalUpdateFieldBase<value_type>, U>, OptionalUpdateFieldSetter<value_type>>
            ModifyValue()
        {
            return { _value };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of_v<OptionalUpdateFieldBase<value_type>, U>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldStructureTag, value_type>,
            MutableFieldReference<value_type, PublicSet>,
            std::conditional_t<std::is_base_of_v<IsUpdateFieldHolderTag, value_type>,
            MutableNestedFieldReference<value_type, PublicSet>,
            UpdateFieldSetter<value_type, PublicSet>>>>
            ModifyValue(uint32 /*dummy*/)
        {
            if (!_value.has_value())
                _value.ConstructValue();

            return { *(_value._value) };
        }

    private:
        T& _value;
    };

    template<std::size_t Bits>
    class HasChangesMask : public HasChangesMaskTag
    {
        template<typename T>
        friend struct DynamicUpdateFieldSetter;

        template<typename T, bool PublicSet>
        friend struct MutableFieldReferenceWithChangesMask;

        template<typename T, bool PublicSet>
        friend struct MutableFieldReferenceNoChangesMask;

        template<typename T, int32 BlockBit, uint32 Bit>
        friend class UpdateField;

        template<typename T, std::size_t Size, uint32 Bit, int32 FirstElementBit>
        friend class UpdateFieldArray;

        template<typename T, int32 BlockBit, uint32 Bit>
        friend class DynamicUpdateField;

    public:
        using Base = HasChangesMask<Bits>;
        using Mask = UpdateMask<Bits>;

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        MutableFieldReference<T, false> ModifyValue(UpdateField<T, BlockBit, Bit>(Derived::* field))
        {
            MarkChanged(field);
            return { (static_cast<Derived*>(this)->*field)._value };
        }

        template<typename Derived, typename T, std::size_t Size, uint32 Bit, int32 FirstElementBit>
        MutableFieldReference<T, false> ModifyValue(UpdateFieldArray<T, Size, Bit, FirstElementBit>(Derived::* field), uint32 index)
        {
            MarkChanged(field, index);
            return { (static_cast<Derived*>(this)->*field)._values[index] };
        }

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        MutableFieldReference<T, false> ModifyValue(DynamicUpdateField<T, BlockBit, Bit>(Derived::* field))
        {
            MarkChanged(field);
            return { (static_cast<Derived*>(this)->*field)._values };
        }

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        MutableFieldReference<T, false> ModifyValue(DynamicUpdateField<T, BlockBit, Bit>(Derived::* field), uint32 index)
        {
            DynamicUpdateField<T, BlockBit, Bit>& uf = (static_cast<Derived*>(this)->*field);
            if (index >= uf.size())
            {
                // fill with zeros until reaching desired slot
                uf._values.resize(index + 1);
                uf._updateMask.resize((index + 1 + 31) / 32);
            }

            MarkChanged(field);
            (static_cast<Derived*>(this)->*field).MarkChanged(index);
            return { uf._values[index] };
        }

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        MutableFieldReference<T, false> ModifyValue(OptionalUpdateField<T, BlockBit, Bit>(Derived::* field))
        {
            MarkChanged(field);
            return { *((static_cast<Derived*>(this)->*field)._value) };
        }

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        void MarkChanged(UpdateField<T, BlockBit, Bit>(Derived::*))
        {
            static_assert(std::is_base_of_v<Base, Derived>, "Given field argument must belong to the same structure as this HasChangesMask");

            if constexpr  (BlockBit >= 0)
                _changesMask.Set(BlockBit);

            _changesMask.Set(Bit);
        }

        template<typename Derived, typename T, std::size_t Size, uint32 Bit, int32 FirstElementBit>
        void MarkChanged(UpdateFieldArray<T, Size, Bit, FirstElementBit>(Derived::*), uint32 index)
        {
            static_assert(std::is_base_of_v<Base, Derived>, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Set(Bit);
            if constexpr (FirstElementBit >= 0)
            {
                if constexpr (!std::is_base_of_v<IsUpdateFieldHolderTag, T>)
                    _changesMask.Set(FirstElementBit + index);
                else
                    _changesMask.Set(FirstElementBit);
            }
        }

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        void MarkChanged(DynamicUpdateField<T, BlockBit, Bit>(Derived::*), uint32)
        {
            static_assert(std::is_base_of_v<Base, Derived>, "Given field argument must belong to the same structure as this HasChangesMask");

            if constexpr (BlockBit >= 0)
                _changesMask.Set(BlockBit);

            _changesMask.Set(Bit);
        }

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        void MarkChanged(OptionalUpdateField<T, BlockBit, Bit>(Derived::*))
        {
            static_assert(std::is_base_of_v<Base, Derived>, "Given field argument must belong to the same structure as this HasChangesMask");

            if constexpr (BlockBit >= 0)
                _changesMask.Set(BlockBit);

            _changesMask.Set(Bit);
        }

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        void ClearChanged(UpdateField<T, BlockBit, Bit>(Derived::*))
        {
            static_assert(std::is_base_of_v<Base, Derived>, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Reset(Bit);
        }

        template<typename Derived, typename T, std::size_t Size, uint32 Bit, int32 FirstElementBit>
        void ClearChanged(UpdateFieldArray<T, Size, Bit, FirstElementBit>(Derived::*), uint32 index)
        {
            static_assert(std::is_base_of_v<Base, Derived>, "Given field argument must belong to the same structure as this HasChangesMask");

            if constexpr (FirstElementBit >= 0)
            {
                if constexpr (!std::is_base_of_v<IsUpdateFieldHolderTag, T>)
                    _changesMask.Reset(FirstElementBit + index);
                else
                    _changesMask.Reset(FirstElementBit);
            }
        }

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        void ClearChanged(DynamicUpdateField<T, BlockBit, Bit>(Derived::* field), uint32 index)
        {
            _changesMask.Reset(Bit);

            DynamicUpdateField<T, BlockBit, Bit>& uf = (static_cast<Derived*>(this)->*field);
            if (index < uf.size())
                uf.ClearChanged(index);
        }

        template<typename Derived, typename T, int32 BlockBit, uint32 Bit>
        void ClearChanged(OptionalUpdateField<T, BlockBit, Bit>(Derived::*))
        {
            static_assert(std::is_base_of_v<Base, Derived>, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Reset(Bit);
        }

        Mask const& GetChangesMask() const { return _changesMask; }

    protected:
        template<typename T, int32 BlockBit, uint32 Bit>
        static void ClearChangesMask(UpdateField<T, BlockBit, Bit>& field)
        {
            if constexpr (std::is_base_of_v<HasChangesMaskTag, T>)
                field._value.ClearChangesMask();
        }

        template<typename T, std::size_t Size, uint32 Bit, int32 FirstElementBit>
        static void ClearChangesMask(UpdateFieldArray<T, Size, Bit, FirstElementBit>& field)
        {
            if constexpr (std::disjunction_v<std::is_base_of<HasChangesMaskTag, T>, std::is_base_of<IsUpdateFieldHolderTag, T>>)
                for (T& value : field._values)
                    value.ClearChangesMask();
        }

        template<typename T, int32 BlockBit, uint32 Bit>
        static void ClearChangesMask(DynamicUpdateField<T, BlockBit, Bit>& field)
        {
            if constexpr (std::is_base_of_v<HasChangesMaskTag, T>)
                for (T& value : field._values)
                    value.ClearChangesMask();

            field.ClearChangesMask();
        }

        template<typename T, int32 BlockBit, uint32 Bit>
        static void ClearChangesMask(OptionalUpdateField<T, BlockBit, Bit>& field)
        {
            if constexpr (std::is_base_of_v<HasChangesMaskTag, T>)
                if (field.has_value())
                    field._value->ClearChangesMask();
        }

        Mask _changesMask;
    };

    template<typename T>
    class UpdateFieldBase : public IsUpdateFieldHolderTag
    {
        template<typename F, bool PublicSet>
        friend struct MutableFieldReferenceWithChangesMask;

        template<typename F, bool PublicSet>
        friend struct MutableFieldReferenceNoChangesMask;

        template<typename F, bool PublicSet>
        friend struct MutableNestedFieldReference;

        template<std::size_t Bits>
        friend class HasChangesMask;

        friend class UpdateFieldHolder;

    public:
        using value_type = T;

        operator T const& () const
        {
            return _value;
        }
        T const* operator->() const
        {
            return &_value;
        }
        T const& operator*() const
        {
            return _value;
        }

    private:
        T _value = {};
    };

    template<typename T, int32 BlockBit, uint32 Bit>
    class UpdateField : public UpdateFieldBase<T>
    {
    };

    template<typename T>
    class UpdateFieldArrayBaseWithoutSize : public IsUpdateFieldHolderTag
    {
    };

    template<typename T, std::size_t Size>
    class UpdateFieldArrayBase : public UpdateFieldArrayBaseWithoutSize<T>
    {
        template<typename F, bool PublicSet>
        friend struct MutableFieldReferenceWithChangesMask;

        template<typename F, bool PublicSet>
        friend struct MutableFieldReferenceNoChangesMask;

        template<typename F, bool PublicSet>
        friend struct MutableNestedFieldReference;

        template<std::size_t Bits>
        friend class HasChangesMask;

    public:
        using value_type = T;

        T const* begin() const
        {
            return std::begin(_values);
        }

        T const* end() const
        {
            return std::end(_values);
        }

        static constexpr std::size_t size()
        {
            return Size;
        }

        T const& operator[](std::size_t index) const
        {
            return _values[index];
        }

    private:
        T _values[Size] = {};
    };

    // workaround functions for internal compiler errors in msvc 19.33.31629
    template<typename T>
    constexpr std::size_t size()
    {
        return T::size();
    }

    template<typename T>
    constexpr std::size_t size_of_value_type()
    {
        return sizeof(typename T::value_type);
    }

    template<typename T, std::size_t Size, uint32 Bit, int32 FirstElementBit>
    class UpdateFieldArray : public UpdateFieldArrayBase<T, Size>
    {
    };

    void WriteDynamicFieldUpdateMask(std::size_t size, std::vector<uint32> const& updateMask, ByteBuffer& data, int32 bitsForSize = 32);
    void WriteCompleteDynamicFieldUpdateMask(std::size_t size, ByteBuffer& data, int32 bitsForSize = 32);

    template<typename T>
    class DynamicUpdateFieldBase : public IsUpdateFieldHolderTag
    {
        template<typename F, bool PublicSet>
        friend struct MutableFieldReferenceWithChangesMask;

        template<typename F, bool PublicSet>
        friend struct MutableFieldReferenceNoChangesMask;

        template<typename F, bool PublicSet>
        friend struct MutableNestedFieldReference;

        template<std::size_t Bits>
        friend class HasChangesMask;

    public:
        using value_type = T;

        T const* data() const
        {
            return _values.data();
        }

        typename std::vector<T>::const_iterator begin() const
        {
            return _values.begin();
        }

        typename std::vector<T>::const_iterator end() const
        {
            return _values.end();
        }

        bool empty() const
        {
            return _values.empty();
        }

        std::size_t size() const
        {
            return _values.size();
        }

        T const& operator[](std::size_t index) const
        {
            return _values[index];
        }

        int32 FindIndex(T const& value) const
        {
            auto itr = std::find(_values.begin(), _values.end(), value);
            if (itr != _values.end())
                return int32(std::distance(_values.begin(), itr));

            return -1;
        }

        template<typename Pred>
        int32 FindIndexIf(Pred pred) const
        {
            auto itr = std::find_if(_values.begin(), _values.end(), std::ref(pred));
            if (itr != _values.end())
                return int32(std::distance(_values.begin(), itr));

            return -1;
        }

        bool HasChanged(uint32 index) const
        {
            return (_updateMask[UpdateMaskHelpers::GetBlockIndex(index)] & UpdateMaskHelpers::GetBlockFlag(index)) != 0;
        }

        void WriteUpdateMask(ByteBuffer& data, int32 bitsForSize = 32) const
        {
            WriteDynamicFieldUpdateMask(_values.size(), _updateMask, data, bitsForSize);
        }

    private:
        void MarkChanged(std::size_t index)
        {
            _updateMask[UpdateMaskHelpers::GetBlockIndex(index)] |= UpdateMaskHelpers::GetBlockFlag(index);
        }

        void ClearChanged(std::size_t index)
        {
            _updateMask[UpdateMaskHelpers::GetBlockIndex(index)] &= ~UpdateMaskHelpers::GetBlockFlag(index);
        }

        void ClearChangesMask()
        {
            std::memset(_updateMask.data(), 0, _updateMask.size() * sizeof(uint32));
        }

        std::vector<T> _values;
        std::vector<uint32> _updateMask;
    };

    template<typename T, int32 BlockBit, uint32 Bit>
    class DynamicUpdateField : public DynamicUpdateFieldBase<T>
    {
    };

    template<typename T>
    class OptionalUpdateFieldBase : public IsUpdateFieldHolderTag
    {
        template<typename F, bool PublicSet>
        friend struct MutableFieldReferenceWithChangesMask;

        template<typename F, bool PublicSet>
        friend struct MutableFieldReferenceNoChangesMask;

        template<typename F, bool PublicSet>
        friend struct MutableNestedFieldReference;

        template<std::size_t Bits>
        friend class HasChangesMask;

        template<typename F>
        friend struct OptionalUpdateFieldSetter;

        friend class UpdateFieldHolder;

    public:
        using value_type = T;
        using IsLarge = std::integral_constant<bool, sizeof(void*) * 3 < sizeof(T)>;
        using StorageType = std::conditional_t<IsLarge::value, std::unique_ptr<T>, Optional<T>>;

        ~OptionalUpdateFieldBase()
        {
            DestroyValue();
        }

        bool has_value() const
        {
            return !!_value;
        }

        operator bool() const
        {
            return has_value();
        }
        T const* operator->() const
        {
            return &(*_value);
        }
        T const& operator*() const
        {
            return *_value;
        }

    private:
        void ConstructValue()
        {
            if constexpr (IsLarge::value)
                _value = std::make_unique<T>();
            else
                _value.emplace();
        }

        void DestroyValue()
        {
            _value.reset();
        }

        StorageType _value = { };
    };

    template<typename T, int32 BlockBit, uint32 Bit>
    class OptionalUpdateField : public OptionalUpdateFieldBase<T>
    {
    };

    template<typename T>
    struct ViewerDependentValueTag
    {
        using value_type = T;
    };
}

#endif // UpdateField_h__
