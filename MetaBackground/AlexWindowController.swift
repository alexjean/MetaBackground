//
//  AlexWindowController.swift
//  MetaBackground
//
//  Created by Alex on 2021/12/30.
//

import Cocoa

class AlexWindowController: NSWindowController, NSWindowDelegate {

    override func windowDidLoad() {
        super.windowDidLoad()
    
        // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
    }
    
//    var counter = 0
    func windowDidResize(_ notification: Notification) {
//        print("window resized \(counter)!")
//        counter += 1
        guard let view = window?.contentViewController as? ViewController else { return }
        if view.m_dataProcess.isBlocking() {  // on origin mode
            view.m_capturer.linkPreview(imageView: view.videoView)
        }
    }

}
