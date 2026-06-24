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

namespace pagx {

class ObserverHandle;
class PAGScene;
class PAGViewModel;
class PropertyData;
class DataBindRuntime;

/**
 * ViewModelValueType enumerates the runtime value types of a PAGViewModelValue.
 */
enum class ViewModelValueType { Number, String, Boolean, Color, Image, ViewModel, Enum, Trigger };

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
   * did not define customData for this property. Created during ViewModel instantiation.
   */
  std::shared_ptr<PropertyData> propertyData() const {
    return pd;
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
   * Notifies registered observers, or defers notification when SuppressDelegation is active on
   * the owning scene. This does NOT notify dependent DataBindRuntime instances — callers must
   * also invoke notifyDependents() for full change propagation to the DataBind update chain.
   * Subclasses call this after updating their stored value.
   */
  void notifyValueChanged();

  /**
   * Dispatches change notification after a subclass updates its stored value. When fromVM is
   * true, marks this value dirty and notifies both observers and dependent DataBindRuntimes.
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
  void advanced();

  std::string propertyName = {};
  bool dirty = false;
  ViewModelValueType type = ViewModelValueType::Number;
  std::weak_ptr<PAGScene> scene = {};
  bool notifying = false;
  class DataConverter* converter = nullptr;
  std::shared_ptr<PropertyData> pd;

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
  std::vector<DataBindRuntime*> dependents = {};

  void removeObserver(int id);

  void setScene(const std::shared_ptr<PAGScene>& scene) {
    this->scene = scene;
  }

  friend class ObserverHandle;
  friend class PAGViewModel;
  friend class PAGScene;
  friend class SuppressDelegation;
  friend class DataBindRuntime;
};

}  // namespace pagx
