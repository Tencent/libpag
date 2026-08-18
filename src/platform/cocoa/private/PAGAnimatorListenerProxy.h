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

#import <Foundation/Foundation.h>
#import "PAGAnimator.h"

/**
 * The object that receives the animator events relayed by a PAGAnimatorListenerProxy. PAGView and
 * PAGImageView adopt this protocol to dispatch the events to their own public listener list.
 */
@protocol PAGViewAnimatorForwarder <NSObject>

/**
 * Dispatches the animation event identified by the selector to the public listeners.
 */
- (void)dispatchListenerEvent:(SEL)selector;

@end

/**
 * A standalone listener that receives PAGAnimator events and relays them to a forwarder. It
 * decouples the internal "animator listener" role from the view itself. Without this proxy, the
 * view has to implement PAGAnimatorListener directly, whose selectors collide with the public
 * PAGViewListener selectors. A view subclass that also implements the public listener protocol
 * would then override the internal animator callbacks, and registering the view itself as a
 * listener would recurse infinitely. Relaying through a dedicated proxy keeps the two roles
 * separated so the collision can no longer happen.
 */
@interface PAGAnimatorListenerProxy : NSObject <PAGAnimatorListener>

/**
 * Initializes the proxy with the forwarder that receives the relayed events. The proxy holds an
 * unretained reference to the forwarder, so the forwarder must outlive the proxy or call detach
 * before it is deallocated.
 */
- (instancetype)initWithForwarder:(id<PAGViewAnimatorForwarder>)forwarder;

/**
 * Drops the reference to the forwarder. Must be called before the forwarder is deallocated so that
 * no event is relayed to a dangling forwarder.
 */
- (void)detach;

@end
