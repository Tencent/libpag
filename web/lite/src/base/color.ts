export interface Color {
  red: number; // in the range [0 - 255]
  green: number;
  blue: number;
}

export const Black: Color = { red: 0, green: 0, blue: 0 };
export const White: Color = { red: 255, green: 255, blue: 255 };
export const Red: Color = { red: 255, green: 0, blue: 0 };
export const Green: Color = { red: 0, green: 255, blue: 0 };
export const Blue: Color = { red: 0, green: 0, blue: 255 };
