// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ELEMENTS_H_
#define V8_ELEMENTS_H_

#include "src/elements-kind.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// Abstract base class for handles that can operate on objects with differing
// ElementsKinds.
class ElementsAccessor {
 public:
  explicit ElementsAccessor(const char* name) : name_(name) { }
  virtual ~ElementsAccessor() { }

  const char* name() const { return name_; }

  // Checks the elements of an object for consistency, asserting when a problem
  // is found.
  virtual void Validate(Handle<JSObject> obj) = 0;

  // Returns true if a holder contains an element with the specified key
  // without iterating up the prototype chain.  The caller can optionally pass
  // in the backing store to use for the check, which must be compatible with
  // the ElementsKind of the ElementsAccessor. If backing_store is NULL, the
  // holder->elements() is used as the backing store.
  virtual bool HasElement(
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  inline bool HasElement(
      Handle<JSObject> holder,
      uint32_t key) {
    return HasElement(holder, key, handle(holder->elements()));
  }

  // Returns the element with the specified key or undefined if there is no such
  // element. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  virtual Handle<Object> Get(Handle<JSObject> holder, uint32_t key,
                             Handle<FixedArrayBase> backing_store) = 0;

  inline Handle<Object> Get(Handle<JSObject> holder, uint32_t key) {
    return Get(holder, key, handle(holder->elements()));
  }

  // Returns an element's accessors, or NULL if the element does not exist or
  // is plain. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  virtual MaybeHandle<AccessorPair> GetAccessorPair(
      Handle<JSObject> holder, uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  inline MaybeHandle<AccessorPair> GetAccessorPair(Handle<JSObject> holder,
                                                   uint32_t key) {
    return GetAccessorPair(holder, key, handle(holder->elements()));
  }

  // Modifies the length data property as specified for JSArrays and resizes the
  // underlying backing store accordingly. The method honors the semantics of
  // changing array sizes as defined in EcmaScript 5.1 15.4.5.2, i.e. array that
  // have non-deletable elements can only be shrunk to the size of highest
  // element that is non-deletable.
  virtual void SetLength(Handle<JSArray> holder, uint32_t new_length) = 0;

  // Deletes an element in an object.
  virtual void Delete(Handle<JSObject> holder, uint32_t key,
                      LanguageMode language_mode) = 0;

  // If kCopyToEnd is specified as the copy_size to CopyElements, it copies all
  // of elements from source after source_start to the destination array.
  static const int kCopyToEnd = -1;
  // If kCopyToEndAndInitializeToHole is specified as the copy_size to
  // CopyElements, it copies all of elements from source after source_start to
  // destination array, padding any remaining uninitialized elements in the
  // destination array with the hole.
  static const int kCopyToEndAndInitializeToHole = -2;

  // Copy elements from one backing store to another. Typically, callers specify
  // the source JSObject or JSArray in source_holder. If the holder's backing
  // store is available, it can be passed in source and source_holder is
  // ignored.
  virtual void CopyElements(
      Handle<FixedArrayBase> source,
      uint32_t source_start,
      ElementsKind source_kind,
      Handle<FixedArrayBase> destination,
      uint32_t destination_start,
      int copy_size) = 0;

  // NOTE: this method violates the handlified function signature convention:
  // raw pointer parameter |source_holder| in the function that allocates.
  // This is done intentionally to avoid ArrayConcat() builtin performance
  // degradation.
  virtual void CopyElements(
      JSObject* source_holder,
      uint32_t source_start,
      ElementsKind source_kind,
      Handle<FixedArrayBase> destination,
      uint32_t destination_start,
      int copy_size) = 0;

  inline void CopyElements(
      Handle<JSObject> from_holder,
      Handle<FixedArrayBase> to,
      ElementsKind from_kind) {
    CopyElements(
      *from_holder, 0, from_kind, to, 0, kCopyToEndAndInitializeToHole);
  }

  virtual void GrowCapacityAndConvert(Handle<JSObject> object,
                                      uint32_t capacity) = 0;

  virtual Handle<FixedArray> AddElementsToFixedArray(
      Handle<JSObject> receiver, Handle<FixedArray> to,
      FixedArray::KeyFilter filter) = 0;

  // Returns a shared ElementsAccessor for the specified ElementsKind.
  static ElementsAccessor* ForKind(ElementsKind elements_kind) {
    DCHECK(static_cast<int>(elements_kind) < kElementsKindCount);
    return elements_accessors_[elements_kind];
  }

  static ElementsAccessor* ForArray(Handle<FixedArrayBase> array);

  static void InitializeOncePerProcess();
  static void TearDown();

  virtual void Set(FixedArrayBase* backing_store, uint32_t key,
                   Object* value) = 0;
  virtual void Reconfigure(Handle<JSObject> object,
                           Handle<FixedArrayBase> backing_store, uint32_t index,
                           Handle<Object> value,
                           PropertyAttributes attributes) = 0;
  virtual void Add(Handle<JSObject> object, uint32_t index,
                   Handle<Object> value, PropertyAttributes attributes,
                   uint32_t new_capacity) = 0;

 protected:
  friend class SloppyArgumentsElementsAccessor;
  friend class LookupIterator;

  static ElementsAccessor* ForArray(FixedArrayBase* array);

  virtual uint32_t GetCapacity(JSObject* holder,
                               FixedArrayBase* backing_store) = 0;

  // Element handlers distinguish between indexes and keys when they manipulate
  // elements.  Indexes refer to elements in terms of their location in the
  // underlying storage's backing store representation, and are between 0 and
  // GetCapacity.  Keys refer to elements in terms of the value that would be
  // specified in JavaScript to access the element. In most implementations,
  // keys are equivalent to indexes, and GetKeyForIndex returns the same value
  // it is passed. In the NumberDictionary ElementsAccessor, GetKeyForIndex maps
  // the index to a key using the KeyAt method on the NumberDictionary.
  virtual uint32_t GetKeyForIndex(FixedArrayBase* backing_store,
                                  uint32_t index) = 0;
  virtual uint32_t GetIndexForKey(JSObject* holder,
                                  FixedArrayBase* backing_store,
                                  uint32_t key) = 0;
  virtual PropertyDetails GetDetails(FixedArrayBase* backing_store,
                                     uint32_t index) = 0;
  virtual bool HasIndex(FixedArrayBase* backing_store, uint32_t key) = 0;

 private:
  static ElementsAccessor** elements_accessors_;
  const char* name_;

  DISALLOW_COPY_AND_ASSIGN(ElementsAccessor);
};

void CheckArrayAbuse(Handle<JSObject> obj, const char* op, uint32_t key,
                     bool allow_appending = false);

MUST_USE_RESULT MaybeHandle<Object> ArrayConstructInitializeElements(
    Handle<JSArray> array,
    Arguments* args);

} }  // namespace v8::internal

#endif  // V8_ELEMENTS_H_
