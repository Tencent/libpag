const compare = (array_1: Uint8Array | Uint8ClampedArray, array_2: Uint8Array | Uint8ClampedArray) => {
  for (let i = 0; i < array_1.length; i++) {
    if (array_1[i] !== array_2[i]) return false;
  }
  return true;
};

const nodeUint8Array2uint8Array = (data: { [key: string]: number }) => {
  return new Uint8Array(Object.keys(data).map((key) => data[key]));
};

export const saveData = (filename: string, data: Uint8Array) => {
  return cy.task('saveData', { filename, data });
};

export const compareData = (filename, compareArray: Uint8Array | Uint8ClampedArray) => {
  cy.task('readData', filename).then(async (data: { [key: string]: number }) => {
    expect(!!data).to.be.true;
    const buffer = nodeUint8Array2uint8Array(data);
    const res = compare(compareArray, buffer);
    expect(res).to.be.true;
  });
};
