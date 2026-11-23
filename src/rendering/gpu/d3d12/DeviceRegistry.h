#pragma once

// Internal device registry implementation. This file is included inside the anonymous namespace
// of Devices.cpp so every helper keeps internal linkage; it is split out only to keep the D3D12
// device import code separate from validation and rendering entry points.
// TODO: Fold this registry into tgfx Device::RegisterNative once DeviceKey supports D3D12.

using DeviceHealthProbe = HRESULT (*)(void* device);
std::atomic<DeviceHealthProbe> deviceHealthProbe = nullptr;

HRESULT QueryDeviceRemovedReason(ID3D12Device* device) {
  auto probe = deviceHealthProbe.load();
  return probe != nullptr ? probe(device) : device->GetDeviceRemovedReason();
}

/**
 * One live tgfx device, keyed by the canonical IUnknown identity of its native ID3D12Device.
 * The identity is strongly held so pointer comparison stays valid after the caller releases its
 * own reference; the tgfx device is kept alive by the entry while any aliased owner exists.
 */
struct DeviceEntry {
  IUnknown* identity = nullptr;
  std::shared_ptr<tgfx::D3D12Device> device = nullptr;

  DeviceEntry() = default;

  DeviceEntry(const DeviceEntry&) = delete;
  DeviceEntry& operator=(const DeviceEntry&) = delete;

  ~DeviceEntry() {
    if (identity != nullptr) {
      identity->Release();
    }
  }
};

/**
 * Borrows the native identity only while the tracked tgfx device is alive. The cache must not
 * retain either device: an expired owner invalidates the borrowed pointer without dereferencing it.
 */
struct ContextIdentity {
  std::weak_ptr<tgfx::Device> owner = {};
  IUnknown* identity = nullptr;
};

struct Registry {
  std::mutex locker = {};
  std::vector<std::weak_ptr<DeviceEntry>> entries = {};
  std::weak_ptr<DeviceEntry> defaultEntry = {};
  std::unordered_map<uint32_t, ContextIdentity> contextIdentities = {};
  size_t identityCaptureCount = 0;
  size_t deviceCreationCount = 0;
};

void PurgeContextIdentitiesLocked(Registry& registry) {
  for (auto iterator = registry.contextIdentities.begin();
       iterator != registry.contextIdentities.end();) {
    if (iterator->second.owner.expired()) {
      iterator = registry.contextIdentities.erase(iterator);
    } else {
      ++iterator;
    }
  }
}

std::mutex& GetCreationLocker() {
  static std::mutex locker = {};
  return locker;
}

bool IsEntryHealthy(const std::shared_ptr<DeviceEntry>& entry) {
  if (entry == nullptr || entry->identity == nullptr) {
    return false;
  }
  ID3D12Device* nativeDevice = nullptr;
  entry->identity->QueryInterface(IID_PPV_ARGS(&nativeDevice));
  if (nativeDevice == nullptr) {
    return false;
  }
  auto reason = QueryDeviceRemovedReason(nativeDevice);
  nativeDevice->Release();
  return SUCCEEDED(reason);
}

void EraseEntryLocked(Registry& registry, DeviceEntry* target) {
  auto defaultEntry = registry.defaultEntry.lock();
  if (defaultEntry.get() == target) {
    registry.defaultEntry.reset();
  }
  for (auto iterator = registry.entries.begin(); iterator != registry.entries.end();) {
    auto entry = iterator->lock();
    if (entry == nullptr || entry.get() == target) {
      iterator = registry.entries.erase(iterator);
    } else {
      ++iterator;
    }
  }
}

Registry& GetRegistry() {
  static Registry registry = {};
  return registry;
}

std::shared_ptr<DeviceEntry> FindEntryLocked(Registry& registry, IUnknown* identity) {
  auto iterator = registry.entries.begin();
  while (iterator != registry.entries.end()) {
    auto entry = iterator->lock();
    if (entry == nullptr) {
      iterator = registry.entries.erase(iterator);
      continue;
    }
    if (entry->identity == identity) {
      return entry;
    }
    ++iterator;
  }
  return nullptr;
}

std::shared_ptr<DeviceEntry> PublishEntry(IUnknown* identity,
                                          std::shared_ptr<tgfx::D3D12Device> device) {
  std::shared_ptr<DeviceEntry> existing = nullptr;
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    existing = FindEntryLocked(registry, identity);
    if (existing == nullptr) {
      auto entry = std::make_shared<DeviceEntry>();
      entry->identity = identity;
      identity = nullptr;
      entry->device = std::move(device);
      registry.entries.push_back(entry);
      return entry;
    }
  }
  if (identity != nullptr) {
    identity->Release();
  }
  return existing;
}

std::shared_ptr<tgfx::Device> MakeAliasedDevice(const std::shared_ptr<DeviceEntry>& entry) {
  if (entry == nullptr || entry->device == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<tgfx::Device>(entry, entry->device.get());
}

ID3D12Resource* GetResource(const tgfx::D3D12TextureInfo& info) {
  return const_cast<ID3D12Resource*>(static_cast<const ID3D12Resource*>(info.resource));
}

IUnknown* CaptureIdentityFromResource(ID3D12Resource* resource) {
  if (resource == nullptr) {
    return nullptr;
  }
  ID3D12Device* nativeDevice = nullptr;
  if (FAILED(resource->GetDevice(IID_PPV_ARGS(&nativeDevice))) || nativeDevice == nullptr) {
    return nullptr;
  }
  IUnknown* identity = nullptr;
  nativeDevice->QueryInterface(IID_PPV_ARGS(&identity));
  nativeDevice->Release();
  return identity;
}

std::shared_ptr<DeviceEntry> RegisterContextDevice(tgfx::Context* context) {
  if (context == nullptr) {
    return nullptr;
  }
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    for (auto& weakEntry : registry.entries) {
      auto entry = weakEntry.lock();
      if (entry != nullptr && entry->device.get() == context->device()) {
        return entry;
      }
    }
  }
  return nullptr;
}

IUnknown* CaptureIdentityFromContext(tgfx::Context* context) {
  if (context == nullptr || context->device() == nullptr) {
    return nullptr;
  }
  auto entry = RegisterContextDevice(context);
  if (entry != nullptr) {
    entry->identity->AddRef();
    return entry->identity;
  }
  auto deviceID = context->device()->uniqueID();
  std::shared_ptr<tgfx::Device> owner = nullptr;
  IUnknown* identity = nullptr;
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    PurgeContextIdentitiesLocked(registry);
    auto iterator = registry.contextIdentities.find(deviceID);
    if (iterator != registry.contextIdentities.end()) {
      owner = iterator->second.owner.lock();
      if (owner.get() == context->device() && iterator->second.identity != nullptr) {
        identity = iterator->second.identity;
        identity->AddRef();
      }
    }
    if (identity == nullptr) {
      registry.identityCaptureCount++;
    }
  }
  if (identity != nullptr) {
    return identity;
  }
  {
    tgfx::TextureDescriptor descriptor(1, 1, tgfx::PixelFormat::RGBA_8888, false, 1,
                                       tgfx::TextureUsage::TEXTURE_BINDING);
    auto texture = context->gpu()->createTexture(descriptor);
    if (texture != nullptr) {
      auto backendTexture = texture->getBackendTexture();
      tgfx::D3D12TextureInfo info = {};
      if (backendTexture.getD3D12TextureInfo(&info)) {
        identity = CaptureIdentityFromResource(GetResource(info));
      }
    }
  }
  if (identity != nullptr && owner.get() == context->device()) {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    auto iterator = registry.contextIdentities.find(deviceID);
    if (iterator != registry.contextIdentities.end()) {
      iterator->second.identity = identity;
    }
  }
  return identity;
}

std::shared_ptr<DeviceEntry> RegisterNewDevice(std::shared_ptr<tgfx::D3D12Device> device) {
  if (device == nullptr) {
    return nullptr;
  }
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    registry.deviceCreationCount++;
  }
  Devices::TrackContextDevice(device);
  auto context = device->lockContext();
  if (context == nullptr) {
    LOGE("Devices::MakeDefault() Failed to lock the new D3D12 device.");
    return nullptr;
  }
  auto identity = CaptureIdentityFromContext(context);
  device->unlock();
  if (identity == nullptr) {
    LOGE("Devices::MakeDefault() Failed to capture the native D3D12 device identity.");
    return nullptr;
  }
  return PublishEntry(identity, std::move(device));
}

std::shared_ptr<DeviceEntry> FindHealthyEntryLocked(Registry& registry, IUnknown* identity) {
  auto existing = FindEntryLocked(registry, identity);
  if (existing == nullptr) {
    return nullptr;
  }
  if (IsEntryHealthy(existing)) {
    return existing;
  }
  EraseEntryLocked(registry, existing.get());
  return nullptr;
}

std::shared_ptr<DeviceEntry> GetOrCreateEntry(ID3D12Resource* resource) {
  auto identity = CaptureIdentityFromResource(resource);
  if (identity == nullptr) {
    return nullptr;
  }
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    auto existing = FindHealthyEntryLocked(registry, identity);
    if (existing != nullptr) {
      identity->Release();
      return existing;
    }
  }
  std::lock_guard<std::mutex> creationLock(GetCreationLocker());
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    auto existing = FindHealthyEntryLocked(registry, identity);
    if (existing != nullptr) {
      identity->Release();
      return existing;
    }
  }
  ID3D12Device* nativeDevice = nullptr;
  if (FAILED(resource->GetDevice(IID_PPV_ARGS(&nativeDevice))) || nativeDevice == nullptr) {
    identity->Release();
    return nullptr;
  }
  auto device = tgfx::D3D12Device::MakeFrom(nativeDevice);
  nativeDevice->Release();
  if (device == nullptr) {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    auto existing = FindEntryLocked(registry, identity);
    identity->Release();
    return existing;
  }
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    registry.deviceCreationCount++;
  }
  return PublishEntry(identity, std::move(device));
}

size_t CountRegisteredDevices() {
  auto& registry = GetRegistry();
  std::lock_guard<std::mutex> autoLock(registry.locker);
  size_t count = 0;
  for (auto& weakEntry : registry.entries) {
    if (weakEntry.lock() != nullptr) {
      count++;
    }
  }
  return count;
}
