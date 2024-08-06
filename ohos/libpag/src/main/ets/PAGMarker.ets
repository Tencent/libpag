/**
 * Marker stores comments and other metadata and mark important times in a composition or layer.
 */
export interface PAGMarker {
  startTime: number
  duration: number
  comment: string
}