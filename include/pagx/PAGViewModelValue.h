/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace pagx {

class ObserverHandle;
class PAGScene;
class PAGViewModel;
class PropertyData;
class DataBindRuntime;

/**
 * ViewModelValueType enumerates the runtime value types of a PAGViewModelValue.
 */
enum class ViewModelValueType { Number, String, Boolean, Color, Image, ViewModel };

/**
 * PAGViewModelValue is the base class for typed ViewModel property values. Each value holds the
 * current data for one property in a runtime PAGViewModel instance. Subclasses add typed getters
 * and setters for Number, String, Boolean, Color, Image, and nested ViewModel properties.
 *
 * Changes are tracked via a simple dirty flag and propagated to registered observers (business-side
 * callbacks) and dependent DataBind objects (engine-side update chain). Setting the same value is
 * a no-op and produces no change notification.
 */
class PAGViewModelValue : public std::enable_shared_from_this<PAGViewModelValue> {
 public:
  using Observer = std::function<void()>;

  virtual ~PAGViewModelValue() = default;

  /**
   * Adds an observer callback that is invoked synchronously when the value changes.
   * Returns an ObserverHandle; the observer is automatically removed when the handle is destroyed
   * or detached.
   */
  ObserverHandle addObserver(Observer observer);

  /**
   * Returns the property name as declared in the ViewModel schema.
   */
  const std::string& name() const {
    return propertyName;
  }

  /**
   * Returns true if the value has changed since the last frame-level advanced() call.
   */
  bool hasChanged() const {
    return dirty;
  }

  /**
   * Returns the runtime value type of this property.
   */
  ViewModelValueType valueType() const {
    return type;
  }

  /**
   * Returns the PropertyData reflection metadata for this property, or nullptr if the schema
   * did not define customData for this property. Lazy-created on first access.
   */
  PropertyData* propertyData() const;

  void setScene(const std::shared_ptr<PAGScene>& scn) {
    scene = scn;
  }

  /**
   * Registers a DataBindRuntime as a dependent of this value. When the value changes, the
   * runtime's markDirty() is called for the associated DataBind node.
   */
  void addDependent(DataBindRuntime* runtime);

  /**
   * Removes a previously registered dependent.
   */
  void removeDependent(DataBindRuntime* runtime);

 protected:
  PAGViewModelValue() = default;

  /**
   * Notifies all registered observers that this value has changed. Called automatically by
   * subclasses after updating their stored value, unless SuppressDelegation is active.
   */
  void notifyObservers();

  /**
   * Resets the dirty flag. Called at the end of each frame after DataBinds have been applied.
   */
  void advanced();

  std::string propertyName = {};
  bool dirty = false;
  ViewModelValueType type = ViewModelValueType::Number;
  std::weak_ptr<PAGScene> scene = {};
  bool notifying = false;

  /**
   * Notifies all registered dependent DataBindRuntime instances that this value changed.
   */
  void notifyDependents();

 private:
  struct ObserverEntry {
    int id = 0;
    Observer callback;
  };
  std::vector<ObserverEntry> observers = {};
  int nextObserverId = 1;
  mutable PropertyData* cachedPropertyData = nullptr;
  std::vector<DataBindRuntime*> dependents = {};

  /**
   * Removes the observer with the given id. Called by ObserverHandle::detach().
   */
  void removeObserver(int id);

  friend class ObserverHandle;
  friend class PAGViewModel;
  friend class PAGScene;
  friend class SuppressDelegation;
  friend class DataBindRuntime;
};

}  // namespace pagx
