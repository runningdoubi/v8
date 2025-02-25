// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/logging/counters.h"
#include "src/objects/keys.h"
#include "src/objects/lookup.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-descriptor.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 26.1 The Reflect Object

// ES6 section 26.1.3 Reflect.defineProperty
BUILTIN(ReflectDefineProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<Object> target = args.at(1);
  Handle<Object> key = args.at(2);
  Handle<Object> attributes = args.at(3);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.defineProperty")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  PropertyDescriptor desc;
  if (!PropertyDescriptor::ToPropertyDescriptor(isolate, attributes, &desc)) {
    return ReadOnlyRoots(isolate).exception();
  }

  Maybe<bool> result = JSReceiver::DefineOwnProperty(
      isolate, Handle<JSReceiver>::cast(target), name, &desc, Just(kDontThrow));
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

// ES6 section 26.1.4 Reflect.deleteProperty
BUILTIN(ReflectDeleteProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> target = args.at(1);
  Handle<Object> key = args.at(2);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.deleteProperty")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  Maybe<bool> result = JSReceiver::DeletePropertyOrElement(
      Handle<JSReceiver>::cast(target), name, LanguageMode::kSloppy);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

// ES6 section 26.1.6 Reflect.get
BUILTIN(ReflectGet) {
  HandleScope scope(isolate);
  Handle<Object> target = args.atOrUndefined(isolate, 1);
  Handle<Object> key = args.atOrUndefined(isolate, 2);
  Handle<Object> receiver = args.length() > 3 ? args.at(3) : target;

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.get")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  RETURN_RESULT_OR_FAILURE(
      isolate, Object::GetPropertyOrElement(receiver, name,
                                            Handle<JSReceiver>::cast(target)));
}

// ES6 section 26.1.7 Reflect.getOwnPropertyDescriptor
BUILTIN(ReflectGetOwnPropertyDescriptor) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> target = args.at(1);
  Handle<Object> key = args.at(2);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.getOwnPropertyDescriptor")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  PropertyDescriptor desc;
  Maybe<bool> found = JSReceiver::GetOwnPropertyDescriptor(
      isolate, Handle<JSReceiver>::cast(target), name, &desc);
  MAYBE_RETURN(found, ReadOnlyRoots(isolate).exception());
  if (!found.FromJust()) return ReadOnlyRoots(isolate).undefined_value();
  return *desc.ToObject(isolate);
}

// ES6 section 26.1.8 Reflect.getPrototypeOf
BUILTIN(ReflectGetPrototypeOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> target = args.at(1);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.getPrototypeOf")));
  }
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(target);
  RETURN_RESULT_OR_FAILURE(isolate,
                           JSReceiver::GetPrototype(isolate, receiver));
}

// ES6 section 26.1.11 Reflect.ownKeys
BUILTIN(ReflectOwnKeys) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> target = args.at(1);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.ownKeys")));
  }

  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys,
      KeyAccumulator::GetKeys(Handle<JSReceiver>::cast(target),
                              KeyCollectionMode::kOwnOnly, ALL_PROPERTIES,
                              GetKeysConversion::kConvertToString));
  return *isolate->factory()->NewJSArrayWithElements(keys);
}

// ES6 section 26.1.13 Reflect.set
BUILTIN(ReflectSet) {
  HandleScope scope(isolate);
  Handle<Object> target = args.atOrUndefined(isolate, 1);
  Handle<Object> key = args.atOrUndefined(isolate, 2);
  Handle<Object> value = args.atOrUndefined(isolate, 3);
  Handle<Object> receiver = args.length() > 4 ? args.at(4) : target;

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.set")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, receiver, name, Handle<JSReceiver>::cast(target));
  Maybe<bool> result = Object::SetSuperProperty(
      &it, value, StoreOrigin::kMaybeKeyed, Just(ShouldThrow::kDontThrow));
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

// ES6 section 26.1.14 Reflect.setPrototypeOf
BUILTIN(ReflectSetPrototypeOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> target = args.at(1);
  Handle<Object> proto = args.at(2);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.setPrototypeOf")));
  }

  if (!proto->IsJSReceiver() && !proto->IsNull(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kProtoObjectOrNull, proto));
  }

  Maybe<bool> result = JSReceiver::SetPrototype(
      Handle<JSReceiver>::cast(target), proto, true, kDontThrow);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

}  // namespace internal
}  // namespace v8
