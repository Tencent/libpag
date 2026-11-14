'use strict';

const { measureHierarchy } = require('../eval/hierarchy');

test('measures scene nesting separately from vector and resource structure', () => {
  expect(measureHierarchy(`<pagx><Resources><Composition><Layer><Layer/></Layer></Composition></Resources>
    <Layer mask="@mask"><Group><TextBox><Text/></TextBox></Group>
      <Layer id="mask"><Rectangle/></Layer><Layer><Layer/></Layer>
    </Layer></pagx>`)).toEqual({
    layers: 4, layerDepth: 3, structureDepth: 3, groups: 2, masks: 1,
    internalPaintMarkers: 0, depthHistogram: { 1: 1, 2: 2, 3: 1 },
  });
});

test('vector wrappers remain observable even when there is only one Layer', () => {
  expect(measureHierarchy('<pagx><Layer data-pagx-paint-order="fixed"><Group><Group/></Group></Layer></pagx>'))
    .toMatchObject({ layers: 1, layerDepth: 1, structureDepth: 3, internalPaintMarkers: 1 });
});

test('rejects non-PAGX and malformed inputs', () => {
  expect(() => measureHierarchy('<html/>')).toThrow('Expected a PAGX document');
  expect(() => measureHierarchy('<pagx><Layer></pagx>')).toThrow();
});
