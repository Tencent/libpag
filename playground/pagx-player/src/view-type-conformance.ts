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

// Dev-only type conformance check. The player deliberately keeps its own copies of the data-only
// shapes that cross the wasm boundary so the published bundle carries no reference to pagx-viewer.
// Those copies are hand-synced to the marshalling in PAGXView.cpp and to the viewer's own types;
// without a compile-time link, a renamed field compiles clean here and fails only at runtime.
// This module is not part of the rollup entry graph, so it never enters the emitted bundle or .d.ts.
// `npm run typecheck` includes it via tsconfig and turns any drift into a build error.
import type {
    HitTestResult as ViewerHitTestResult,
    NodeBounds as ViewerNodeBounds,
    NodeSourceEntry as ViewerNodeSourceEntry,
    PagxSchemaDiagnostic as ViewerPagxSchemaDiagnostic,
} from '../../pagx-viewer/src/ts/types';
import type {
    HitTestResult as PlayerHitTestResult,
    NodeBounds as PlayerNodeBounds,
    NodeSourceEntry as PlayerNodeSourceEntry,
    PagxSchemaDiagnostic as PlayerPagxSchemaDiagnostic,
} from './pagx-view-types';

// Bidirectional assignment requires the same field set on both sides: a field added or removed on
// either side fails one of the two assertions. These declarations are erased at build time.
type AssertConformance<T extends U, U extends T> = [T, U];

type NodeBoundsConformance =
    AssertConformance<PlayerNodeBounds, ViewerNodeBounds>;
type NodeSourceEntryConformance =
    AssertConformance<PlayerNodeSourceEntry, ViewerNodeSourceEntry>;
type HitTestResultConformance =
    AssertConformance<PlayerHitTestResult, ViewerHitTestResult>;
type PagxSchemaDiagnosticConformance =
    AssertConformance<PlayerPagxSchemaDiagnostic, ViewerPagxSchemaDiagnostic>;

export type ViewTypeConformance = [
    NodeBoundsConformance,
    NodeSourceEntryConformance,
    HitTestResultConformance,
    PagxSchemaDiagnosticConformance,
];
