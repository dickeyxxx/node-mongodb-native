#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "long.h"

// BSON MAX VALUES
const int32_t BSON_INT32_MAX = 2147483648;
const int32_t BSON_INT32_MIN = -2147483648;
const int64_t BSON_INT32_ = pow(2, 32);

const double LN2 = 0.6931471805599453;

// Max Values
const int64_t BSON_INT64_MAX = 9223372036854775807;
const int64_t BSON_INT64_MIN = -(9223372036854775807);

// Constant objects used in calculations
Long* MIN_VALUE = Long::fromBits(0, 0x80000000 | 0);
Long* MAX_VALUE = Long::fromBits(0xFFFFFFFF | 0, 0x7FFFFFFF | 0);
Long* ZERO = Long::fromInt(0);
Long* ONE = Long::fromInt(1);
Long* NEG_ONE = Long::fromInt(-1);

#define max(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a > _b ? _a : _b; })

static Handle<Value> VException(const char *msg) {
    HandleScope scope;
    return ThrowException(Exception::Error(String::New(msg)));
  };

Persistent<FunctionTemplate> Long::constructor_template;

Long::Long(int32_t low_bits, int32_t high_bits) : ObjectWrap() {
  this->low_bits = low_bits;
  this->high_bits = high_bits;
}

Long::~Long() {}

Handle<Value> Long::New(const Arguments &args) {
  HandleScope scope;

  // Ensure that we have an parameter
  if(args.Length() == 1 && args[0]->IsNumber()) {
    // Unpack the value
    double value = args[0]->NumberValue();
    // Create an instance of long
    Long *l = Long::fromNumber(value);
    // Wrap it in the object wrap
    l->Wrap(args.This());
    // Return the context
    return args.This();
  } else if(args.Length() == 2 && args[0]->IsNumber() && args[1]->IsNumber()) {
    // Unpack the value
    int32_t low_bits = args[0]->Int32Value();
    int32_t high_bits = args[1]->Int32Value();
    // Create an instance of long
    Long *l = new Long(low_bits, high_bits);
    // Wrap it in the object wrap
    l->Wrap(args.This());
    // Return the context
    return args.This();    
  } else {
    return VException("Argument passed in must be either a 64 bit number or two 32 bit numbers.");
  }
}

static Persistent<String> low_bits_symbol;
static Persistent<String> high_bits_symbol;

void Long::Initialize(Handle<Object> target) {
  // Grab the scope of the call from Node
  HandleScope scope;
  // Define a new function template
  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Long"));
  
  // Propertry symbols
  low_bits_symbol = NODE_PSYMBOL("low_");
  high_bits_symbol = NODE_PSYMBOL("high_");  
  
  // Instance methods
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "toString", ToString);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "isZero", IsZero);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "getLowBits", GetLowBits);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "getHighBits", GetHighBits);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "inspect", Inspect);  

  // Getters for correct serialization of the object  
  constructor_template->InstanceTemplate()->SetAccessor(low_bits_symbol, LowGetter, LowSetter);
  constructor_template->InstanceTemplate()->SetAccessor(high_bits_symbol, HighGetter, HighSetter);
  
  // Class methods
  NODE_SET_METHOD(constructor_template->GetFunction(), "fromNumber", FromNumber);
  
  // Add class to scope
  target->Set(String::NewSymbol("Long"), constructor_template->GetFunction());
}

Handle<Value> Long::LowGetter(Local<String> property, const AccessorInfo& info) {
  HandleScope scope;
  
  // Unpack object reference
  Local<Object> self = info.Holder();
  // Fetch external reference (reference to Long object)
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  // Get pointer to the object
  void *ptr = wrap->Value();
  // Extract value doing a cast of the pointer to Long and accessing low_bits
  int32_t low_bits = static_cast<Long *>(ptr)->low_bits;
  Local<Integer> integer = Integer::New(low_bits);
  return scope.Close(integer);
}

void Long::LowSetter(Local<String> property, Local<Value> value, const AccessorInfo& info) {
  if(value->IsNumber()) {
    // Unpack object reference
    Local<Object> self = info.Holder();
    // Fetch external reference (reference to Long object)
    Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
    // Get pointer to the object
    void *ptr = wrap->Value();
    // Set the low bits
    static_cast<Long *>(ptr)->low_bits = value->Int32Value();    
  }
}

Handle<Value> Long::HighGetter(Local<String> property, const AccessorInfo& info) {
  HandleScope scope;
  
  // Unpack object reference
  Local<Object> self = info.Holder();
  // Fetch external reference (reference to Long object)
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  // Get pointer to the object
  void *ptr = wrap->Value();
  // Extract value doing a cast of the pointer to Long and accessing low_bits
  int32_t high_bits = static_cast<Long *>(ptr)->high_bits;
  Local<Integer> integer = Integer::New(high_bits);
  return scope.Close(integer);
}

void Long::HighSetter(Local<String> property, Local<Value> value, const AccessorInfo& info) {
  if(value->IsNumber()) {
    // Unpack object reference
    Local<Object> self = info.Holder();
    // Fetch external reference (reference to Long object)
    Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
    // Get pointer to the object
    void *ptr = wrap->Value();
    // Set the low bits
    static_cast<Long *>(ptr)->high_bits = value->Int32Value();  
  }
}

Handle<Value> Long::Inspect(const Arguments &args) {
  HandleScope scope;
  
  // Let's unpack the Long instance that contains the number in low_bits and high_bits form
  Long *l = ObjectWrap::Unwrap<Long>(args.This());
  // Let's create the string from the Long number
  char *result = l->toString(10);
  // Package the result in a V8 String object and return
  return String::New(result);
}

Handle<Value> Long::GetLowBits(const Arguments &args) {
  HandleScope scope;

  // Let's unpack the Long instance that contains the number in low_bits and high_bits form
  Long *l = ObjectWrap::Unwrap<Long>(args.This());
  // Let's fetch the low bits
  int32_t low_bits = l->low_bits;
  // Package the result in a V8 Integer object and return
  return Integer::New(low_bits);  
}

Handle<Value> Long::GetHighBits(const Arguments &args) {
  HandleScope scope;

  // Let's unpack the Long instance that contains the number in low_bits and high_bits form
  Long *l = ObjectWrap::Unwrap<Long>(args.This());
  // Let's fetch the low bits
  int32_t high_bits = l->high_bits;
  // Package the result in a V8 Integer object and return
  return Integer::New(high_bits);    
}

bool Long::isZero() {
  int32_t low_bits = this->low_bits;
  int32_t high_bits = this->high_bits;
  return low_bits == 0 && high_bits == 0;
}

bool Long::isNegative() {
  int32_t low_bits = this->low_bits;
  int32_t high_bits = this->high_bits;
  return high_bits < 0;
}

bool Long::equals(Long *l) {
  int32_t low_bits = this->low_bits;
  int32_t high_bits = this->high_bits;  
  return (high_bits == l->high_bits) && (low_bits == l->low_bits);
}

Handle<Value> Long::IsZero(const Arguments &args) {
  HandleScope scope;      
    
  // Let's unpack the Long instance that contains the number in low_bits and high_bits form
  Long *l = ObjectWrap::Unwrap<Long>(args.This());
  return Boolean::New(l->isZero());
}

int32_t Long::toInt() {
  return this->low_bits;
}

char *Long::toString(int32_t opt_radix) {
  // Set the radix
  int32_t radix = opt_radix;
  // Check if we have a zero value
  if(this->isZero()) {
    // Allocate a string to return
    char *result = (char *)malloc(1 * sizeof(char) + 1);
    // Set the string to the character 0
    *(result) = '0';
    // Terminate the C String
    *(result + 1) = '\0';
    return result;
  }
  
  // If the long is negative we need to perform som arithmetics
  if(this->isNegative()) {
    // Min value object
    Long *minLong = new Long(0, 0x80000000 | 0);
    
    if(this->equals(minLong)) {
      // We need to change the exports.Long value before it can be negated, so we remove
      // the bottom-most digit in this base and then recurse to do the rest.
      Long *radix_long = Long::fromNumber(radix);
      Long *div = this->div(radix_long);
      Long *rem = div->multiply(radix_long)->subtract(this);
      // Fetch div result      
      char *div_result = div->toString(radix);
      // Unpack the rem result and convert int to string
      char *int_buf = (char *)malloc(50 * sizeof(char) + 1);
      *(int_buf) = '\0';
      uint32_t rem_int = rem->toInt();
      sprintf(int_buf, "%d", rem_int);
      // Final bufferr
      char *final_buffer = (char *)malloc(50 * sizeof(char) + 1);
      *(final_buffer) = '\0';
      strncat(final_buffer, div_result, strlen(div_result));
      strncat(final_buffer + strlen(div_result), int_buf, strlen(div_result));
      // Release some memory
      free(int_buf);
      return final_buffer;
    } else {
      char *buf = (char *)malloc(50 * sizeof(char) + 1);
      *(buf) = '\0';
      char *result = this->negate()->toString(radix);      
      strncat(buf, "-", 1);
      strncat(buf + 1, result, strlen(result));
      return buf;
    }  
  }
  
  // Do several (6) digits each time through the loop, so as to
  // minimize the calls to the very expensive emulated div.
  Long *radix_to_power = Long::fromInt(pow(radix, 6));
  Long *rem = this;
  char *result = (char *)malloc(1024 * sizeof(char) + 1);
  // Ensure the allocated space is null terminated to ensure a proper CString
  *(result) = '\0';
  
  while(true) {
    Long *rem_div = rem->div(radix_to_power);
    int32_t interval = rem->subtract(rem_div->multiply(radix_to_power))->toInt();
    // Convert interval into string
    char digits[50];    
    sprintf(digits, "%d", interval);
    
    rem = rem_div;
    if(rem->isZero()) {
      // Join digits and result to create final result
      int total_length = strlen(digits) + strlen(result);      
      char *new_result = (char *)malloc(total_length * sizeof(char) + 1);
      *(new_result) = '\0';
      strncat(new_result, digits, strlen(digits));
      strncat(new_result + strlen(digits), result, strlen(result));
      // Free the existing structure
      free(result);
      return new_result;
    } else {
      // Allocate some new space for the number
      char *new_result = (char *)malloc(1024 * sizeof(char) + 1);
      *(new_result) = '\0';
      int digits_length = (int)strlen(digits);
      int index = 0;
      // Pad with zeros
      while(digits_length < 6) {
        strncat(new_result + index, "0", 1);
        digits_length = digits_length + 1;
        index = index + 1;
      }
  
      strncat(new_result + index, digits, strlen(digits));
      strncat(new_result + strlen(digits) + index, result, strlen(result));
      
      free(result);
      result = new_result;
    }
  }  
}

Handle<Value> Long::ToString(const Arguments &args) {
  HandleScope scope;

  // Let's unpack the Long instance that contains the number in low_bits and high_bits form
  Long *l = ObjectWrap::Unwrap<Long>(args.This());
  // Let's create the string from the Long number
  char *result = l->toString(10);
  // Package the result in a V8 String object and return
  return String::New(result);
}

Long *Long::shiftRight(int32_t number_bits) {
  number_bits &= 63;
  if(number_bits == 0) {
    return this;
  } else {
    int32_t high_bits = this->high_bits;
    if(number_bits < 32) {
      int32_t low_bits = this->low_bits;
      return Long::fromBits((low_bits >> number_bits) | (high_bits << (32 - number_bits)), high_bits >> number_bits);
    } else {
      return Long::fromBits(high_bits >> (number_bits - 32), high_bits >= 0 ? 0 : -1);
    }
  }
}

Long *Long::shiftLeft(int32_t number_bits) {
  number_bits &= 63;
  if(number_bits == 0) {
    return this;
  } else {
    int32_t low_bits = this->low_bits;
    if(number_bits < 32) {
      int32_t high_bits = this->high_bits;
      return Long::fromBits(low_bits << number_bits, (high_bits << number_bits) | (low_bits >> (32 - number_bits)));
    } else {
      return Long::fromBits(0, low_bits << (number_bits - 32));
    }
  }  
}

Long *Long::div(Long *other) {
  // If we are about to do a divide by zero throw an exception
  if(other->isZero()) {
    throw "division by zero";
  } else if(this->isZero()) {
    return new Long(0, 0);
  }
    
  if(this->equals(MIN_VALUE)) {    
    if(other->equals(ONE) || other->equals(NEG_ONE)) {
      return Long::fromBits(0, 0x80000000 | 0);
    } else if(other->equals(MIN_VALUE)) {
      return Long::fromNumber(1);
    } else {
      Long *half_this = this->shiftRight(1);
      Long *approx = half_this->div(other)->shiftLeft(1);
      if(approx->equals(ZERO)) {
        return other->isNegative() ? Long::fromNumber(0) : Long::fromNumber(-1);
      } else {
        Long *rem = this->subtract(other->multiply(approx));
        Long *result = approx->add(rem->div(other));
        return result;
      }
    }    
  } else if(other->equals(MIN_VALUE)) {
    return new Long(0, 0);
  }
  
  // If the value is negative
  if(this->isNegative()) {
    if(other->isNegative()) {
      return this->negate()->div(other->negate());
    } else {
      return this->negate()->div(other)->negate();
    }    
  } else if(other->isNegative()) {
    return this->div(other->negate())->negate();
  }  
  
  int64_t this_number = this->toNumber();
  int64_t other_number = other->toNumber();
  int64_t result = this_number / other_number;
  // Split into the 32 bit valu
  int32_t low32, high32;
  high32 = (uint64_t)result >> 32;
  low32 = (int32_t)result;
  return Long::fromBits(low32, high32);
}

Long *Long::multiply(Long *other) {
  if(this->isZero() || other->isZero()) {
    return new Long(0, 0);    
  }
  
  int64_t this_number = this->toNumber();
  int64_t other_number = other->toNumber();
  int64_t result = this_number * other_number;
  
  // Split into the 32 bit valu
  int32_t low32, high32;
  high32 = (uint64_t)result >> 32;
  low32 = (int32_t)result;
  return Long::fromBits(low32, high32);
}

bool Long::isOdd() {
  return (this->low_bits & 1) == 1;
}

int64_t Long::toNumber() {
  return (int64_t)(this->high_bits * BSON_INT32_ + this->getLowBitsUnsigned());
}

int64_t Long::getLowBitsUnsigned() {
  return (this->low_bits >= 0) ? this->low_bits : BSON_INT32_ + this->low_bits;
}

int64_t Long::compare(Long *other) {
  if(this->equals(other)) {
    return 0;
  }
  
  bool this_neg = this->isNegative();
  bool other_neg = other->isNegative();
  if(this_neg && !other_neg) {
    return -1;
  }
  if(!this_neg && other_neg) {
    return 1;
  }
  
  // At this point, the signs are the same, so subtraction will not overflow
  if(this->subtract(other)->isNegative()) {
    return -1;
  } else {
    return 1;
  }
}

Long *Long::negate() {
  if(this->equals(MIN_VALUE)) {
    return MIN_VALUE;
  } else {
    return this->not_()->add(ONE);
  }
}

Long *Long::not_() {
  return new Long(~this->low_bits, ~this->high_bits);
}

Long *Long::add(Long *other) {
  int64_t this_number = this->toNumber();
  int64_t other_number = other->toNumber();
  int64_t result = this_number + other_number;  
  // Split into the 32 bit valu
  int32_t low32, high32;
  high32 = (uint64_t)result >> 32;
  low32 = (int32_t)result;
  return Long::fromBits(low32, high32);
}

Long *Long::subtract(Long *other) {
  int64_t this_number = this->toNumber();
  int64_t other_number = other->toNumber();
  int64_t result = this_number - other_number;
  // Split into the 32 bit valu
  int32_t low32, high32;
  high32 = (uint64_t)result >> 32;
  low32 = (int32_t)result;
  return Long::fromBits(low32, high32);
}

bool Long::greaterThan(Long *other) {
  return this->compare(other) > 0;  
}

bool Long::greaterThanOrEqual(Long *other) {
  return this->compare(other) >= 0;
}

Long *Long::fromInt(int64_t value) {
  return new Long((value | 0), (value < 0 ? -1 : 0));
}

Long *Long::fromBits(int32_t low_bits, int32_t high_bits) {
  return new Long(low_bits, high_bits);
}

Long *Long::fromNumber(double value) {
  // Ensure we have a valid ranged number
  if(std::isinf(value) || std::isnan(value)) {
    return Long::fromBits(0, 0);
  } else if(value <= BSON_INT64_MIN) {
    return Long::fromBits(0, 0x80000000 | 0);
  } else if(value >= BSON_INT64_MAX) {
    return Long::fromBits(0xFFFFFFFF | 0, 0x7FFFFFFF | 0);
  } else if(value < 0) {
    return Long::fromNumber(-value)->negate();
  } else {
    int64_t int_value = (int64_t)value;
    return Long::fromBits((int_value % BSON_INT32_) | 0, (int_value / BSON_INT32_) | 0);
  }  
}

Handle<Value> Long::FromNumber(const Arguments &args) {
  HandleScope scope;
  
  // Ensure that we have an parameter
  if(args.Length() != 1) return VException("One argument required - number.");
  if(!args[0]->IsNumber()) return VException("Arguments passed in must be numbers.");  
  // Unpack the variable as a 64 bit integer
  int64_t value = args[0]->IntegerValue();
  double double_value = args[0]->NumberValue();
  // Ensure we have a valid ranged number
  if(std::isinf(double_value) || std::isnan(double_value)) {
    Local<Value> argv[] = {Integer::New(0), Integer::New(0)};
    Local<Object> long_obj = constructor_template->GetFunction()->NewInstance(2, argv);
    return scope.Close(long_obj);
  } else if(double_value <= BSON_INT64_MIN) {
    Local<Value> argv[] = {Integer::New(0), Integer::New(0x80000000 | 0)};
    Local<Object> long_obj = constructor_template->GetFunction()->NewInstance(2, argv);    
    return scope.Close(long_obj);    
  } else if(double_value >= BSON_INT64_MAX) {
    Local<Value> argv[] = {Integer::New(0xFFFFFFFF | 0), Integer::New(0x7FFFFFFF | 0)};
    Local<Object> long_obj = constructor_template->GetFunction()->NewInstance(2, argv);    
    return scope.Close(long_obj);        
  } else if(double_value < 0) {
    Local<Value> argv[] = {Number::New(double_value)};
    Local<Object> long_obj = constructor_template->GetFunction()->NewInstance(1, argv);    
    return scope.Close(long_obj);    
  } else {
    Local<Value> argv[] = {Integer::New((value % BSON_INT32_) | 0), Integer::New((value / BSON_INT32_) | 0)};
    Local<Object> long_obj = constructor_template->GetFunction()->NewInstance(2, argv);    
    return scope.Close(long_obj);    
  }
}
