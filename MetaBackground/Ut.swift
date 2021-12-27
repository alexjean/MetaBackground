//
//  Ut.swift
//  MetaBackground
//
//  Created by Alex on 2021/12/23.
//

import Foundation
import AppKit
// origin lower-left on AppKit & Cocoa
// origin upper-left on UIKit  & CoreGraphics 
public struct RECT
 {
    public var left:CGFloat , top:CGFloat
    public var right:CGFloat, bottom:CGFloat
    public var width:CGFloat  { return right - left }
    public var height:CGFloat { return bottom - top }

    public init(_ x:Int, _ y:Int, _ width:Int, _ height:Int)
    {
        left = CGFloat(x); top = CGFloat(y)
        right = left + CGFloat(width)
        bottom = top + CGFloat(height)
    }
    public init(_ x:CGFloat, _ y:CGFloat, _ width:CGFloat, _ height:CGFloat)
    {
        left = x; top = y
        right = x + width
        bottom = y + height
    }
    public func toCGRect() -> CGRect { return  CGRect(x:left, y:top, width:width, height:height) }
    public static let zero = RECT(0, 0, 0, 0)
    public var isEmpty:Bool {
        if (left == 0 && right == 0 && bottom == 0 && height == 0) { return true }
        return false
    }
 }

class Ut {
    static let shared = Ut()
    private init() {}
    
    public func getAppByName(appName:String) -> [NSRunningApplication] {
        let workspace = NSWorkspace.shared
        var index = 1 ,name:String
        var apps:[NSRunningApplication] = []
        for app in workspace.runningApplications {
            name = app.localizedName ?? ""
            if (name == appName) {
                apps.append(app)
                print("\(index) \(name) <== Got it!")
            } else {
                print("\(index) \(name)")
            }
            index += 1
        }
        return apps
    }
    
    public func getWindowID(appId: pid_t, windowTitle: String) -> UInt32 {
        guard let infoList = CGWindowListCopyWindowInfo(.optionAll, .zero) else { return 0 }
        defer {  }
        guard let windows = infoList as NSArray? as? [[String: AnyObject]] else { return 0 }
        for win in windows {
            guard let pid = win["kCGWindowOwnerPID"] as? pid_t else { continue }
            if pid != appId { continue }
            guard let name =  win["kCGWindowName"] as? String else { continue }
            if name == windowTitle {
                guard let winNo = win["kCGWindowNumber"] else { continue }
                return winNo as? UInt32 ?? 0
            }
        }
        return 0
    }
    
    public func getWindowID(bundleID:String, winTitle:String) -> CGWindowID {
        var windowID = CGWindowID.zero
        guard let windowInfoList = CGWindowListCopyWindowInfo(.optionAll, kCGNullWindowID)
                as NSArray? as? [[String: AnyObject]] else { return windowID }
        let apps = NSRunningApplication.runningApplications(withBundleIdentifier:bundleID)
        if apps.isEmpty {
            print("The \(bundleID) is not running")
            return windowID
        }
        let appPID = apps[0].processIdentifier
        
        var appWindows:[[String:AnyObject]] = []
        for info in windowInfoList {
            guard let pid = info[kCGWindowOwnerPID as String] as? pid_t else { continue }
            if pid == appPID {  appWindows.append(info) }
        }
        for win in appWindows {
            guard let title = win[kCGWindowName as String] as? String else { continue }
            if title == winTitle  {
                guard let number = win[kCGWindowNumber as String] as? NSNumber else { continue }
                windowID = number as! CGWindowID
                break
            }
        }
        return windowID
    }
    
    public func getWindowRect(winId:UInt32, removeTitle:Bool = true) -> RECT
    {   // 這裹的左上是(0,0)  x y己經都除BackingScaleFactor, 含了TitleBar
        guard let infoList = CGWindowListCopyWindowInfo([.optionIncludingWindow,.excludeDesktopElements], winId) else {
            return RECT.zero
        }
        guard let windows = infoList as NSArray? as? [[String:AnyObject]]  else { return RECT.zero }
        if windows.count == 0 { return RECT.zero }
        let win = windows[0]
        guard let bounds = win["kCGWindowBounds"] else { return RECT.zero }
        guard let width  = bounds["Width"] as? CGFloat else { return RECT.zero }
        guard let height = bounds["Height"] as? CGFloat else { return RECT.zero }
        guard let x      = bounds["X"] as? CGFloat else { return RECT.zero }
        guard let y      = bounds["Y"] as? CGFloat else { return RECT.zero }
        if removeTitle {  return RECT(x,y + 28, width, height - 28)  } //猜的Title高28 
        return RECT(x, y, width, height)
    }
    
    // Mac 此處rect呼叫前要先除BackingScaleFactor
    public func captureWindowFromScreen(winId:UInt32, rect:RECT) -> CGImage?
    {
        let client = getWindowRect(winId: winId)
        if client.height == 0 || client.width == 0 { return nil }
        var tile:RECT
        if rect.isEmpty {
            tile = client
        } else {
            let x = client.left + rect.left ;
            let y = client.top + rect.top;
            //let y = client.Top + (client.Height - rect.Top);
            tile = RECT(x, y, rect.width, rect.height);
        }
        let img = CGWindowListCreateImage(tile.toCGRect(),
                                          [.optionIncludingWindow, .excludeDesktopElements],
                                          winId, .bestResolution)
        return img
    }

}
