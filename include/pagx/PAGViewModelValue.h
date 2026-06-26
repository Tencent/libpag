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

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "pagx/nodes/ViewModelProperty.h"

namespace pagx {

class ObserverHandle;
class PAGScene;
class PAGViewModel;
class PropertyData;
class DataBindRuntime;
class DataConverter;

/**
 * PAGViewModelValue is the base class for typed ViewModel property values. Each value holds the
 * current data for one property in a runtime PAGViewModel instance. Subclasses add typed getters
 * and setters for Number, String, Boolean, Color, Image, and nested ViewModel properties.
 *
 * Changes are tracked via a dirty flag and propagated to registered observers (business-side
 * callbacks) and to any data bindings that consume this property. Setting the same value is a
 * no-op and produces no change notification.
 */
class PAGViewModelValue : public std::enable_shared_from_this<PAGViewModelValue> {
 public:
  using Observer = std::function<void()>;

  virtual ~PAGViewModelValue() = default;

  /**
   * Adds an observer callback that is invoked when the value changes. Notification is immediate
   * unless a SuppressDelegation scope is active, in which case it is coalesced and delivered at
   * scope exit. Returns an ObserverHandle; the observer is automatically removed when the handle
   * is destroyed or detached.
   */
  ObserverHandle addObserver(Observer observer);

  /**
   * Returns the property name as declared in the ViewModel schema.
   */
  const std::string& name() const {
    return propertyName;
  }

  /**
   * Returns true if the value has changed since the last frame-level clearDirty() call.
   */
  bool hasChanged() const {
    return dirty;
  }

  /**
   * Returns the runtime value type of this property.
   */
  ViewModelPropertyType valueType() const {
    return type;
  }

  /**
   * Returns the PropertyData reflection metadata for this property, holding the schema-derived
   * information such as the default value and the customData map. Created during ViewModel
   * instantiation, so a schema-built value always has non-null PropertyData.
   */
  std::shared_ptr<PropertyData> propertyData() const {
    return pd;
  }

 protected:
  PAGViewModelValue() = default;

  /**
   * Invoked by notifyChanged(): dispatches the registered observers, or defers them when
   * SuppressDelegation is active on the owning scene. It does not touch the dirty flag or notify
   * dependent data bindings; notifyChanged() orchestrates those for full propagation.
   */
  void notifyValueChanged();

  /**
   * Dispatches change notification after a subclass updates its stored value. When fromVM is
   * true, marks this value dirty and notifies both observers and dependent data bindings.
   * When fromVM is false, notifies observers only.
   */
  void notifyChanged(bool fromVM);

  /**
   * Notifies all registered observers that this value has changed.
   */
  void notifyObservers();

  /**
   * Resets the dirty flag. Called at the end of each frame after DataBinds have been applied.
   */
  void clearDirty();

  std::string propertyName = {};
  bool dirty = false;
  ViewModelPropertyType type = ViewModelPropertyType::Number;
  std::weak_ptr<PAGScene> scene = {};
  bool notifying = false;
  DataConverter* converter = nullptr;
  std::shared_ptr<PropertyData> pd;

  /**
   * Notifies the data bindings that depend on this value that it has changed.
   */
  void notifyDependents();

 private:
  struct ObserverEntry {
    int id = 0;
    Observer callback;
  };
  std::vector<ObserverEntry> observers = {};
  int nextObserverId = 1;
  std::vector<DataBindRuntime*> dependents = {};

  void removeObserver(int id);

  void setScene(const std::shared_ptr<PAGScene>& scene) {
    this->scene = scene;
  }

  void addDependent(DataBindRuntime* runtime);

  void removeDependent(DataBindRuntime* runtime);

  friend class ObserverHandle;
  friend class PAGViewModel;
  friend class PAGScene;
  friend class SuppressDelegation;
  friend class DataBindRuntime;
};

}  // namespace pagx
