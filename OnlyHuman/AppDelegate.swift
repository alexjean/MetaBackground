//
//  AppDelegate.swift
//  OnlyHuman
//
//  Created by Alex on 2021/12/9.
//

import Cocoa

@main
class AppDelegate: NSObject, NSApplicationDelegate {

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Insert code here to initialize your application
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }
    
    func applicationDidBecomeActive(_ notification: Notification) {
        let list = NSApplication.shared.windows
        if list.isEmpty { return }
        let win = list[0]
        win.styleMask.update(with: .titled)
        // this application only 1 viewController
        guard let nSViewController = win.contentViewController else { return }
        let viewController = nSViewController as! ViewController
        viewController.panel(hide:false)
    }
    
    func applicationDidResignActive(_ notification: Notification) {
        // NSApplication.shared.mainWindow  always nil
        let list = NSApplication.shared.windows
        if list.isEmpty { return}
        let win = list[0]
        win.styleMask.remove(.titled)
        // this application only 1 viewController
        guard let nSViewController = win.contentViewController else { return }
        let viewController = nSViewController as! ViewController
        viewController.panel(hide:true)
    }
    
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return true
    }

}

