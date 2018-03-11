#ifndef TACO_STORAGE_TYPED_VALUE_H
#define TACO_STORAGE_TYPED_VALUE_H

#include <taco/type.h>

namespace taco {
namespace storage {

class TypedValue;
class TypedRef;

// Holds a dynamically typed value
class Typed {
public:
  const DataType& getType() const;
  size_t getAsIndex(const DataTypeUnion mem) const;

  void set(DataTypeUnion& mem, DataTypeUnion value, DataType valueType);
  void set(DataTypeUnion& mem, DataTypeUnion value);

  void add(DataTypeUnion& result, const DataTypeUnion a, const DataTypeUnion b) const;
  void multiply(DataTypeUnion& result, const DataTypeUnion a, const DataTypeUnion b) const;

  TypedValue operator*(const Typed& other) const;
protected:
  DataType dType;
};

// Allocates a union to hold a dynamically typed value
class TypedValue: public Typed {
public:
  TypedValue();
  TypedValue(DataType type);
  TypedValue(TypedRef ref);

  template<typename T>
  TypedValue(DataType t, T constant) {
    dType = t;
    set(constant);
  }

  template<typename T>
  TypedValue(const T& constant) {
    dType = type<T>();
    set(constant);
  }

  template<typename T>
  TypedValue(DataType t, T *ptr) {
    dType = t;
    switch (dType.getKind()) {
      case DataType::Bool: set(*((bool*) ptr)); break;
      case DataType::UInt8: set(*((uint8_t*) ptr)); break;
      case DataType::UInt16: set(*((uint16_t*) ptr)); break;
      case DataType::UInt32: set(*((uint32_t*) ptr)); break;
      case DataType::UInt64: set(*((uint64_t*) ptr)); break;
      case DataType::UInt128: set(*((unsigned long long*) ptr)); break;
      case DataType::Int8: set(*((int8_t*) ptr)); break;
      case DataType::Int16: set(*((int16_t*) ptr)); break;
      case DataType::Int32: set(*((int32_t*) ptr)); break;
      case DataType::Int64: set(*((int64_t*) ptr)); break;
      case DataType::Int128: set(*((long long*) ptr)); break;
      case DataType::Float32: set(*((float*) ptr)); break;
      case DataType::Float64: set(*((double*) ptr)); break;
      case DataType::Complex64: taco_ierror; break;
      case DataType::Complex128: taco_ierror; break;
      case DataType::Undefined: taco_ierror; break;
    }
  }

  DataTypeUnion& get();

  DataTypeUnion get() const;

  const DataType& getType() const;

  size_t getAsIndex() const;

  void set(TypedValue value);

  void set(TypedRef value);

  //Casts constant to type
  template<typename T>
  void set(T constant) {
    Typed::set(val, *((DataTypeUnion *) &constant), type<T>());
  }

  TypedValue operator++();

  TypedValue operator++(int junk);

  TypedValue operator+(const TypedValue other) const;

  TypedValue operator*(const TypedValue other) const;

private:
  DataTypeUnion val;
};


// dereferences to typedref
class TypedPtr {
public:
  TypedPtr() : ptr(nullptr) {}

  TypedPtr (DataType type, void *ptr) : type(type), ptr(ptr) {
  }

  void* get();

  TypedRef operator*() const;
  
  bool operator> (const TypedPtr &other) const;
  bool operator<= (const TypedPtr &other) const;

  bool operator< (const TypedPtr &other) const;
  bool operator>= (const TypedPtr &other) const;

  bool operator== (const TypedPtr &other) const;
  bool operator!= (const TypedPtr &other) const;

  TypedPtr operator+(int value) const;
  TypedPtr operator++();
  TypedPtr operator++(int junk);

private:
  DataType type;
  void *ptr;
};

class TypedRef: public Typed{
public:
  template<typename T>
  TypedRef(DataType t, T *ptr) : ptr(reinterpret_cast<DataTypeUnion *>(ptr)) {
    dType = t;
  }

  DataTypeUnion& get();

  DataTypeUnion get() const;

  TypedPtr operator&() const;

  void set(TypedValue value);

  TypedRef operator=(TypedValue other);

  TypedRef operator=(TypedRef other);

  TypedRef operator++();

  TypedRef operator++(int junk);

  TypedValue operator+(const TypedValue other) const;

  TypedValue operator*(const TypedValue other) const;

  const DataType& getType() const;

  size_t getAsIndex() const;


private:
  DataTypeUnion *ptr;
};


bool operator>(const TypedValue& a, const TypedValue &other);

bool operator==(const TypedValue& a, const TypedValue &other);

bool operator>=(const TypedValue& a,const TypedValue &other);

bool operator<(const TypedValue& a, const TypedValue &other);

bool operator<=(const TypedValue& a, const TypedValue &other);

bool operator!=(const TypedValue& a, const TypedValue &other);

}}
#endif

