//
//  MoveAndRotateLayerViewController.swift
//  pag-ios
//
//  Created by lvpengwei on 2021/3/4.
//  Copyright © 2021 PAG. All rights reserved.
//

import UIKit

class MoveAndRotateLayerViewController: BaseViewController {
    @IBOutlet weak var contentView: UIView!
    override func viewDidLoad() {
        super.viewDidLoad()
        pagView.removeFromSuperview()
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        pagView.frame = contentView.bounds
        contentView.addSubview(pagView)
    }
    
    private enum ActionType {
        case move_up
        case move_left
        case move_right
        case move_down
        case rotate_left
        case rotate_right
    }
    @IBAction func upAction(_ sender: Any) {
        updateMatrix(ActionType.move_up)
    }
    @IBAction func downAction(_ sender: Any) {
        updateMatrix(ActionType.move_down)
    }
    @IBAction func rightAction(_ sender: Any) {
        updateMatrix(ActionType.move_right)
    }
    @IBAction func leftAction(_ sender: Any) {
        updateMatrix(ActionType.move_left)
    }
    @IBAction func leftRotateAction(_ sender: Any) {
        updateMatrix(ActionType.rotate_left)
    }
    @IBAction func rightRotateAction(_ sender: Any) {
        updateMatrix(ActionType.rotate_right)
    }
    private let MOVE_DISTANCE: CGFloat = 10
    private let ROTATE_DEGREE: CGFloat = CGFloat(Double.pi / 6)
    private var moveX: CGFloat = 0
    private var moveY: CGFloat = 0
    private var rotation: CGFloat = 0
    private func updateMatrix(_ type: ActionType) {
        guard let layer = textLayer() else { return }
        switch type {
        case .move_up:
            moveY -= MOVE_DISTANCE
        case .move_down:
            moveY += MOVE_DISTANCE
        case .move_left:
            moveX -= MOVE_DISTANCE
        case .move_right:
            moveX += MOVE_DISTANCE
        case .rotate_left:
            rotation -= ROTATE_DEGREE
        case .rotate_right:
            rotation += ROTATE_DEGREE
        }
        var matrix = CGAffineTransform.identity
        // move
        matrix = matrix.translatedBy(x: moveX, y: moveY)
        // rotate
        layer.resetMatrix()
        var rect = layer.getBounds()
        rect = rect.applying(layer.getTotalMatrix())
        let centerX = rect.origin.x + rect.width * 0.5
        let centerY = rect.origin.y + rect.height * 0.5
        matrix = matrix.translatedBy(x: centerX, y: centerY)
        matrix = matrix.rotated(by: rotation)
        matrix = matrix.translatedBy(x: -centerX, y: -centerY)
        layer.setMatrix(matrix)
    }
    private func textLayer() -> PAGTextLayer? {
        return pagFile.getLayersByEditableIndex(0, layerType: PAGLayerType.text)?.first as? PAGTextLayer
    }
    @objc override func resourcePath() -> String! {
        return Bundle.main.path(forResource: "test2", ofType: "pag")
    }
}
