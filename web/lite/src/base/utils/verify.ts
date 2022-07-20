export const verifyFailed = () => {
  console.error('PAG Verify Failed!');
};

export const verifyAndReturn = (expression: boolean): boolean => {
  if (expression) {
    return true;
  }
  console.error('PAG Verify Failed!');
  return false;
};
